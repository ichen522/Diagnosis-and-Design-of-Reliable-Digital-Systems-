# P1 — Logic Simulator

Continues from P0. Key requirements from the spec:

- New command `LOGICSIM <input-file> <output-file>`
- Input/output format: one `PI-ID,VALUE` per line
- Netlists include XOR, XNOR, BUF; gates may have more than 2 inputs
- Separate `.cpp` / `.h` files (parser, levelizer, simulator); OOP encouraged
- `CMakeLists.txt` must build an executable named `simulator` (P0 builds `levelizer`)
- Grading commands can be long; P0 already reads commands into `std::string`, with no fixed buffer
