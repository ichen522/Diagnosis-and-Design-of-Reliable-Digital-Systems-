// Levelization (LEV command).
//
// A line's level is its depth in the circuit DAG: the arrival time of its
// value when every gate and every branch costs one unit of delay. Evaluating
// lines in increasing level order guarantees every fan-in is ready first,
// which is the order later projects use for logic and fault simulation.
//
// The .ckt lines are not in topological order for the larger benchmarks
// (c432 lists all branches at the end of the file), so one pass in file order
// is not enough. This uses Kahn's topological sort instead:
//   pending[v] = number of fan-ins of v whose level is not final yet.
//   A line enters the ready queue when pending reaches 0; its level is final
//   then, and it pushes level+1 to each fan-out.
// Every node and every edge is touched once: O(#nodes + #edges), independent
// of the line order in the file.

#include "levelizer.h"

#include <algorithm>
#include <cstdio>
#include <vector>

int levelize(Circuit& c) {
    const int n = static_cast<int>(c.nodes.size());
    std::vector<int> pending(n);
    std::vector<int> ready;  // used as a FIFO queue; head index below
    ready.reserve(n);

    for (int i = 0; i < n; ++i) {
        Node& node = c.nodes[i];
        pending[i] = static_cast<int>(node.fanin.size());
        node.level = -1;
        if (pending[i] == 0) {  // primary input
            node.level = 0;
            ready.push_back(i);
        }
    }

    for (std::size_t head = 0; head < ready.size(); ++head) {
        const int u = ready[head];
        const int next = c.nodes[u].level + 1;
        for (int v : c.nodes[u].fanout) {
            c.nodes[v].level = std::max(c.nodes[v].level, next);
            if (--pending[v] == 0) ready.push_back(v);
        }
    }

    // Lines on (or behind) a combinational loop never reach pending == 0;
    // drop the partial level they may have picked up.
    for (int i = 0; i < n; ++i)
        if (pending[i] > 0) c.nodes[i].level = -1;
    return n - static_cast<int>(ready.size());
}

bool writeLevels(const Circuit& c, const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) return false;
    std::fprintf(fp, "%s\n", c.name.c_str());
    std::fprintf(fp, "#PI: %d\n", static_cast<int>(c.pis.size()));
    std::fprintf(fp, "#PO: %d\n", static_cast<int>(c.pos.size()));
    std::fprintf(fp, "#Nodes: %d\n", static_cast<int>(c.nodes.size()));
    std::fprintf(fp, "#Gates: %d\n", c.numGates());
    for (const Node& node : c.nodes) std::fprintf(fp, "%d %d\n", node.id, node.level);
    std::fclose(fp);
    return true;
}
