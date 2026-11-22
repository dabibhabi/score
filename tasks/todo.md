# Clang-format → Google style (C++23) + Python formatter (yapf/isort)

- [x] Replace `.clang-format` with the Google-style config, `Standard: Latest` (C++23+).
- [x] Add `[tool.yapf]` and `[tool.isort]` to `pyproject.toml`; add `yapf`/`isort` to `dev` deps.
- [x] Split `Makefile` `format` into `format-cpp` + `format-py`, update `make help`.
- [x] `make dev` to install the new tools into `.venv`.
- [x] `make format` end-to-end; spot-checked diffs (C++ brace/column, Python indent/line length).
- [x] Sanity-checked reformatted Python files parse (`ast.parse`); `make test-py` passes.
- [x] `git diff --stat` reviewed for blast radius.
- [x] Appended `CHANGELOG.md` entry.

## Review

- Phase 3 net bodies implemented: `make test-cpp` 24/24 passed (6 new in `test_net.cpp`). The loop is driven by a
  fake `EventLoop` in tests; real concurrency arrives with `BlockingThreadEventLoop` (Phase 5). `Server`/`Connection`
  are single-threaded (no locking) until then. POSIX-only (no Windows sockets).

- Switched `.clang-format` to the Google C++ Style Guide config. `clang-format` has no
  `c++23` enum literal (it tops out at `c++20`, then `Latest`), so `Standard: Latest`
  is used to request C++23+. The old file already said `c++23`, which was never valid.
- Added `yapf` (knob-configured, like clang-format) + `isort` (import sorting, the
  Python analogue of `IncludeBlocks`/`SortIncludes`) as dev deps, configured in
  `pyproject.toml` with knobs mirroring `.clang-format` (120 col, 4-space indent,
  2 spaces before comments, operator-stays-at-EOL).
- `make format` now runs `format-cpp` (clang-format) then `format-py` (isort + yapf).
- Found and fixed an unrelated pre-existing bug while running the formatter:
  `src/score/market.py` had `def low(self):xp` (stray `xp` typo) which was a syntax
  error and blocked yapf. Fixed; behavior unchanged.
- Verified: `make test-cpp` 16/16 passed, `make test-py` 21/21 passed, all reformatted
  `.py` files parse cleanly post-format. `git diff --stat` shows only mechanical
  whitespace/brace/import-order changes plus the one-line typo fix.

# DB storage + net engine scaffolding, VaR/CVaR risk extension

## Phase 0 — Risk: historical VaR/CVaR
- [x] Add `historical_var`/`historical_cvar` to `include/score/statistics/risk.hpp` + `src/statistics/risk.cpp`.
- [ ] Add `tests/cpp/test_risk.cpp`; register it in `tests/cpp/CMakeLists.txt`.

## Phase 1 — Storage: `ColumnStore` interface + v1 stub
- [x] Add `include/score/db/bar.hpp`, `column_store.hpp`, `column_store_v1.hpp`/`.cpp`.
- [x] Add `db/column_store_v1.cpp` to `SCORE_CORE_CPP_SOURCES` in `src/CMakeLists.txt`.
- [x] Implement basic `append()` method with file-based storage.
- [x] Add test coverage for the implementation.
- [ ] On-disk byte layout is a separate, later decision — not this phase.

## Phase 2 — Ingestion interface
- [x] Add `include/score/db/ingestion.hpp` (header-only `IngestionSource`).
- [ ] Note `src/score/market.py` / `scripts/gen_mock_data.py` as integration points (Python wiring: Phase 10).

## Phase 3 — Net: reactor/codec abstractions (interfaces only)
- [x] Add `include/score/net/event_loop.hpp`, `codec.hpp`.
- [x] Add `include/score/net/json_codec.hpp`/`.cpp`, `connection.hpp`/`.cpp`, `server.hpp`/`.cpp` (stub bodies).
- [x] Replace the stub bodies: `JsonFrameCodec` (4-byte BE length prefix + JSON, 16 MiB cap), `Connection` (POSIX send/recv, frame handler), `Server` (bind/listen/accept via `EventLoop`).
- [x] Add `tests/cpp/test_net.cpp` (codec round-trip/fragmentation/rejection, loopback echo via a fake `EventLoop`).

