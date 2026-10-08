#ifndef LEVELIZER_H
#define LEVELIZER_H

#include <string>

#include "circuit.h"

// Assigns Node::level to every line:
//   level = 0                              for a line with no fan-in (PI)
//   level = 1 + max(level of its fan-ins)  for a gate output or a branch
// Returns the number of lines left unlevelized (> 0 only if the netlist
// contains a combinational loop; those lines keep level -1).
int levelize(Circuit& c);

// Writes the LEV report: circuit name, #PI, #PO, #Nodes, #Gates, then one
// "node-ID level" line per node in file order.
bool writeLevels(const Circuit& c, const std::string& path);

#endif
