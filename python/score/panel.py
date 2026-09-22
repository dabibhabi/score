"""Loaders for the cross-sectional price/factor/market panel.

This file is INTENTIONALLY scaffolded with stubs, in the same spirit as
:mod:`score.analysis`.  Each function has a complete signature and
docstring; the bodies raise :class:`NotImplementedError` until they are
filled in.  See ``tasks/risk_engine_todo.md`` for the order of work and
for reading material on the pieces involved.

Scope: this module is pure data assembly.  It deliberately does **not**
import :mod:`score._core`, so it stays usable before the native
extension is built, exactly like :mod:`score.news`.  Every number that
counts as *math* belongs in C++ and is reached through
:mod:`score.risk`.

Sources
-------
Kaggle ``andrewmvd/sp-500-stocks``
    Long-format daily OHLCV for the S&P 500, plus sector metadata.
Kaggle ``nikitamanaenkov/famafrench-factors-and-portfolios``
    Daily Fama-French 5 factors and the daily risk-free rate.
yfinance
    ``^VIX``, ``^GSPC``, ``^IRX`` for market context, and gap-filling.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Sequence

import pandas as pd

# ---------------------------------------------------------------------------
# Path resolution
# ---------------------------------------------------------------------------
_PACKAGE_ROOT = Path(__file__).resolve().parent
_REPO_ROOT = _PACKAGE_ROOT.parent.parent

DEFAULT_SP500_DIR = _REPO_ROOT / "data" / "kaggle" / "sp500"
DEFAULT_FACTORS_DIR = _REPO_ROOT / "data" / "kaggle" / "factors"
DEFAULT_MARKET_PATH = _REPO_ROOT / "data" / "market" / "benchmarks.parquet"
DEFAULT_PANEL_DIR = _REPO_ROOT / "data" / "panel"

#: Columns of the assembled price panel, in order.
PRICE_COLUMNS = (
    "ticker",
    "date",
    "open",
    "high",
    "low",
    "close",
    "adj_close",
    "volume",
    "log_return",
    "source",
)

#: Fama-French factor columns, after parsing to decimal fractions.
FACTOR_COLUMNS = ("mkt_rf", "smb", "hml", "rmw", "cma", "mom", "rf")

#: Sentinel values the Fama-French files use for missing observations.
FF_MISSING_SENTINELS = (-99.99, -999.0)


# ---------------------------------------------------------------------------
# Containers
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class PanelSources:
    """Resolved on-disk locations of the raw inputs.

    Attributes
    ----------
    sp500_prices
        Path to ``sp500_stocks.csv``.
    sp500_metadata
        Path to ``sp500_companies.csv``.
    factors
        Path to the daily Fama-French 5-factor CSV.
    momentum
        Path to the daily momentum factor CSV, when present.
    market
        Path to the yfinance benchmark parquet.
    """

    sp500_prices: Path
    sp500_metadata: Path
    factors: Path
    momentum: Path | None
    market: Path


@dataclass(frozen=True)
class PanelManifest:
    """Provenance record written alongside a built panel.

    Tracked in git even though the panel itself is not, so a result can
    be traced back to the snapshot that produced it.

    Attributes
    ----------
    built_at
        UTC timestamp of the build.
    row_counts
        Rows per output table.
    date_range
        ``(first, last)`` trading day covered.
    tickers
        Tickers that met the eligibility rule.
    source_digests
        Hash per raw input file, so a silently-updated Kaggle snapshot is
        detectable.
    """

    built_at: str
    row_counts: dict[str, int]
    date_range: tuple[str, str]
    tickers: list[str]
    source_digests: dict[str, str]


# ---------------------------------------------------------------------------
# Raw loaders
# ---------------------------------------------------------------------------
def load_sp500_prices(
    path: str | Path | None = None,
    tickers: Iterable[str] | None = None,
    start: str | None = None,
    end: str | None = None,
) -> pd.DataFrame:
    """Load the long-format S&P 500 OHLCV CSV.

    The file is ~96 MB and ~1.9 M rows, so it is read in chunks and
    filtered during the read rather than after it.

    Parameters
    ----------
    path
        Optional explicit path; defaults to
        ``data/kaggle/sp500/sp500_stocks.csv``.
    tickers
        Restrict to these symbols; ``None`` keeps all.
    start, end
        Inclusive ``YYYY-MM-DD`` date bounds.

    Returns
    -------
    pandas.DataFrame
        Columns ``ticker``, ``date``, ``open``, ``high``, ``low``,
        ``close``, ``adj_close``, ``volume``.  Rows with a null
        ``adj_close`` (pre-listing padding) are dropped rather than
        filled.
    """
    raise NotImplementedError("load_sp500_prices: chunked read + filter of the Kaggle CSV")


def load_sp500_metadata(path: str | Path | None = None) -> pd.DataFrame:
    """Load per-ticker sector and market-cap metadata.

    Note this is a *current* membership snapshot, not point-in-time; see
    the survivorship caveat in ``tasks/risk_engine_todo.md``.

    Parameters
    ----------
    path
        Optional explicit path; defaults to
        ``data/kaggle/sp500/sp500_companies.csv``.

    Returns
    -------
    pandas.DataFrame
        Indexed by ticker, with at least ``sector`` and ``market_cap``.
    """
    raise NotImplementedError("load_sp500_metadata: read the companies CSV")


def load_ff_factors(
    path: str | Path | None = None,
    momentum_path: str | Path | None = None,
) -> pd.DataFrame:
    """Load and normalize the daily Fama-French factors.

    The upstream files carry a copyright preamble before the header,
    encode dates as ``YYYYMMDD`` integers, express values in **percent**,
    and mark missing observations with the sentinels in
    :data:`FF_MISSING_SENTINELS`.  The Kaggle repackage may or may not
    have cleaned the preamble, so locate the first data row rather than
    assuming a fixed ``skiprows``.

    Parameters
    ----------
    path
        Optional explicit path to the 5-factor daily CSV.
    momentum_path
        Optional explicit path to the momentum daily CSV; when omitted
        the momentum column is absent from the result.

    Returns
    -------
    pandas.DataFrame
        Indexed by ``date``, with the :data:`FACTOR_COLUMNS` present in
        the input, as decimal fractions (0.01 == 1%).
    """
    raise NotImplementedError(
        "load_ff_factors: locate the first YYYYMMDD row, mask sentinels, then rescale"
    )


def load_market_context(path: str | Path | None = None) -> pd.DataFrame:
    """Load the yfinance benchmark series.

    Parameters
    ----------
    path
        Optional explicit path; defaults to
        ``data/market/benchmarks.parquet``.

    Returns
    -------
    pandas.DataFrame
        Indexed by ``date``, with one column per benchmark close
        (``vix``, ``gspc``, ``irx``, ``spy``).
    """
    raise NotImplementedError("load_market_context: read the benchmark parquet")


# ---------------------------------------------------------------------------
# Assembly
# ---------------------------------------------------------------------------
def build_trading_calendar(
    market: pd.DataFrame,
    factors: pd.DataFrame,
) -> pd.DatetimeIndex:
    """Derive the master trading calendar.

    The index benchmark defines which days are trading days; the factor
    file must also cover a day for it to be usable by the regressions.

    Parameters
    ----------
    market
        Output of :func:`load_market_context`.
    factors
        Output of :func:`load_ff_factors`.

    Returns
    -------
    pandas.DatetimeIndex
        Ascending, unique trading days.
    """
    raise NotImplementedError("build_trading_calendar: intersect the source calendars")


def build_prices_panel(
    prices: pd.DataFrame,
    calendar: pd.DatetimeIndex,
    min_observations: int = 252,
) -> pd.DataFrame:
    """Reindex prices onto the master calendar and derive returns.

    A ticker missing a day the calendar has is a genuine halt or missing
    datum: leave it null, do not forward-fill.  Returns are computed
    **within** a source and never across a source boundary, because
    adjusted prices from different vintages disagree on every pre-split
    price.

    Parameters
    ----------
    prices
        Output of :func:`load_sp500_prices`, possibly concatenated with
        a yfinance top-up carrying a different ``source`` value.
    calendar
        Output of :func:`build_trading_calendar`.
    min_observations
        A ticker becomes eligible on day *t* only once it has this many
        prior non-null observations.

    Returns
    -------
    pandas.DataFrame
        One row per eligible ``(ticker, trading day)``, columns
        :data:`PRICE_COLUMNS`.
    """
    raise NotImplementedError("build_prices_panel: reindex, derive log returns per source")


def attach_news_features(
    panel: pd.DataFrame,
    news: Any,
) -> pd.DataFrame:
    """Left-join the daily news features onto the price panel.

    Reuses :func:`score.news.daily_news_features`, which already
    attributes each headline to the next trading day after its listed
    date.  Join on that attributed ``date``, never on ``published``, or
    the lookahead control is undone.

    Coverage is uneven (some large caps have no headlines at all), so
    the result must carry a coverage flag rather than silently encoding
    "no news" as a neutral value.

    Parameters
    ----------
    panel
        Output of :func:`build_prices_panel`.
    news
        A :class:`score.news.NewsData`.

    Returns
    -------
    pandas.DataFrame
        ``panel`` plus ``news_count``, ``news_sentiment`` and
        ``has_news_coverage``.
    """
    raise NotImplementedError("attach_news_features: per-ticker merge on the attributed date")


def validate_prices(panel: pd.DataFrame, max_abs_return: float = 0.5) -> pd.DataFrame:
    """Report data-quality problems in an assembled price panel.

    Kaggle repackages are not always clean.  Check at least: duplicate
    ``(ticker, date)`` rows, non-positive prices, and daily moves beyond
    ``max_abs_return`` that no split explains.

    Parameters
    ----------
    panel
        Output of :func:`build_prices_panel`.
    max_abs_return
        Threshold above which a daily move is flagged for inspection.

    Returns
    -------
    pandas.DataFrame
        One row per suspect observation, with a ``reason`` column.
        Empty means the panel passed.
    """
    raise NotImplementedError("validate_prices: duplicate/sign/outlier checks")


def write_panel(
    frames: dict[str, pd.DataFrame],
    out_dir: str | Path | None = None,
    sources: PanelSources | None = None,
) -> PanelManifest:
    """Write the panel tables to Parquet and record a manifest.

    Parameters
    ----------
    frames
        Mapping of table name to frame, e.g. ``{"prices": ...}``.
    out_dir
        Optional explicit output directory; defaults to ``data/panel/``.
    sources
        Raw inputs to digest into the manifest.

    Returns
    -------
    PanelManifest
        The manifest that was written next to the tables.
    """
    raise NotImplementedError("write_panel: parquet write + manifest")


def read_panel(
    name: str = "prices",
    out_dir: str | Path | None = None,
    columns: Sequence[str] | None = None,
) -> pd.DataFrame:
    """Read one panel table back from Parquet.

    Parameters
    ----------
    name
        Table name, e.g. ``"prices"`` or ``"features"``.
    out_dir
        Optional explicit directory; defaults to ``data/panel/``.
    columns
        Optional column projection, pushed down to the reader.

    Returns
    -------
    pandas.DataFrame
        The requested table.
    """
    raise NotImplementedError("read_panel: parquet read with column pushdown")
