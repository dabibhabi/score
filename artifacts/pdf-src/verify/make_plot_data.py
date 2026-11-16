"""Regenerate ../data/*.dat used by the pgfplots figures.
Inputs: aapl_returns.txt (written by mock_var.cpp) and bench_run2.txt (output of bench.cpp)."""
import numpy as np, math
from fractions import Fraction as F
D = "../data/"
r = np.loadtxt("aapl_returns.txt")
cnt, edges = np.histogram(r, bins=np.arange(-0.07, 0.0701, 0.005))
with open(D + "aapl_hist.dat", "w") as f:
    f.write("x y\n")
    for c, e in zip(cnt, edges[:-1]): f.write(f"{e:.4f} {c}\n")
    f.write(f"{edges[-1]:.4f} 0\n")
s = np.sort(r); n = len(r)
with open(D + "aapl_varcurve.dat", "w") as f:
    f.write("c var cvar\n")
    for c in np.arange(0.90, 0.9951, 0.0025):
        k = int((1.0 - c) * n + 1e-9)
        if k == 0: continue
        f.write(f"{c:.4f} {max(0,-s[k-1]):.6f} {max(0,-s[:k].mean()):.6f}\n")
lines = open("bench_run2.txt").read().splitlines()[:10]
open(D + "bench.dat", "w").write("\n".join(l.replace(",", " ") for l in lines) + "\n")
# Vancouver-style truncation simulation (illustrative, not historical data)
rng = np.random.default_rng(1982)
U = 1_386_000; steps = rng.normal(0, 0.00005, size=U)
ex = tr = ro = 1000.0; out = []
for i, s_ in enumerate(steps):
    ex *= 1 + s_; tr = math.floor(tr * (1 + s_) * 1000) / 1000; ro = round(ro * (1 + s_) * 1000) / 1000
    if i % 27720 == 0 or i == U - 1: out.append((i + 1, ex, tr, ro))
with open(D + "vse.dat", "w") as f:
    f.write("u exact trunc round\n")
    for u, a, b, c in out: f.write(f"{u} {a:.3f} {b:.3f} {c:.3f}\n")
print("VSE final exact %.3f trunc %.3f round %.3f; mean loss/update %.6f" % (out[-1][1], out[-1][2], out[-1][3], (out[-1][1]-out[-1][2])/U))
# Patriot drift: 1/10 chopped after 23 binary places
err = F(1, 10) - F(math.floor(F(1, 10) * 2**23), 2**23)
with open(D + "patriot.dat", "w") as f:
    f.write("h drift\n")
    for h in [0, 1, 8, 20, 48, 72, 100]: f.write(f"{h} {float(err*h*36000):.4f}\n")
print("patriot err/tick", float(err), "chopped =", F(math.floor(F(1,10)*2**23), 2**23))
