"""Score: precise stock-market metrics in C/C++ with a Python interface.

The native heavy lifting lives in the compiled extension ``_score_native``
(built from ``src/bindings/nanobindings.cpp``).  This package re-exports the
key types so consumers can write::

    from score import Series, DescriptiveStats, StockAnalyzer

without ever touching the underscore-prefixed module directly.
"""

from __future__ import annotations

from ._core import (
    DescriptiveStats,
    DimensionMismatchError,
    DomainError,
    EmptySeriesError,
    NativeNotImplementedError,
    Series,
    StockAnalyzer,
    normal_cdf,
    normal_cdf_batch,
)

__all__ = [
    "Series",
    "DescriptiveStats",
    "StockAnalyzer",
    "normal_cdf",
    "normal_cdf_batch",
    "EmptySeriesError",
    "DimensionMismatchError",
    "DomainError",
    "NativeNotImplementedError",
    "__version__",
]

__version__ = "0.1.0"
