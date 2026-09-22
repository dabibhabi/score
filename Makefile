# Convenience wrappers around CMake / pip / pytest.  All real build logic
# lives in CMakeLists.txt, pyproject.toml, and the scripts under scripts/.

BUILD_DIR       ?= build
DEBUG_BUILD_DIR ?= build-debug
JOBS      ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc)

UV ?= uv

# Prefer the project .venv (created by `make dev` / `uv venv`) so CMake finds
# pip-installed nanobind.
ifeq ($(wildcard .venv/bin/python),)
  PYTHON ?= python3
else
  PYTHON ?= .venv/bin/python
endif
VENV_PYTHON := $(CURDIR)/.venv/bin/python

.PHONY: help build build-debug sync-extension configure clean rebuild submodules venv dev test test-cpp test-py mock-data format

help:
	@echo "Score build targets:"
	@echo "  make submodules   - git submodule update --init --recursive"
	@echo "  make venv         - create .venv with uv (no-op if it already exists)"
	@echo "  make dev          - uv pip install -e .[dev] (creates .venv if needed)"
	@echo "  make build        - incremental build (configures first if needed)"
	@echo "  make build-debug  - incremental build with -DCMAKE_BUILD_TYPE=Debug"
	@echo "  make rebuild      - clean, reconfigure, and build from scratch"
	@echo "  make test-cpp     - run C++ tests via ctest"
	@echo "  make test-py      - run Python tests via pytest"
	@echo "  make test         - run both test suites"
	@echo "  make mock-data    - regenerate data/mock_stocks.json"
	@echo "  make format       - clang-format C/C++ sources"
	@echo "  make clean        - remove build directory"

submodules:
	git submodule update --init --recursive

# Configure only.  Deliberately does NOT depend on `clean`: a from-scratch
# build is `make rebuild`.  Re-running cmake on an existing build dir is
# cheap, while wiping it re-fetches and rebuilds Catch2 every time.
configure: submodules dev
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release \
		-DPython3_EXECUTABLE=$(VENV_PYTHON)

build:
	@test -f $(BUILD_DIR)/CMakeCache.txt || $(MAKE) configure
	cmake --build $(BUILD_DIR) -j$(JOBS)
	@$(MAKE) --no-print-directory sync-extension

# The scikit-build editable install keeps its OWN copy of the compiled
# extension under site-packages, and that copy is what `import score`
# actually loads -- `python/score/*.so` is only used when the editable
# install is absent.  Without this step a freshly built .so is silently
# ignored and the tests run against whatever `make dev` last installed.
sync-extension:
	@for dest in $(wildcard .venv/lib/python*/site-packages/score); do \
		for so in python/score/_score_native*.so python/score/_score_native*.dylib \
		          $(BUILD_DIR)/src/libscore_core.so* $(BUILD_DIR)/src/libscore_core*.dylib; do \
			test -e "$$so" && cp -Pf "$$so" "$$dest/"; \
		done; \
		echo "synced extension + core library -> $$dest"; \
	done; true

# Debug build in its own directory, so it never fights the Release cache.
# Optimized + LTO builds make the numerical code (Nelder-Mead, GARCH) very
# hard to step through.
build-debug: submodules dev
	cmake -S . -B $(DEBUG_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug \
		-DPython3_EXECUTABLE=$(VENV_PYTHON)
	cmake --build $(DEBUG_BUILD_DIR) -j$(JOBS)

rebuild: clean configure build

clean:
	rm -rf $(BUILD_DIR) $(DEBUG_BUILD_DIR)
	rm -f python/score/_score_native.*

venv:
	@test -d .venv || $(UV) venv

dev: venv
	$(UV) pip install -e ".[dev]"

test-cpp: build
	cd $(BUILD_DIR) && ctest --output-on-failure

test-py:
	$(PYTHON) -m pytest tests/python -v

test: test-cpp test-py

mock-data:
	$(PYTHON) scripts/gen_mock_data.py

format:
	@command -v clang-format >/dev/null 2>&1 || { echo "clang-format not found"; exit 1; }
	find include src tests/cpp -type f \( -name '*.hpp' -o -name '*.cpp' -o -name '*.h' -o -name '*.c' \) \
		-print0 | xargs -0 clang-format -i
