#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// === AST Definitions ===
struct AstNode {
    virtual json to_json() const = 0;
    virtual ~AstNode() = default;
};

struct ActionNode : AstNode {
    string name;
    ActionNode(const string& name) : name(name) {}
    json to_json() const override {
        return json{{"type", "action"}, {"name", name}};
    }
};

struct SequenceNode : AstNode {
    vector<unique_ptr<AstNode>> children;
    json to_json() const override {
        json arr = json::array();
        for (const auto& child : children)
            arr.push_back(child->to_json());
        return json{{"type", "sequence"}, {"children", arr}};
    }
};

struct ParallelNode : AstNode {
    unique_ptr<AstNode> left, right;
    ParallelNode(unique_ptr<AstNode> l, unique_ptr<AstNode> r) : left(move(l)), right(move(r)) {}
    json to_json() const override {
        return json{{"type", "parallel"}, {"left", left->to_json()}, {"right", right->to_json()}};
    }
};

struct ChoiceNode : AstNode {
    unique_ptr<AstNode> left, right;
    ChoiceNode(unique_ptr<AstNode> l, unique_ptr<AstNode> r) : left(move(l)), right(move(r)) {}
    json to_json() const override {
        return json{{"type", "choice"}, {"left", left->to_json()}, {"right", right->to_json()}};
    }
};

struct PrioNode : AstNode {
    int priority;
    unique_ptr<AstNode> child;
    PrioNode(int p, unique_ptr<AstNode> c) : priority(p), child(move(c)) {}
    json to_json() const override {
        return json{{"type", "prio"}, {"priority", priority}, {"child", child->to_json()}};
    }
};

struct CallModuleNode : AstNode {
    string moduleName;
    CallModuleNode(const string& name) : moduleName(name) {}
    json to_json() const override {
        return json{{"type", "call"}, {"module", moduleName}};
    }
};

// === Petri Net Definitions ===
struct Place {
    string id;
};

struct Transition {
    string id;
    int priority = 0;
};

struct Arc {
    enum Type { PLACE_TO_TRANS, TRANS_TO_PLACE } type;
    string src, dst;
};

struct PetriModule {
    string moduleName;
    Place entry, exit;
    vector<Place> places;
    vector<Transition> transitions;
    vector<Arc> arcs;

    json to_json() const {
        json j;
        j["moduleName"] = moduleName;
        j["entry"] = entry.id;
        j["exit"] = exit.id;

        // places
        for (const auto& p : places) {
            j["places"].push_back({
                {"id", p.id}
                });
        }

        // transitions, including priority
        for (const auto& t : transitions) {
            j["transitions"].push_back({
                {"id",       t.id},
                {"priority", t.priority}
                });
        }

        // arcs, including type
        for (const auto& a : arcs) {
            j["arcs"].push_back({
                {"id",   a.src + "_to_" + a.dst},
                {"type", a.type == Arc::PLACE_TO_TRANS ? "P2T" : "T2P"},
                {"src",  a.src},
                {"dst",  a.dst}
                });
        }

        return j;
    }

    string to_pnml() const {
        ostringstream os;
        os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        os << "<pnml>\n";
        os << "  <net id=\"" << moduleName
            << "\" type=\"http://www.pnml.org/version-2009/grammar/pnml\">\n";

        // places
        for (const auto& p : places) {
            os << "    <place id=\"" << p.id << "\">\n";
            os << "      <name><text>" << p.id << "</text></name>\n";
            os << "      <initialMarking><text>0</text></initialMarking>\n";
            os << "    </place>\n";
        }

        // transitions (with priority as a tool‐specific extension)
        for (const auto& t : transitions) {
            os << "    <transition id=\"" << t.id << "\">\n";
            os << "      <name><text>" << t.id << "</text></name>\n";
            // only emit if priority > 0
            if (t.priority > 0) {
                os << "      <toolspecific tool=\"FlowNetDSL\">\n";
                os << "        <priority>" << t.priority << "</priority>\n";
                os << "      </toolspecific>\n";
            }
            os << "    </transition>\n";
        }

        // arcs, carrying our P2T/T2P label as a tool‐specific subelement
        int arcId = 0;
        for (const auto& a : arcs) {
            os << "    <arc id=\"a" << arcId++
                << "\" source=\"" << a.src
                << "\" target=\"" << a.dst << "\">\n";
            os << "      <toolspecific tool=\"FlowNetDSL\">\n";
            os << "        <type>"
                << (a.type == Arc::PLACE_TO_TRANS ? "P2T" : "T2P")
                << "</type>\n";
            os << "      </toolspecific>\n";
            os << "    </arc>\n";
        }

        os << "  </net>\n";
        os << "</pnml>\n";
        return os.str();
    }

};

