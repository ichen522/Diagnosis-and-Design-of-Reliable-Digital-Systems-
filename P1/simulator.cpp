// Logic simulation (LOGICSIM command).
//
// Simulation order comes from levelization: a line at level k depends only
// on lines at levels < k, so evaluating lines in increasing level order sees
// every fan-in already final. The order is built once with a counting sort on
// level (O(nodes)); each pattern is then a single pass, O(nodes + edges).
//
// Gate evaluation uses controlling values, which handles any number of
// inputs and the unknown value X in one rule:
//   AND : 0 if any input is 0, else X if any input is X, else 1
//   OR  : 1 if any input is 1, else X if any input is X, else 0
//   XOR : X if any input is X, else the parity of the inputs
//   NAND / NOR / XNOR invert the result; NOT inverts; BUF and branches copy.

#include "simulator.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unordered_map>

namespace {

Logic invert(Logic v) {
    return v == LX ? LX : (v == L0 ? L1 : L0);
}

// Value of an AND (control = L0) or OR (control = L1) over the inputs.
Logic controlled(const std::vector<int>& fanin, const std::vector<Logic>& value, Logic control) {
    bool sawX = false;
    for (int u : fanin) {
        if (value[u] == control) return control;
        if (value[u] == LX) sawX = true;
    }
    return sawX ? LX : invert(control);
}

Logic parity(const std::vector<int>& fanin, const std::vector<Logic>& value) {
    unsigned ones = 0;
    for (int u : fanin) {
        if (value[u] == LX) return LX;
        ones += (value[u] == L1);
    }
    return (ones & 1u) ? L1 : L0;
}

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool parseInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (*end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

}  // namespace

char logicChar(Logic v) {
    return v == L0 ? '0' : (v == L1 ? '1' : 'X');
}

Logic evalGate(GateType type, const std::vector<int>& fanin, const std::vector<Logic>& value) {
    if (fanin.empty()) return LX;  // undriven line
    switch (type) {
        case BRCH:
        case BUF:  return value[fanin[0]];
        case NOT:  return invert(value[fanin[0]]);
        case AND:  return controlled(fanin, value, L0);
        case NAND: return invert(controlled(fanin, value, L0));
        case OR:   return controlled(fanin, value, L1);
        case NOR:  return invert(controlled(fanin, value, L1));
        case XOR:  return parity(fanin, value);
        case XNOR: return invert(parity(fanin, value));
        case IPT:  break;
    }
    return LX;
}

LogicSim::LogicSim(const Circuit& c) : c_(c), value_(c.nodes.size(), LX) {
    // Counting sort of the non-PI lines by level. Within a level the file
    // order is kept, which makes runs reproducible.
    int maxLevel = 0;
    for (const Node& n : c.nodes) maxLevel = std::max(maxLevel, n.level);
    std::vector<int> start(maxLevel + 2, 0);
    for (const Node& n : c.nodes)
        if (n.ntype != PI && n.level >= 0) ++start[n.level + 1];
    for (int k = 1; k <= maxLevel + 1; ++k) start[k] += start[k - 1];

    order_.resize(start[maxLevel + 1]);
    for (std::size_t i = 0; i < c.nodes.size(); ++i) {
        const Node& n = c.nodes[i];
        if (n.ntype != PI && n.level >= 0) order_[start[n.level]++] = static_cast<int>(i);
    }
}

void LogicSim::run(const std::vector<Logic>& pi) {
    for (std::size_t k = 0; k < c_.pis.size(); ++k) value_[c_.pis[k]] = pi[k];
    for (int i : order_) {
        const Node& n = c_.nodes[i];
        value_[i] = evalGate(n.gtype, n.fanin, value_);
    }
}

bool readPattern(const Circuit& c, const std::string& path, std::vector<Logic>& pi,
                 std::string& err, std::string& warning) {
    std::ifstream in(path);
    if (!in) {
        err = "Cannot open input file " + path;
        return false;
    }

    std::unordered_map<int, int> nodeIndex;  // node ID -> index in c.nodes
    for (std::size_t i = 0; i < c.nodes.size(); ++i) nodeIndex[c.nodes[i].id] = static_cast<int>(i);
    std::unordered_map<int, int> piSlot;     // node index -> position in c.pis
    for (std::size_t k = 0; k < c.pis.size(); ++k) piSlot[c.pis[k]] = static_cast<int>(k);

    std::vector<Logic> values(c.pis.size(), LX);
    std::vector<bool> assigned(c.pis.size(), false);
    std::string text;
    int lineNo = 0;
    while (std::getline(in, text)) {
        ++lineNo;
        const std::string line = trim(text);
        if (line.empty()) continue;
        const std::string where = path + " line " + std::to_string(lineNo) + ": ";

        std::size_t comma = line.find(',');
        int id;
        if (comma == std::string::npos || !parseInt(trim(line.substr(0, comma)), id)) {
            err = where + "expected PI-ID,VALUE but got '" + line + "'";
            return false;
        }
        const std::string v = trim(line.substr(comma + 1));
        Logic val;
        if (v == "0") val = L0;
        else if (v == "1") val = L1;
        else if (v == "x" || v == "X") val = LX;
        else {
            err = where + "value must be 0, 1 or X, got '" + v + "'";
            return false;
        }

        auto node = nodeIndex.find(id);
        if (node == nodeIndex.end()) {
            err = where + "node " + std::to_string(id) + " is not in the circuit";
            return false;
        }
        auto slot = piSlot.find(node->second);
        if (slot == piSlot.end()) {
            err = where + "node " + std::to_string(id) + " is not a primary input";
            return false;
        }
        if (assigned[slot->second]) {
            err = where + "primary input " + std::to_string(id) + " is assigned twice";
            return false;
        }
        assigned[slot->second] = true;
        values[slot->second] = val;
    }

    int missing = 0;
    for (bool a : assigned) missing += !a;
    warning.clear();
    if (missing > 0)
        warning = std::to_string(missing) + " primary input(s) not in " + path + ", set to X";
    pi.swap(values);
    return true;
}

bool writeOutputs(const Circuit& c, const LogicSim& sim, const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) return false;
    for (int i : c.pos) std::fprintf(fp, "%d,%c\n", c.nodes[i].id, logicChar(sim.value(i)));
    std::fclose(fp);
    return true;
}
