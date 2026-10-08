// .ckt netlist parser (READ) and circuit printer (PC).
//
// .ckt format, one line per net:
//
//   col1      col2   col3       col4        col5       col6 ...
//   0 GATE    id     gate type  #fanout     #fanin     fan-in IDs
//   1 PI      id     0          #fanout     0
//   2 FB      id     1 (BRCH)   stem ID
//   3 PO      id     gate type  0           #fanin     fan-in IDs
//
// Gate types: 0 IPT, 1 BRCH, 2 XOR, 3 OR, 4 NOR, 5 NOT, 6 NAND, 7 AND, 8 XNOR, 9 BUF
//
// Only fan-ins are listed in the file. Fan-outs are rebuilt from those
// fan-in references; column 4 is ignored because it is 0 for a PO and
// implied 1 for a branch, which is wrong whenever a PO or a branch drives
// more lines.

#include "circuit.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {

struct Token {
    long value;
    int line;
};

// Reads every whitespace-separated integer in the file, remembering the
// line it came from. Treats the file as a token stream, so CRLF endings,
// blank lines and records wrapped over several lines are all accepted.
bool tokenize(std::istream& in, std::vector<Token>& toks, std::string& err) {
    std::string text;
    int lineNo = 0;
    while (std::getline(in, text)) {
        ++lineNo;
        std::istringstream ss(text);
        std::string word;
        while (ss >> word) {
            char* end = nullptr;
            long v = std::strtol(word.c_str(), &end, 10);
            if (*end != '\0') {
                err = "line " + std::to_string(lineNo) + ": '" + word + "' is not an integer";
                return false;
            }
            toks.push_back({v, lineNo});
        }
    }
    return true;
}

// Raw record of one .ckt line; fan-ins are still node IDs here.
struct Record {
    int id;
    int ntype;
    int gtype;
    std::vector<int> faninIds;
    int line;
};

std::string baseName(const std::string& path) {
    std::string name = path;
    std::size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const std::string ext = ".ckt";
    if (name.size() > ext.size() && name.compare(name.size() - ext.size(), ext.size(), ext) == 0)
        name.erase(name.size() - ext.size());
    return name;
}

std::string joinIds(const std::vector<Node>& nodes, const std::vector<int>& idx) {
    std::string s;
    for (int i : idx) s += std::to_string(nodes[i].id) + " ";
    return s;
}

}  // namespace

std::string gateName(GateType t) {
    static const char* names[] = {"PI", "BRANCH", "XOR", "OR", "NOR",
                                  "NOT", "NAND", "AND", "XNOR", "BUF"};
    return (t >= IPT && t <= BUF) ? names[t] : "?";
}

bool Circuit::read(const std::string& path, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "File does not exist!";
        return false;
    }

    std::vector<Token> toks;
    if (!tokenize(in, toks, err)) return false;

    // Pass 1: split the token stream into records.
    std::vector<Record> recs;
    std::size_t p = 0;
    auto need = [&](std::size_t n, int line) {
        if (p + n <= toks.size()) return true;
        err = "line " + std::to_string(line) + ": record ends early";
        return false;
    };
    while (p < toks.size()) {
        Record r;
        r.line = toks[p].line;
        if (!need(2, r.line)) return false;
        r.ntype = static_cast<int>(toks[p++].value);
        r.id = static_cast<int>(toks[p++].value);

        if (r.ntype == FB) {
            if (!need(2, r.line)) return false;
            r.gtype = static_cast<int>(toks[p++].value);
            r.faninIds.push_back(static_cast<int>(toks[p++].value));
        } else if (r.ntype == GATE || r.ntype == PI || r.ntype == PO) {
            if (!need(3, r.line)) return false;
            r.gtype = static_cast<int>(toks[p++].value);
            ++p;  // column 4 (#fanout) -- rebuilt below instead
            long nfin = toks[p++].value;
            if (nfin < 0 || !need(static_cast<std::size_t>(nfin), r.line)) {
                if (nfin < 0) err = "line " + std::to_string(r.line) + ": negative fan-in count";
                return false;
            }
            for (long k = 0; k < nfin; ++k) r.faninIds.push_back(static_cast<int>(toks[p++].value));
        } else {
            err = "line " + std::to_string(r.line) + ": unknown node type " + std::to_string(r.ntype);
            return false;
        }
        if (r.gtype < IPT || r.gtype > BUF) {
            err = "line " + std::to_string(r.line) + ": unknown gate type " + std::to_string(r.gtype);
            return false;
        }
        recs.push_back(r);
    }

    // Pass 2: map node IDs to indices. Done before resolving fan-ins, so a
    // line may refer to a node defined later in the file.
    std::unordered_map<int, int> indexOf;
    for (std::size_t i = 0; i < recs.size(); ++i) {
        if (!indexOf.emplace(recs[i].id, static_cast<int>(i)).second) {
            err = "line " + std::to_string(recs[i].line) + ": node " +
                  std::to_string(recs[i].id) + " defined twice";
            return false;
        }
    }

    // Pass 3: build nodes, resolve fan-ins, derive fan-outs.
    std::vector<Node> newNodes(recs.size());
    std::vector<int> newPis, newPos;
    for (std::size_t i = 0; i < recs.size(); ++i) {
        const Record& r = recs[i];
        Node& n = newNodes[i];
        n.id = r.id;
        n.ntype = static_cast<NodeType>(r.ntype);
        n.gtype = static_cast<GateType>(r.gtype);
        n.level = -1;
        for (int fid : r.faninIds) {
            auto it = indexOf.find(fid);
            if (it == indexOf.end()) {
                err = "line " + std::to_string(r.line) + ": node " + std::to_string(r.id) +
                      " uses undefined node " + std::to_string(fid);
                return false;
            }
            n.fanin.push_back(it->second);
        }
        if (n.ntype == PI) newPis.push_back(static_cast<int>(i));
        if (n.ntype == PO) newPos.push_back(static_cast<int>(i));
    }
    for (std::size_t i = 0; i < newNodes.size(); ++i)
        for (int u : newNodes[i].fanin) newNodes[u].fanout.push_back(static_cast<int>(i));

    // Commit only after the whole file parsed cleanly.
    nodes.swap(newNodes);
    pis.swap(newPis);
    pos.swap(newPos);
    name = baseName(path);
    return true;
}

void Circuit::print() const {
    std::printf(" Node  Type    In                        Out\n");
    std::printf("------ ------  ------------------------  ------------------------\n");
    for (const Node& n : nodes)
        std::printf("%6d  %-6s  %-24s  %s\n", n.id, gateName(n.gtype).c_str(),
                    joinIds(nodes, n.fanin).c_str(), joinIds(nodes, n.fanout).c_str());

    std::printf("Primary inputs:  %s\n", joinIds(nodes, pis).c_str());
    std::printf("Primary outputs: %s\n\n", joinIds(nodes, pos).c_str());
    std::printf("Number of nodes = %d\n", static_cast<int>(nodes.size()));
    std::printf("Number of primary inputs = %d\n", static_cast<int>(pis.size()));
    std::printf("Number of primary outputs = %d\n", static_cast<int>(pos.size()));
}

int Circuit::numGates() const {
    int count = 0;
    for (const Node& n : nodes)
        if (n.gtype != IPT && n.gtype != BRCH) ++count;
    return count;
}
