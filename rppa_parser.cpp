// rppa_parser_resolved.cpp - RPPA DSL parser with module call support
#include "engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// === ParseError ===
struct ParseError : runtime_error {
    int line, col;
    ParseError(const string& msg, int l=0, int c=0)
      : runtime_error("Parse error at " + to_string(l) + ":" + to_string(c) + " - " + msg), line(l), col(c) {}
};

// === Tokenizer ===
struct Token {
    enum Type { ID, DOT, PARALLEL, CHOICE, PRIO, LBRACK, RBRACK, LPAREN, RPAREN, END } type;
    string value;
    int line, col;
    Token(Type t=END, const string& v="", int l=0, int c=0)
      : type(t), value(v), line(l), col(c) {}
};

class Tokenizer {
    istream& in;
    char ch;
    int line = 1, col = 0;
public:
    Tokenizer(istream& input) : in(input) { next(); }
    void next() { ch = in.get(); ++col; if (ch == '\n') { ++line; col = 0; } }
    Token next_token() {
        while (isspace(ch)) next();
        if (in.eof()) return Token(Token::END, "", line, col);
        if (isalpha(ch)) {
            string id;
            while (isalnum(ch) || ch == '_') { id += ch; next(); }
            return Token(Token::ID, id, line, col);
        }
        int l = line, c = col;
        switch (ch) {
            case '.': next(); return Token(Token::DOT, ".", l, c);
            case '|': next(); if (ch == '|') { next(); return Token(Token::PARALLEL, "||", l, c); } throw ParseError("Expected '||'", l, c);
            case '[': next(); return Token(Token::LBRACK, "[", l, c);
            case ']': next(); return Token(Token::RBRACK, "]", l, c);
            case '^': next(); {
                string num;
                while (isdigit(ch)) { num += ch; next(); }
                return Token(Token::PRIO, num, l, c);
            }
            case '(': next(); return Token(Token::LPAREN, "(", l, c);
            case ')': next(); return Token(Token::RPAREN, ")", l, c);
        }
        throw ParseError(string("Unknown character: ") + ch, l, c);
    }
};

// === Parser ===
class Parser {
    Tokenizer tz;
    Token cur;
    void advance() { cur = tz.next_token(); }
    void expect(Token::Type t) {
        if (cur.type != t) throw ParseError("Unexpected token", cur.line, cur.col);
    }

    unique_ptr<AstNode> parse_term() {
        if (cur.type == Token::ID) {
            string name = cur.value;
            advance();
            // Module call syntax: name()
            if (cur.type == Token::LPAREN) {
                advance();
                expect(Token::RPAREN);
                advance();
                auto node = make_unique<CallModuleNode>(name);
                // optional priority after call
                if (cur.type == Token::PRIO) {
                    int p = stoi(cur.value);
                    advance();
                    return make_unique<PrioNode>(p, move(node));
                }
                return node;
            }
            // Action with optional priority
            if (cur.type == Token::PRIO) {
                int p = stoi(cur.value);
                advance();
                return make_unique<PrioNode>(p, make_unique<ActionNode>(name));
            }
            return make_unique<ActionNode>(name);
        }
        if (cur.type == Token::LBRACK) {
            advance();
            auto node = parse_expr();
            expect(Token::RBRACK);
            advance();
            if (cur.type == Token::PRIO) {
                int p = stoi(cur.value);
                advance();
                return make_unique<PrioNode>(p, move(node));
            }
            return node;
        }
        throw ParseError("Expected term", cur.line, cur.col);
    }

    unique_ptr<AstNode> parse_expr() {
        auto left = parse_term();
        while (cur.type == Token::DOT || cur.type == Token::PARALLEL || cur.type == Token::CHOICE) {
            auto op = cur.type;
            advance();
            auto right = parse_term();
            if (op == Token::DOT) {
                auto seq = make_unique<SequenceNode>();
                seq->children.clear();
                seq->children.push_back(move(left));
                seq->children.push_back(move(right));
                left = move(seq);
            } else if (op == Token::PARALLEL) {
                left = make_unique<ParallelNode>(move(left), move(right));
            } else {
                left = make_unique<ChoiceNode>(move(left), move(right));
            }
        }
        return left;
    }

public:
    Parser(istream& in) : tz(in) { advance(); }
    unique_ptr<AstNode> parse() {
        auto ast = parse_expr();
        if (cur.type != Token::END) throw ParseError("Extra input", cur.line, cur.col);
        return ast;
    }
};

// === Manifest Loader ===
json load_manifest(const string& path) {
    ifstream f(path);
    if (!f) throw runtime_error("Cannot open manifest file: " + path);
    json j; f >> j; return j;
}

// === Lockfile Registrar ===
void register_from_lock(const string& lockfile, CodegenVisitor& cg) {
    ifstream f(lockfile);
    if (!f) throw runtime_error("Cannot open lockfile: " + lockfile);
    json j; f >> j;
    for (auto& m : j["modules"]) {
        string file = m["file"];
        ifstream r(file);
        if (!r) throw runtime_error("Cannot open module file: " + file);
        Parser p(r);
        auto ast = p.parse();
        cg.register_module(filesystem::path(file).stem().string(), move(ast));
    }
}

// === CLI ===
int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: parser --compile <file.rppa> <module> | --manifest <json> | --lock <lockfile> | --test\n";
        return 1;
    }
    string cmd = argv[1];
    try {
        if (cmd == "--manifest") {
            auto m = load_manifest(argv[2]);
            cout << m.dump(2) << endl;

        } else if (cmd == "--compile") {
            if (argc < 4) throw runtime_error("--compile <file> <module>");
            ifstream f(argv[2]); if (!f) throw runtime_error("Cannot open input: " + string(argv[2]));
            Parser p(f);
            auto ast = p.parse();
            CodegenVisitor cg;
            cg.register_module(argv[3], move(ast));
            auto mod = cg.generate(argv[3]);
            ofstream jout(string(argv[3]) + ".json"); jout << mod.to_json().dump(2);
            ofstream pout(string(argv[3]) + ".pnml"); pout << mod.to_pnml();
            cout << "Emitted " << argv[3] << ".json and .pnml" << endl;

        } else if (cmd == "--lock") {
            if (argc < 3) throw runtime_error("--lock <lockfile>");
            CodegenVisitor cg;
            register_from_lock(argv[2], cg);
            cout << "Registered modules from lockfile" << endl;

        } else if (cmd == "--test") {
            vector<string> val = {"a.b.c", "a||b", "[a||b]", "x^5", "modA()"};
            vector<string> inv = {"a|b", ".a", "[a.b", "a^^2", "modA"};
            for (auto& s : val) {
                try {
                    istringstream ss{s};
                    Parser p{ss};
                    p.parse();
                    cout << "OK: " << s << endl;
                } catch (const exception& e) {
                    cout << "FAIL(valid): " << s << " (" << e.what() << ")" << endl;
                }
            }
            for (auto& s : inv) {
                try {
                    istringstream ss{s};
                    Parser p{ss};
                    p.parse();
                    cout << "FAIL(invalid): " << s << endl;
                } catch (...) {
                    cout << "OK(invalid): " << s << endl;
                }
            }

        } else {
            throw runtime_error("Unknown command");
        }
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}
