#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <string>
#include <vector>

// Column 1 of a .ckt line: what kind of line (net) this is.
enum NodeType { GATE = 0, PI = 1, FB = 2, PO = 3 };

// Column 3 of a .ckt line: the gate that drives this line.
enum GateType { IPT = 0, BRCH, XOR, OR, NOR, NOT, NAND, AND, XNOR, BUF };

// One line (net) of the circuit. A gate is represented by its output line.
struct Node {
    int id;                    // node ID as written in the .ckt file
    NodeType ntype;
    GateType gtype;
    std::vector<int> fanin;    // indices into Circuit::nodes (towards the PIs)
    std::vector<int> fanout;   // indices into Circuit::nodes (towards the POs)
    int level;                 // set by levelize(); -1 = not levelized
};

class Circuit {
public:
    std::string name;          // file name without directory and ".ckt"
    std::vector<Node> nodes;   // in file order
    std::vector<int> pis;      // indices of primary inputs, in file order
    std::vector<int> pos;      // indices of primary outputs, in file order

    // Parses a .ckt file. On failure returns false, fills err and leaves
    // the circuit unchanged.
    bool read(const std::string& path, std::string& err);

    // Prints every node with its type, fan-ins and fan-outs (PC command).
    void print() const;

    // Lines driven by a logic gate, i.e. everything except PIs and branches.
    int numGates() const;
};

std::string gateName(GateType t);

#endif
