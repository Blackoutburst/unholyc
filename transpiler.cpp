#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

enum class TK {
    IDENT,
    NUMBER,
    STRING,
    CHAR_LIT,
    LINE_COMMENT,
    BLOCK_COMMENT,
    PREPROCESSOR,
    DOT,
    SCOPE,
    LBRACE,
    RBRACE,
    LPAREN,
    RPAREN,
    SEMICOLON,
    OTHER,
    END
};

struct Token {
    TK          type;
    std::string value;
    int         line;
};

class Lexer {
    const std::string& src;
    size_t pos  = 0;
    int    line = 1;

    char peek(int off = 0) const {
        size_t i = pos + off;
        return i < src.size() ? src[i] : '\0';
    }

    char advance() {
        char c = src[pos++];
        if (c == '\n') line++;
        return c;
    }

    std::string readUntil(const std::string& end) {
        std::string val;
        while (pos < src.size()) {
            if (src.compare(pos, end.size(), end) == 0) {
                for (size_t i = 0; i < end.size(); i++) val += advance();
                return val;
            }
            val += advance();
        }
        return val;
    }

    std::string readLineRest() {
        std::string val;
        while (pos < src.size() && peek() != '\n') val += advance();
        return val;
    }

    Token lexString(char delim) {
        std::string val(1, advance());
        while (pos < src.size() && peek() != delim) {
            if (peek() == '\\') val += advance();
            if (pos < src.size()) val += advance();
        }
        if (pos < src.size()) val += advance();
        return { delim == '"' ? TK::STRING : TK::CHAR_LIT, val, line };
    }

    Token lexNumber() {
        std::string val;
        if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
            val += advance(); val += advance();
            while (std::isxdigit(peek())) val += advance();
        } else {
            while (std::isdigit(peek())) val += advance();
            if (peek() == '.') {
                val += advance();
                while (std::isdigit(peek())) val += advance();
            }
            if (peek() == 'e' || peek() == 'E') {
                val += advance();
                if (peek() == '+' || peek() == '-') val += advance();
                while (std::isdigit(peek())) val += advance();
            }
        }
        while (peek() == 'f' || peek() == 'F' ||
               peek() == 'u' || peek() == 'U' ||
               peek() == 'l' || peek() == 'L') val += advance();
        return { TK::NUMBER, val, line };
    }

    Token lexIdent() {
        std::string val;
        while (std::isalnum(peek()) || peek() == '_') val += advance();
        return { TK::IDENT, val, line };
    }

public:
    explicit Lexer(const std::string& source) : src(source) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;

        while (pos < src.size()) {
            if (std::isspace(peek())) {
                std::string ws;
                while (pos < src.size() && std::isspace(peek())) {
                    ws += advance();
                }
                tokens.push_back({ TK::OTHER, ws, line });
                continue;
            }

            char c = peek();
            int  l = line;

            if (c == '/' && peek(1) == '/') {
                advance(); advance();
                std::string val = "//" + readLineRest();
                tokens.push_back({ TK::LINE_COMMENT, val, l });
                continue;
            }

            if (c == '/' && peek(1) == '*') {
                advance(); advance();
                std::string val = "/*" + readUntil("*/");
                tokens.push_back({ TK::BLOCK_COMMENT, val, l });
                continue;
            }

            if (c == '#') {
                std::string val = readLineRest();
                tokens.push_back({ TK::PREPROCESSOR, val, l });
                continue;
            }

            if (c == '"' || c == '\'') {
                tokens.push_back(lexString(c));
                continue;
            }

            if (std::isdigit(c) || (c == '.' && std::isdigit(peek(1)))) {
                tokens.push_back(lexNumber());
                continue;
            }

            if (std::isalpha(c) || c == '_') {
                tokens.push_back(lexIdent());
                continue;
            }

            if (c == ':' && peek(1) == ':') {
                advance(); advance();
                tokens.push_back({ TK::SCOPE, "::", l });
                continue;
            }

            advance();
            TK t = TK::OTHER;
            switch (c) {
                case '.': t = TK::DOT;       break;
                case '{': t = TK::LBRACE;    break;
                case '}': t = TK::RBRACE;    break;
                case '(': t = TK::LPAREN;    break;
                case ')': t = TK::RPAREN;    break;
                case ';': t = TK::SEMICOLON; break;
                default:  break;
            }
            tokens.push_back({ t, std::string(1, c), l });
        }

        tokens.push_back({ TK::END, "", line });
        return tokens;
    }
};

static const std::unordered_map<std::string, std::string> TYPE_MAP = {
    { "U0",   "void"               },
    { "U8",   "unsigned char"      },
    { "U16",  "unsigned short"     },
    { "U32",  "unsigned int"       },
    { "U64",  "unsigned long long" },
    { "I8",   "char"               },
    { "I16",  "short"              },
    { "I32",  "int"                },
    { "I64",  "long long"          },
    { "F32",  "float"              },
    { "F64",  "double"             },
    { "F128", "long double"        },
};

static const std::unordered_set<std::string> CONTROL_FLOW_KW = {
    "if", "for", "while", "do", "switch", "else"
};

// Keywords that suppress implicit semicolon insertion after themselves or their closing paren
static const std::unordered_set<std::string> NO_SEMI_KW = {
    "if","else","while","for","do","switch",
    "namespace","struct","class","enum","union","template"
};

// Keywords/tokens that begin a new statement — used to inject ; between two statements
// on the same line (e.g. `union { U32 i F32 f }`)
static const std::unordered_set<std::string> STMT_START_KW = {
    // UnholyC type keywords
    "U0","U8","U16","U32","U64","I8","I16","I32","I64","F32","F64","F128",
    // C/C++ type and storage keywords that can appear in UnholyC source
    "auto","const","static","volatile","unsigned","signed",
    "int","short","long","char","float","double","void",
    "struct","union","enum","class","typedef",
    // Statement-starting flow keywords
    "return","break","continue","goto",
};

