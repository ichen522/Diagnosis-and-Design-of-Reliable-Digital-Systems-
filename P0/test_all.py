# Run LEV on every circuit in auto-tests-parser/input-ckts and compare with golden-outputs.
# test.py only checks 3 small circuits (c17, add2, x3mult), all listed in topological order.
# The larger ISCAS85 circuits (c432 and up) are NOT in topological order, so a
# single-pass levelizer passes test.py but fails here.
#
# Usage (from this P0 folder, on Linux / viterbi-scf):  python3 test_all.py

import os
import subprocess
from collections import Counter

CKT_DIR = "auto-tests-parser/input-ckts"
GOLD_DIR = "auto-tests-parser/golden-outputs"
OUT_DIR = "build/out"   # relative to P0; levelizer runs inside build/

os.system("rm -rf build; mkdir build && cd build && cmake .. > /dev/null && make > /dev/null")
if not os.path.exists("build/levelizer"):
    print("build failed")
    exit(1)
os.makedirs(OUT_DIR, exist_ok=True)

names = sorted(f[:-4] for f in os.listdir(CKT_DIR) if f.endswith(".ckt"))
passed = 0
for n in names:
    out = f"{OUT_DIR}/{n}_lev.txt"
    if os.path.exists(out):
        os.remove(out)
    cmd =f"read ../{CKT_DIR}/{n}.ckt\nlev out/{n}_lev.txt\nquit\n"
    try:
        subprocess.run(["./levelizer"], input=cmd, cwd="build", timeout=60,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       universal_newlines=True)
    except subprocess.TimeoutExpired:
        print(f"{n:8s} TIMEOUT (infinite loop?)")
        continue
    if not os.path.exists(out):
        print(f"{n:8s} NO OUTPUT FILE")
        continue
    with open(out) as f:
        ans = Counter(l for l in f if l.strip())
    with open(f"{GOLD_DIR}/{n}.log") as f:
        gold = Counter(l for l in f if l.strip())
    if ans == gold:
        passed += 1
        print(f"{n:8s} PASS")
    else:
        wrong = [l.rstrip() for l in (ans - gold)][:3]
        missing = [l.rstrip() for l in (gold - ans)][:3]
        print(f"{n:8s} FAIL  wrong: {wrong}  missing: {missing}")

print(f"{passed}/{len(names)} passed")
exit(0 if passed == len(names) else 1)