## Phase 4 — Net CMake wiring (platform guards)
- [x] Add the existing platform-independent `net/*.cpp` files to `src/CMakeLists.txt`.
- [ ] Guard `event_loop_epoll.cpp` with `if(CMAKE_SYSTEM_NAME STREQUAL "Linux")` when that source is added in Phase 6.
- [x] Confirm `score_core` builds on macOS and the C++ tests pass.
- [ ] Confirm the build on Linux using the existing CI matrix.

## Phase 5 — v1 concurrency: thread-per-connection
- [ ] Add `include/score/net/event_loop_blocking.hpp`/`.cpp` (`BlockingThreadEventLoop`); replace TODO stubs with real worker-thread logic.
- [ ] Add `tests/cpp/test_net_blocking.cpp` (loopback echo round-trip).

## Phase 6 — v2 concurrency: epoll event loop (Linux)
- [ ] Add `include/score/net/event_loop_epoll.hpp`/`.cpp` (`EpollEventLoop`, `#if defined(__linux__)`); real `epoll_ctl`/`epoll_wait` logic.
- [ ] Add `tests/cpp/test_net_epoll.cpp` (Linux-only); macOS CI keeps using `BlockingThreadEventLoop`.

## Phase 7 — v3 (stretch): io_uring/coroutines (Linux-only, optional)
- [ ] Add `include/score/net/event_loop_io_uring.hpp`/`.cpp` (`IoUringEventLoop`), liburing dependency TODO.
- [ ] Add `tests/cpp/test_net_io_uring.cpp` (skipped where liburing is unavailable).

## Phase 8 — Wire protocol: binary codec refactor
- [ ] Design the binary frame format; add `include/score/net/binary_codec.hpp`/`.cpp` (`BinaryFrameCodec`, implements `FrameCodec`).
- [ ] Swap the codec factory at `Server` call sites; verify zero `Server`/`Connection` changes.
- [ ] Add `tests/cpp/test_binary_codec.cpp` (round-trip + fragmentation).

## Phase 9 — Test coverage pass
- [x] `tests/cpp/test_db_column_store.cpp`: append/scan/symbols/count, overlapping-date rejection.
- [ ] `tests/cpp/test_ingestion.cpp`: fake `IngestionSource` against `ColumnStoreV1`.

## Phase 10 — nanobind exposure
- [ ] Bind `score::db::Bar`, `ColumnStore`/`ColumnStoreV1`, `IngestionSource`, `risk::historical_var`/`historical_cvar` in `src/bindings/nanobindings.cpp`.
- [ ] Wire `src/score/market.py` (yfinance) and `scripts/gen_mock_data.py` (mock) through `IngestionSource`.
- [ ] Decide the Python surface for `net::Server`/`Connection` (GIL implications of a background reactor thread).

## Phase 11 — gRPC facade (v2+, explicitly out of scope now)
- [ ] Evaluate gRPC as a facade over the `FrameCodec`/`Server` abstraction once Phase 8 is stable. Do not start before Phases 0-10 are done and reviewed.

## Review

- Phases 0–3 scaffolded in this pass: real `historical_var`/`historical_cvar` implementations
  (historical-simulation, sharing a private `historical_tail` sort+validate helper) added to
  `risk.hpp`/`risk.cpp`; `score::db` (`Bar`, `ColumnStore`, `ColumnStoreV1`, `IngestionSource`)
  and `score::net` (`EventLoop`, `FrameCodec`, `JsonFrameCodec`, `Connection`, `Server`) added
  as pure interface/stub scaffolding — concrete `.cpp` bodies `throw score::NotImplementedError`.