static std::vector<Token> injectImplicitSemicolons(const std::vector<Token>& tokens) {
    std::vector<Token> out;
    out.reserve(tokens.size() + 64);

    int  lastReal        = -1;
    bool lastRParenWasCF = false;
    int  parenDepth      = 0;
    std::vector<bool> cfParenStack;
    // true = initializer list  {}; false = code block {}
    std::vector<bool> initBraceStack;

    // Keywords after which '{' introduces an initializer, not a code block
    static const std::unordered_set<std::string> initBraceKw = { "return", "throw" };

    auto isWS = [](const Token& t) -> bool {
        if (t.type != TK::OTHER || t.value.empty()) return false;
        for (char c : t.value)
            if (!std::isspace((unsigned char)c)) return false;
        return true;
    };

    // First non-WS, non-comment token at or after index start in the ORIGINAL token array
    auto firstRealAfter = [&](size_t start) -> const Token* {
        for (size_t k = start; k < tokens.size(); k++) {
            const Token& t = tokens[k];
            if (t.type == TK::END) break;
            if (isWS(t) || t.type == TK::LINE_COMMENT || t.type == TK::BLOCK_COMMENT)
                continue;
            return &t;
        }
        return nullptr;
    };

    auto qualifies = [&](int idx) -> bool {
        if (idx < 0) return false;
        // Never inject inside parentheses (multi-line function args, casts, etc.)
        if (parenDepth > 0) return false;
        // Never inject inside an initializer-list brace
        if (!initBraceStack.empty() && initBraceStack.back()) return false;
        const Token& t = out[idx];
        switch (t.type) {
            case TK::NUMBER: case TK::STRING: case TK::CHAR_LIT: case TK::RBRACE:
                return true;
            case TK::RPAREN:
                return !lastRParenWasCF;
            case TK::IDENT:
                return !NO_SEMI_KW.count(t.value);
            default:
                // Array subscript closer ] also ends a statement
                return t.type == TK::OTHER && t.value == "]";
        }
    };

    for (size_t ti = 0; ti < tokens.size(); ti++) {
        const Token& tok = tokens[ti];
        const bool ws     = isWS(tok);
        const bool hasNL  = ws && tok.value.find('\n') != std::string::npos;
        const bool isCmt  = tok.type == TK::LINE_COMMENT || tok.type == TK::BLOCK_COMMENT;
        const bool isPrep = tok.type == TK::PREPROCESSOR;
        const bool isEnd  = tok.type == TK::END;

        // Inject semicolon before a line break if the last real token qualifies
        if (hasNL && qualifies(lastReal)) {
            // Lookahead: skip injection if next real token starts a continuation
            // (ternary ? / :, and similar line-continuation operators)
            const Token* next = firstRealAfter(ti + 1);
            bool isContinuation = next && next->type == TK::OTHER &&
                (next->value == "?" || next->value == ":");
            if (!isContinuation)
                out.push_back({ TK::SEMICOLON, ";", out[lastReal].line });
        }

        // Inject ; before a code-block closing brace for the last in-line statement
        // e.g. `{ ...; va_end(a) }` → `{ ...; va_end(a); }`
        if (tok.type == TK::RBRACE &&
            !initBraceStack.empty() && !initBraceStack.back() &&
            qualifies(lastReal))
            out.push_back({ TK::SEMICOLON, ";", out[lastReal].line });

        // Inject ; before a TYPE keyword that starts a new statement on the same line
        // e.g. `union { U32 i F32 f }` → `union { U32 i; F32 f }`
        // Only inject when the previous real token genuinely ends a statement — NOT
        // when it is itself a type/storage qualifier (static, const, typedef, etc.).
        if (!ws && !isCmt && !isPrep && !isEnd &&
            tok.type == TK::IDENT && STMT_START_KW.count(tok.value) &&
            parenDepth == 0 &&
            (initBraceStack.empty() || !initBraceStack.back()) &&
            lastReal >= 0) {
            const Token& prev = out[lastReal];
            bool prevEndsStmt =
                prev.type == TK::NUMBER ||
                prev.type == TK::STRING ||
                prev.type == TK::CHAR_LIT ||
                prev.type == TK::RBRACE ||
                (prev.type == TK::OTHER && prev.value == "]") ||
                (prev.type == TK::RPAREN && !lastRParenWasCF) ||
                (prev.type == TK::IDENT &&
                 !STMT_START_KW.count(prev.value) &&
                 !NO_SEMI_KW.count(prev.value));
            if (prevEndsStmt)
                out.push_back({ TK::SEMICOLON, ";", tok.line });
        }

        // Track paren depth and control-flow parens
        if (tok.type == TK::LPAREN) {
            parenDepth++;
            bool cf = lastReal >= 0 && out[lastReal].type == TK::IDENT &&
                      CONTROL_FLOW_KW.count(out[lastReal].value);
            cfParenStack.push_back(cf);
        } else if (tok.type == TK::RPAREN) {
            if (parenDepth > 0) parenDepth--;
            if (!cfParenStack.empty()) {
                lastRParenWasCF = cfParenStack.back();
                cfParenStack.pop_back();
            } else {
                lastRParenWasCF = false;
            }
        }

        // Track whether each brace level is an initializer list or a code block.
        // Rule: code block if preceded by IDENT (non-return/throw), RPAREN, or RBRACE;
        //       initializer list otherwise (preceded by =, ,, [, etc. or return/throw).
        if (tok.type == TK::LBRACE) {
            bool isInit = false;
            if (lastReal >= 0) {
                const Token& prev = out[lastReal];
                if (prev.type == TK::IDENT)
                    isInit = initBraceKw.count(prev.value) > 0;
                else if (prev.type == TK::RPAREN || prev.type == TK::RBRACE)
                    isInit = false;
                else
                    isInit = true; // OTHER (=, ,, [, etc.), LBRACE, LPAREN, ...
            }
            initBraceStack.push_back(isInit);
        } else if (tok.type == TK::RBRACE) {
            if (!initBraceStack.empty()) initBraceStack.pop_back();
        }

        out.push_back(tok);

        // Update lastReal for meaningful tokens
        if (!ws && !isCmt && !isPrep && !isEnd) {
            lastReal = (int)out.size() - 1;
            if (tok.type != TK::RPAREN)
                lastRParenWasCF = false;
        }

        // Reset line context after a line boundary
        if (hasNL || isPrep || (isCmt && tok.value.find('\n') != std::string::npos)) {
            lastReal        = -1;
            lastRParenWasCF = false;
        }
    }

    return out;
}

std::unordered_set<std::string> collectNamespaces(const std::vector<Token>& tokens) {
    std::unordered_set<std::string> ns;

    for (size_t i = 0; i + 1 < tokens.size(); i++) {
        if (tokens[i].type == TK::IDENT && tokens[i].value == "namespace") {
            size_t j = i + 1;

            while (j < tokens.size() && tokens[j].type == TK::OTHER) j++;

            if (j < tokens.size() && tokens[j].type == TK::IDENT)
                ns.insert(tokens[j].value);
        }
    }
    return ns;
}

