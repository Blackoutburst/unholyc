#include <cctype>
#include <chrono>
#include <cstdlib>
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

#ifndef UHC_COMMIT
#define UHC_COMMIT dev
#endif
#define UHC_STR(x) #x
#define UHC_VERSION(x) UHC_STR(x)

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

            if (c == '+' && peek(1) == '+') {
                advance(); advance();
                tokens.push_back({ TK::OTHER, "++", l });
                continue;
            }

            if (c == '-' && peek(1) == '-') {
                advance(); advance();
                tokens.push_back({ TK::OTHER, "--", l });
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
                // Array subscript closer ] and post-increment/decrement end a statement
                return t.type == TK::OTHER &&
                       (t.value == "]" || t.value == "++" || t.value == "--");
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

struct FieldInfo {
    std::string name;
    std::string fmtSpec; // empty = skip this field
};

struct NSInfo {
    bool hasSelf         = false;
    bool hasUserToString = false;
    bool isTemplate      = false;
    std::vector<FieldInfo> selfFields;
};

static bool isWSTok(const Token& t) {
    if (t.type != TK::OTHER || t.value.empty()) return false;
    for (char c : t.value) if (!std::isspace((unsigned char)c)) return false;
    return true;
}

static size_t skipWSIdx(const std::vector<Token>& toks, size_t i) {
    while (i < toks.size() && isWSTok(toks[i])) i++;
    return i;
}

static std::string uhcTypeToFmt(const std::string& t, bool ptr) {
    if (ptr) return (t == "U8" || t == "I8") ? "%s" : "";
    if (t == "F32" || t == "F64" || t == "F128") return "%f";
    if (t == "I8"  || t == "I16" || t == "I32")  return "%d";
    if (t == "I64") return "%lld";
    if (t == "U8"  || t == "U16" || t == "U32")  return "%u";
    if (t == "U64") return "%llu";
    return "";
}

// Per-file: which namespaces have `self`, a user-defined toString, or are templates.
// Also extracts primitive fields from self blocks for auto-generated toString formatting.
static std::unordered_map<std::string, NSInfo> collectNSInfo(const std::vector<Token>& tokens) {
    std::unordered_map<std::string, NSInfo> result;
    int         depth          = 0;
    std::string curNS;
    bool        seenTemplate   = false; // persists through <typename T>; consumed by namespace/self/struct/{
    bool        pendingSelf    = false; // seen `self`, waiting for its opening {
    bool        inSelfBlock    = false;
    int         selfParenDepth = 0;    // paren depth inside self block; skip field parsing when > 0

    for (size_t i = 0; i < tokens.size(); i++) {
        const Token& tok = tokens[i];

        if (tok.type == TK::LBRACE) {
            seenTemplate = false;
            depth++;
            if (pendingSelf && depth == 2) { inSelfBlock = true; selfParenDepth = 0; }
            pendingSelf = false;
            continue;
        }
        if (tok.type == TK::RBRACE) {
            if (inSelfBlock && depth == 2) inSelfBlock = false;
            depth--;
            if (depth == 0) curNS.clear();
            continue;
        }
        // Track paren depth inside self block to avoid treating function params as fields
        if (inSelfBlock) {
            if (tok.type == TK::LPAREN) { selfParenDepth++; continue; }
            if (tok.type == TK::RPAREN) { selfParenDepth--; continue; }
        }
        if (tok.type != TK::IDENT) continue;

        if (tok.value == "template") { seenTemplate = true; continue; }

        if (tok.value == "namespace" && depth == 0) {
            size_t j = i + 1;
            while (j < tokens.size() && tokens[j].type == TK::OTHER) j++;
            if (j < tokens.size() && tokens[j].type == TK::IDENT) {
                curNS = tokens[j].value;
                result[curNS].isTemplate |= seenTemplate;
                i = j;
            }
            seenTemplate = false;
            continue;
        }

        if (tok.value == "self" && depth == 1 && !curNS.empty()) {
            result[curNS].hasSelf = true;
            if (seenTemplate) result[curNS].isTemplate = true;
            seenTemplate = false;
            if (!result[curNS].isTemplate) pendingSelf = true;
            continue;
        }

        if (tok.value == "struct" || tok.value == "class" || tok.value == "union") {
            seenTemplate = false; continue;
        }

        // Parse primitive fields inside the self block
        if (inSelfBlock && selfParenDepth == 0 && !curNS.empty() && !result[curNS].isTemplate) {
            // Skip 'const' prefix
            if (tok.value == "const") continue;

            if (TYPE_MAP.count(tok.value)) {
                std::string typeName = tok.value;
                size_t j = skipWSIdx(tokens, i + 1);
                bool isPointer = false;
                if (j < tokens.size() && tokens[j].type == TK::OTHER && tokens[j].value == "*") {
                    isPointer = true;
                    j = skipWSIdx(tokens, j + 1);
                }
                if (j < tokens.size() && tokens[j].type == TK::IDENT) {
                    std::string fieldName = tokens[j].value;
                    // Skip operator overloads (e.g. `U8 operator==(...)`)
                    if (fieldName == "operator") continue;
                    size_t k = skipWSIdx(tokens, j + 1);
                    bool isArray = (k < tokens.size() &&
                                   tokens[k].type == TK::OTHER && tokens[k].value == "[");
                    bool isFunc  = (k < tokens.size() && tokens[k].type == TK::LPAREN);
                    if (!isArray && !isFunc) {
                        std::string fmt = uhcTypeToFmt(typeName, isPointer);
                        if (!fmt.empty())
                            result[curNS].selfFields.push_back({fieldName, fmt});
                    }
                }
            }
            continue;
        }

        if (curNS.empty()) continue;
        if (tok.value == "toString" && depth >= 1)
            result[curNS].hasUserToString = true;
    }
    return result;
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
    bool        hasLambdaParam = false;
};

// Scan tokens for lambda parameter declarations and return a registry usable as
// seed data for files that include this one (cross-file trailing-lambda support).
static std::unordered_map<std::string, LambdaReg>
collectLambdaDecls(const std::vector<Token>& tokens) {
    std::unordered_map<std::string, LambdaReg> result;

    auto skipWS = [&](size_t i) {
        while (i < tokens.size() && tokens[i].type == TK::OTHER &&
               !tokens[i].value.empty() && std::isspace((unsigned char)tokens[i].value[0]))
            i++;
        return i;
    };

    // Resolve UHC type names to C++ equivalents.
    auto resolveT = [&](const std::string& v) -> std::string {
        auto it = TYPE_MAP.find(v);
        return (it != TYPE_MAP.end()) ? it->second : v;
    };

    // Parse a C type (including template args) starting at i; advance i past it.
    std::function<std::string(size_t&)> parseCT = [&](size_t& i) -> std::string {
        i = skipWS(i);
        if (i >= tokens.size()) return "";
        std::string result;
        if (tokens[i].type == TK::IDENT) {
            std::string base = tokens[i].value;
            size_t j = skipWS(i + 1);
            if (j < tokens.size() && tokens[j].type == TK::DOT) {
                size_t k = skipWS(j + 1);
                if (k < tokens.size() && tokens[k].type == TK::IDENT) {
                    result = base + "::" + tokens[k].value;
                    i = k + 1;
                } else { result = resolveT(base); i++; }
            } else { result = resolveT(base); i++; }
        } else { result = tokens[i].value; i++; }
        // template args
        size_t j = skipWS(i);
        if (j < tokens.size() && tokens[j].type == TK::OTHER && tokens[j].value == "<") {
            result += "<"; i = j + 1; int depth = 1;
            while (i < tokens.size() && depth > 0) {
                const std::string& v = tokens[i].value;
                if (tokens[i].type == TK::OTHER && v == "<")  { depth++; result += v; i++; }
                else if (tokens[i].type == TK::OTHER && v == ">") { depth--; result += ">"; i++; }
                else if (tokens[i].type == TK::IDENT) { result += resolveT(v); i++; }
                else { result += v; i++; }
            }
        }
        // pointer/ref suffixes
        while (true) {
            size_t k = skipWS(i);
            if (k < tokens.size() && tokens[k].type == TK::OTHER &&
                (tokens[k].value == "*" || tokens[k].value == "&")) {
                result += tokens[k].value; i = k + 1;
            } else break;
        }
        return result;
    };

    for (size_t i = 0; i < tokens.size(); i++) {
        // Look for IDENT followed by LPAREN — a function declaration.
        if (tokens[i].type != TK::IDENT) continue;
        std::string funcName = tokens[i].value;
        size_t j = skipWS(i + 1);
        if (j >= tokens.size() || tokens[j].type != TK::LPAREN) continue;
        j++; // past (

        // Scan inside the param list for a 'lambda' keyword at depth 1.
        int depth = 1;
        while (j < tokens.size() && depth > 0) {
            if (tokens[j].type == TK::LPAREN)  { depth++; j++; continue; }
            if (tokens[j].type == TK::RPAREN)  { depth--; j++; continue; }
            if (depth == 1 && tokens[j].type == TK::IDENT && tokens[j].value == "lambda") {
                j++;
                j = skipWS(j);
                if (j >= tokens.size() || tokens[j].type != TK::IDENT) { j++; continue; }
                std::string paramName = tokens[j].value;
                j++;
                j = skipWS(j);
                if (j >= tokens.size() || tokens[j].type != TK::LPAREN) continue;
                j++; // past (

                std::vector<std::string> argCTypes;
                while (true) {
                    j = skipWS(j);
                    if (j >= tokens.size()) break;
                    if (tokens[j].type == TK::RPAREN) { j++; break; }
                    if (tokens[j].type == TK::OTHER && tokens[j].value == ",") { j++; continue; }
                    std::string ct = parseCT(j);
                    if (!ct.empty()) argCTypes.push_back(ct);
                    // skip optional variable name
                    size_t k = skipWS(j);
                    if (k < tokens.size() && tokens[k].type == TK::IDENT &&
                        tokens[k].value != "," ) {
                        // only skip if next after that is , or )
                        size_t k2 = skipWS(k + 1);
                        if (k2 < tokens.size() &&
                            (tokens[k2].value == "," || tokens[k2].type == TK::RPAREN))
                            j = k + 1;
                    }
                }

                std::string retCType = "void";
                j = skipWS(j);
                if (j < tokens.size() && tokens[j].value == "-") {
                    j++;
                    j = skipWS(j);
                    if (j < tokens.size() && tokens[j].value == ">") {
                        j++;
                        retCType = parseCT(j);
                    }
                }

                LambdaReg reg;
                reg.lambdaParamName = paramName;
                reg.calleeName      = funcName;
                reg.argCTypes       = argCTypes;
                reg.retCType        = retCType;
                result[funcName + "::" + paramName] = reg;
                result[paramName] = reg;
                result[funcName]  = reg; // allow lookup by callee name alone
            } else {
                j++;
            }
        }
    }
    return result;
}

// Scan a compiled .hh file for __Block_<name> parameters — these signal that the
// function accepts a trailing lambda.  argCTypes is left empty (call site uses auto).
static std::unordered_map<std::string, LambdaReg>
collectLambdaDeclsFromHH(const std::vector<Token>& tokens) {
    std::unordered_map<std::string, LambdaReg> result;

    auto skipWS = [&](size_t i) {
        while (i < tokens.size() && tokens[i].type == TK::OTHER &&
               !tokens[i].value.empty() && std::isspace((unsigned char)tokens[i].value[0]))
            i++;
        return i;
    };

    for (size_t i = 0; i < tokens.size(); i++) {
        // Look for IDENT followed by LPAREN
        if (tokens[i].type != TK::IDENT) continue;
        std::string funcName = tokens[i].value;
        size_t j = skipWS(i + 1);
        if (j >= tokens.size() || tokens[j].type != TK::LPAREN) continue;
        j++;

        // Scan params for __Block_<name>
        int depth = 1;
        while (j < tokens.size() && depth > 0) {
            if (tokens[j].type == TK::LPAREN)  { depth++; j++; continue; }
            if (tokens[j].type == TK::RPAREN)  { depth--; j++; continue; }
            if (depth == 1 && tokens[j].type == TK::IDENT) {
                const std::string& v = tokens[j].value;
                if (v.size() > 8 && v.substr(0, 8) == "__Block_") {
                    std::string paramName = v.substr(8); // strip __Block_ prefix
                    LambdaReg reg;
                    reg.lambdaParamName = paramName;
                    reg.calleeName      = funcName;
                    reg.retCType        = "void";
                    result[funcName + "::" + paramName] = reg;
                    result[paramName] = reg;
                    result[funcName]  = reg;
                }
            }
            j++;
        }
    }
    return result;
}

static uint32_t fnv1a32(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

class Transpiler {
    const std::vector<Token>&              tokens;
    const std::unordered_set<std::string>& namespaces;
    const std::unordered_map<std::string, NSInfo>& fileNSInfo;
    const std::unordered_set<std::string>& globalUserToStringNS;

    std::ostringstream out;
    std::ostringstream preamble;

    std::unordered_map<std::string, LambdaReg> lambdaRegistry;

    std::vector<PatchSite> patchSites;
    // Template typename patches: insert ", typename Block" at template > offset.
    struct TemplatePatch { size_t offset; std::string insertion; };
    std::vector<TemplatePatch> templatePatches;

    std::vector<CallSiteCapture> callSiteCaptures;
    bool needsFunctional = false;
    bool needsTypeTraits = false;

    static bool isSwizzleIdent(const std::string& s) {
        if (s.size() < 2 || s.size() > 4) return false;
        for (char c : s)
            if (c != 'x' && c != 'y' && c != 'z' && c != 'w' &&
                c != 'r' && c != 'g' && c != 'b' && c != 'a')
                return false;
        return true;
    }
    static char swizzleComp(char c) {
        if (c == 'r') return 'x';
        if (c == 'g') return 'y';
        if (c == 'b') return 'z';
        if (c == 'a') return 'w';
        return c;
    }
    void emitSwizzle(std::ostringstream& dst, const std::string& varName, const std::string& sw) {
        needsTypeTraits = true;
        dst << "std::remove_reference_t<decltype(" << varName << ")>{";
        for (int ci = 0; ci < 4; ci++) {
            if (ci > 0) dst << ", ";
            if ((size_t)ci < sw.size()) dst << varName << "." << swizzleComp(sw[ci]);
            else                        dst << "0";
        }
        dst << "}";
    }

    // Track position of the closing > of the most recent template<...> so we can
    // inject extra typename Block params when a lambda parameter is encountered.
    size_t lastTemplateCloseOffset = SIZE_MAX;

    std::vector<ScopeVar> scopeVars;
    bool inFunctionBody       = false;
    int  braceDepth           = 0;
    int  globalBraceDepth     = 0;
    int  funcBodyStartDepth   = -1; // globalBraceDepth when function body opens

    std::vector<CallFrame> callStack;

    std::string currentNamespace;
    std::string pendingNamespaceName;
    std::unordered_set<std::string> emittedUhcToString;

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

        // Consume template argument list: Foo<Bar, Baz<T>>
        {
            size_t j = nextNonWS(i);
            if (j < tokens.size() && tokens[j].type == TK::OTHER && tokens[j].value == "<") {
                result += "<";
                i = j + 1;
                int depth = 1;
                while (i < tokens.size() && depth > 0) {
                    const std::string& v = tokens[i].value;
                    if (tokens[i].type == TK::OTHER && v == "<") { depth++; result += v; i++; }
                    else if (tokens[i].type == TK::OTHER && v == ">") {
                        depth--;
                        result += ">";
                        i++;
                    } else if (tokens[i].type == TK::IDENT) {
                        result += resolveType(v);
                        i++;
                    } else {
                        result += v;
                        i++;
                    }
                }
            }
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

        if (!callStack.empty()) callStack.back().hasLambdaParam = true;

        std::string regKey = calleeName + "::" + lambdaParamName;
        LambdaReg reg;
        reg.lambdaParamName = lambdaParamName;
        reg.calleeName      = calleeName;
        reg.argCTypes       = argCTypes;
        reg.retCType        = retCType;
        lambdaRegistry[regKey] = reg;
        lambdaRegistry[lambdaParamName] = reg;

        // Emit as a deduced template Block type so any callable (including
        // capturing lambdas) can be passed.  Patch the preceding template<...>
        // to insert the extra typename.
        std::string blockTypeName = "__Block_" + lambdaParamName;
        if (lastTemplateCloseOffset != SIZE_MAX) {
            templatePatches.push_back({lastTemplateCloseOffset,
                                       ", typename " + blockTypeName});
        }
        out << blockTypeName << " " << lambdaParamName;

        PatchSite ps;
        ps.calleeName            = calleeName;
        ps.lambdaParamName       = lambdaParamName;
        ps.fptrInnerCloseOffset  = SIZE_MAX;
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

    // Returns the set of names from `captures` that are written to in `bodyTokens`.
    // A variable is written if it appears as the target of =, +=, -=, *=, /=, %=, etc.,
    // or as the operand of ++ / -- (prefix or postfix).
    // Note: compound assignment operators like += are lexed as two tokens (+ then =).
    std::unordered_set<std::string> detectWrittenCaptures(
        const std::vector<std::pair<std::string,std::string>>& captures,
        const std::vector<Token>& bodyTokens)
    {
        std::unordered_set<std::string> captureNames;
        for (auto& [n, _] : captures) captureNames.insert(n);

        // Helper: skip whitespace tokens
        auto skipWS = [&](size_t start) -> size_t {
            while (start < bodyTokens.size() && bodyTokens[start].type == TK::OTHER &&
                   (bodyTokens[start].value == " "  || bodyTokens[start].value == "\t" ||
                    bodyTokens[start].value == "\r" || bodyTokens[start].value == "\n"))
                start++;
            return start;
        };

        std::unordered_set<std::string> written;
        for (size_t i = 0; i < bodyTokens.size(); i++) {
            const Token& t = bodyTokens[i];
            // Prefix ++ / -- before an ident
            if (t.type == TK::OTHER && (t.value == "++" || t.value == "--")) {
                size_t j = skipWS(i + 1);
                if (j < bodyTokens.size() && bodyTokens[j].type == TK::IDENT &&
                    captureNames.count(bodyTokens[j].value))
                    written.insert(bodyTokens[j].value);
                continue;
            }
            if (t.type != TK::IDENT) continue;
            if (!captureNames.count(t.value)) continue;
            // Skip whitespace after variable name
            size_t j = skipWS(i + 1);
            if (j >= bodyTokens.size() || bodyTokens[j].type != TK::OTHER) continue;
            const std::string& op = bodyTokens[j].value;
            // Postfix ++ or --
            if (op == "++" || op == "--") { written.insert(t.value); continue; }
            // Plain assignment: `name =` (but not `name ==`)
            if (op == "=") {
                size_t k = skipWS(j + 1);
                if (k >= bodyTokens.size() || bodyTokens[k].value != "=")
                    written.insert(t.value);
                continue;
            }
            // Compound assignment: lexed as two tokens e.g. + then =  or ++ (already handled)
            // Operators that can precede =: + - * / % & | ^ << >>
            static const std::string compoundOps = "+-*/%&|^";
            if (compoundOps.find(op) != std::string::npos) {
                size_t k = skipWS(j + 1);
                if (k < bodyTokens.size() && bodyTokens[k].value == "=")
                    written.insert(t.value);
            }
        }
        return written;
    }

    void emitCaptureList(std::ostringstream& dst,
                         const std::vector<std::pair<std::string,std::string>>& captures,
                         const std::unordered_set<std::string>& written)
    {
        dst << "[";
        for (size_t ci = 0; ci < captures.size(); ci++) {
            if (ci > 0) dst << ", ";
            if (written.count(captures[ci].first)) dst << "&";
            dst << captures[ci].first;
        }
        dst << "]";
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
        std::string bodyStr = transpileTokensToString(bodyTokens);

        // Collect template type parameters used in arg/ret types (e.g. T, U, K, V).
        // A token is a template param if it's a short all-uppercase identifier that
        // isn't a known C++ keyword or type.
        static const std::unordered_set<std::string> NOT_TMPL = {
            "int","long","short","char","void","float","double","bool",
            "unsigned","signed","const","auto","nullptr","NULL"
        };
        auto extractTemplateParams = [&](const std::string& t) {
            std::vector<std::string> params;
            size_t i = 0;
            while (i < t.size()) {
                if (std::isalpha((unsigned char)t[i]) || t[i] == '_') {
                    size_t start = i;
                    while (i < t.size() && (std::isalnum((unsigned char)t[i]) || t[i] == '_')) i++;
                    std::string tok = t.substr(start, i - start);
                    // treat as template param if all-uppercase, length 1-3, not a known type
                    bool allUpper = true;
                    for (char c : tok) if (!std::isupper((unsigned char)c)) { allUpper = false; break; }
                    if (allUpper && tok.size() <= 3 && !NOT_TMPL.count(tok))
                        params.push_back(tok);
                } else {
                    i++;
                }
            }
            return params;
        };
        std::unordered_set<std::string> tmplParamSet;
        for (auto& ct : argCTypes)
            for (auto& p : extractTemplateParams(ct)) tmplParamSet.insert(p);
        for (auto& p : extractTemplateParams(retCType)) tmplParamSet.insert(p);

        // When arg types are unknown (registry from compiled .hh) or contain template
        // type params (T, U, …), emit as a generic auto lambda object (C++14) so the
        // block can be passed to __Block_xxx template params without deduction issues.
        if ((argCTypes.empty() || !tmplParamSet.empty()) && !headerParams.empty()) {
            preamble << "auto " << name << " = [](";
            for (size_t k = 0; k < headerParams.size(); k++) {
                if (k > 0) preamble << ", ";
                preamble << "auto " << headerParams[k];
            }
            preamble << ") {" << bodyStr << "};\n";
            return;
        }

        if (!tmplParamSet.empty()) {
            preamble << "template<";
            bool ft = true;
            for (auto& p : tmplParamSet) {
                if (!ft) preamble << ", ";
                ft = false;
                preamble << "typename " << p;
            }
            preamble << ">\n";
        }

        preamble << retCType << " " << name << "(";

        bool first = true;
        if (!argCTypes.empty()) {
            for (size_t k = 0; k < argCTypes.size(); k++) {
                if (!first) preamble << ", ";
                first = false;
                preamble << argCTypes[k];
                if (k < headerParams.size()) preamble << " " << headerParams[k];
            }
        } else {
            // argCTypes unknown (from compiled .hh registry) — emit auto for each header param
            for (size_t k = 0; k < headerParams.size(); k++) {
                if (!first) preamble << ", ";
                first = false;
                preamble << "auto " << headerParams[k];
            }
        }

        for (auto& [capName, capType] : captures) {
            if (!first) preamble << ", ";
            first = false;
            preamble << capType << " " << capName;
        }

        // Split into lines, strip common leading indent, re-apply one level
        {
            // collect lines
            std::vector<std::string> lines;
            std::istringstream ss(bodyStr);
            std::string ln;
            while (std::getline(ss, ln)) lines.push_back(ln);

            // find min indent among non-blank lines
            size_t minIndent = std::string::npos;
            for (auto& l : lines) {
                size_t first = l.find_first_not_of(" \t");
                if (first == std::string::npos) continue;
                if (minIndent == std::string::npos || first < minIndent) minIndent = first;
            }
            if (minIndent == std::string::npos) minIndent = 0;

            // rebuild with single indent level
            std::ostringstream norm;
            bool any = false;
            for (auto& l : lines) {
                size_t first = l.find_first_not_of(" \t");
                if (first == std::string::npos) continue; // skip blank lines
                if (any) norm << "\n";
                norm << "    " << l.substr(minIndent);
                any = true;
            }
            bodyStr = norm.str();
        }

        preamble << ") {\n" << bodyStr << "\n}\n\n";
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

            // %T rewriting inside lambda bodies
            if (tok.type == TK::STRING && tok.value.find("%T") != std::string::npos) {
                auto tPositions = findTArgPositions(tok.value);
                if (!tPositions.empty()) {
                    // Collect comma-separated arg token ranges until matching RPAREN
                    auto collectArgsLocal = [&](size_t start, size_t& endOut)
                        -> std::vector<std::pair<size_t,size_t>>
                    {
                        std::vector<std::pair<size_t,size_t>> ranges;
                        size_t j = start;
                        while (j < toks.size() && toks[j].type != TK::END) {
                            if (toks[j].type == TK::RPAREN) { endOut = j; return ranges; }
                            if (toks[j].type == TK::OTHER && toks[j].value == ",") {
                                j++;
                                while (j < toks.size() && toks[j].type == TK::OTHER &&
                                       !toks[j].value.empty() &&
                                       std::isspace((unsigned char)toks[j].value[0])) j++;
                                size_t argStart = j;
                                int depth = 0;
                                while (j < toks.size() && toks[j].type != TK::END) {
                                    if (toks[j].type == TK::LPAREN) depth++;
                                    else if (toks[j].type == TK::RPAREN) {
                                        if (depth == 0) break;
                                        depth--;
                                    } else if (toks[j].type == TK::OTHER &&
                                               toks[j].value == "," && depth == 0) break;
                                    j++;
                                }
                                size_t argEnd = j;
                                while (argEnd > argStart && toks[argEnd-1].type == TK::OTHER &&
                                       !toks[argEnd-1].value.empty() &&
                                       std::isspace((unsigned char)toks[argEnd-1].value[0])) argEnd--;
                                ranges.push_back({argStart, argEnd});
                            } else { j++; }
                        }
                        endOut = j;
                        return ranges;
                    };

                    size_t endIdx = i + 1;
                    auto argRanges = collectArgsLocal(i + 1, endIdx);
                    std::string newFmt = tok.value;
                    size_t p = 0;
                    while ((p = newFmt.find("%T", p)) != std::string::npos) {
                        newFmt.replace(p, 2, "%s"); p += 2;
                    }
                    buf << newFmt;
                    std::unordered_set<size_t> tSet(tPositions.begin(), tPositions.end());
                    for (size_t ai = 0; ai < argRanges.size(); ai++) {
                        auto [argStart, argEnd] = argRanges[ai];
                        std::vector<Token> argToks(toks.begin() + argStart, toks.begin() + argEnd);
                        std::string argStr;
                        for (auto& at : argToks) {
                            if (namespaces.count(at.value) && at.type == TK::IDENT) {
                                argStr += at.value + "::It"; // basic namespace→It expansion
                            } else if (at.type == TK::DOT) {
                                argStr += "::";
                            } else {
                                auto tm = TYPE_MAP.find(at.value);
                                argStr += (tm != TYPE_MAP.end()) ? tm->second : at.value;
                            }
                        }
                        if (tSet.count(ai)) buf << ", uhc_tostring(" << argStr << ")";
                        else               buf << ", " << argStr;
                    }
                    i = endIdx; // stop before the RPAREN — caller emits it
                    continue;
                }
                buf << tok.value; i++; continue;
            }

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

                // Swizzle: ident.xyzw / ident.rgba (2-4 chars from {x,y,z,w,r,g,b,a})
                {
                    size_t j = nextNonWSLocal(i + 1);
                    if (j < toks.size() && toks[j].type == TK::DOT) {
                        size_t k = nextNonWSLocal(j + 1);
                        if (k < toks.size() && toks[k].type == TK::IDENT &&
                            isSwizzleIdent(toks[k].value)) {
                            emitSwizzle(buf, v, toks[k].value);
                            i = k + 1;
                            continue;
                        }
                    }
                }

                buf << v; i++; continue;
            }

            buf << tok.value; i++;
        }

        return buf.str();
    }

    std::string applyPatches(std::string outStr) {
        // Inject template Block type params into template<...> declarations.
        if (!templatePatches.empty()) {
            auto sorted = templatePatches;
            std::sort(sorted.begin(), sorted.end(),
                      [](auto& a, auto& b) { return a.offset > b.offset; });
            for (auto& p : sorted)
                if (p.offset <= outStr.size()) outStr.insert(p.offset, p.insertion);
        }

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

            // skip const/unsigned/signed qualifiers
            if (tokens[i].type == TK::IDENT &&
                (tokens[i].value == "const" || tokens[i].value == "unsigned" ||
                 tokens[i].value == "signed")) {
                i++;
                continue;
            }

            if (tokens[i].type != TK::IDENT) { i++; continue; }

            // Use parseCType to handle templates like List<T> and pointer/ref suffixes.
            std::string typePart = parseCType(i);

            i = nextNonWS(i);
            if (i < tokens.size() && tokens[i].type == TK::IDENT &&
                tokens[i].value != "const" && tokens[i].value != "unsigned") {
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

    // --- toString helpers ---

    static std::vector<size_t> findTArgPositions(const std::string& fmt) {
        std::vector<size_t> positions;
        size_t argIdx = 0;
        for (size_t k = 1; k + 1 < fmt.size(); k++) {
            if (fmt[k] != '%') continue;
            k++;
            if (k + 1 >= fmt.size()) break;
            if (fmt[k] == '%') continue;
            while (k < fmt.size()-1 && (fmt[k]=='-'||fmt[k]=='+'||fmt[k]==' '||fmt[k]=='#'||fmt[k]=='0')) k++;
            if (k < fmt.size()-1 && fmt[k]=='*') k++;
            else while (k < fmt.size()-1 && std::isdigit((unsigned char)fmt[k])) k++;
            if (k < fmt.size()-1 && fmt[k]=='.') {
                k++;
                if (k < fmt.size()-1 && fmt[k]=='*') k++;
                else while (k < fmt.size()-1 && std::isdigit((unsigned char)fmt[k])) k++;
            }
            while (k < fmt.size()-1 && (fmt[k]=='h'||fmt[k]=='l'||fmt[k]=='L'||fmt[k]=='z'||fmt[k]=='j'||fmt[k]=='t')) k++;
            if (k < fmt.size()-1 && fmt[k]=='T') positions.push_back(argIdx);
            argIdx++;
        }
        return positions;
    }

    // Collect per-arg token ranges [start,end) after a format string token.
    // Stops at the RPAREN that closes the current call level (depth 0 relative to start).
    // Sets endIdx to that RPAREN's position.
    std::vector<std::pair<size_t,size_t>> collectFormatArgs(size_t start, size_t& endIdx) const {
        std::vector<std::pair<size_t,size_t>> ranges;
        size_t j = start;
        while (j < tokens.size() && tokens[j].type != TK::END) {
            if (tokens[j].type == TK::RPAREN) { endIdx = j; return ranges; }
            if (tokens[j].type == TK::OTHER && tokens[j].value == ",") {
                j++;
                while (j < tokens.size() && tokens[j].type == TK::OTHER &&
                       !tokens[j].value.empty() && std::isspace((unsigned char)tokens[j].value[0])) j++;
                size_t argStart = j;
                int depth = 0;
                while (j < tokens.size() && tokens[j].type != TK::END) {
                    if (tokens[j].type == TK::LPAREN) depth++;
                    else if (tokens[j].type == TK::RPAREN) { if (depth==0) break; depth--; }
                    else if (depth==0 && tokens[j].type==TK::OTHER && tokens[j].value==",") break;
                    j++;
                }
                size_t argEnd = j;
                while (argEnd > argStart && tokens[argEnd-1].type == TK::OTHER &&
                       !tokens[argEnd-1].value.empty() &&
                       std::isspace((unsigned char)tokens[argEnd-1].value[0])) argEnd--;
                ranges.push_back({argStart, argEnd});
            } else {
                j++;
            }
        }
        endIdx = j;
        return ranges;
    }

public:
    Transpiler(const std::vector<Token>& toks,
               const std::unordered_set<std::string>& ns,
               const std::unordered_map<std::string, NSInfo>& fileInfo,
               const std::unordered_set<std::string>& globalUserToString,
               const std::unordered_map<std::string, LambdaReg>& externalLambdas = {})
        : tokens(toks), namespaces(ns), fileNSInfo(fileInfo), globalUserToStringNS(globalUserToString) {
        lambdaRegistry = externalLambdas;
    }

    std::string run() {
        std::string currentFuncName;
        std::string lastIdentAtDepth0;

        std::string paramListCallee;
        size_t paramListOpenIdx = 0;

        // Emit uhc_tostring template base (guarded so multiple includes don't redefine it).
        // Specializations are emitted after each self-namespace's closing brace.
        preamble << "#include <stdio.h>\n"
                 << "#ifndef UHC_TOSTRING_DEFINED\n"
                 << "#define UHC_TOSTRING_DEFINED\n"
                 << "template<typename T> inline const char* uhc_tostring(T&) { return \"[object]\"; }\n"
                 << "#endif\n";

        size_t i = 0;
        while (i < tokens.size()) {
            const Token& tok = tokens[i];

            if (tok.type == TK::END) break;

            if (tok.type == TK::LBRACE) {
                globalBraceDepth++;
                if (inFunctionBody) braceDepth++;

                // Only enter function body if this { is not a namespace/struct {
                if (!inFunctionBody && !pendingFuncForBrace.empty() &&
                    pendingNamespaceName.empty()) {
                    currentFuncName    = pendingFuncForBrace;
                    inFunctionBody     = true;
                    braceDepth         = 1;
                    funcBodyStartDepth = globalBraceDepth;
                    scopeVars.clear();
                    size_t pi = pendingFuncParamIdx + 1;
                    recordFunctionParams(pi);
                    pendingFuncForBrace.clear();
                } else if (!pendingFuncForBrace.empty() && !pendingNamespaceName.empty()) {
                    pendingFuncForBrace.clear(); // namespace { consumed the brace, not a function
                }
                // Track namespace name when entering a namespace block
                if (globalBraceDepth == 1 && !pendingNamespaceName.empty()) {
                    currentNamespace = pendingNamespaceName;
                    pendingNamespaceName.clear();
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
                // Before closing a namespace block, inject auto-generated toString if needed,
                // then emit a uhc_tostring specialization after the closing brace.
                std::string closingNS;
                if (globalBraceDepth == 1 && !currentNamespace.empty()) {
                    closingNS = currentNamespace;
                    auto it = fileNSInfo.find(closingNS);
                    if (it != fileNSInfo.end() && it->second.hasSelf &&
                        !it->second.isTemplate &&
                        !globalUserToStringNS.count(closingNS)) {
                        const auto& fields = it->second.selfFields;
                        if (fields.empty()) {
                            out << "    static const char* toString(It& __self) {"
                                << " (void)__self; return \"" << closingNS << "{}\"; }\n";
                        } else {
                            std::string fmt = closingNS + "{";
                            std::string args;
                            for (size_t fi = 0; fi < fields.size(); fi++) {
                                if (fi > 0) { fmt += ", "; args += ", "; }
                                fmt += fields[fi].name + ": " + fields[fi].fmtSpec;
                                args += "__self." + fields[fi].name;
                            }
                            fmt += "}";
                            // Rotating pool of 4 buffers so multiple %T in one printf call
                            // don't clobber each other (each uhc_tostring() call gets its own slot).
                            out << "    static const char* toString(It& __self) {"
                                << " static char __pool[4][512]; static int __pi = 0;"
                                << " __pi = (__pi + 1) & 3; char* __buf = __pool[__pi];"
                                << " snprintf(__buf, 512, \"" << fmt << "\", " << args << ");"
                                << " return __buf; }\n";
                        }
                    }
                    currentNamespace.clear();
                }
                globalBraceDepth--;
                out << tok.value;
                if (!closingNS.empty()) {
                    auto it = fileNSInfo.find(closingNS);
                    if (it != fileNSInfo.end() && it->second.hasSelf && !it->second.isTemplate &&
                        !emittedUhcToString.count(closingNS)) {
                        emittedUhcToString.insert(closingNS);
                        out << "\ninline const char* uhc_tostring(" << closingNS
                            << "::It& v) { return " << closingNS << "::toString(v); }";
                    }
                }
                i++; continue;
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
                    if (!inFunctionBody) {
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
                            !frame.hasLambdaParam && globalBraceDepth > 0) {
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

                            auto captures = detectCaptures(eb.headerParams, eb.bodyTokens);

                            {
                                std::string argsSoFar = out.str().substr(outLenAtArgStart + 1);
                                bool hasArgs = argsSoFar.find_first_not_of(" \t\r\n") != std::string::npos;
                                if (hasArgs) out << ", ";
                            }

                            if (!captures.empty()) {
                                // Inline C++ lambda with capture list; use auto params
                                // so template type params (e.g. T) don't leak into scope.
                                auto written = detectWrittenCaptures(captures, eb.bodyTokens);
                                emitCaptureList(out, captures, written);
                                out << "(";
                                // argCTypes may be empty when registry came from a compiled .hh;
                                // fall back to headerParams count so params are still emitted.
                                size_t paramCount = reg->argCTypes.empty()
                                    ? eb.headerParams.size()
                                    : reg->argCTypes.size();
                                for (size_t k = 0; k < paramCount; k++) {
                                    if (k > 0) out << ", ";
                                    out << "auto";
                                    if (k < eb.headerParams.size()) out << " " << eb.headerParams[k];
                                }
                                out << ") {" << transpileTokensToString(eb.bodyTokens) << "}";
                            } else {
                                std::string lambdaName = generateLambdaName(eb.bodyTokens);
                                emitLambdaFunction(lambdaName, reg->retCType, reg->argCTypes,
                                                   eb.headerParams, eb.bodyTokens, captures);
                                out << lambdaName;
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

            // Handle %T format specifier: rewrite to %s and wrap args with uhc_tostring()
            if (tok.type == TK::STRING && parenDepth > 0 &&
                tok.value.find("%T") != std::string::npos) {
                auto tPositions = findTArgPositions(tok.value);
                if (!tPositions.empty()) {
                    size_t endIdx = i + 1;
                    auto argRanges = collectFormatArgs(i + 1, endIdx);
                    // Rewrite %T → %s in format string
                    std::string newFmt = tok.value;
                    size_t p = 0;
                    while ((p = newFmt.find("%T", p)) != std::string::npos) {
                        newFmt.replace(p, 2, "%s");
                        p += 2;
                    }
                    out << newFmt;
                    std::unordered_set<size_t> tSet(tPositions.begin(), tPositions.end());
                    for (size_t ai = 0; ai < argRanges.size(); ai++) {
                        auto [argStart, argEnd] = argRanges[ai];
                        std::vector<Token> argToks(tokens.begin() + argStart,
                                                    tokens.begin() + argEnd);
                        out << ", ";
                        if (tSet.count(ai)) out << "uhc_tostring(";
                        out << transpileTokensToString(argToks);
                        if (tSet.count(ai)) out << ")";
                    }
                    i = endIdx; // let normal loop handle the closing RPAREN
                    continue;
                }
            }

            if (tok.type == TK::OTHER        ||
                tok.type == TK::LINE_COMMENT ||
                tok.type == TK::BLOCK_COMMENT||
                tok.type == TK::NUMBER       ||
                tok.type == TK::STRING       ||
                tok.type == TK::CHAR_LIT     ||
                tok.type == TK::SEMICOLON)
            {
                // Preserve lastIdentAtDepth0 through explicit template args like foo<int>(...)
                // so that trailing-lambda detection still works for calls like foo<T>(...) { ... }
                // Requires lookahead to distinguish from comparison operators (e.g. `diff < 0`).
                if (parenDepth == 0 && tok.type == TK::OTHER && tok.value == "<" &&
                    !lastIdentAtDepth0.empty()) {
                    // Lookahead: find matching '>' then check next token is '('
                    size_t la = i + 1;
                    int laDepth = 1;
                    bool isTmplCall = false;
                    while (la < tokens.size() && laDepth > 0) {
                        if (tokens[la].type == TK::OTHER && tokens[la].value == "<") laDepth++;
                        else if (tokens[la].type == TK::OTHER && tokens[la].value == ">") {
                            laDepth--;
                            if (laDepth == 0) {
                                size_t after = nextNonWS(la + 1);
                                isTmplCall = (after < tokens.size() &&
                                              tokens[after].type == TK::LPAREN);
                            }
                        } else if (tokens[la].type == TK::SEMICOLON ||
                                   tokens[la].type == TK::LBRACE  ||
                                   tokens[la].type == TK::RBRACE) {
                            break; // definitely not template args
                        }
                        la++;
                    }
                    if (isTmplCall) {
                        // Emit <...> with type translation, preserving lastIdentAtDepth0
                        out << tok.value; i++;
                        int tmplDepth = 1;
                        while (i < tokens.size() && tmplDepth > 0) {
                            if (tokens[i].type == TK::OTHER && tokens[i].value == "<")
                                { tmplDepth++; out << "<"; i++; }
                            else if (tokens[i].type == TK::OTHER && tokens[i].value == ">") {
                                tmplDepth--;
                                out << ">"; i++;
                            } else if (tokens[i].type == TK::IDENT) {
                                auto tm = TYPE_MAP.find(tokens[i].value);
                                out << (tm != TYPE_MAP.end() ? tm->second : tokens[i].value);
                                i++;
                            } else {
                                out << tokens[i].value; i++;
                            }
                        }
                        continue; // lastIdentAtDepth0 preserved
                    }
                }
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

                if (v == "template") {
                    out << v; i++;
                    // Emit the <typename ...> block and record where the closing > is.
                    size_t j = nextNonWS(i);
                    if (j < tokens.size() && tokens[j].type == TK::OTHER && tokens[j].value == "<") {
                        out << tokens[j].value; i = j + 1;
                        int depth = 1;
                        while (i < tokens.size() && depth > 0) {
                            const std::string& cv = tokens[i].value;
                            if (tokens[i].type == TK::OTHER && cv == "<") { depth++; out << cv; i++; }
                            else if (tokens[i].type == TK::OTHER && cv == ">") {
                                depth--;
                                if (depth == 0) {
                                    lastTemplateCloseOffset = out.str().size(); // before >
                                    out << cv; i++;
                                } else { out << cv; i++; }
                            } else { out << tokens[i].value; i++; }
                        }
                    }
                    continue;
                }

                if (v == "namespace" && globalBraceDepth == 0) {
                    size_t j = nextNonWS(i + 1);
                    if (j < tokens.size() && tokens[j].type == TK::IDENT)
                        pendingNamespaceName = tokens[j].value;
                }

                // `return "fmt", args` sugar: rewrite to snprintf + return inside toString
                if (v == "return" && globalBraceDepth > 0) {
                    size_t n = nextNonWS(i + 1);
                    if (n < tokens.size() && tokens[n].type == TK::STRING) {
                        size_t m = nextNonWS(n + 1);
                        if (m < tokens.size() && tokens[m].type == TK::OTHER &&
                            tokens[m].value == ",") {
                            std::string fmtStr = tokens[n].value;
                            std::vector<Token> argToks;
                            size_t j = m + 1;
                            while (j < tokens.size()) {
                                if (tokens[j].type == TK::END ||
                                    tokens[j].type == TK::SEMICOLON) { j++; break; }
                                argToks.push_back(tokens[j]);
                                j++;
                            }
                            while (!argToks.empty() && argToks.front().type == TK::OTHER &&
                                   !argToks.front().value.empty() &&
                                   std::isspace((unsigned char)argToks.front().value[0]))
                                argToks.erase(argToks.begin());
                            while (!argToks.empty() && argToks.back().type == TK::OTHER &&
                                   !argToks.back().value.empty() &&
                                   std::isspace((unsigned char)argToks.back().value[0]))
                                argToks.pop_back();
                            out << "{ static char __pool[4][512]; static int __pi = 0;"
                                << " __pi = (__pi + 1) & 3; char* __buf = __pool[__pi];"
                                << " snprintf(__buf, 512, " << fmtStr;
                            if (!argToks.empty())
                                out << ", " << transpileTokensToString(argToks);
                            out << "); return __buf; }";
                            i = j;
                            continue;
                        }
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
                        // If followed by ::, user wrote explicit scope (e.g. Namespace::It).
                        // In a function param list, still add & so it's passed by reference.
                        {
                            size_t nextCheck = nextNonWS(i + 1);
                            if (nextCheck < tokens.size() && tokens[nextCheck].type == TK::SCOPE) {
                                if (parenDepth > 0 && !inFunctionBody) {
                                    // Lookahead: Namespace :: It  → emit Namespace::It&
                                    size_t afterScope = nextNonWS(nextCheck + 1);
                                    if (afterScope < tokens.size() &&
                                        tokens[afterScope].type == TK::IDENT &&
                                        tokens[afterScope].value == "It") {
                                        // Emit Namespace::It (all tokens including whitespace)
                                        for (size_t w = i; w <= afterScope; w++)
                                            out << tokens[w].value;
                                        i = afterScope + 1;
                                        // Add & unless user already wrote * or &
                                        size_t ni = nextNonWS(i);
                                        if (ni >= tokens.size() ||
                                            (tokens[ni].value != "*" && tokens[ni].value != "&"))
                                            out << "&";
                                        lastIdentAtDepth0.clear();
                                        continue;
                                    }
                                }
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

                            auto captures = detectCaptures(eb.headerParams, eb.bodyTokens);

                            out << v << "(";
                            if (!captures.empty()) {
                                // Inline C++ lambda with capture list; use auto params
                                // so template type params (e.g. T) don't leak into scope.
                                auto written = detectWrittenCaptures(captures, eb.bodyTokens);
                                emitCaptureList(out, captures, written);
                                out << "(";
                                // argCTypes may be empty when registry came from a compiled .hh;
                                // fall back to headerParams count so params are still emitted.
                                size_t paramCount = reg->argCTypes.empty()
                                    ? eb.headerParams.size()
                                    : reg->argCTypes.size();
                                for (size_t k = 0; k < paramCount; k++) {
                                    if (k > 0) out << ", ";
                                    out << "auto";
                                    if (k < eb.headerParams.size()) out << " " << eb.headerParams[k];
                                }
                                out << ") {" << transpileTokensToString(eb.bodyTokens) << "}";
                            } else {
                                std::string lambdaName = generateLambdaName(eb.bodyTokens);
                                emitLambdaFunction(lambdaName, reg->retCType, reg->argCTypes,
                                                   eb.headerParams, eb.bodyTokens, captures);
                                out << lambdaName;
                            }
                            out << ")";
                            lastIdentAtDepth0.clear();
                            continue;
                        }
                    }
                }

                // Swizzle: ident.xyzw / ident.rgba (2-4 chars from {x,y,z,w,r,g,b,a})
                {
                    size_t j = nextNonWS(i + 1);
                    if (j < tokens.size() && tokens[j].type == TK::DOT) {
                        size_t k = nextNonWS(j + 1);
                        if (k < tokens.size() && tokens[k].type == TK::IDENT &&
                            isSwizzleIdent(tokens[k].value)) {
                            emitSwizzle(out, v, tokens[k].value);
                            i = k + 1;
                            continue;
                        }
                    }
                }

                out << v; i++; continue;
            }

            out << tok.value; i++;
        }

        std::string outStr = applyPatches(out.str());

        if (needsTypeTraits) {
            outStr = "#include <type_traits>\n" + outStr;
        }
        if (needsFunctional) {
            outStr = "#include <functional>\n" + outStr;
        }

        std::string lambdaStr = preamble.str();
        if (lambdaStr.empty()) return outStr;

        // Find insertion point: after all leading preprocessor lines (includes, #if guards, etc.)
        // Insert lambdas there so they come after includes but before C++ code.
        size_t insertPos = 0;
        size_t pos = 0;
        while (pos < outStr.size()) {
            size_t lineEnd = outStr.find('\n', pos);
            if (lineEnd == std::string::npos) lineEnd = outStr.size();
            size_t nonWS = pos;
            while (nonWS < lineEnd && std::isspace((unsigned char)outStr[nonWS])) nonWS++;
            if (nonWS < lineEnd && outStr[nonWS] == '#') {
                insertPos = lineEnd + 1;
                pos = lineEnd + 1;
                continue;
            }
            if (nonWS >= lineEnd) { pos = lineEnd + 1; continue; } // blank line
            break; // first non-empty non-preprocessor line: stop
        }
        outStr.insert(insertPos, lambdaStr);
        return outStr;
    }
};

std::string transpile(const std::vector<Token>& tokens,
                      const std::unordered_set<std::string>& namespaces,
                      const std::unordered_map<std::string, NSInfo>& fileNSInfo,
                      const std::unordered_set<std::string>& globalUserToStringNS,
                      const std::unordered_map<std::string, LambdaReg>& externalLambdas = {}) {
    auto processed = injectImplicitSemicolons(tokens);
    return Transpiler(processed, namespaces, fileNSInfo, globalUserToStringNS, externalLambdas).run();
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
    std::vector<fs::path>    includeDirs;
    std::vector<std::string> positional;
    std::vector<std::string> compilerFlags;
    std::string outputBinary;
    bool preserveSource = false;
    bool verbose        = false;

    static const std::unordered_set<std::string> twoArgFlags = {
        "-framework", "-isystem", "-arch", "-target", "-x",
        "-include", "-MF", "-MT", "-iframework", "-isysroot", "-rpath"
    };

    for (int a = 1; a < argc; a++) {
        std::string arg = argv[a];
        if (arg == "--version") {
            std::cout << "unholyc " UHC_VERSION(UHC_COMMIT) "\n";
            return 0;
        } else if (arg == "-v") {
            verbose = true;
        } else if (arg == "--preserve-source") {
            preserveSource = true;
        } else if (arg == "-o" && a + 1 < argc) {
            outputBinary = argv[++a];
        } else if (arg.size() >= 2 && arg[0] == '-' && arg[1] == 'I') {
            std::string dirStr = arg.substr(2);
            if (dirStr.empty() && a + 1 < argc) {
                dirStr = argv[++a];
                compilerFlags.push_back("-I" + dirStr);
            } else {
                compilerFlags.push_back(arg);
            }
            fs::path dir = dirStr;
            if (!fs::is_directory(dir)) {
                std::cerr << "Warning: -I path is not a directory: " << dir << "\n";
            } else {
                includeDirs.push_back(dir);
            }
        } else if (arg.size() >= 2 && arg[0] == '-' && arg[1] == 'L') {
            std::string dirStr = arg.substr(2);
            if (dirStr.empty() && a + 1 < argc) {
                dirStr = argv[++a];
                compilerFlags.push_back("-L" + dirStr);
            } else {
                compilerFlags.push_back(arg);
            }
        } else if (!arg.empty() && arg[0] == '-') {
            compilerFlags.push_back(arg);
            if (twoArgFlags.count(arg) && a + 1 < argc)
                compilerFlags.push_back(argv[++a]);
        } else {
            positional.push_back(arg);
        }
    }

    bool driverMode = !outputBinary.empty();

    // Auto-detect UHC stdlib prefix: UHC_HOME env > binary sibling > $HOME/.local > /usr/local
    {
        auto probePrefix = [](const fs::path& p) {
            return fs::exists(p / "lib" / "libuhc.a");
        };
        fs::path prefix;
        const char* uhcHomeEnv = std::getenv("UHC_HOME");
        if (!prefix.empty()) {
            // already set
        } else if (uhcHomeEnv && *uhcHomeEnv) {
            fs::path p = uhcHomeEnv;
            if (probePrefix(p)) prefix = p;
        }
        if (prefix.empty()) {
            try {
                fs::path bin = fs::canonical(argv[0]);
                fs::path candidate = bin.parent_path().parent_path();
                if (probePrefix(candidate)) prefix = candidate;
            } catch (...) {}
        }
        if (prefix.empty()) {
            const char* home = std::getenv("HOME");
#if defined(_WIN32)
            if (!home) home = std::getenv("USERPROFILE");
#endif
            if (home && *home) {
                fs::path p = fs::path(home) / ".local";
                if (probePrefix(p)) prefix = p;
            }
        }
        if (prefix.empty() && probePrefix("/usr/local")) prefix = "/usr/local";

        if (!prefix.empty()) {
            fs::path incDir = prefix / "include";
            fs::path libDir = prefix / "lib";
            if (fs::is_directory(incDir))
                includeDirs.push_back(incDir);
            if (driverMode) {
                bool hasLuhc = std::find(compilerFlags.begin(), compilerFlags.end(), "-luhc") != compilerFlags.end();
                compilerFlags.insert(compilerFlags.begin(), "-I" + incDir.string());
                compilerFlags.insert(compilerFlags.begin() + 1, "-L" + libDir.string());
                if (!hasLuhc) compilerFlags.push_back("-luhc");
            }
        }
    }

    if (driverMode && positional.size() < 1) {
        std::cerr << "Usage: unholyc <input_dir_or_file> -o <output> [flags...] [--preserve-source]\n";
        return 1;
    }
    if (!driverMode && positional.size() < 2) {
        std::cerr << "Usage: unholyc <input_dir_or_file> <output_dir> [-I<include_dir> ...]\n";
        return 1;
    }

    fs::path inRoot  = positional[0];
    fs::path outRoot;
    if (driverMode) {
        auto ts  = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count();
        outRoot  = fs::temp_directory_path() / ("uhc_" + std::to_string(ts));
    } else {
        outRoot  = fs::absolute(fs::path(positional[1]));
    }

    // Accept a single file as input: use its parent as the scan root but only
    // compile that one file in the main processing loop.
    fs::path singleInputFile;
    if (fs::is_regular_file(inRoot)) {
        singleInputFile = fs::canonical(inRoot);
        inRoot          = singleInputFile.parent_path();
    } else if (!fs::is_directory(inRoot)) {
        std::cerr << "Error: " << inRoot << " is not a file or directory\n";
        return 1;
    }

    inRoot = fs::canonical(inRoot);

    std::unordered_set<std::string> globalNS;
    std::unordered_set<std::string> globalUserToStringNS;

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
                auto info   = collectNSInfo(tokens);
                // Only treat toString as user-defined when found in UHC source files
                // (.uhh/.uhc). Compiled .hh headers may contain auto-generated toString
                // methods from a previous build — scanning those would incorrectly suppress
                // toString auto-generation for the same namespace in the current build.
                if (ext == ".uhh" || ext == ".uhc") {
                    for (auto& [name, ni] : info)
                        if (ni.hasUserToString) globalUserToStringNS.insert(name);
                }
            }
        }
    };

    scanDir(inRoot);
    for (auto& incDir : includeDirs)
        scanDir(incDir);

    // Pre-collect lambda declarations from all .uhh files so trailing-lambda
    // syntax works when the function is defined in an included header.
    std::unordered_map<std::string, LambdaReg> globalLambdas;
    auto scanLambdas = [&](const fs::path& dir) {
        for (auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            auto src    = readFile(entry.path().string());
            auto tokens = Lexer(src).tokenize();
            if (ext == ".uhh") {
                auto decls = collectLambdaDecls(tokens);
                globalLambdas.insert(decls.begin(), decls.end());
            } else if (ext == ".hh") {
                auto decls = collectLambdaDeclsFromHH(tokens);
                globalLambdas.insert(decls.begin(), decls.end());
            }
        }
    };
    scanLambdas(inRoot);
    for (auto& incDir : includeDirs)
        scanLambdas(incDir);

    static const std::unordered_set<std::string> copyExts = {
        ".c", ".cpp", ".cc", ".h", ".hpp", ".hh"
    };

    for (auto& entry : fs::recursive_directory_iterator(inRoot)) {
        if (!entry.is_regular_file()) continue;
        if (!singleInputFile.empty() && entry.path() != singleInputFile) continue;
        auto ext     = entry.path().extension().string();
        if (ext != ".uhc" && ext != ".uhh" && !copyExts.count(ext)) continue;

        auto outPath = relativeOutputPath(entry.path(), inRoot, outRoot);
        fs::create_directories(outPath.parent_path());

        if (ext == ".uhc" || ext == ".uhh") {
            auto src      = readFile(entry.path().string());
            auto tokens   = Lexer(src).tokenize();
            auto fileInfo = collectNSInfo(tokens);
            auto output   = transpile(tokens, globalNS, fileInfo, globalUserToStringNS, globalLambdas);
            writeFile(outPath.string(), output);
            if (verbose) std::cout << entry.path().string() << "  ->  " << outPath.string() << "\n";
        } else {
            fs::copy_file(entry.path(), outPath, fs::copy_options::overwrite_existing);
            if (verbose) std::cout << entry.path().string() << "  ->  " << outPath.string() << " (copied)\n";
        }
    }

    if (!driverMode)
        return 0;

    // --- compiler driver ---
    std::vector<std::string> sources;
    std::ostringstream       autoIncludes;
    autoIncludes << " -I\"" << outRoot.string() << "\"";
    for (auto& e : fs::recursive_directory_iterator(outRoot)) {
        if (e.is_directory())
            autoIncludes << " -I\"" << e.path().string() << "\"";
        else if (e.path().extension() == ".cc")
            sources.push_back("\"" + e.path().string() + "\"");
    }

    const char* envCxx = std::getenv("CXX");
    std::string cxx    = (envCxx && *envCxx) ? envCxx : "c++";

    std::ostringstream cmd;
    cmd << cxx << " -std=c++17";
    cmd << autoIncludes.str();
    for (auto& s : sources)      cmd << " " << s;
    for (auto& f : compilerFlags) cmd << " " << f;
    cmd << " -o \"" << outputBinary << "\"";

    if (verbose) std::cout << cmd.str() << "\n";

    int ret = std::system(cmd.str().c_str());

    if (!preserveSource)
        fs::remove_all(outRoot);

    return ret == 0 ? 0 : 1;
}
