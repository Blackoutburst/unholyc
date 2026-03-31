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

// ─── Data structures ───────────────────────────────────────────────────────

struct ScopeVar {
    std::string name;
    std::string cType;
};

struct LambdaReg {
    std::string              lambdaParamName; // the param name (e.g. "block")
    std::string              calleeName;      // the function that accepts this lambda
    std::vector<std::string> argCTypes;       // C types of lambda parameters
    std::string              retCType;        // C return type
};

struct PatchSite {
    size_t      outputByteOffset; // byte offset in out buffer of the closing ')' of callee param list
    std::string calleeName;
    // Also patch the lambda function-pointer for captures:
    // offset of the closing ')' of the inner func-ptr param list
    size_t      fptrInnerCloseOffset; // offset of ')' that closes the lambda's arg type list
    std::string lambdaParamName;      // which lambda param to patch
};

struct CallSiteCapture {
    std::string calleeName;
    int         sourceLine;
    std::vector<std::pair<std::string,std::string>> captures; // (name, cType)
};

struct CallFrame {
    std::string calleeName;
    int         parenDepthAtOpen; // parenDepth when '(' was seen
};

// ─── FNV-1a hash helper ────────────────────────────────────────────────────

static uint32_t fnv1a32(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

// ─── Transpiler class ──────────────────────────────────────────────────────

class Transpiler {
    const std::vector<Token>&            tokens;
    const std::unordered_set<std::string>& namespaces;

    std::ostringstream out;
    std::ostringstream preamble;

    // Lambda registry: maps lambdaParamName -> LambdaReg (keyed by callee+param for uniqueness)
    // We key by callee::paramName
    std::unordered_map<std::string, LambdaReg> lambdaRegistry;

    // Patch sites recorded during lambda declaration parsing
    std::vector<PatchSite> patchSites;

    // Call site captures recorded when call sites are processed
    std::vector<CallSiteCapture> callSiteCaptures;

    // Scope tracking
    std::vector<ScopeVar> scopeVars;
    bool inFunctionBody    = false;
    int  braceDepth        = 0;  // depth inside top-level function body (1 = inside it)
    int  globalBraceDepth  = 0;  // absolute brace depth for detecting function entry

    // Call stack for trailing lambda detection
    std::vector<CallFrame> callStack;

    // Hash collision tracking: hash -> count of times seen
    std::unordered_map<std::string, int> lambdaNameCount;

    // paren depth for general tracking
    int parenDepth = 0;

    // ── helpers ──────────────────────────────────────────────────────────

    size_t nextNonWS(size_t i) const {
        while (i < tokens.size() && tokens[i].type == TK::OTHER &&
               !tokens[i].value.empty() && std::isspace((unsigned char)tokens[i].value[0]))
            i++;
        return i;
    }

    // Resolve a single identifier through TYPE_MAP; return original if not found
    std::string resolveType(const std::string& v) const {
        auto it = TYPE_MAP.find(v);
        return (it != TYPE_MAP.end()) ? it->second : v;
    }

    // parseCType: parse one C type from tokens starting at i, advance i.
    // Returns resolved C type string. Handles namespace.Type and pointer/ref suffixes.
    std::string parseCType(size_t& i) {
        // skip whitespace
        i = nextNonWS(i);
        if (i >= tokens.size() || tokens[i].type == TK::END) return "";

        std::string result;

        // Handle namespace-qualified type: NS DOT IDENT
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
            // unexpected - just take whatever token is there
            result = tokens[i].value;
            i++;
        }

        // Consume pointer/ref suffixes
        while (true) {
            size_t j = nextNonWS(i);
            if (j < tokens.size() && tokens[j].type == TK::OTHER &&
                (tokens[j].value == "*" || tokens[j].value == "&")) {
                // emit whitespace between result and suffix
                result += tokens[j].value;
                i = j + 1;
            } else {
                break;
            }
        }

        return result;
    }

    // parseLambdaDecl: called when we're at IDENT("lambda") inside a param list.
    // Consumes: lambda NAME ( types... ) -> rettype
    // Emits to out: retCType (*NAME)(argCTypes...)
    // Side-effect: registers in lambdaRegistry, records PatchSite for callee param list close.
    // Returns the emitted string.
    // i is at the token AFTER "lambda"
    // calleeName: the function whose param list we're inside
    void parseLambdaDecl(size_t& i, const std::string& calleeName) {
        // skip whitespace
        i = nextNonWS(i);

        // parse NAME
        std::string lambdaParamName;
        if (i < tokens.size() && tokens[i].type == TK::IDENT) {
            lambdaParamName = tokens[i].value;
            i++;
        } else {
            std::cerr << "Error: expected lambda parameter name after 'lambda'\n";
            return;
        }

        i = nextNonWS(i);

        // expect '('
        if (i >= tokens.size() || tokens[i].type != TK::LPAREN) {
            std::cerr << "Error: expected '(' after lambda name '" << lambdaParamName << "'\n";
            return;
        }
        i++; // consume '('

        // parse arg types
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

        // expect '-' '>'
        std::string retCType = "void";
        if (i < tokens.size() && tokens[i].type == TK::OTHER && tokens[i].value == "-") {
            i++;
            i = nextNonWS(i);
            if (i < tokens.size() && tokens[i].type == TK::OTHER && tokens[i].value == ">") {
                i++;
                retCType = parseCType(i);
            }
        }

        // Register this lambda
        std::string regKey = calleeName + "::" + lambdaParamName;
        LambdaReg reg;
        reg.lambdaParamName = lambdaParamName;
        reg.calleeName      = calleeName;
        reg.argCTypes       = argCTypes;
        reg.retCType        = retCType;
        lambdaRegistry[regKey] = reg;
        // Also register by just lambdaParamName for quick lookup at call sites
        lambdaRegistry[lambdaParamName] = reg;

        // Emit the function pointer: retCType (*NAME)(argCTypes...)
        // First we need to record where the inner ')' is so we can patch captures later.
        // Emit: retCType (*NAME)(
        out << retCType << " (*" << lambdaParamName << ")(";
        // Record offset just before we emit arg types - we need offset of the closing ')'
        // We'll emit args, then record where ')' is.
        for (size_t k = 0; k < argCTypes.size(); k++) {
            if (k > 0) out << ", ";
            out << argCTypes[k];
        }
        // Record offset of the closing ')' of the inner arg list
        size_t fptrInnerClose = out.str().size();
        out << ")";

        // We'll record PatchSite after the caller emits the outer ')' of the callee param list.
        // Store temporarily - the caller (run()) will set outputByteOffset after seeing callee close paren.
        // We push a sentinel PatchSite with fptrInnerCloseOffset set now.
        PatchSite ps;
        ps.calleeName            = calleeName;
        ps.lambdaParamName       = lambdaParamName;
        ps.fptrInnerCloseOffset  = fptrInnerClose;
        ps.outputByteOffset      = SIZE_MAX; // will be filled in when outer ')' is seen
        patchSites.push_back(ps);
    }

    // generateLambdaName: FNV-1a hash of body token values → __lambda_XXXXXXXX
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

    // detectCaptures: find identifiers used in body that are in scopeVars but not in headerParams
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

            // Check scopeVars
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

    // extractBody: called when we're at LBRACE that is a trailing lambda body.
    // Parses optional (param, ...) -> header, then body tokens.
    // Returns {headerParamNames, bodyTokens}
    // i should be pointing at LBRACE; on return, i is after the closing RBRACE.
    struct ExtractedBody {
        std::vector<std::string> headerParams;
        std::vector<Token>       bodyTokens;
    };

    ExtractedBody extractBody(size_t& i) {
        ExtractedBody result;

        // consume LBRACE
        i++; // skip '{'

        // skip whitespace
        size_t j = nextNonWS(i);

        // Check for optional header: ( params... ) ->
        if (j < tokens.size() && tokens[j].type == TK::LPAREN) {
            // Try to parse header: ( name, name, ... ) ->
            // We need to look ahead to confirm there's a '->' after the ')'
            size_t save_j = j;
            j++; // skip '('
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
                        // confirmed header
                        result.headerParams = params;
                        i = j3 + 1;
                    } else {
                        // not a valid header, treat as body
                        i = save_j + 1; // rewind to after '{'  (j was at '(')
                        // actually rewind to the position after '{'
                        i = nextNonWS(i);
                        // We already incremented past '{' at start, i is at next token
                        // Reset: we need to re-set i to the position right after '{'
                        // Let's just not consume the header tokens
                        i = save_j; // point to '('
                    }
                } else {
                    i = save_j; // not a header, leave '(' to be part of body
                }
            } else {
                i = save_j;
            }
        }

        // Collect body tokens until matching RBRACE (depth 0)
        int depth = 1;
        while (i < tokens.size()) {
            const Token& t = tokens[i];
            if (t.type == TK::END) break;
            if (t.type == TK::LBRACE) { depth++; result.bodyTokens.push_back(t); i++; continue; }
            if (t.type == TK::RBRACE) {
                depth--;
                if (depth == 0) { i++; break; } // consume closing brace, don't include in body
                result.bodyTokens.push_back(t); i++; continue;
            }
            result.bodyTokens.push_back(t);
            i++;
        }

        return result;
    }

    // emitLambdaFunction: writes the lambda function to preamble
    void emitLambdaFunction(
        const std::string& name,
        const std::string& retCType,
        const std::vector<std::string>& argCTypes,
        const std::vector<std::string>& headerParams,
        const std::vector<Token>& bodyTokens,
        const std::vector<std::pair<std::string,std::string>>& captures)
    {
        preamble << retCType << " " << name << "(";

        // params: zip argCTypes with headerParams
        bool first = true;
        for (size_t k = 0; k < argCTypes.size(); k++) {
            if (!first) preamble << ", ";
            first = false;
            preamble << argCTypes[k];
            if (k < headerParams.size()) preamble << " " << headerParams[k];
        }

        // captures as extra params
        for (auto& [capName, capType] : captures) {
            if (!first) preamble << ", ";
            first = false;
            preamble << capType << " " << capName;
        }

        preamble << ") {\n";

        // Emit body tokens through transpile logic
        // We do a mini-transpile of the body tokens
        preamble << transpileTokensToString(bodyTokens);

        preamble << "}\n\n";
    }

    // transpileTokensToString: runs the same transpile logic on a token slice, returns string
    // This handles TYPE_MAP substitution, namespace dot-notation, etc.
    // It does NOT handle lambda declarations or trailing lambda call sites (those are top-level concerns)
    // but for lambda bodies that have nested calls, we'd need to be recursive -
    // for simplicity, we handle the same transforms as the main loop.
    std::string transpileTokensToString(const std::vector<Token>& toks) {
        std::ostringstream buf;
        int pd = 0; // local paren depth

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

            if (tok.type == TK::LPAREN)  { pd++; buf << tok.value; i++; continue; }
            if (tok.type == TK::RPAREN)  { pd--; buf << tok.value; i++; continue; }

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

                auto typeIt = TYPE_MAP.find(v);
                if (typeIt != TYPE_MAP.end()) {
                    buf << typeIt->second; i++; continue;
                }

                // namespace dot-notation
                if (namespaces.count(v)) {
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
                    }
                }

                buf << v; i++; continue;
            }

            buf << tok.value; i++;
        }

        return buf.str();
    }

    // applyPatches: apply callee signature patches to the output string
    std::string applyPatches(std::string outStr) {
        if (callSiteCaptures.empty()) return outStr;

        // Build a map from calleeName -> canonical capture set
        // All call sites for the same callee must have the same capture variable names
        std::unordered_map<std::string, std::vector<std::pair<std::string,std::string>>> calleeCaps;

        for (auto& csc : callSiteCaptures) {
            auto it = calleeCaps.find(csc.calleeName);
            if (it == calleeCaps.end()) {
                calleeCaps[csc.calleeName] = csc.captures;
            } else {
                // Validate consistency: same names
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

        // Collect patches: for each callee with captures, find its patch sites
        // Sort patch sites by offset descending so we can insert without invalidating earlier offsets
        struct Patch {
            size_t offset;
            std::string insertion;
        };
        std::vector<Patch> patches;

        for (auto& [calleeName, caps] : calleeCaps) {
            if (caps.empty()) continue;

            // Check if callee has patch sites
            bool found = false;
            for (auto& ps : patchSites) {
                if (ps.calleeName != calleeName) continue;
                if (ps.outputByteOffset == SIZE_MAX) continue;
                found = true;

                // Build insertion for outer param list (the callee's params)
                std::string outerInsert;
                for (auto& [capName, capType] : caps) {
                    // Check if already present (idempotency): look backwards from offset
                    // Simple check: search for capName in the callee param region
                    // For robustness, just always insert (the spec says check, but in practice
                    // we control generation so it won't already be there on first run)
                    outerInsert += ", " + capType + " " + capName;
                }

                // Patch outer closing ')'
                patches.push_back({ps.outputByteOffset, outerInsert});

                // Patch inner function pointer arg list closing ')'
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

        // Sort descending by offset
        std::sort(patches.begin(), patches.end(), [](const Patch& a, const Patch& b) {
            return a.offset > b.offset;
        });

        // Apply patches
        for (auto& p : patches) {
            if (p.offset <= outStr.size()) {
                outStr.insert(p.offset, p.insertion);
            }
        }

        return outStr;
    }

    // ── Scope tracking helpers ────────────────────────────────────────────

    void recordFunctionParams(size_t funcParamStart) {
        // funcParamStart: index right after '(' of function parameter list
        // Parse params to record them in scopeVars
        size_t i = funcParamStart;
        while (i < tokens.size()) {
            i = nextNonWS(i);
            if (i >= tokens.size() || tokens[i].type == TK::END) break;
            if (tokens[i].type == TK::RPAREN) break;
            if (tokens[i].type == TK::OTHER && tokens[i].value == ",") { i++; continue; }

            // skip "lambda" params - already handled
            if (tokens[i].type == TK::IDENT && tokens[i].value == "lambda") {
                // skip until next comma or )
                while (i < tokens.size() && tokens[i].type != TK::END) {
                    if (tokens[i].type == TK::RPAREN) break;
                    if (tokens[i].type == TK::OTHER && tokens[i].value == ",") break;
                    i++;
                }
                continue;
            }

            // Try to parse: type name (possibly namespace-qualified type)
            // Type tokens: IDENT (possibly NS DOT IDENT) followed by IDENT param name
            // We do a simple 2-token lookahead
            if (tokens[i].type != TK::IDENT) { i++; continue; }

            std::string typePart;
            size_t typeStart = i;

            // Check for namespace-qualified: IDENT DOT IDENT
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

            // Consume pointer/ref suffixes
            while (true) {
                size_t jj = nextNonWS(i);
                if (jj < tokens.size() && tokens[jj].type == TK::OTHER &&
                    (tokens[jj].value == "*" || tokens[jj].value == "&")) {
                    typePart += tokens[jj].value;
                    i = jj + 1;
                } else break;
            }

            // Next should be IDENT = param name, or comma/rparen (unnamed param)
            i = nextNonWS(i);
            if (i < tokens.size() && tokens[i].type == TK::IDENT) {
                scopeVars.push_back({tokens[i].value, typePart});
                i++;
            }
            // skip to next comma or end
            // actually just let the outer loop handle it
        }
    }

    // Try to detect variable declarations inside function body
    // Pattern: TYPE IDENT (= or ; or ,)
    // Called when we see a potential type token at statement position
    void tryRecordVarDecl(size_t i) {
        if (!inFunctionBody) return;
        if (i >= tokens.size() || tokens[i].type != TK::IDENT) return;

        const std::string& v = tokens[i].value;
        if (CONTROL_FLOW_KW.count(v)) return;
        if (v == "return" || v == "lambda") return;

        // Must be a type (in TYPE_MAP) or a known type-ish identifier
        if (!TYPE_MAP.count(v)) return; // only track known type keywords for simplicity

        size_t j = nextNonWS(i + 1);
        if (j >= tokens.size() || tokens[j].type != TK::IDENT) return;
        const std::string& paramName = tokens[j].value;
        if (CONTROL_FLOW_KW.count(paramName)) return;

        size_t k = nextNonWS(j + 1);
        if (k >= tokens.size()) return;
        auto& nextTok = tokens[k];
        if (nextTok.type == TK::OTHER && (nextTok.value == "=" || nextTok.value == ",")) {
            // record it
            scopeVars.push_back({paramName, resolveType(v)});
        } else if (nextTok.type == TK::SEMICOLON) {
            scopeVars.push_back({paramName, resolveType(v)});
        }
    }

public:
    Transpiler(const std::vector<Token>& toks, const std::unordered_set<std::string>& ns)
        : tokens(toks), namespaces(ns) {}

    std::string run() {
        // We need to track:
        // - callee name for lambda declarations
        // - function param list boundaries for param recording
        // - call sites for trailing lambda detection

        // State for detecting function definitions: IDENT LPAREN...RPAREN LBRACE
        // We track the current top-level function name
        std::string currentFuncName;
        // When we see IDENT at depth 0, record it as a candidate function name
        std::string lastIdentAtDepth0;

        // Track pending patch sites that need their outer close-paren offset filled in
        // (keyed by calleeName, we fill in patchSites.back() when we see the outer RPAREN)
        // We track: when we're inside a param list at parenDepth==1, the function name
        std::string paramListCallee; // set when we open '(' at parenDepth 0->1 for a function
        size_t paramListOpenIdx = 0; // token index of '(' that opened the param list

        size_t i = 0;
        while (i < tokens.size()) {
            const Token& tok = tokens[i];

            if (tok.type == TK::END) break;

            // ── LBRACE ──────────────────────────────────────────────────
            if (tok.type == TK::LBRACE) {
                // Check if this is a trailing lambda call site
                bool isTrailingLambda = false;

                if (!callStack.empty()) {
                    CallFrame& frame = callStack.back();
                    // parenDepth was decremented when we saw RPAREN, so now it equals frame.parenDepthAtOpen - 1
                    // The call's ')' was seen, now we see '{' - this is trailing lambda
                    // But we need to check: this LBRACE comes right after the call's closing RPAREN
                    // We track this via a flag set when RPAREN closes a call frame
                    // Actually let's handle it differently - see RPAREN section
                    // This path handles: IDENT LBRACE (no-arg lambda)
                }

                // No-paren trailing lambda: IDENT LBRACE
                // This was handled by setting a flag in the IDENT section below

                globalBraceDepth++;
                if (inFunctionBody) braceDepth++;

                // If at globalBraceDepth 1: entering a top-level function body
                if (globalBraceDepth == 1 && !paramListCallee.empty()) {
                    currentFuncName = paramListCallee;
                    inFunctionBody  = true;
                    braceDepth      = 1;
                    // Record function params into scopeVars
                    scopeVars.clear();
                    // Find the parameter list: from paramListOpenIdx+1 to the matching RPAREN
                    size_t pi = paramListOpenIdx + 1;
                    recordFunctionParams(pi);
                    paramListCallee.clear();
                }

                out << tok.value; i++; continue;
            }

            // ── RBRACE ──────────────────────────────────────────────────
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

            // ── LPAREN ──────────────────────────────────────────────────
            if (tok.type == TK::LPAREN) {
                parenDepth++;
                if (parenDepth == 1 && !lastIdentAtDepth0.empty() &&
                    !CONTROL_FLOW_KW.count(lastIdentAtDepth0)) {
                    // Opening paren of a function call or declaration
                    paramListCallee   = lastIdentAtDepth0;
                    paramListOpenIdx  = i;
                    // Push call frame for trailing lambda detection
                    CallFrame cf;
                    cf.calleeName      = lastIdentAtDepth0;
                    cf.parenDepthAtOpen = parenDepth; // = 1
                    callStack.push_back(cf);
                }
                lastIdentAtDepth0.clear();
                out << tok.value; i++; continue;
            }

            // ── RPAREN ──────────────────────────────────────────────────
            if (tok.type == TK::RPAREN) {
                parenDepth--;

                // Check if this closes a lambda declaration's outer param list
                // i.e., parenDepth goes from 1 to 0 with a pending patch site
                if (parenDepth == 0) {
                    // Check for pending patch sites (outputByteOffset == SIZE_MAX)
                    // These need the byte offset of the position BEFORE the closing ')'
                    // Actually we insert capture params BEFORE the ')', so offset is current out.str().size()
                    for (auto& ps : patchSites) {
                        if (ps.outputByteOffset == SIZE_MAX && ps.calleeName == paramListCallee) {
                            ps.outputByteOffset = out.str().size();
                        }
                    }
                    paramListCallee.clear();
                }

                // Check if this closes a call frame
                if (!callStack.empty()) {
                    CallFrame& frame = callStack.back();
                    if (parenDepth == frame.parenDepthAtOpen - 1) {
                        // This RPAREN closes the call's argument list
                        // Peek ahead for LBRACE (trailing lambda)
                        size_t peek = nextNonWS(i + 1);
                        if (peek < tokens.size() && tokens[peek].type == TK::LBRACE) {
                            // Trailing lambda call site!
                            std::string calleeName = frame.calleeName;
                            callStack.pop_back();

                            // Find the LambdaReg for this callee
                            LambdaReg* reg = nullptr;
                            for (auto& [key, lr] : lambdaRegistry) {
                                if (lr.calleeName == calleeName) { reg = &lr; break; }
                            }

                            if (!reg) {
                                // Not a lambda callee - just emit RPAREN normally
                                out << tok.value; i++;
                                continue;
                            }

                            // Extract the lambda body
                            size_t bodyStart = peek;
                            ExtractedBody eb = extractBody(bodyStart);
                            i = bodyStart; // advance past the body

                            // Generate lambda function name
                            std::string lambdaName = generateLambdaName(eb.bodyTokens);

                            // Detect captures
                            auto captures = detectCaptures(eb.headerParams, eb.bodyTokens);

                            // Emit the lambda function to preamble
                            emitLambdaFunction(lambdaName, reg->retCType, reg->argCTypes,
                                               eb.headerParams, eb.bodyTokens, captures);

                            // Record call site captures for patching
                            if (!captures.empty()) {
                                CallSiteCapture csc;
                                csc.calleeName = calleeName;
                                csc.sourceLine = tok.line;
                                csc.captures   = captures;
                                callSiteCaptures.push_back(csc);
                            }

                            // Emit: , lambdaName [, capName...])
                            out << ", " << lambdaName;
                            for (auto& [capName, capType] : captures) {
                                out << ", " << capName;
                            }
                            out << tok.value; // the closing ')'
                            continue;
                        } else {
                            callStack.pop_back();
                        }
                    }
                }

                out << tok.value; i++; continue;
            }

            // ── Passthrough tokens ───────────────────────────────────────
            if (tok.type == TK::OTHER        ||
                tok.type == TK::LINE_COMMENT ||
                tok.type == TK::BLOCK_COMMENT||
                tok.type == TK::NUMBER       ||
                tok.type == TK::STRING       ||
                tok.type == TK::CHAR_LIT     ||
                tok.type == TK::SEMICOLON)
            {
                if (parenDepth == 0 && globalBraceDepth == 0 &&
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

                // Track last ident at depth 0 for function name detection
                if (parenDepth == 0 && globalBraceDepth == 0) {
                    if (!CONTROL_FLOW_KW.count(v) && v != "lambda") {
                        lastIdentAtDepth0 = v;
                    } else {
                        lastIdentAtDepth0.clear();
                    }
                } else if (parenDepth == 0 && globalBraceDepth > 0) {
                    // Inside function body, track ident for no-paren lambda call detection
                    // but don't update lastIdentAtDepth0 which is for global scope
                }

                // ── lambda keyword ──────────────────────────────────────
                if (v == "lambda" && parenDepth > 0) {
                    // Determine callee name: the function whose param list we're currently in
                    // That's paramListCallee (set when we opened the '(' at depth 0->1)
                    // But we might be deeper. For top-level function decl, the callee is
                    // the last ident seen before the outer '('.
                    // We stored paramListCallee when parenDepth went 0->1.
                    std::string callee = paramListCallee;
                    if (callee.empty() && !callStack.empty()) {
                        callee = callStack[0].calleeName;
                    }
                    i++;
                    parseLambdaDecl(i, callee);
                    continue;
                }

                // ── unused keyword ──────────────────────────────────────
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

                // ── type mapping ────────────────────────────────────────
                auto typeIt = TYPE_MAP.find(v);
                if (typeIt != TYPE_MAP.end()) {
                    // Try to detect variable declaration: TYPE IDENT (= or ; or ,)
                    tryRecordVarDecl(i);
                    out << typeIt->second; i++; continue;
                }

                // ── namespace dot-notation ──────────────────────────────
                if (namespaces.count(v)) {
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
                    }
                }

                // ── no-paren trailing lambda: IDENT LBRACE ──────────────
                // Check if next non-WS is LBRACE and this ident is a registered no-arg lambda callee
                {
                    size_t peek = nextNonWS(i + 1);
                    if (peek < tokens.size() && tokens[peek].type == TK::LBRACE &&
                        !CONTROL_FLOW_KW.count(v)) {
                        // Check if this function takes a no-arg lambda
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

        // Apply patches
        std::string outStr = applyPatches(out.str());

        // Return preamble + patched output
        return preamble.str() + outStr;
    }
};

std::string transpile(const std::vector<Token>& tokens, const std::unordered_set<std::string>& namespaces) {
    return Transpiler(tokens, namespaces).run();
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

    for (int a = 1; a < argc; a++) {
        std::string arg = argv[a];
        if (arg.size() >= 2 && arg[0] == '-' && arg[1] == 'I') {
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
            std::cout << entry.path().string() << "  ->  " << outPath.string() << "\n";
        } else {
            fs::copy_file(entry.path(), outPath, fs::copy_options::overwrite_existing);
            std::cout << entry.path().string() << "  ->  " << outPath.string() << " (copied)\n";
        }
    }

    return 0;
}
