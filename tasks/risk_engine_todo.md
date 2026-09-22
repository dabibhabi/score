# Risk engine — TODO and reading list

Scaffolding is in place; the implementations are not. `python/score/panel.py`
and `python/score/risk.py` carry the full Python signatures with
`NotImplementedError` bodies, matching how `python/score/analysis.py` was set
up. The C++ side of `score::risk` does not exist yet.

This file is a map, not a manual. Each link answers a *question* you will hit;
none of them are here to be copied from.

---

## Checklist

### Data
- [ ] `scripts/fetch_sp500_kaggle.py` — `andrewmvd/sp-500-stocks` (mirror `scripts/fetch_news_data.py`)
- [ ] `scripts/fetch_factors_kaggle.py` — `nikitamanaenkov/famafrench-factors-and-portfolios`
- [ ] `scripts/fetch_benchmarks_yf.py` — `^VIX`, `^GSPC`, `^IRX` (start a year early so the first 252-day window is full)
- [ ] `python/score/panel.py` bodies
- [ ] `.gitignore` + `data/README.md` entries for the new raw data

### C++ numerics (prerequisites — nothing else works without these)
- [ ] `statistics/special` — inverse normal CDF, incomplete gamma/beta, Student-t and chi-square CDFs
- [ ] `optimize/nelder_mead` — one optimizer, reused by GARCH / Student-t / GPD fits
- [ ] `core/matrix` + `linalg/decomp` — Matrix value type, QR, Cholesky, symmetric eigensolver
- [ ] `core/rng` — seedable, splittable, so threaded Monte Carlo stays reproducible

### C++ risk
- [ ] `risk/returns`, `risk/var`, `risk/backtest`
- [ ] `risk/garch`, `risk/evt`, `risk/drawdown`
- [ ] `risk/factors`, `risk/covariance`
- [ ] `risk/monte_carlo` (`PathModel` strategy, on top of the Wiener process)

### Bindings and Python
- [ ] Split `src/bindings/nanobindings.cpp` before it passes ~1500 lines
- [ ] `python/score/risk.py` bodies
- [ ] Fill the remaining `python/score/analysis.py` stubs
- [ ] Tests: Catch2 per primitive, pytest for the pandas layer

---

## Building the C++ → Python pipeline

### nanobind
The binding layer already in `src/bindings/nanobindings.cpp` uses about a third
of what you will need.

