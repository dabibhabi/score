"""Loader for the historical headline CSV and price/news alignment.

Mirrors :mod:`score.data` in spirit: small dataclass containers and pure
functions.  The only non-stdlib dependency is pandas (already an optional
dependency of the project, see the ``data`` extra), and — unlike
:mod:`score.data` — nothing here touches the native ``_core`` extension, so
the module is usable before the C++ side is built.

The raw CSV is the ``analyst_ratings_processed.csv`` file fetched by
``scripts/fetch_news_data.py``; the price bars are the JSON written by
``scripts/fetch_historical_prices.py``.

Lookahead bias
--------------
The dataset author notes that headline timestamps are not precise enough to
be trusted intraday.  Every headline is therefore treated as *known* only on
the next trading day strictly after its listed date, so a backtest can never
act on a headline before the market had a chance to see it.
"""

from __future__ import annotations

import json
from bisect import bisect_right
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Sequence

import pandas as pd

# ---------------------------------------------------------------------------
# Path resolution
# ---------------------------------------------------------------------------
_PACKAGE_ROOT = Path(__file__).resolve().parent
_REPO_ROOT = _PACKAGE_ROOT.parent.parent
DEFAULT_NEWS_PATH = _REPO_ROOT / "data" / "news" / "analyst_ratings_processed.csv"
DEFAULT_PRICES_PATH = _REPO_ROOT / "data" / "historical_prices.json"

DEFAULT_TICKERS = ("MRK", "GILD", "KO", "HD", "FDX")
"""Tickers with dense headline coverage across 2009-2020 in the Kaggle CSV.

The mock-data mega-caps (AAPL/GOOGL/MSFT/AMZN/TSLA) are nearly absent from
that file — MSFT has zero rows — so they are not the default here.
"""

#: Headline timestamps in the CSV carry US-market offsets; normalise to the
#: exchange's own timezone before taking a calendar date.
MARKET_TZ = "America/New_York"

#: Placeholder lexicon for :func:`score_headline`.  Deliberately crude — it
#: exists so the aligned dataset has a sentiment column to hang a real model
#: on later, not because it is a good sentiment model.
_POSITIVE_WORDS = frozenset(
    """
    upgrade upgrades upgraded beat beats raises raised outperform buy
    bullish surges surge soars soar jumps jump gains gain rises rise high
    highs record strong growth profit profits tops top positive overweight
    """.split()
)
_NEGATIVE_WORDS = frozenset(
    """
    downgrade downgrades downgraded miss misses missed cuts cut underperform
    sell bearish plunges plunge sinks sink falls fall drops drop lows low
    weak loss losses warns warn negative underweight halted probe lawsuit
    """.split()
)


# ---------------------------------------------------------------------------
# Lightweight containers
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class NewsData:
    """Result of :func:`load_news`.

    Attributes
    ----------
    headlines
        DataFrame with one row per headline and the columns ``ticker``,
        ``title``, ``published`` (the date listed in the CSV, as a
        ``datetime.date``) and ``sentiment`` (see :func:`score_headline`).
        Sorted by ticker then published date.
    tickers
        The tickers retained by the filter, in sorted order.
    n_dropped
        How many rows were dropped because their date could not be parsed.
    """

    headlines: pd.DataFrame
    tickers: list[str]
    n_dropped: int

    def __repr__(self) -> str:  # pragma: no cover - cosmetic
        return (
            f"<NewsData tickers={self.tickers!r} "
            f"n_headlines={len(self.headlines)} n_dropped={self.n_dropped}>"
        )


# ---------------------------------------------------------------------------
# Pure helpers
# ---------------------------------------------------------------------------
def score_headline(title: str) -> float:
    """Return a crude lexicon sentiment score in ``[-1, 1]`` for ``title``.

    This is a **placeholder** so downstream code has a numeric sentiment
    feature to work with; swap it for a real model later.

    Parameters
    ----------
    title
        The headline text.

    Returns
    -------
    float
        ``(pos - neg) / (pos + neg)`` over the matched lexicon words, or
        ``0.0`` when the headline matches nothing.
    """
    words = [w.strip(".,:;!?()'\"").lower() for w in str(title).split()]
    pos = sum(w in _POSITIVE_WORDS for w in words)
    neg = sum(w in _NEGATIVE_WORDS for w in words)
    if pos + neg == 0:
        return 0.0
    return (pos - neg) / (pos + neg)