struct ScopeVar {
    std::string name;
    std::string cType;
};

struct LambdaReg {
    std::string              lambdaParamName;
    std::string              calleeName;
    std::vector<std::string> argCTypes;
    std::string              retCType;
};

struct PatchSite {
    size_t      outputByteOffset;
    std::string calleeName;
    size_t      fptrInnerCloseOffset;
    std::string lambdaParamName;
};

struct CallSiteCapture {
    std::string calleeName;
    int         sourceLine;
    std::vector<std::pair<std::string,std::string>> captures;
};

struct CallFrame {
    std::string calleeName;
    int         parenDepthAtOpen;
    size_t      outLenAtArgStart;
};

static uint32_t fnv1a32(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

class Transpiler {
    const std::vector<Token>&            tokens;
    const std::unordered_set<std::string>& namespaces;

    std::ostringstream out;
    std::ostringstream preamble;

    std::unordered_map<std::string, LambdaReg> lambdaRegistry;

    std::vector<PatchSite> patchSites;

    std::vector<CallSiteCapture> callSiteCaptures;

    std::vector<ScopeVar> scopeVars;
    bool inFunctionBody    = false;
    int  braceDepth        = 0;
    int  globalBraceDepth  = 0;

    std::vector<CallFrame> callStack;

    std::unordered_map<std::string, int> lambdaNameCount;

    int parenDepth = 0;

    std::string pendingFuncForBrace;
    size_t      pendingFuncParamIdx = 0;

    size_t nextNonWS(size_t i) const {
        while (i < tokens.size() && tokens[i].type == TK::OTHER &&
               !tokens[i].value.empty() && std::isspace((unsigned char)tokens[i].value[0]))
            i++;
        return i;
    }

    std::string resolveType(const std::string& v) const {
        auto it = TYPE_MAP.find(v);
        return (it != TYPE_MAP.end()) ? it->second : v;
    }

    std::string parseCType(size_t& i) {
        i = nextNonWS(i);
        if (i >= tokens.size() || tokens[i].type == TK::END) return "";

        std::string result;

        if (tokens[i].type == TK::IDENT) {
            std::string base = tokens[i].value;
            size_t j = nextNonWS(i + 1);
            if (j < tokens.size() && tokens[j].type == TK::DOT) {
                size_t k = nextNonWS(j + 1);
                if (k < tokens.size() && tokens[k].type == TK::IDENT) {
                    result = base + "::" + tokens[k].value;
                    i = k + 1;
                } else {
                    result = resolveType(base);
                    i++;
                }
            } else {
                result = resolveType(base);
                i++;
            }
        } else {
            result = tokens[i].value;
            i++;
        }

        while (true) {
            size_t j = nextNonWS(i);
            if (j < tokens.size() && tokens[j].type == TK::OTHER &&
                (tokens[j].value == "*" || tokens[j].value == "&")) {
                result += tokens[j].value;
                i = j + 1;
            } else {
                break;
            }
        }

        return result;
    }

    void parseLambdaDecl(size_t& i, const std::string& calleeName) {
        i = nextNonWS(i);

        std::string lambdaParamName;
        if (i < tokens.size() && tokens[i].type == TK::IDENT) {
            lambdaParamName = tokens[i].value;
            i++;
        } else {
            std::cerr << "Error: expected lambda parameter name after 'lambda'\n";
            return;
        }

        i = nextNonWS(i);

        if (i >= tokens.size() || tokens[i].type != TK::LPAREN) {
            std::cerr << "Error: expected '(' after lambda name '" << lambdaParamName << "'\n";
            return;
        }
        i++;

        std::vector<std::string> argCTypes;
        while (true) {
            i = nextNonWS(i);
            if (i >= tokens.size()) break;
            if (tokens[i].type == TK::RPAREN) { i++; break; }
            if (tokens[i].type == TK::OTHER && tokens[i].value == ",") { i++; continue; }

            std::string ctype = parseCType(i);
            if (!ctype.empty()) argCTypes.push_back(ctype);
        }

        i = nextNonWS(i);

        std::string retCType = "void";
        if (i < tokens.size() && tokens[i].type == TK::OTHER && tokens[i].value == "-") {
            i++;
            i = nextNonWS(i);
            if (i < tokens.size() && tokens[i].type == TK::OTHER && tokens[i].value == ">") {
                i++;
                retCType = parseCType(i);
            }
        }

        std::string regKey = calleeName + "::" + lambdaParamName;
        LambdaReg reg;
        reg.lambdaParamName = lambdaParamName;
        reg.calleeName      = calleeName;
        reg.argCTypes       = argCTypes;
        reg.retCType        = retCType;
        lambdaRegistry[regKey] = reg;
        lambdaRegistry[lambdaParamName] = reg;

        out << retCType << " (*" << lambdaParamName << ")(";
        for (size_t k = 0; k < argCTypes.size(); k++) {
            if (k > 0) out << ", ";
            out << argCTypes[k];
        }
        size_t fptrInnerClose = out.str().size();
        out << ")";

        PatchSite ps;
        ps.calleeName            = calleeName;
        ps.lambdaParamName       = lambdaParamName;
        ps.fptrInnerCloseOffset  = fptrInnerClose;
        ps.outputByteOffset      = SIZE_MAX;
        patchSites.push_back(ps);
    }

    std::string generateLambdaName(const std::vector<Token>& bodyTokens) {
        std::string concat;
        for (auto& t : bodyTokens) concat += t.value;
        uint32_t h = fnv1a32(concat);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "__lambda_%08x", h);
        std::string base(buf);

        auto it = lambdaNameCount.find(base);
        if (it == lambdaNameCount.end()) {
            lambdaNameCount[base] = 1;
            return base;
        } else {
            it->second++;
            return base + "_" + std::to_string(it->second);
        }
    }

    std::vector<std::pair<std::string,std::string>> detectCaptures(
        const std::vector<std::string>& headerParams,
        const std::vector<Token>& bodyTokens)
    {
        std::unordered_set<std::string> headerSet(headerParams.begin(), headerParams.end());
        std::vector<std::pair<std::string,std::string>> captures;
        std::unordered_set<std::string> seen;

        for (auto& t : bodyTokens) {
            if (t.type != TK::IDENT) continue;
            const std::string& name = t.value;
            if (headerSet.count(name)) continue;
            if (seen.count(name)) continue;
            if (TYPE_MAP.count(name)) continue;
            if (CONTROL_FLOW_KW.count(name)) continue;
            if (namespaces.count(name)) continue;

            for (auto& sv : scopeVars) {
                if (sv.name == name) {
                    seen.insert(name);
                    captures.emplace_back(name, sv.cType);
                    break;
                }
            }
        }
        return captures;
    }

    struct ExtractedBody {
        std::vector<std::string> headerParams;
        std::vector<Token>       bodyTokens;
    };

    ExtractedBody extractBody(size_t& i) {
        ExtractedBody result;

        i++;

        size_t j = nextNonWS(i);

        if (j < tokens.size() && tokens[j].type == TK::LPAREN) {
            size_t save_j = j;
            j++;
            std::vector<std::string> params;
            bool valid = true;
            while (true) {
                j = nextNonWS(j);
                if (j >= tokens.size() || tokens[j].type == TK::END) { valid = false; break; }
                if (tokens[j].type == TK::RPAREN) { j++; break; }
                if (tokens[j].type == TK::OTHER && tokens[j].value == ",") { j++; continue; }
                if (tokens[j].type == TK::IDENT) {
                    params.push_back(tokens[j].value);
                    j++;
                } else {
                    valid = false; break;
                }
            }
            if (valid) {
                size_t j2 = nextNonWS(j);
                if (j2 < tokens.size() && tokens[j2].type == TK::OTHER && tokens[j2].value == "-") {
                    size_t j3 = nextNonWS(j2 + 1);
                    if (j3 < tokens.size() && tokens[j3].type == TK::OTHER && tokens[j3].value == ">") {
                        result.headerParams = params;
                        i = j3 + 1;
                    } else {
                        i = save_j + 1;
                        i = nextNonWS(i);
                        i = save_j;
                    }
                } else {
                    i = save_j;
                }
            } else {
                i = save_j;
            }
        }

        int depth = 1;
        while (i < tokens.size()) {
            const Token& t = tokens[i];
            if (t.type == TK::END) break;
            if (t.type == TK::LBRACE) { depth++; result.bodyTokens.push_back(t); i++; continue; }
            if (t.type == TK::RBRACE) {
                depth--;
                if (depth == 0) { i++; break; }
                result.bodyTokens.push_back(t); i++; continue;
            }
            result.bodyTokens.push_back(t);
            i++;
        }

        return result;
    }

    void emitLambdaFunction(
        const std::string& name,
        const std::string& retCType,
        const std::vector<std::string>& argCTypes,
        const std::vector<std::string>& headerParams,
        const std::vector<Token>& bodyTokens,
        const std::vector<std::pair<std::string,std::string>>& captures)
    {
        preamble << retCType << " " << name << "(";

        bool first = true;
        for (size_t k = 0; k < argCTypes.size(); k++) {
            if (!first) preamble << ", ";
            first = false;
            preamble << argCTypes[k];
            if (k < headerParams.size()) preamble << " " << headerParams[k];
        }

        for (auto& [capName, capType] : captures) {
            if (!first) preamble << ", ";
            first = false;
            preamble << capType << " " << capName;
        }

        preamble << ") {\n";

        preamble << transpileTokensToString(bodyTokens);

        preamble << "}\n\n";
    }

    std::string transpileTokensToString(const std::vector<Token>& toks) {
        std::ostringstream buf;
        //int pd = 0;

        auto nextNonWSLocal = [&](size_t i) -> size_t {
            while (i < toks.size() && toks[i].type == TK::OTHER &&
                   !toks[i].value.empty() && std::isspace((unsigned char)toks[i].value[0]))
                i++;
            return i;
        };

        size_t i = 0;
        while (i < toks.size()) {
            const Token& tok = toks[i];
            if (tok.type == TK::END) break;

            //if (tok.type == TK::LPAREN)  { pd++; buf << tok.value; i++; continue; }
            //if (tok.type == TK::RPAREN)  { pd--; buf << tok.value; i++; continue; }

            if (tok.type == TK::OTHER        ||
                tok.type == TK::LINE_COMMENT ||
                tok.type == TK::BLOCK_COMMENT||
                tok.type == TK::NUMBER       ||
                tok.type == TK::STRING       ||
                tok.type == TK::CHAR_LIT     ||
                tok.type == TK::LBRACE       ||
                tok.type == TK::RBRACE       ||
                tok.type == TK::SEMICOLON)
            {
                buf << tok.value; i++; continue;
            }

            if (tok.type == TK::PREPROCESSOR) {
                std::string val = tok.value;
                if (val.find("#include") != std::string::npos) {
                    auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
                        size_t p = 0;
                        while ((p = s.find(from, p)) != std::string::npos) {
                            s.replace(p, from.size(), to);
                            p += to.size();
                        }
                    };
                    replaceAll(val, ".uhh\"", ".hh\"");
                    replaceAll(val, ".uhh>",  ".hh>");
                }
                buf << val; i++; continue;
            }

            if (tok.type == TK::SCOPE) { buf << "::"; i++; continue; }
            if (tok.type == TK::DOT)   { buf << tok.value; i++; continue; }

            if (tok.type == TK::IDENT) {
                const std::string& v = tok.value;

                if (v == "self") {
                    buf << "struct It"; i++; continue;
                }

                auto typeIt = TYPE_MAP.find(v);
                if (typeIt != TYPE_MAP.end()) {
                    buf << typeIt->second; i++; continue;
                }

                if (namespaces.count(v)) {
                    // Helper: was the previous meaningful token the given keyword?
                    auto prevMeaningfulIs = [&](size_t idx, const std::string& kw) -> bool {
                        size_t j = idx;
                        while (j > 0) {
                            j--;
                            const Token& t = toks[j];
                            if (t.type == TK::OTHER || t.type == TK::LINE_COMMENT ||
                                t.type == TK::BLOCK_COMMENT) continue;
                            return t.type == TK::IDENT && t.value == kw;
                        }
                        return false;
                    };

                    size_t j = nextNonWSLocal(i + 1);
                    if (j < toks.size() && toks[j].type == TK::DOT) {
                        size_t k = nextNonWSLocal(j + 1);
                        if (k < toks.size() && toks[k].type == TK::IDENT) {
                            for (size_t w = i + 1; w < j; w++) buf << toks[w].value;
                            buf << v << "::";
                            for (size_t w = j + 1; w < k; w++) buf << toks[w].value;
                            i = k;

                            while (true) {
                                const std::string& ci = toks[i].value;
                                size_t j2 = nextNonWSLocal(i + 1);
                                if (j2 < toks.size() && toks[j2].type == TK::DOT) {
                                    size_t k2 = nextNonWSLocal(j2 + 1);
                                    if (k2 < toks.size() && toks[k2].type == TK::IDENT) {
                                        buf << ci << "::";
                                        for (size_t w = j2 + 1; w < k2; w++) buf << toks[w].value;
                                        i = k2;
                                        continue;
                                    }
                                }
                                auto typeIt2 = TYPE_MAP.find(ci);
                                buf << (typeIt2 != TYPE_MAP.end() ? typeIt2->second : ci);
                                i++;
                                break;
                            }
                            continue;
                        }
                    } else if (!prevMeaningfulIs(i, "namespace")) {
                        // If followed by ::, user wrote explicit scope — pass through as-is
                        size_t nextCheck = nextNonWSLocal(i + 1);
                        if (nextCheck < toks.size() && toks[nextCheck].type == TK::SCOPE) {
                            buf << v; i++; continue;
                        }
                        // Standalone namespace name → expand to Namespace::It
                        buf << v << "::It"; i++; continue;
                    }
                }

                buf << v; i++; continue;
            }

            buf << tok.value; i++;
        }

        return buf.str();
    }

    std::string applyPatches(std::string outStr) {
        if (callSiteCaptures.empty()) return outStr;

        std::unordered_map<std::string, std::vector<std::pair<std::string,std::string>>> calleeCaps;

        for (auto& csc : callSiteCaptures) {
            auto it = calleeCaps.find(csc.calleeName);
            if (it == calleeCaps.end()) {
                calleeCaps[csc.calleeName] = csc.captures;
            } else {
                auto& existing = it->second;
                if (existing.size() != csc.captures.size()) {
                    std::cerr << "Error: Incompatible capture sets across call sites for '"
                              << csc.calleeName << "'\n";
                    continue;
                }
                for (size_t k = 0; k < existing.size(); k++) {
                    if (existing[k].first != csc.captures[k].first) {
                        std::cerr << "Error: Incompatible capture sets across call sites for '"
                                  << csc.calleeName << "'\n";
                        break;
                    }
                }
            }
        }

        struct Patch {
            size_t offset;
            std::string insertion;
        };
        std::vector<Patch> patches;

        for (auto& [calleeName, caps] : calleeCaps) {
            if (caps.empty()) continue;

            bool found = false;
            for (auto& ps : patchSites) {
                if (ps.calleeName != calleeName) continue;
                if (ps.outputByteOffset == SIZE_MAX) continue;
                found = true;

                std::string outerInsert;
                for (auto& [capName, capType] : caps) {
                    outerInsert += ", " + capType + " " + capName;
                }

                patches.push_back({ps.outputByteOffset, outerInsert});

                if (ps.fptrInnerCloseOffset != SIZE_MAX) {
                    std::string innerInsert;
                    for (auto& [capName, capType] : caps) {
                        innerInsert += ", " + capType;
                    }
                    patches.push_back({ps.fptrInnerCloseOffset, innerInsert});
                }
            }

            if (!found) {
                std::cerr << "Warning: Cannot auto-patch '" << calleeName
                          << "' (defined in another file). Add capture parameters manually.\n";
            }
        }

        std::sort(patches.begin(), patches.end(), [](const Patch& a, const Patch& b) {
            return a.offset > b.offset;
        });

        for (auto& p : patches) {
            if (p.offset <= outStr.size()) {
                outStr.insert(p.offset, p.insertion);
            }
        }

        return outStr;
    }

    void recordFunctionParams(size_t funcParamStart) {
        size_t i = funcParamStart;
        while (i < tokens.size()) {
            i = nextNonWS(i);
            if (i >= tokens.size() || tokens[i].type == TK::END) break;
            if (tokens[i].type == TK::RPAREN) break;
            if (tokens[i].type == TK::OTHER && tokens[i].value == ",") { i++; continue; }

            if (tokens[i].type == TK::IDENT && tokens[i].value == "lambda") {
                while (i < tokens.size() && tokens[i].type != TK::END) {
                    if (tokens[i].type == TK::RPAREN) break;
                    if (tokens[i].type == TK::OTHER && tokens[i].value == ",") break;
                    i++;
                }
                continue;
            }

            if (tokens[i].type != TK::IDENT) { i++; continue; }

            std::string typePart;

            size_t j = nextNonWS(i + 1);
            if (j < tokens.size() && tokens[j].type == TK::DOT) {
                size_t k = nextNonWS(j + 1);
                if (k < tokens.size() && tokens[k].type == TK::IDENT) {
                    typePart = tokens[i].value + "::" + tokens[k].value;
                    i = k + 1;
                } else {
                    typePart = resolveType(tokens[i].value);
                    i++;
                }
            } else {
                typePart = resolveType(tokens[i].value);
                i++;
            }

            while (true) {
                size_t jj = nextNonWS(i);
                if (jj < tokens.size() && tokens[jj].type == TK::OTHER &&
                    (tokens[jj].value == "*" || tokens[jj].value == "&")) {
                    typePart += tokens[jj].value;
                    i = jj + 1;
                } else break;
            }

            i = nextNonWS(i);
            if (i < tokens.size() && tokens[i].type == TK::IDENT) {
                scopeVars.push_back({tokens[i].value, typePart});
                i++;
            }
        }
    }

    void tryRecordVarDecl(size_t i) {
        if (!inFunctionBody) return;
        if (i >= tokens.size() || tokens[i].type != TK::IDENT) return;

        const std::string& v = tokens[i].value;
        if (CONTROL_FLOW_KW.count(v)) return;
        if (v == "return" || v == "lambda") return;

        if (!TYPE_MAP.count(v)) return;

        size_t j = nextNonWS(i + 1);
        if (j >= tokens.size() || tokens[j].type != TK::IDENT) return;
        const std::string& paramName = tokens[j].value;
        if (CONTROL_FLOW_KW.count(paramName)) return;

        size_t k = nextNonWS(j + 1);
        if (k >= tokens.size()) return;
        auto& nextTok = tokens[k];
        if (nextTok.type == TK::OTHER && (nextTok.value == "=" || nextTok.value == ",")) {
            scopeVars.push_back({paramName, resolveType(v)});
        } else if (nextTok.type == TK::SEMICOLON) {
            scopeVars.push_back({paramName, resolveType(v)});
        }
    }