// === Compiler ===
struct FlowDef {
    unique_ptr<AstNode> root;
};

class CodegenVisitor {
    map<string, FlowDef> modules;
    map<string, PetriModule> generated;
    int counter = 0;

    string fresh(const string& base = "id") {
        return base + to_string(counter++);
    }

public:
    void register_module(const string& name, unique_ptr<AstNode> root) {
        modules[name] = FlowDef{move(root)};
    }

    PetriModule generate(const string& moduleName) {
        if (generated.count(moduleName)) return generated[moduleName];

        PetriModule m;
        m.moduleName = moduleName;
        m.entry.id = fresh("entry");
        m.exit.id = fresh("exit");
        m.places.push_back(m.entry);
        m.places.push_back(m.exit);

        compileExpr(modules[moduleName].root.get(), m.entry.id, m.exit.id, m);
        generated[moduleName] = m;
        return m;
    }

    void compileExpr(AstNode* node, const string& in, const string& out, PetriModule& m) {
        if (dynamic_cast<ActionNode*>(node)){
            string tid = fresh("T");
            m.transitions.push_back({tid});
            m.arcs.push_back({Arc::PLACE_TO_TRANS, in, tid});
            m.arcs.push_back({Arc::TRANS_TO_PLACE, tid, out});
        } else if (auto seq = dynamic_cast<SequenceNode*>(node)) {
            string current = in;
            for (size_t i = 0; i < seq->children.size(); ++i) {
                string next = (i == seq->children.size() - 1) ? out : fresh("P");
                if (i != seq->children.size() - 1)
                    m.places.push_back({next});
                compileExpr(seq->children[i].get(), current, next, m);
                current = next;
            }
        } else if (auto par = dynamic_cast<ParallelNode*>(node)) {
            string leftIn = fresh("P"), rightIn = fresh("P");
            string leftOut = fresh("P"), rightOut = fresh("P");
            m.places.insert(m.places.end(), {{leftIn}, {rightIn}, {leftOut}, {rightOut}});
            string fork = fresh("T"), join = fresh("T");
            m.transitions.insert(m.transitions.end(), {{fork}, {join}});
            m.arcs.insert(m.arcs.end(), {
                {Arc::PLACE_TO_TRANS, in, fork},
                {Arc::TRANS_TO_PLACE, fork, leftIn},
                {Arc::TRANS_TO_PLACE, fork, rightIn}
            });
            compileExpr(par->left.get(), leftIn, leftOut, m);
            compileExpr(par->right.get(), rightIn, rightOut, m);
            m.arcs.insert(m.arcs.end(), {
                {Arc::PLACE_TO_TRANS, leftOut, join},
                {Arc::PLACE_TO_TRANS, rightOut, join},
                {Arc::TRANS_TO_PLACE, join, out}
            });
        } else if (auto choice = dynamic_cast<ChoiceNode*>(node)) {
            compileExpr(choice->left.get(), in, out, m);
            compileExpr(choice->right.get(), in, out, m);
        } else if (auto prio = dynamic_cast<PrioNode*>(node)) {
            string mid = fresh("P"), tid = fresh("T");
            m.places.push_back({mid});
            m.transitions.push_back({tid, prio->priority});
            m.arcs.push_back({Arc::PLACE_TO_TRANS, in, tid});
            m.arcs.push_back({Arc::TRANS_TO_PLACE, tid, mid});
            compileExpr(prio->child.get(), mid, out, m);
        } else if (auto call = dynamic_cast<CallModuleNode*>(node)) {
            auto sub = generate(call->moduleName);
            m.places.insert(m.places.end(), sub.places.begin(), sub.places.end());
            m.transitions.insert(m.transitions.end(), sub.transitions.begin(), sub.transitions.end());
            m.arcs.insert(m.arcs.end(), sub.arcs.begin(), sub.arcs.end());
            string callT = fresh("T_call"), retT = fresh("T_ret");
            m.transitions.push_back({callT});
            m.transitions.push_back({retT});
            m.arcs.insert(m.arcs.end(), {
                {Arc::PLACE_TO_TRANS, in, callT},
                {Arc::TRANS_TO_PLACE, callT, sub.entry.id},
                {Arc::PLACE_TO_TRANS, sub.exit.id, retT},
                {Arc::TRANS_TO_PLACE, retT, out}
            });
        }
    }
};