- `db/column_store_v1.cpp` and the existing platform-independent `net/*.cpp` files are wired into
  `src/CMakeLists.txt`. `score_core` builds on macOS and the C++ suite passes; the Linux-only
  `event_loop_epoll.cpp` guard will be added when that source is introduced in Phase 6.
- No tests or nanobind bindings added yet, per plan — those are later phases.

---

# Storage engine v1: from-scratch column store for daily bars (2026-11-14)

Supersedes the open storage items in Phases 1, 2, 9 and 10 above. Network Phases 5-8 and 11 are
**frozen** until every phase below is done and reviewed.

## Decisions (grilling sessions, 2026-11-14)

| # | Decision | Choice |
|---|----------|--------|
| 1 | Goal | Learn database internals; build the engine from scratch |
| 2 | First workload | Append-only daily OHLCV bars, date-range scans feeding returns/VaR/CVaR |
| 3 | Postgres | Correctness oracle and benchmark baseline only, stock config, no tuning track |
| 4 | On-disk layout | One fixed-width file per column per symbol |
| 5 | Crash safety | Committed row-count marker (temp file + rename); no write-ahead log |
| 6 | Query API | Keep `scan()`, add a zero-copy single-column scan; no operators, no SQL |
| 7 | Ordering | Storage end to end first as an embedded library; net parked |
| 8 | Concurrency | Multiple writers across processes, per-symbol `flock`; lock-free snapshot readers |
| 9 | Scale target | 5,000 symbols x 30 years, about 38M bars, about 1.7 GB uncompressed |
| 10 | Who codes | Stubs, hints and tests are provided; every storage function body is written by hand |
| 11 | Scope of stubs | All phases A-G stubbed up front |
| 12 | Test state | New tests are hidden with the Catch2 tag `[.todo]`; remove the tag one test at a time |
| 13 | Old `ColumnStoreV1` code | Row-file bodies replaced by stubs for the column-file format |
| 14 | Snippets | One section per phase in [tasks/code_snippets.md](code_snippets.md); techniques, not answers |
| 15 | Resources | Two or three checked links per phase, in the same file |
| 16 | Risk inputs | `log_returns`/`historical_var`/`historical_cvar` change to `std::span<const double>` in Phase C |
| 17 | Stub split | Four helpers (`date_codec`, `column_file`, `symbol_dir`, `column_view`) + `ColumnStoreV1` as glue |

Out of scope for v1: compression, tick/intraday data, updates and deletes, secondary indexes,
query operators, SQL, the network server.

## How to work through this

One step = one test. For each unchecked item below:

1. Read the matching section of [tasks/code_snippets.md](code_snippets.md) (once per phase, not per item).
2. In the named test file, delete `[.todo]` from the **next** `TEST_CASE` only.
3. Implement the function named in the item (its `TODO(xx)` comment lists the steps).
4. Build and run until that test passes, then tick the box.

```sh
cmake --build build -j                                   # incremental; `make build` wipes build/ and starts over
./build/tests/cpp/score_tests "[date]"                   # run the visible tests with one tag
./build/tests/cpp/score_tests "days_from_iso: the epoch and its neighbours"   # run one test by name, hidden or not
./build/tests/cpp/score_tests "[.todo]" --list-tests     # what is still hidden
cd build && ctest --output-on-failure                    # everything that is enabled
make test-tag TAG="[A][column_file]"                     # same filters through make (incremental, no wipe)
# `make test-cpp` depends on `build`, which wipes build/ first; prefer `make test-tag` while iterating.
```

A stub that has not been written throws `score::NotImplementedError`, so a test that reaches one
fails with `Not implemented: score::db::<function>`; that message names what to write next.

Provided complete, not for you to write: `UniqueFd` and the `ColumnView` move operations
([include/score/db/](../include/score/db/)), the test helpers in
[tests/cpp/db_test_util.hpp](../tests/cpp/db_test_util.hpp), and the `Bar`/`ColumnStoreV1` Python bindings.

## Phase A — File format (no crash safety, no store yet)

