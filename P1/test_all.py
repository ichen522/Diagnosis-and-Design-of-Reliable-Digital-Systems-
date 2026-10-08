# Extended tests for LOGICSIM, beyond the course test.py (5 circuits, 27 patterns).
#
# 1. Reference check: every circuit in circuits/ (17 netlists, up to c6288)
#    is simulated by ./simulator and by an independent reference model in this
#    file. Circuits with <= 10 PIs get all 2^n patterns; larger ones get the
#    all-0 and all-1 patterns plus random ones, some containing X.
#    The reference evaluates each PO recursively from the .ckt records, with
#    no levelization and no fan-out lists, so it shares no logic with the C++.
#
# 2. Functional check: compares against what the circuits are known to
#    compute, so a bug shared by both models above would still be caught.
#      FA, FA_NAND  full adder (sum, carry)
#      add2         2-bit adder with carry-in
#      c6288        16 x 16 multiplier
#      c1355        same function as c499 (c499 with every XOR expanded into NANDs)
#
# Usage (from this P1 folder, Linux + Python 3):  python3 test_all.py [random-patterns]

import os
import random
import subprocess
import sys

CKT_DIR = "circuits"
WORK = "build/sim"           # relative to P1; the simulator runs inside build/
RANDOM_PATTERNS = int(sys.argv[1]) if len(sys.argv) > 1 else 200
EXHAUSTIVE_MAX_PI = 10
X = "X"

sys.setrecursionlimit(100000)


# ---------------------------------------------------------------- reference model
def load(path):
    gates, pis, pos = {}, [], []
    with open(path) as f:
        for line in f:
            t = list(map(int, line.split()))
            if not t:
                continue
            kind, node = t[0], t[1]
            if kind == 2:                                  # branch: copy of its stem
                gates[node] = (1, [t[3]])
            else:
                gates[node] = (t[2], t[5:5 + t[4]])
            if kind == 1:
                pis.append(node)
            if kind == 3:
                pos.append(node)
    return {"gates": gates, "pis": pis, "pos": pos}


def inv(v):
    return X if v == X else 1 - v


def gate(g, v):
    if g in (1, 9):                                        # branch, BUF
        return v[0]
    if g == 5:                                             # NOT
        return inv(v[0])
    if g in (7, 6):                                        # AND, NAND
        r = 0 if 0 in v else (X if X in v else 1)
        return r if g == 7 else inv(r)
    if g in (3, 4):                                        # OR, NOR
        r = 1 if 1 in v else (X if X in v else 0)
        return r if g == 3 else inv(r)
    if g in (2, 8):                                        # XOR, XNOR
        r = X if X in v else sum(v) % 2
        return r if g == 2 else inv(r)
    raise ValueError("gate type %d" % g)


def reference(ckt, pattern):
    memo = dict(pattern)

    def value(n):
        if n not in memo:
            g, fanin = ckt["gates"][n]
            memo[n] = gate(g, [value(u) for u in fanin])
        return memo[n]

    return {p: value(p) for p in ckt["pos"]}


# ---------------------------------------------------------------- run ./simulator
def run_simulator(name, patterns):
    """Simulate all patterns in one simulator session; return one {PO: value} per pattern."""
    cmds = ["READ ../%s/%s.ckt" % (CKT_DIR, name)]
    for k, pat in enumerate(patterns):
        with open("%s/%s_%d.in" % (WORK, name, k), "w") as f:
            for node, v in pat.items():
                f.write("%d,%s\n" % (node, v))
        cmds.append("LOGICSIM sim/%s_%d.in sim/%s_%d.out" % (name, k, name, k))
    cmds.append("QUIT")
    subprocess.run(["./simulator"], input="\n".join(cmds) + "\n", cwd="build", timeout=600,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, universal_newlines=True)
    results = []
    for k in range(len(patterns)):
        out = {}
        path = "%s/%s_%d.out" % (WORK, name, k)
        if os.path.exists(path):
            with open(path) as f:
                for line in f:
                    if line.strip():
                        node, v = line.strip().split(",")
                        out[int(node)] = v if v == X else int(v)
        results.append(out)
    return results


def patterns_for(ckt, rng):
    pis = ckt["pis"]
    n = len(pis)
    if n <= EXHAUSTIVE_MAX_PI:
        return [{p: (v >> i) & 1 for i, p in enumerate(pis)} for v in range(2 ** n)]
    pats = [{p: 0 for p in pis}, {p: 1 for p in pis}]
    for k in range(RANDOM_PATTERNS):
        with_x = k % 4 == 3                                # every 4th pattern has some X inputs
        pats.append({p: (X if with_x and rng.random() < 0.1 else rng.getrandbits(1)) for p in pis})
    return pats