def next_trading_day(day: Any, trading_days: Sequence[Any]) -> Any | None:
    """Return the first entry of ``trading_days`` strictly after ``day``.

    Parameters
    ----------
    day
        The headline's listed date.
    trading_days
        Ascending sequence of trading dates (same type as ``day``).

    Returns
    -------
    date or None
        ``None`` when ``day`` is on or after the last trading day, i.e. the
        headline could not have been acted on inside the sample.
    """
    idx = bisect_right(trading_days, day)
    if idx >= len(trading_days):
        return None
    return trading_days[idx]


# ---------------------------------------------------------------------------
# Loading
# ---------------------------------------------------------------------------
def load_price_bars(path: str | Path | None = None) -> dict[str, list[dict[str, Any]]]:
    """Load OHLCV bars from a ``mock_stocks.json``-schema file.

    Reads the JSON directly rather than going through
    :func:`score.data.load_mock_data`, so this module stays free of the
    native extension.

    Parameters
    ----------
    path
        Optional explicit path; defaults to ``data/historical_prices.json``.

    Returns
    -------
    dict
        ``{ticker: [bar, ...]}``, the same structure as ``MockData.bars``.
    """
    target = Path(path) if path is not None else DEFAULT_PRICES_PATH
    with target.open("r", encoding="utf-8") as fh:
        raw = json.load(fh)
    if "data" not in raw:
        raise KeyError(f"{target} is missing the required top-level key 'data'")
    return raw["data"]


def load_news(
    path: str | Path | None = None,
    tickers: Iterable[str] = DEFAULT_TICKERS,
    chunksize: int = 250_000,
) -> NewsData:
    """Load and filter the headline CSV.

    Rows whose ``date`` cannot be parsed are coerced to ``NaT`` and dropped;
    the count is reported in :attr:`NewsData.n_dropped`.  The file is read in
    chunks because the full CSV is ~150 MB while the filtered result is tiny.

    Parameters
    ----------
    path
        Optional explicit CSV path; defaults to
        ``data/news/analyst_ratings_processed.csv``.
    tickers
        Ticker symbols to keep.
    chunksize
        Rows per read chunk.

    Returns
    -------
    NewsData
        The filtered headlines.

    Raises
    ------
    FileNotFoundError
        If the CSV does not exist.
    """
    target = Path(path) if path is not None else DEFAULT_NEWS_PATH
    if not target.exists():
        raise FileNotFoundError(
            f"{target} not found; run scripts/fetch_news_data.py first"
        )

    wanted = set(tickers)
    kept: list[pd.DataFrame] = []
    n_dropped = 0

    reader = pd.read_csv(
        target,
        usecols=["title", "date", "stock"],
        dtype={"title": "string", "date": "string", "stock": "string"},
        chunksize=chunksize,
    )
    for chunk in reader:
        chunk = chunk[chunk["stock"].isin(wanted)]
        if chunk.empty:
            continue
        parsed = pd.to_datetime(chunk["date"], errors="coerce", utc=True, format="mixed")
        bad = parsed.isna()
        n_dropped += int(bad.sum())
        chunk = chunk.loc[~bad]
        parsed = parsed.loc[~bad].dt.tz_convert(MARKET_TZ)
        kept.append(
            pd.DataFrame(
                {
                    "ticker": chunk["stock"].astype("object"),
                    "title": chunk["title"].astype("object"),
                    "published": parsed.dt.date,
                }
            )
        )

    if kept:
        headlines = pd.concat(kept, ignore_index=True)
    else:
        headlines = pd.DataFrame(columns=["ticker", "title", "published"])

    headlines["sentiment"] = headlines["title"].map(score_headline)
    headlines = headlines.sort_values(["ticker", "published"], ignore_index=True)

    return NewsData(
        headlines=headlines,
        tickers=sorted(headlines["ticker"].unique().tolist()),
        n_dropped=n_dropped,
    )