public:
    Transpiler(const std::vector<Token>& toks, const std::unordered_set<std::string>& ns)
        : tokens(toks), namespaces(ns) {}

    std::string run() {
        std::string currentFuncName;
        std::string lastIdentAtDepth0;

        std::string paramListCallee;
        size_t paramListOpenIdx = 0;

        size_t i = 0;
        while (i < tokens.size()) {
            const Token& tok = tokens[i];

            if (tok.type == TK::END) break;

            if (tok.type == TK::LBRACE) {
                globalBraceDepth++;
                if (inFunctionBody) braceDepth++;

                if (globalBraceDepth == 1 && !pendingFuncForBrace.empty()) {
                    currentFuncName = pendingFuncForBrace;
                    inFunctionBody  = true;
                    braceDepth      = 1;
                    scopeVars.clear();
                    size_t pi = pendingFuncParamIdx + 1;
                    recordFunctionParams(pi);
                    pendingFuncForBrace.clear();
                }

                out << tok.value; i++; continue;
            }

            if (tok.type == TK::RBRACE) {
                if (inFunctionBody) {
                    braceDepth--;
                    if (braceDepth == 0) {
                        inFunctionBody = false;
                        scopeVars.clear();
                    }
                }
                globalBraceDepth--;
                out << tok.value; i++; continue;
            }

            if (tok.type == TK::LPAREN) {
                parenDepth++;
                if (parenDepth == 1 && !lastIdentAtDepth0.empty() &&
                    !CONTROL_FLOW_KW.count(lastIdentAtDepth0)) {
                    paramListCallee   = lastIdentAtDepth0;
                    paramListOpenIdx  = i;
                    CallFrame cf;
                    cf.calleeName       = lastIdentAtDepth0;
                    cf.parenDepthAtOpen = parenDepth;
                    cf.outLenAtArgStart = out.str().size();
                    callStack.push_back(cf);
                }
                lastIdentAtDepth0.clear();
                out << tok.value; i++; continue;
            }

            if (tok.type == TK::RPAREN) {
                parenDepth--;

                if (parenDepth == 0) {
                    for (auto& ps : patchSites) {
                        if (ps.outputByteOffset == SIZE_MAX && ps.calleeName == paramListCallee) {
                            ps.outputByteOffset = out.str().size();
                        }
                    }
                    if (globalBraceDepth == 0) {
                        pendingFuncForBrace = paramListCallee;
                        pendingFuncParamIdx = paramListOpenIdx;
                    }
                    paramListCallee.clear();
                }

                if (!callStack.empty()) {
                    CallFrame& frame = callStack.back();
                    if (parenDepth == frame.parenDepthAtOpen - 1) {
                        size_t peek = nextNonWS(i + 1);
                        if (peek < tokens.size() && tokens[peek].type == TK::LBRACE &&
                            globalBraceDepth > 0) {
                            std::string calleeName      = frame.calleeName;
                            size_t      outLenAtArgStart = frame.outLenAtArgStart;
                            callStack.pop_back();

                            LambdaReg* reg = nullptr;
                            for (auto& [key, lr] : lambdaRegistry) {
                                if (lr.calleeName == calleeName) { reg = &lr; break; }
                            }

                            if (!reg) {
                                out << tok.value; i++;
                                continue;
                            }

                            size_t bodyStart = peek;
                            ExtractedBody eb = extractBody(bodyStart);
                            i = bodyStart;

                            std::string lambdaName = generateLambdaName(eb.bodyTokens);

                            auto captures = detectCaptures(eb.headerParams, eb.bodyTokens);

                            emitLambdaFunction(lambdaName, reg->retCType, reg->argCTypes,
                                               eb.headerParams, eb.bodyTokens, captures);

                            if (!captures.empty()) {
                                CallSiteCapture csc;
                                csc.calleeName = calleeName;
                                csc.sourceLine = tok.line;
                                csc.captures   = captures;
                                callSiteCaptures.push_back(csc);
                            }

                            {
                                std::string argsSoFar = out.str().substr(outLenAtArgStart + 1);
                                bool hasArgs = argsSoFar.find_first_not_of(" \t\r\n") != std::string::npos;
                                if (hasArgs) out << ", ";
                            }
                            out << lambdaName;
                            for (auto& [capName, capType] : captures) {
                                out << ", " << capName;
                            }
                            out << tok.value;
                            continue;
                        } else {
                            callStack.pop_back();
                        }
                    }
                }

                out << tok.value; i++; continue;
            }

            if (tok.type == TK::OTHER        ||
                tok.type == TK::LINE_COMMENT ||
                tok.type == TK::BLOCK_COMMENT||
                tok.type == TK::NUMBER       ||
                tok.type == TK::STRING       ||
                tok.type == TK::CHAR_LIT     ||
                tok.type == TK::SEMICOLON)
            {
                if (parenDepth == 0 &&
                    !tok.value.empty() && !std::isspace((unsigned char)tok.value[0])) {
                    lastIdentAtDepth0.clear();
                }
                out << tok.value; i++; continue;
            }

            if (tok.type == TK::PREPROCESSOR) {
                std::string val = tok.value;
                if (val.find("#include") != std::string::npos) {
                    auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
                        size_t p = 0;
                        while ((p = s.find(from, p)) != std::string::npos) {
                            s.replace(p, from.size(), to);
                            p += to.size();
                        }
                    };
                    replaceAll(val, ".uhh\"", ".hh\"");
                    replaceAll(val, ".uhh>",  ".hh>");
                }
                lastIdentAtDepth0.clear();
                out << val; i++; continue;
            }

            if (tok.type == TK::SCOPE) {
                lastIdentAtDepth0.clear();
                out << "::"; i++; continue;
            }

            if (tok.type == TK::DOT) {
                lastIdentAtDepth0.clear();
                out << tok.value; i++; continue;
            }

            if (tok.type == TK::IDENT) {
                const std::string& v = tok.value;

                if (parenDepth == 0) {
                    if (!CONTROL_FLOW_KW.count(v) && v != "lambda") {
                        lastIdentAtDepth0 = v;
                    } else {
                        lastIdentAtDepth0.clear();
                    }
                }

                if (v == "self") {
                    out << "struct It"; i++; continue;
                }

                if (v == "lambda" && parenDepth > 0) {
                    std::string callee = paramListCallee;
                    if (callee.empty() && !callStack.empty()) {
                        callee = callStack[0].calleeName;
                    }
                    i++;
                    parseLambdaDecl(i, callee);
                    continue;
                }

                if (v == "deprecated") {
                    i++;
                    size_t n = nextNonWS(i);
                    if (n < tokens.size() && tokens[n].type == TK::LPAREN) {
                        size_t m = nextNonWS(n + 1);
                        if (m < tokens.size() && tokens[m].type == TK::STRING) {
                            size_t r = nextNonWS(m + 1);
                            if (r < tokens.size() && tokens[r].type == TK::RPAREN) {
                                out << "[[deprecated(" << tokens[m].value << ")]]";
                                i = r + 1;
                                continue;
                            }
                        }
                    }
                    out << "[[deprecated]]";
                    continue;
                }

                if (v == "unused") {
                    if (parenDepth > 0) {
                        i++;
                        std::ostringstream paramBuf;
                        int relDepth = 0;
                        while (i < tokens.size()) {
                            const Token& pt = tokens[i];
                            if (pt.type == TK::END) break;
                            if (relDepth == 0 &&
                                (pt.type == TK::RPAREN ||
                                 (pt.type == TK::OTHER && pt.value == ",")))
                                break;
                            if (pt.type == TK::LPAREN) relDepth++;
                            if (pt.type == TK::RPAREN) relDepth--;
                            if (pt.type == TK::IDENT) {
                                auto pit = TYPE_MAP.find(pt.value);
                                std::string pv = (pit != TYPE_MAP.end()) ? pit->second : pt.value;
                                paramBuf << pv;
                            } else {
                                paramBuf << pt.value;
                            }
                            i++;
                        }
                        out << paramBuf.str() << " __attribute__((unused))";
                    } else {
                        i++;
                        size_t n1 = nextNonWS(i);
                        if (n1 < tokens.size() && tokens[n1].type == TK::IDENT) {
                            size_t n2 = nextNonWS(n1 + 1);
                            if (n2 < tokens.size() && tokens[n2].type == TK::SEMICOLON) {
                                out << "(void)" << tokens[n1].value << ";";
                                i = n2 + 1;
                                continue;
                            }
                        }
                        std::string varName;
                        std::ostringstream stmtBuf;
                        bool foundVar = false;
                        while (i < tokens.size()) {
                            const Token& st = tokens[i];
                            if (st.type == TK::END) break;
                            if (st.type == TK::IDENT) {
                                auto sit = TYPE_MAP.find(st.value);
                                if (sit != TYPE_MAP.end()) {
                                    stmtBuf << sit->second;
                                } else if (!foundVar) {
                                    varName  = st.value;
                                    foundVar = true;
                                    stmtBuf << st.value;
                                } else {
                                    if (namespaces.count(st.value)) {
                                        size_t j = nextNonWS(i + 1);
                                        if (j < tokens.size() && tokens[j].type == TK::DOT) {
                                            size_t k = nextNonWS(j + 1);
                                            if (k < tokens.size() && tokens[k].type == TK::IDENT) {
                                                for (size_t w = i + 1; w < j; w++) stmtBuf << tokens[w].value;
                                                stmtBuf << st.value << "::";
                                                for (size_t w = j + 1; w < k; w++) stmtBuf << tokens[w].value;
                                                i = k;
                                                continue;
                                            }
                                        }
                                    }
                                    stmtBuf << st.value;
                                }
                            } else {
                                stmtBuf << st.value;
                            }
                            if (st.type == TK::SEMICOLON) { i++; break; }
                            i++;
                        }
                        out << stmtBuf.str();
                        if (!varName.empty())
                            out << " (void)" << varName << ";";
                    }
                    continue;
                }

                auto typeIt = TYPE_MAP.find(v);
                if (typeIt != TYPE_MAP.end()) {
                    tryRecordVarDecl(i);
                    out << typeIt->second; i++; continue;
                }

                if (namespaces.count(v)) {
                    auto prevMeaningfulIs = [&](size_t idx, const std::string& kw) -> bool {
                        size_t j = idx;
                        while (j > 0) {
                            j--;
                            const Token& t = tokens[j];
                            if (t.type == TK::OTHER || t.type == TK::LINE_COMMENT ||
                                t.type == TK::BLOCK_COMMENT) continue;
                            return t.type == TK::IDENT && t.value == kw;
                        }
                        return false;
                    };

                    size_t j = nextNonWS(i + 1);
                    if (j < tokens.size() && tokens[j].type == TK::DOT) {
                        size_t k = nextNonWS(j + 1);
                        if (k < tokens.size() && tokens[k].type == TK::IDENT) {
                            for (size_t w = i + 1; w < j; w++) out << tokens[w].value;
                            out << v << "::";
                            for (size_t w = j + 1; w < k; w++) out << tokens[w].value;
                            i = k;

                            while (true) {
                                const std::string& ci = tokens[i].value;
                                size_t j2 = nextNonWS(i + 1);
                                if (j2 < tokens.size() && tokens[j2].type == TK::DOT) {
                                    size_t k2 = nextNonWS(j2 + 1);
                                    if (k2 < tokens.size() && tokens[k2].type == TK::IDENT) {
                                        out << ci << "::";
                                        for (size_t w = j2 + 1; w < k2; w++) out << tokens[w].value;
                                        i = k2;
                                        continue;
                                    }
                                }
                                auto typeIt2 = TYPE_MAP.find(ci);
                                out << (typeIt2 != TYPE_MAP.end() ? typeIt2->second : ci);
                                i++;
                                break;
                            }
                            lastIdentAtDepth0.clear();
                            continue;
                        }
                    } else if (!prevMeaningfulIs(i, "namespace")) {
                        // If followed by ::, user wrote explicit scope — pass through as-is
                        {
                            size_t nextCheck = nextNonWS(i + 1);
                            if (nextCheck < tokens.size() && tokens[nextCheck].type == TK::SCOPE) {
                                out << v; i++; continue;
                            }
                        }
                        // Standalone namespace name → expand to Namespace::It
                        out << v << "::It";
                        i++; // consume namespace name token

                        // In a function definition's parameter list, automatically add &
                        // unless the user explicitly wrote * (pointer) or & (reference).
                        // Template args like List<T> must be emitted before the &.
                        if (parenDepth > 0 && !inFunctionBody) {
                            size_t ni = nextNonWS(i);
                            if (ni < tokens.size() && tokens[ni].value == "<") {
                                // Emit whitespace up to and including '<'
                                for (size_t w = i; w <= ni; w++) out << tokens[w].value;
                                i = ni + 1;
                                // Emit the full template argument list, tracking < > depth.
                                // Expand namespace names inside <...> to Namespace::It.
                                int depth = 1;
                                while (i < tokens.size() && depth > 0) {
                                    const std::string& cv = tokens[i].value;
                                    if (cv == "<") { depth++; out << cv; i++; }
                                    else if (cv == ">") { depth--; out << cv; i++; }
                                    else if (tokens[i].type == TK::IDENT && namespaces.count(cv)) {
                                        out << cv << "::It"; i++;
                                    } else {
                                        out << cv; i++;
                                    }
                                }
                                // After <T...>, check for explicit * or &
                                size_t ni2 = nextNonWS(i);
                                if (ni2 >= tokens.size() ||
                                    (tokens[ni2].value != "*" && tokens[ni2].value != "&"))
                                    out << "&";
                            } else if (ni < tokens.size() &&
                                       tokens[ni].value != "*" && tokens[ni].value != "&") {
                                out << "&";
                            }
                        }

                        lastIdentAtDepth0.clear();
                        continue; // i already advanced above
                    }
                }

                {
                    size_t peek = nextNonWS(i + 1);
                    if (peek < tokens.size() && tokens[peek].type == TK::LBRACE &&
                        !CONTROL_FLOW_KW.count(v)) {
                        LambdaReg* reg = nullptr;
                        for (auto& [key, lr] : lambdaRegistry) {
                            if (lr.calleeName == v && lr.argCTypes.empty()) {
                                reg = &lr; break;
                            }
                        }
                        if (reg) {
                            size_t bodyStart = peek;
                            ExtractedBody eb = extractBody(bodyStart);
                            i = bodyStart;

                            std::string lambdaName = generateLambdaName(eb.bodyTokens);
                            auto captures = detectCaptures(eb.headerParams, eb.bodyTokens);

                            emitLambdaFunction(lambdaName, reg->retCType, reg->argCTypes,
                                               eb.headerParams, eb.bodyTokens, captures);

                            if (!captures.empty()) {
                                CallSiteCapture csc;
                                csc.calleeName = v;
                                csc.sourceLine = tok.line;
                                csc.captures   = captures;
                                callSiteCaptures.push_back(csc);
                            }

                            out << v << "(" << lambdaName;
                            for (auto& [capName, capType] : captures) {
                                out << ", " << capName;
                            }
                            out << ")";
                            lastIdentAtDepth0.clear();
                            continue;
                        }
                    }
                }

                out << v; i++; continue;
            }

            out << tok.value; i++;
        }

        std::string outStr = applyPatches(out.str());

        return preamble.str() + outStr;
    }
};

