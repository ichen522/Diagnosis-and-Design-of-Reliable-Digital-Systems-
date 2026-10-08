# P1 — Logic Simulator

Adds a `LOGICSIM` command to the P0 parser and levelizer. Given one input pattern (a value for every primary input), it computes the value of every line in the circuit and writes the primary outputs. This simulator is the core that fault simulation and ATPG in later projects run on top of.

## Usage

```
mkdir build && cd build
cmake .. && make
./simulator
```

```
Command>READ ../circuits/c17.ckt
==> OK
Command>LOGICSIM c17_t1.txt c17_t1_output.txt
==> OK
Command>QUIT
```

| Command | Description |
|---|---|
| `READ <file.ckt>` | Parse a netlist and build the graph |
| `PC` | Print every node with its fan-ins and fan-outs |
| `LEV <output-file>` | Levelize and write the report (same as P0) |
| `LOGICSIM <input-file> <output-file>` | Simulate one pattern and write the primary outputs |
| `HELP`, `QUIT` | |

`LOGICSIM` levelizes the circuit itself, so it does not need `LEV` to run first. Commands are case-insensitive.

### Pattern files

Input and output use the same format: one `NODE-ID,VALUE` per line.

```
c17_t1.txt          c17_t1_output.txt
1,1                 22,1
2,1                 23,0
3,1
6,1
7,1
```

