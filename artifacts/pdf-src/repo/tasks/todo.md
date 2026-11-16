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
- [ ] On-disk byte layout is a separate, later decision — not this phase.

## Phase 2 — Ingestion interface
- [x] Add `include/score/db/ingestion.hpp` (header-only `IngestionSource`).
- [ ] Note `src/score/market.py` / `scripts/gen_mock_data.py` as integration points (Python wiring: Phase 10).

## Phase 3 — Net: reactor/codec abstractions (interfaces only)
- [x] Add `include/score/net/event_loop.hpp`, `codec.hpp`.
- [x] Add `include/score/net/json_codec.hpp`/`.cpp`, `connection.hpp`/`.cpp`, `server.hpp`/`.cpp` (stub bodies).

## Phase 4 — Net CMake wiring (platform guards)
- [ ] Add `net/*.cpp` to `src/CMakeLists.txt`; guard `event_loop_epoll.cpp` with `if(CMAKE_SYSTEM_NAME STREQUAL "Linux")`.
- [ ] Confirm `score_core` builds clean on macOS (dev machine) and Linux (CI).

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
- [ ] `tests/cpp/test_db_column_store.cpp`: append/scan/symbols/count, overlapping-date rejection.
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
- `db/column_store_v1.cpp` wired into `src/CMakeLists.txt` now (platform-independent). `net/*.cpp`
  intentionally left unwired — Phase 4 adds it with the Linux-only `epoll` guard so that's a
  reviewed, isolated CMake change rather than folded into this scaffolding pass.
- No tests or nanobind bindings added yet, per plan — those are later phases.