# ---------------------------------------------------------------------------
# Alignment
# ---------------------------------------------------------------------------
def daily_news_features(
    news: NewsData,
    ticker: str,
    trading_days: Sequence[Any],
) -> pd.DataFrame:
    """Aggregate a ticker's headlines onto the trading days they are known.

    Each headline is attributed to :func:`next_trading_day` of its listed
    date, so no trading day sees a headline dated that same day or later.

    Parameters
    ----------
    news
        A :class:`NewsData` from :func:`load_news`.
    ticker
        The ticker to aggregate.
    trading_days
        Ascending sequence of the ticker's trading dates.

    Returns
    -------
    pandas.DataFrame
        Columns ``date``, ``news_count``, ``news_sentiment`` (mean of the
        day's headline scores), one row per day that has at least one
        headline.
    """
    rows = news.headlines[news.headlines["ticker"] == ticker]
    if rows.empty:
        return pd.DataFrame(columns=["date", "news_count", "news_sentiment"])

    known = rows["published"].map(lambda d: next_trading_day(d, trading_days))
    rows = rows.assign(date=known).dropna(subset=["date"])

    grouped = (
        rows.groupby("date", sort=True)
        .agg(news_count=("title", "size"), news_sentiment=("sentiment", "mean"))
        .reset_index()
    )
    return grouped


def align_news_to_prices(
    bars: dict[str, list[dict[str, Any]]],
    news: NewsData,
    tickers: Iterable[str] | None = None,
) -> pd.DataFrame:
    """Join daily news features onto price bars.

    Parameters
    ----------
    bars
        ``{ticker: [bar, ...]}`` as returned by :func:`load_price_bars` (or
        ``score.data.MockData.bars``).
    news
        A :class:`NewsData` from :func:`load_news`.
    tickers
        Which tickers to align; defaults to the intersection of ``bars`` and
        the tickers present in ``news``.

    Returns
    -------
    pandas.DataFrame
        One row per ``(ticker, trading day)``, sorted by ticker then date,
        with the columns ``ticker``, ``date``, ``open``, ``high``, ``low``,
        ``close``, ``volume``, ``news_count`` (0 on quiet days) and
        ``news_sentiment`` (0.0 on quiet days).
    """
    if tickers is None:
        tickers = [t for t in bars if t in set(news.tickers)]

    frames: list[pd.DataFrame] = []
    for ticker in tickers:
        if ticker not in bars:
            raise KeyError(
                f"ticker {ticker!r} not in bars; available: {list(bars)!r}"
            )
        prices = pd.DataFrame(bars[ticker])
        if prices.empty:
            continue
        prices["date"] = pd.to_datetime(prices["date"]).dt.date
        prices = prices.sort_values("date", ignore_index=True)

        features = daily_news_features(news, ticker, prices["date"].tolist())
        merged = prices.merge(features, on="date", how="left")
        merged["news_count"] = merged["news_count"].fillna(0).astype(int)
        merged["news_sentiment"] = merged["news_sentiment"].fillna(0.0).astype(float)
        merged.insert(0, "ticker", ticker)
        frames.append(merged)

    if not frames:
        return pd.DataFrame(
            columns=[
                "ticker", "date", "open", "high", "low", "close", "volume",
                "news_count", "news_sentiment",
            ]
        )

    out = pd.concat(frames, ignore_index=True)
    return out.sort_values(["ticker", "date"], ignore_index=True)


def build_aligned_dataset(
    news_path: str | Path | None = None,
    prices_path: str | Path | None = None,
    tickers: Iterable[str] = DEFAULT_TICKERS,
) -> pd.DataFrame:
    """Load both sources and return the aligned ``(ticker, day)`` table.

    Convenience wrapper over :func:`load_news`, :func:`load_price_bars` and
    :func:`align_news_to_prices`.

    Parameters
    ----------
    news_path
        Optional explicit headline CSV path.
    prices_path
        Optional explicit price JSON path.
    tickers
        Ticker symbols to include.

    Returns
    -------
    pandas.DataFrame
        See :func:`align_news_to_prices`.
    """
    news = load_news(news_path, tickers)
    bars = load_price_bars(prices_path)
    wanted = [t for t in tickers if t in bars]
    return align_news_to_prices(bars, news, wanted)