Reading: "Phase A" sections of the snippets file. Tests: `[A]`.

Run (incremental build, then filter; hidden `[.todo]` tests run too when selected by tag or name):

```sh
make test-tag TAG="[A]"        # whole phase
make test-tag TAG="[date]"       # A1-A2
make test-tag TAG="value_size"   # A3
make test-tag TAG="encode_header*"   # A4
make test-tag TAG="header:*"         # A5 round trip (needs A4)
make test-tag TAG="decode_header*"   # A5 corrupt headers
make test-tag TAG="open_column_file*"   # A8
make test-tag TAG="write_rows*"      # A6/A7 (also matches "write_rows / read_rows: ...")
make test-tag TAG="is_valid_symbol"  # A9
make test-tag TAG="[column_file]"    # all of test_db_column_file.cpp
```

- [x] Format spec written: [column_file.hpp](../include/score/db/column_file.hpp) (bytes),
      [symbol_dir.hpp](../include/score/db/symbol_dir.hpp) (directory), [column_store_v1.hpp](../include/score/db/column_store_v1.hpp) (guarantees).
- [x] A1 `days_from_iso` — [src/db/date_codec.cpp](../src/db/date_codec.cpp); tests in `test_db_date_codec.cpp` (do A1 and A2 together: 6 tests)
- [x] A2 `iso_from_days`
- [ ] A3 `value_size` — [src/db/column_file.cpp](../src/db/column_file.cpp); tests in `test_db_column_file.cpp` (13 tests, in file order)
- [ ] A4 `encode_header`
- [ ] A5 `decode_header`
- [ ] A8 `open_column_file` (the tests need it before A6/A7)
- [ ] A6 `write_rows`
- [ ] A7 `read_rows`
- [ ] A9 `is_valid_symbol`

## Phase B — Append with commit marker

Reading: "Phase B" section. Tests: `[B]`.

Run (incremental build, then filter; hidden `[.todo]` tests run too when selected by tag or name):

```sh
make test-tag TAG="[B]"        # whole phase
make test-tag TAG="[symbol_dir][B]"    # B1-B5 (test_db_symbol_dir.cpp)
make test-tag TAG="[columnstore][B]"   # B6-B9 (test_db_column_store.cpp)
```

- [ ] B1 `full_fsync`, B2 `fsync_directory` — [src/db/symbol_dir.cpp](../src/db/symbol_dir.cpp); first 7 tests of `test_db_symbol_dir.cpp`
- [ ] B3 `read_committed_count`
- [ ] B4 `commit_count`
- [ ] B5 `truncate_to_rows`
- [ ] B6 `ColumnStoreV1::symbol_dir` — [src/db/column_store_v1.cpp](../src/db/column_store_v1.cpp); `[B]` tests of `test_db_column_store.cpp` (12 tests)
- [ ] B7 `ColumnStoreV1::append` (leave the lock step out until Phase D)
- [ ] B9 `count`, B8 `symbols`
- [ ] Both `test_ingestion.cpp` tests pass (they need only append and count).
- [ ] By hand, once: kill a process in the middle of an append (`kill -9` from another terminal, or
      a temporary `std::abort()` between two column writes), reopen, and confirm the old row count.

## Phase C — Readers

Reading: "Phase C" section. Tests: `[C]`.

Run (incremental build, then filter; hidden `[.todo]` tests run too when selected by tag or name):

```sh
make test-tag TAG="[C]"        # whole phase
make test-tag TAG="[column_view]"      # C1-C5
make test-tag TAG="[columnstore][C]"   # C6-C8
```

