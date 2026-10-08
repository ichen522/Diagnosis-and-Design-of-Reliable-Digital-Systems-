# EE658 — Diagnosis and Design of Reliable Digital Systems

USC EE658 (Fall 2026) course projects, in C++. Together they build a small test-generation toolchain for combinational circuits: netlist parser → levelizer → logic simulator → fault simulation / ATPG.

| Folder | Project | New command | Status |
|---|---|---|---|
| [P0](P0/) | Netlist parser & levelizer | `LEV` | done — 17/17 benchmarks |
| [P1](P1/) | Logic simulator | `LOGICSIM` | next |
| [P2](P2/) | TBD | | |

Each project extends the previous one. Every folder is a self-contained snapshot that builds on its own:

```
cd P0
mkdir build && cd build
cmake .. && make
```

## Adding the next project

1. Copy the previous folder: `cp -r P0 P1`
2. Put the new module in its own `.cpp` / `.h`, add one function plus one row to `commands[]` in `main.cpp`, and register the new source in `CMakeLists.txt`
3. Copy the assignment's `test.py`, test zip and golden outputs into the folder
4. Test, update the table above, then:

```
git add P1 README.md
git commit -m "P1: logic simulator"
git push
```
