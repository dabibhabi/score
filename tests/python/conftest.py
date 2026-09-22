"""Shared pytest fixtures for the Python test-suite.

Modules that import the compiled ``_score_native`` extension are skipped
wholesale on a checkout where it has not been built, so the pure-Python
layers (``score.data``'s JSON schema, ``score.news``, ``score.panel``)
stay testable without a C++ toolchain.  ``collect_ignore`` is used rather
than a module-level ``importorskip`` because the latter surfaces as a
collection *error*, not a skip.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path

import pytest

#: True when the nanobind extension has been built (``make build``).
HAS_NATIVE_EXTENSION = importlib.util.find_spec("score._score_native") is not None

#: Test modules that import the native extension at module scope.
_NATIVE_ONLY_MODULES = [
    "test_bindings.py",
    "test_data.py",
]

collect_ignore = [] if HAS_NATIVE_EXTENSION else list(_NATIVE_ONLY_MODULES)


@pytest.fixture(scope="session")
def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


@pytest.fixture(scope="session")
def data_path(repo_root: Path) -> Path:
    return repo_root / "data" / "mock_stocks.json"


@pytest.fixture()
def small_floats() -> list[float]:
    return [1.0, 2.0, 3.0, 4.0, 5.0]


@pytest.fixture()
def known_prices() -> list[float]:
    """A short, deterministic price path used across several tests."""
    return [100.0, 102.5, 105.0, 107.5, 110.0]