std::string transpile(const std::vector<Token>& tokens, const std::unordered_set<std::string>& namespaces) {
    auto processed = injectImplicitSemicolons(tokens);
    return Transpiler(processed, namespaces).run();
}

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "Error: cannot open " << path << "\n"; std::exit(1); }
    return { std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
}

static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { std::cerr << "Error: cannot write " << path << "\n"; std::exit(1); }
    f << content;
}

static fs::path relativeOutputPath(const fs::path& file, const fs::path& inRoot, const fs::path& outRoot) {
    auto rel = fs::relative(file, inRoot);
    auto out = outRoot / rel;
    if (out.extension() == ".uhc") out.replace_extension(".cc");
    else if (out.extension() == ".uhh") out.replace_extension(".hh");
    return out;
}

int main(int argc, char* argv[]) {
    std::vector<fs::path> includeDirs;
    std::vector<std::string> positional;
    bool verbose = false;

    for (int a = 1; a < argc; a++) {
        std::string arg = argv[a];
        if (arg == "-v") {
            verbose = true;
        } else if (arg.size() >= 2 && arg[0] == '-' && arg[1] == 'I') {
            fs::path dir = arg.substr(2);
            if (!fs::is_directory(dir)) {
                std::cerr << "Warning: -I path is not a directory: " << dir << "\n";
            } else {
                includeDirs.push_back(dir);
            }
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.size() < 2) {
        std::cerr << "Usage: unholyc <input_dir> <output_dir> [-I<include_dir> ...]\n";
        return 1;
    }

    fs::path inRoot  = positional[0];
    fs::path outRoot = positional[1];

    if (!fs::is_directory(inRoot)) {
        std::cerr << "Error: " << inRoot << " is not a directory\n";
        return 1;
    }

    inRoot  = fs::canonical(inRoot);
    outRoot = fs::absolute(outRoot);

    std::unordered_set<std::string> globalNS;

    auto scanDir = [&](const fs::path& dir) {
        for (auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext == ".uhc" || ext == ".uhh" ||
                ext == ".hh"  || ext == ".h"   || ext == ".hpp") {
                auto src    = readFile(entry.path().string());
                auto tokens = Lexer(src).tokenize();
                auto ns     = collectNamespaces(tokens);
                globalNS.insert(ns.begin(), ns.end());
            }
        }
    };

    scanDir(inRoot);
    for (auto& incDir : includeDirs)
        scanDir(incDir);

    static const std::unordered_set<std::string> copyExts = {
        ".c", ".cpp", ".cc", ".h", ".hpp", ".hh"
    };

    for (auto& entry : fs::recursive_directory_iterator(inRoot)) {
        if (!entry.is_regular_file()) continue;
        auto ext     = entry.path().extension().string();
        if (ext != ".uhc" && ext != ".uhh" && !copyExts.count(ext)) continue;

        auto outPath = relativeOutputPath(entry.path(), inRoot, outRoot);
        fs::create_directories(outPath.parent_path());

        if (ext == ".uhc" || ext == ".uhh") {
            auto src    = readFile(entry.path().string());
            auto tokens = Lexer(src).tokenize();
            auto output = transpile(tokens, globalNS);
            writeFile(outPath.string(), output);
            if (verbose) std::cout << entry.path().string() << "  ->  " << outPath.string() << "\n";
        } else {
            fs::copy_file(entry.path(), outPath, fs::copy_options::overwrite_existing);
            if (verbose) std::cout << entry.path().string() << "  ->  " << outPath.string() << " (copied)\n";
        }
    }

    return 0;
}