- [ ] C1 `ColumnView::map`, C2 destructor, C3-C5 typed accessors — [src/db/column_view.cpp](../src/db/column_view.cpp); `test_db_column_view.cpp` (10 tests)
- [ ] C6 `find_rows`, C7 `scan` — `[C]` tests of `test_db_column_store.cpp` (8 tests)
- [ ] C8 `scan_column`
- [ ] C9 Change `log_returns`, `calculate_risk_from_returns`, `historical_var`, `historical_cvar` (and the
      private `historical_tail`) in [risk.hpp](../include/score/statistics/risk.hpp) / [risk.cpp](../src/statistics/risk.cpp)
      to take `std::span<const double>`. Callers passing a vector keep compiling. The nanobind
      bindings of those functions need a lambda taking `const std::vector<double>&` (nanobind has no
      `std::span` conversion). Then delete the copy marked `TODO(C9)` in `bench_db.cpp`. `test_risk.cpp` must stay green.

## Phase D — Multiple writers

Reading: "Phase D" section. Tests: `[D]`.

Run (incremental build, then filter; hidden `[.todo]` tests run too when selected by tag or name):

```sh
make test-tag TAG="[D]"        # whole phase
make test-tag TAG="[symbol_dir][D]"    # D1-D2
make test-tag TAG="[columnstore][D]"   # lock in append
```

- [ ] D1/D2 `SymbolLock` — [src/db/symbol_dir.cpp](../src/db/symbol_dir.cpp); last 2 tests of `test_db_symbol_dir.cpp`
- [ ] Add the lock to `ColumnStoreV1::append` (step 2 of its TODO) — `[D]` tests of `test_db_column_store.cpp` (3 tests).
      Run "two processes appending the same bars" before adding the lock as well, to watch it fail.

## Phase E — Ingestion and Python

Reading: "Phase E" section.

```sh
make test-tag TAG="[E]"        # C++ ingestion tests
make test-py                    # Python tests
```

- [x] `Bar` and `ColumnStoreV1` (`append`, `scan`, `symbols`, `count`) bound in [nanobindings.cpp](../src/bindings/nanobindings.cpp) and exported from `score`.
- [ ] E2 Bind `scan_column` as a read-only NumPy view; add a Python test (values, `flags.writeable`, array outliving the store object).
- [ ] Route [scripts/gen_mock_data.py](../scripts/gen_mock_data.py) and [src/score/market.py](../src/score/market.py) through the store.
- [ ] Add a size option to `scripts/gen_mock_data.py` for the 5,000 x 30-year data set (it must write to the store, not to one JSON file).

## Phase F — Postgres oracle and benchmark

Reading: "Phase F" section.

- [ ] Install Postgres and `psycopg[binary]`, set `SCORE_PG_DSN`, and get [tests/python/test_pg_oracle.py](../tests/python/test_pg_oracle.py) to pass.
      The test and the SQL in the snippets file were written without a server to run them on; expect to fix small things.
- [ ] Run [tests/cpp/bench_db.cpp](../tests/cpp/bench_db.cpp) at the default size, then at the design size.
- [ ] Time the same five questions in Postgres (`\timing`, `EXPLAIN (ANALYZE, BUFFERS)`), cold and warm.
- [ ] Record the numbers and what they say about the layout in the Review section below.

## Phase G — Leftovers from the old plan
- [x] `tests/cpp/test_risk.cpp` for `historical_var`/`historical_cvar` (9 tests, enabled, passing).
- [ ] Confirm the Linux build in CI. The storage code uses POSIX calls only; `F_FULLFSYNC` is guarded for macOS.

## Later (not planned yet)
Compression (delta dates, XOR floats), a cross-sectional layout for by-date queries, WAL, query
operators, then un-freeze the network phases.

## Review

**Scaffolding pass (2026-11-14).** Stubs, tests, snippets and reading list are in place; no storage
logic is written. `ctest` runs 31 enabled tests, all passing. 63 storage tests are hidden behind
`[.todo]`; run on their own, all 63 fail, 60 with `Not implemented: score::db::...` and 3 (the
fork-based ones) through a child process that hit a stub. No hidden test passes by accident.
Python: 23 passed, 1 skipped (`test_pg_oracle.py`, no `psycopg`). Not verified: anything involving
a real Postgres server, and the Linux build.

_Engine results: to be filled in after Phase F._