def bits(v, n):
    return [(v >> i) & 1 for i in range(n)]


def num(bs):
    """Unsigned value of an LSB-first bit list; None if any bit is missing or X."""
    if any(b not in (0, 1) for b in bs):
        return None
    return sum(b << i for i, b in enumerate(bs))


# ---------------------------------------------------------------- main
os.system("rm -rf build; mkdir build && cd build && cmake .. > /dev/null && make > /dev/null")
if not os.path.exists("build/simulator"):
    print("build failed")
    sys.exit(1)
os.makedirs(WORK, exist_ok=True)

rng = random.Random(658)
ckts = {f[:-4]: load("%s/%s" % (CKT_DIR, f)) for f in os.listdir(CKT_DIR) if f.endswith(".ckt")}
names = sorted(ckts, key=lambda n: (len(ckts[n]["gates"]), n))
failures = 0

print("1. Reference check")
print("%-8s %4s %4s %6s %10s  %s" % ("circuit", "PI", "PO", "nodes", "patterns", "result"))
for n in names:
    ckt = ckts[n]
    pats = patterns_for(ckt, rng)
    got = run_simulator(n, pats)
    bad = [k for k, p in enumerate(pats) if got[k] != reference(ckt, p)]
    failures += len(bad)
    kind = "all" if len(ckt["pis"]) <= EXHAUSTIVE_MAX_PI else "rand"
    print("%-8s %4d %4d %6d %5d %-4s  %s" % (
        n, len(ckt["pis"]), len(ckt["pos"]), len(ckt["gates"]), len(pats), kind,
        "PASS" if not bad else "FAIL patterns %s" % bad[:5]))

print("\n2. Functional check")


def check(label, ok, total):
    global failures
    failures += total - ok
    print("%-44s %4d/%-4d %s" % (label, ok, total, "PASS" if ok == total else "FAIL"))


for n in ("FA", "FA_NAND"):
    ckt = ckts[n]
    pats = [dict(zip(ckt["pis"], bits(v, 3))) for v in range(8)]
    got = run_simulator(n, pats)
    ok = 0
    for p, o in zip(pats, got):
        a, b, c = (p[i] for i in ckt["pis"])
        s, cy = (o.get(i) for i in ckt["pos"])
        ok += (s, cy) == ((a + b + c) & 1, (a + b + c) >> 1)
    check("%s: (sum, carry) = a + b + cin" % n, ok, 8)

ckt = ckts["add2"]
pats = [dict(zip(ckt["pis"], bits(v, 5))) for v in range(32)]
got = run_simulator("add2", pats)
ok = 0
for p, o in zip(pats, got):
    cin, a0, b0, a1, b1 = (p[i] for i in ckt["pis"])
    ok += num([o.get(i) for i in ckt["pos"]]) == (a0 + 2 * a1) + (b0 + 2 * b1) + cin
check("add2: 2-bit adder with carry-in", ok, 32)

ckt = ckts["c6288"]
ab = [(0, 0), (0xFFFF, 0xFFFF), (1, 0xFFFF), (0xFFFF, 1), (0x8000, 0x8000)]
ab += [(rng.getrandbits(16), rng.getrandbits(16)) for _ in range(RANDOM_PATTERNS)]
pats = [dict(zip(ckt["pis"], bits(a, 16) + bits(b, 16))) for a, b in ab]
got = run_simulator("c6288", pats)
# PO order in the .ckt is P0..P29, then 6287 = P31 and 6288 = P30 (as in the ISCAS-85 netlist)
order = ckt["pos"][:30] + [ckt["pos"][31], ckt["pos"][30]]
ok = sum(num([o.get(i) for i in order]) == a * b for (a, b), o in zip(ab, got))
check("c6288: 32-bit product = a x b", ok, len(ab))

a, b = ckts["c499"], ckts["c1355"]
vecs = [[rng.getrandbits(1) for _ in a["pis"]] for _ in range(RANDOM_PATTERNS)]
got_a = run_simulator("c499", [dict(zip(a["pis"], v)) for v in vecs])
got_b = run_simulator("c1355", [dict(zip(b["pis"], v)) for v in vecs])
ok = sum([oa.get(i) for i in a["pos"]] == [ob.get(i) for i in b["pos"]] and len(oa) == 32
         for oa, ob in zip(got_a, got_b))
check("c1355 == c499 on the same inputs", ok, len(vecs))

print("\n%s" % ("All tests passed." if failures == 0 else "%d failure(s)." % failures))
sys.exit(0 if failures == 0 else 1)
