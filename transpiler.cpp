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

std::string transpile(const std::vector<Token>& tokens, const std::unordered_set<std::string>& namespaces) {
    std::ostringstream out;

    int parenDepth = 0;
    //int braceDepth = 0;

    auto nextNonWS = [&](size_t i) -> size_t {
        while (i < tokens.size() && tokens[i].type == TK::OTHER &&
               !tokens[i].value.empty() && std::isspace((unsigned char)tokens[i].value[0]))
            i++;
        return i;
    };

    size_t i = 0;
    while (i < tokens.size()) {
        const Token& tok = tokens[i];

        if (tok.type == TK::END) break;

        //if (tok.type == TK::LBRACE)  { braceDepth++; out << tok.value; i++; continue; }
        //if (tok.type == TK::RBRACE)  { braceDepth--; out << tok.value; i++; continue; }
        if (tok.type == TK::LPAREN)  { parenDepth++; out << tok.value; i++; continue; }
        if (tok.type == TK::RPAREN)  { parenDepth--; out << tok.value; i++; continue; }

        if (tok.type == TK::OTHER        ||
            tok.type == TK::LINE_COMMENT ||
            tok.type == TK::BLOCK_COMMENT||
            tok.type == TK::NUMBER       ||
            tok.type == TK::STRING       ||
            tok.type == TK::CHAR_LIT     ||
            tok.type == TK::SEMICOLON)
        {
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
            out << val; i++; continue;
        }

        if (tok.type == TK::SCOPE) { out << "::"; i++; continue; }

        if (tok.type == TK::DOT) {
            out << tok.value; i++; continue;
        }

        if (tok.type == TK::IDENT) {
            const std::string& v = tok.value;

            auto typeIt = TYPE_MAP.find(v);
            if (typeIt != TYPE_MAP.end()) {
                out << typeIt->second; i++; continue;
            }

            if (v == "unused") {
                if (parenDepth > 0) {
                    i++;

                    std::ostringstream paramBuf;
                    std::string paramName;
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
                            paramName = pv;
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
                                varName   = st.value;
                                foundVar  = true;
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
                        continue;
                    }
                }
            }

            out << v; i++; continue;
        }

        out << tok.value; i++;
    }

    return out.str();
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