- [Why nanobind exists](https://nanobind.readthedocs.io/en/latest/why.html) —
  what it dropped relative to pybind11, and why that makes it smaller and faster.
  Read this before copying pybind11 answers off Stack Overflow; a lot of them do
  not apply.
- [Binding classes](https://nanobind.readthedocs.io/en/latest/classes.html) —
  how to expose the result structs (`TailRisk`, `GarchFit`, …) with named fields.
- [Object ownership and `keep_alive`](https://nanobind.readthedocs.io/en/latest/ownership_adv.html)
  — **the one to actually understand.** `DescriptiveStats` and `StockAnalyzer`
  hold a `const Series&`; the existing `nb::keep_alive<1,2>` is what stops Python
  from freeing the series while C++ still points at it. Work out for yourself
  which new bindings need it and which do not, because the answer determines
  whether you can take arguments by reference at all.
- [`nb::ndarray`](https://nanobind.readthedocs.io/en/latest/ndarray.html) — how
  arrays cross the boundary without being converted element by element. Compare
  against what `nanobind/stl/vector.h` does to a `std::vector<double>` and decide
  which of your return types can afford it.
- [Exceptions](https://nanobind.readthedocs.io/en/latest/exceptions.html) — how
  the C++ exception hierarchy becomes Python exception types, and what order
  translators run in.
- [The GIL](https://docs.python.org/3/c-api/init.html#thread-state-and-the-global-interpreter-lock)
  and [nanobind's free-threading notes](https://nanobind.readthedocs.io/en/latest/free_threaded.html)
  — required before threading the Monte Carlo. The rule you need to derive: what
  may a released-GIL region touch?

### Data across the boundary
- [DLPack](https://dmlc.github.io/dlpack/latest/) — the protocol `nb::ndarray`
  speaks. Explains why zero-copy is possible at all, and what the memory has to
  look like for it to work.
- [NumPy array interface](https://numpy.org/doc/stable/reference/arrays.interface.html)
  — strides, contiguity, ownership. The vocabulary for "why is my array copied?"
- [Buffer protocol](https://docs.python.org/3/c-api/buffer.html) — the CPython
  layer underneath all of the above.

### Build and packaging
- [scikit-build-core](https://scikit-build-core.readthedocs.io/en/latest/) — how
  `pyproject.toml` drives CMake here.
- [Its editable-install notes](https://scikit-build-core.readthedocs.io/en/latest/configuration/index.html)
  — worth reading closely. An editable install keeps its **own** copy of the
  compiled extension, which is what `import score` loads; that is why
  `make build` has a `sync-extension` step. Understand that before wondering why
  a C++ change had no effect.
- [CMake RPATH handling](https://cmake.org/cmake/help/latest/prop_tgt/BUILD_RPATH.html)
  — why `$ORIGIN` appears in `src/bindings/CMakeLists.txt`, and how
  `libscore_core.so` is found at runtime.
- [Catch2 docs](https://github.com/catchorg/Catch2/blob/devel/docs/Readme.md) —
  matchers and tags; the suite already follows a `[module][feature]` convention.

### Debugging the boundary
- `python -X importtime -c "import score"` — where import time goes.
- `ldd` on the built `.so`, and `nm -D --defined-only` on `libscore_core.so` —
  when a symbol is "missing" but you are sure you compiled it.
- `make build-debug` — the Release build is `-O3` with LTO, which makes stepping
  through the numerical code hopeless.

---

## The math

Enough to know what to search for. None of these hand you an implementation that
fits this codebase.

| Topic | Where to start |
|---|---|
| VaR / Expected Shortfall, coherence | Acerbi & Tasche, *On the coherence of expected shortfall* (2002) |
| EWMA volatility | J.P. Morgan, *RiskMetrics Technical Document* (1996), ch. 5 — the source of λ = 0.94 |
| GARCH | Bollerslev (1986); Engle, *GARCH 101* (2001) for the intuition |
| Cornish-Fisher | Cornish & Fisher (1938); look specifically for when the expansion stops being monotone |
| Extreme value theory / POT | McNeil & Frey (2000); Embrechts, Klüppelberg & Mikosch, *Modelling Extremal Events* |
| VaR backtesting | Kupiec (1995) for coverage; Christoffersen (1998) for independence |
| Factor models | [Ken French's data library](https://mba.tuck.dartmouth.edu/pages/faculty/ken.french/data_library.html) — also documents the file format you are parsing |
| Covariance shrinkage | Ledoit & Wolf, *Honey, I shrunk the sample covariance matrix* (2004) |
| Systemic fragility / absorption ratio | Kritzman, Li, Page & Rigobon (2010) |
| Numerical linear algebra | Golub & Van Loan, *Matrix Computations* — QR, Cholesky, Jacobi eigenvalues |
| Special functions | [Cephes](https://www.netlib.org/cephes/) — the library `cdf.hpp` was ported from; `ndtri`, `igam`, `incbet` are the ones still needed |
| Derivative-free optimization | Nelder & Mead (1965); Lagarias et al. (1998) on when it actually converges |
| Brownian motion / SDEs | Glasserman, *Monte Carlo Methods in Financial Engineering* |

---

## Traps already identified

Written down because each one is silent — nothing errors, the numbers are just
wrong.

- **Survivorship bias.** `sp500_companies.csv` is *current* membership, so the
  panel cannot contain a single company that blew up and left the index. Every
  backtest headline has to say so.
- **Adjusted prices are vintage-dependent.** A 2024 Kaggle snapshot and a fresh
  yfinance pull disagree on every pre-split price. Compute returns within one
  source, then concatenate — never across the seam.
- **Fama-French files** use percent units, `YYYYMMDD` integer dates, `-99.99`
  sentinels and a copyright preamble. Mask before rescaling, not after.
- **Lookahead.** Rolling features are assigned to day *t* and must be shifted
  before meeting anything from *t+1*. `score.news` already shifts headlines to
  the next trading day; joining on `published` instead of `date` undoes it.
- **Cross-sectional scaling** uses that day's cross-section only. A full-sample
  mean leaks the future into every past row.
- **Missing components** must be dropped and the weights renormalized, not
  filled with zero — zero means "exactly average", which is a real claim.
- **Overlapping forward windows** inflate naive t-statistics by roughly 4x.