- Outputs are written in the order the POs appear in the `.ckt` file.
- Values are `0` or `1`. `X` is also accepted (see [Three-valued logic](#three-valued-logic)).
- Blank lines, spaces around the comma, and CRLF line endings are accepted.
- A PI missing from the file is set to `X`, with a warning.
- An unknown node ID, a node that is not a PI, a PI assigned twice, or a bad value is an error that names the file and line, e.g. `in.txt line 5: node 10 is not a primary input`. Nothing is written in that case.

## Design

| File | Contents | Changed from P0 |
|---|---|---|
| `circuit.h`, `readckt.cpp` | Netlist data structure, `READ` parser, `PC` | unchanged |
| `levelizer.h/.cpp` | `LEV`: Kahn's topological sort, report writer | unchanged |
| `simulator.h/.cpp` | `LOGICSIM`: gate evaluation, `LogicSim` class, pattern reader and writer | **new** |
| `main.cpp` | Command shell | one function, one row in `commands[]` |
| `CMakeLists.txt` | Builds the `simulator` executable | target renamed, `simulator.cpp` added |

The simulator module only reads the `Circuit` built by the parser. The interface (`simulator.h`):

```cpp
enum Logic : unsigned char { L0, L1, LX };

Logic evalGate(GateType type, const std::vector<int>& fanin, const std::vector<Logic>& value);

class LogicSim {
public:
    explicit LogicSim(const Circuit& c);       // builds the evaluation order
    void run(const std::vector<Logic>& pi);    // one pattern
    Logic value(int node) const;               // any line, not just POs
};
```

`value()` exposes every internal line, not only the POs. Fault simulation in the next projects needs those internal values.

## Algorithm

### Levelized (compiled) simulation

A line at level *k* depends only on lines at levels below *k*. Evaluating lines in increasing level order therefore finds every fan-in already final, and one pass gives the correct value of every line, with no event queue and no iteration.

1. **Order (once per `LOGICSIM`).** Levelize with P0's Kahn sort, then counting-sort the non-PI lines by level. This is O(nodes), and lines on the same level stay in file order, so runs are reproducible.
2. **Simulate.** Load the PI values, then walk the order and evaluate each line from its fan-ins. This is O(nodes + edges).
3. **Write** the PO values in file order.

The order is needed because the `.ckt` files are not topologically sorted. c432 and every larger benchmark list some lines before their fan-ins. Simulating in file order fails 10 of the 27 course tests, as the mutation check below shows.

A combinational loop has no valid order. In that case `LOGICSIM` reports how many lines are on the loop and does not simulate.

### Gate evaluation

Every gate is evaluated with its controlling value, which covers any number of inputs and the unknown value `X` in one rule:

| Gate | Output |
|---|---|
| AND | 0 if any input is 0; else X if any input is X; else 1 |
| OR | 1 if any input is 1; else X if any input is X; else 0 |
| XOR | X if any input is X; else the parity of the inputs |
| NAND, NOR, XNOR | inverse of AND, OR, XOR |
| NOT | inverse of its input |
| BUF, branch | copy of its input |

Gate types follow the course numbering: 0 PI, 1 branch, 2 XOR, 3 OR, 4 NOR, 5 NOT, 6 NAND, 7 AND, 8 XNOR, 9 BUF.

- **Wide gates** loop over all fan-ins. The benchmarks have gates with up to 9 inputs (c432, c5315).
- **One-input gates.** The course notes say some netlists use one-input AND/OR gates as buffers; add2 and x3mult have them. The general rule handles them without a special case: a one-input AND or OR is a buffer, and a one-input NAND or NOR is an inverter.

### Three-valued logic

P1 patterns only use 0 and 1, but the simulator works in {0, 1, X}. Fault simulation and ATPG (PODEM, D-algorithm) start from all-X and assign PIs one at a time, so supporting X now means `LogicSim` can be reused there unchanged. For 0/1 patterns, the results are identical to two-valued simulation.

## Verification

The course `test.py` has 27 patterns on 5 circuits, all fully specified. The spec says grading uses "many more complicated patterns and large circuits", so the simulator was checked in four further ways.

### 1. Course autograder: 27 / 27

```
$ python3 test.py
...
27 tests are passed.
0 tests are failed.
```

### 2. Independent reference model: 1,896 patterns, 17 circuits, 0 mismatches

`test_all.py` simulates every circuit in `circuits/` with `./simulator` and with a reference model written separately in Python. The reference evaluates each PO recursively straight from the `.ckt` records, with no levelization and no fan-out lists, so it shares no code or algorithm with the C++.

- Circuits with up to 10 PIs: all 2<sup>n</sup> patterns.
- Larger circuits: all-0, all-1 and 200 random patterns. Every 4th random pattern sets about 10% of the PIs to `X`, which tests the three-valued logic.
- Each circuit runs as one simulator session with one `LOGICSIM` per pattern. This also checks that repeated calls don't leave stale state behind.

```
1. Reference check
circuit    PI   PO  nodes   patterns  result
FA          3    2     16     8 all   PASS
c1          5    2     17    32 all   PASS
c17         5    2     17    32 all   PASS
c2          7    1     22   128 all   PASS
c3          4    1     22    16 all   PASS
c4          4    1     24    16 all   PASS
FA_NAND     3    2     26     8 all   PASS
x3mult      3    3     40     8 all   PASS
add2        5    3     52    32 all   PASS
c432       36    7    432   202 rand  PASS
c499       41   32    499   202 rand  PASS
c880       60   26    880   202 rand  PASS
c1355      41   32   1355   202 rand  PASS
c1908      33   25   1908   202 rand  PASS
c3540      50   22   3540   202 rand  PASS
c5315     178  123   5315   202 rand  PASS
c6288      32   32   6288   202 rand  PASS
```

### 3. Functional check: 453 / 453

Two models agreeing could still share a misreading of the format. These checks compare against what the circuits are known to compute instead:

```
2. Functional check
FA: (sum, carry) = a + b + cin                  8/8    PASS
FA_NAND: (sum, carry) = a + b + cin             8/8    PASS
add2: 2-bit adder with carry-in                32/32   PASS
c6288: 32-bit product = a x b                 205/205  PASS
c1355 == c499 on the same inputs              200/200  PASS
```

- **c6288** is a 16 × 16 array multiplier with 2,416 gates and depth 217, the deepest circuit here. The first 16 PIs are *a* (LSB first), the next 16 are *b*, and the 32 POs are the product. Corner cases (0 × 0, 0xFFFF × 0xFFFF, 1 × 0xFFFF, 0x8000 × 0x8000) are included.
- **c1355** is c499 (a 32-bit single-error-correcting circuit) with every XOR rebuilt from NAND gates. The two must give identical outputs for every input, and they do on 200 random patterns.

### 4. Mutation check: do the tests catch real bugs?

Five plausible bugs were injected into `simulator.cpp` one at a time:

| Injected bug | Course `test.py` | `test_all.py` |
|---|---|---|
| XOR evaluated as XNOR | 22/27 pass | 554 failures |
| BUF evaluated as NOT | 4/27 pass | 2,139 failures |
| Evaluate in file order instead of level order | 17/27 pass | 1,796 failures |
| NAND only looks at its first 2 inputs | **27/27 pass** | 448 failures |
| AND/OR ignore X inputs | **27/27 pass** | 353 failures |

The course tests miss two of the five bugs. The extended tests catch all five.

### Edge cases and memory

- `LOGICSIM` before `READ` gives "Execution out of sequence!". A missing argument prints the usage line.
- Missing PI: set to X, with a warning. On c17, leaving out PIs 6 and 7 with PIs 1–3 at 0 still gives `22,0` (a 0 input controls the NAND), while `23` becomes `X`.
- Bad pattern files (unknown node, non-PI node, duplicate PI, bad value, missing comma), plus an unwritable output path: clear error, no output file.
- A 3-input XNOR, 3-input XOR, 3-input NOR, a BUF and a one-input NAND were checked against their full truth tables in a hand-written netlist.
- A combinational loop is refused, without hanging.
- Command paths over 200 characters work. There is no fixed-size buffer anywhere.
- Re-`READ` of another circuit followed by `LOGICSIM` simulates the new circuit.
- **Valgrind** on a session with three `READ`s (c17, c6288 and a looped netlist), `LEV`, several `LOGICSIM` calls and error paths: `All heap blocks were freed -- no leaks are possible`, 0 errors.
- **AddressSanitizer + UndefinedBehaviorSanitizer:** 404 patterns on c5315 and c6288, no reports, and outputs identical to the release build.
- Compiles cleanly with `-std=c++11 -Wall -Wextra -pedantic` on GCC 13 and Clang.

## Results

| Circuit | Nodes | Gates | PI | PO | Max level | Max fan-in | Time per `LOGICSIM` |
|---|---:|---:|---:|---:|---:|---:|---:|
| c17 | 17 | 6 | 5 | 2 | 6 | 2 | 0.25 ms |
| add2 | 52 | 23 | 5 | 3 | 14 | 2 | 0.29 ms |
| x3mult | 40 | 18 | 3 | 3 | 12 | 2 | 0.31 ms |
| c432 | 432 | 160 | 36 | 7 | 29 | 9 | 0.23 ms |
| c499 | 499 | 202 | 41 | 32 | 16 | 5 | 0.26 ms |
| c880 | 880 | 383 | 60 | 26 | 34 | 4 | 0.25 ms |
| c1355 | 1355 | 546 | 41 | 32 | 39 | 5 | 0.32 ms |
| c1908 | 1908 | 880 | 33 | 25 | 60 | 8 | 0.45 ms |
| c3540 | 3540 | 1669 | 50 | 22 | 70 | 8 | 0.50 ms |
| c5315 | 5315 | 2307 | 178 | 123 | 67 | 9 | 0.71 ms |
| c6288 | 6288 | 2416 | 32 | 32 | 217 | 2 | 0.92 ms |

The time per `LOGICSIM` is the marginal cost of one more call in a running session: re-levelizing, reading the pattern file, simulating and writing the output file. It does not include `READ` or process start-up. Measured on a Linux container, best of 5 runs. File I/O takes about 0.2 ms of each call, so even c6288 costs under 1 ms.

## Findings

- **c6288's last two POs are swapped.** In the `.ckt` file the POs run P0 … P29, then node 6287 = P31 and node 6288 = P30, as in the original ISCAS-85 netlist. Reading the POs in file order as P0 … P31 gives a wrong product on 38% of random inputs. This matters only when interpreting outputs as a number; the simulator itself writes POs by node ID.
- **The course test circuits use no X, and their only gates wider than 2 inputs are 4- and 5-input ANDs and 4-input ORs** (in c499 and c1355). That is why two of the five injected bugs pass all 27 course tests (see the mutation check).
- **No XNOR gates appear in any provided netlist.** XNOR is implemented and tested on a hand-written netlist only.

## Testing

```
python3 test.py        # course autograder: c17, add2, x3mult, c499, c1355 (27 patterns)
python3 test_all.py    # 17 circuits vs reference model + functional checks
python3 test_all.py 1000   # more random patterns per circuit
```

Both scripts build with CMake and need a Linux shell with Python 3.

### Memory check

```
cd build
valgrind --leak-check=yes ./simulator
```

## Process

1. Started from the P0 snapshot (`cp -r P0 P1`). The parser and levelizer are unchanged.
2. Read the Phase-1 spec: `LOGICSIM <in> <out>`, `PI-ID,VALUE` files, XOR/XNOR/BUF, gates with more than 2 inputs, a separate simulator module, an executable named `simulator`, and no fixed-size command buffer.
3. Checked the test data before writing code. The PO order in the golden files matches the `.ckt` order. Gate types 2, 3, 5, 6, 7 appear in the test circuits, and 4 and 9 in the larger benchmarks. add2 and x3mult have one-input AND gates.
4. Wrote `simulator.h/.cpp` and added one row to `commands[]`. The course `test.py` passed 27/27 on the first run.
5. Wrote the independent reference model and the functional checks in `test_all.py`. Working out c6288's bit order turned up the swapped P30/P31.
6. Ran the mutation check to confirm the tests catch bugs, then the edge cases, Valgrind and the sanitizers.

## Credits

The netlist format, benchmark circuits, `test.py`, test patterns and golden outputs are provided by the USC EE658 course staff. The original framework is a C parser by Chihang Chen (1994), later ported to C++ for EE658. This version is a rewrite.
