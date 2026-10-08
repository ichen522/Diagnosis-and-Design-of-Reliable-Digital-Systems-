#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <string>
#include <vector>

#include "circuit.h"

// Three-valued logic. P1 patterns are fully specified (0/1); X marks a
// primary input the pattern leaves out, and is the starting value for the
// fault simulation and ATPG projects that follow.
enum Logic : unsigned char { L0 = 0, L1 = 1, LX = 2 };

char logicChar(Logic v);

// Output of one gate, given the values of all lines. Gates take any number
// of inputs; a one-input AND/OR acts as a buffer, a one-input NAND/NOR as an
// inverter.
Logic evalGate(GateType type, const std::vector<int>& fanin, const std::vector<Logic>& value);

// Event-free, levelized ("compiled") logic simulator. The evaluation order is
// built once from the levels computed by levelize(); each pattern is then one
// pass over that order, so every fan-in is final before its gate is evaluated.
class LogicSim {
public:
    // c must already be levelized with no combinational loop, and must
    // outlive the simulator.
    explicit LogicSim(const Circuit& c);

    // Simulates one pattern. pi[k] is the value of primary input c.pis[k].
    void run(const std::vector<Logic>& pi);

    Logic value(int node) const { return value_[node]; }

private:
    const Circuit& c_;
    std::vector<int> order_;     // non-PI node indices, sorted by level
    std::vector<Logic> value_;   // current value of every line
};

// Reads a pattern file: one "PI-ID,VALUE" per line, VALUE is 0, 1 or X.
// Blank lines and CRLF endings are accepted. pi is indexed like c.pis;
// primary inputs missing from the file are set to X and reported in
// warning. Returns false (and sets err) on a malformed line, an unknown or
// non-PI node ID, or a PI assigned twice.
bool readPattern(const Circuit& c, const std::string& path, std::vector<Logic>& pi,
                 std::string& err, std::string& warning);

// Writes one "PO-ID,VALUE" line per primary output, in file order.
bool writeOutputs(const Circuit& c, const LogicSim& sim, const std::string& path);

#endif
