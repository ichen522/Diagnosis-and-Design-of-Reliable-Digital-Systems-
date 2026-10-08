# P0 — Netlist Parser & Levelizer

Reads a gate-level combinational netlist (ISCAS-85 benchmarks in the course `.ckt` format), builds a fan-in / fan-out graph, and assigns a **logic level** to every line. The levels give the evaluation order used by the logic simulator, fault simulator and ATPG in later projects.

## Usage

```
mkdir build && cd build
cmake .. && make
./levelizer
```

```
Command>READ ../circuits/c17.ckt
==> OK
Command>LEV c17_lev.txt
==> OK
Command>QUIT
```

| Command | Description |
|---|---|
| `READ <file.ckt>` | Parse a netlist and build the graph |
| `PC` | Print every node with its fan-ins and fan-outs |
| `LEV <output-file>` | Levelize and write the report |
| `HELP`, `QUIT` | |

Commands are case-insensitive. `PC` and `LEV` need a successful `READ` first.

Report for c17 (`circuit name`, counts, then `node-ID level` in file order):

```
c17
#PI: 5
#PO: 2
#Nodes: 17
#Gates: 6
1 0
2 0
3 0
8 1
...
22 6
23 6
```

## Netlist format

Each line of a `.ckt` file describes one **line (net)**, not one gate. A gate is represented by its output line.

| Col 1 | Meaning | Remaining columns |
|---|---|---|
| `0` GATE | gate output | gate type, #fan-out, #fan-in, fan-in IDs |
| `1` PI | primary input | `0`, #fan-out, `0` |
| `2` FB | fan-out branch | `1`, stem ID |
| `3` PO | gate output that is a primary output | gate type, `0`, #fan-in, fan-in IDs |

Gate types: 0 IPT, 1 BRCH, 2 XOR, 3 OR, 4 NOR, 5 NOT, 6 NAND, 7 AND, 8 XNOR, 9 BUF.

Fan-out branches are separate nodes because a stuck-at fault on a stem and a fault on one of its branches are different faults.

## Design

| File | Contents |
|---|---|
| `circuit.h` | `Node` (ID, type, fan-in / fan-out index lists, level) and `Circuit` |
| `readckt.cpp` | `READ` parser and `PC` printer |
| `levelizer.h/.cpp` | `LEV`: levelization and report writer |
| `main.cpp` | Table-driven command shell; a new command is one function plus one row in `commands[]` |

Parser details:

- Reads the file as a stream of integers tagged with line numbers, so CRLF files, blank lines and records wrapped over several lines all parse. Errors name the line (`line 2: node 3 uses undefined node 9`).
- Maps every node ID to an index before resolving fan-ins, so lines may appear in any order.
- Rebuilds fan-outs from the fan-in references instead of trusting column 4, which is `0` for a PO and implied `1` for a branch. Both are wrong when a PO also drives a gate or a branch feeds further branches.
- All-or-nothing: a failed `READ` leaves the previously loaded circuit intact.
- Uses `std::string` throughout. The course framework's fixed 81-character buffer silently splits long command lines.

## Algorithm

```
level(line) = 0                                for a primary input
level(line) = 1 + max(level of its fan-ins)    for a gate output or a branch
```

The level is the line's arrival time when every gate and branch costs one unit of delay. Evaluating lines in increasing level order guarantees every fan-in is ready first.

A single pass in file order is not enough. Only the small circuits are listed in topological order; c432, for example, lists all fan-out branches at the end of the file. Repeating file-order sweeps until nothing changes takes 13 sweeps on c432, 11 on c880 and 94 on c6288.

This implementation uses Kahn's topological sort:

1. `pending[v]` = number of fan-ins of `v` not yet final. Lines with no fan-in get level 0 and enter a queue.
2. Pop `u`. For each fan-out `v`: `level[v] = max(level[v], level[u] + 1)`, decrement `pending[v]`, and enqueue `v` when it reaches 0.
3. A line's level is final when it is dequeued. Lines that never reach `pending = 0` lie on a combinational loop; they are reported and keep level `-1`.

Every node and every edge is processed once: O(nodes + edges), independent of the order of lines in the file.

## Results

All 17 benchmark circuits match the golden outputs.

| Circuit | Nodes | Gates | PI | PO | Max level | File in topological order |
|---|---:|---:|---:|---:|---:|:---:|
| c17 | 17 | 6 | 5 | 2 | 6 | yes |
| c432 | 432 | 160 | 36 | 7 | 29 | no |
| c499 | 499 | 202 | 41 | 32 | 16 | no |
| c880 | 880 | 383 | 60 | 26 | 34 | no |
| c1355 | 1355 | 546 | 41 | 32 | 39 | no |
| c1908 | 1908 | 880 | 33 | 25 | 60 | no |
| c3540 | 3540 | 1669 | 50 | 22 | 70 | no |
| c5315 | 5315 | 2307 | 178 | 123 | 67 | no |
| c6288 | 6288 | 2416 | 32 | 32 | 217 | no |

Also checked against an independent reference implementation:

- randomly shuffled line order (c17, c432, c3540, c6288)
- CRLF line endings
- a branch feeding further branches, and a PO that also drives gates
- records wrapped over several lines
- a combinational loop (no hang; affected lines reported)
- malformed input: undefined or duplicate node, non-integer token, unknown type, truncated record

## Testing

```
python3 test.py       # course autograder: c17, add2, x3mult
python3 test_all.py   # all 17 circuits in auto-tests-parser/
```

Both scripts build with CMake and need a Linux shell with Python 3.

## Credits

The netlist format, benchmark circuits, `test.py` and golden outputs are provided by the USC EE658 course staff. The original framework is a C parser by Chihang Chen (1994), later ported to C++ for EE658. This version is a rewrite.
