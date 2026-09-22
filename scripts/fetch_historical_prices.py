#!/usr/bin/env python3
"""Download real daily OHLCV bars into ``data/historical_prices.json``.

The output uses the same schema as ``data/mock_stocks.json`` (a top-level
``metadata`` block plus a ``data`` dict keyed by ticker), so the result is a
drop-in replacement for :func:`score.data.load_mock_data`.

The default date range matches the coverage of the Kaggle news CSV fetched by
``scripts/fetch_news_data.py`` (2009-01-01 -> 2020-06-11), so prices and
headlines can be aligned by :mod:`score.news`.  The default tickers are chosen
for headline coverage in that CSV (the mega-caps in ``data/mock_stocks.json``
are barely present in it: MSFT has no rows at all, AAPL only mid-2020).

Run::

    python scripts/fetch_historical_prices.py
"""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path

TICKERS = ("MRK", "GILD", "KO", "HD", "FDX")
START_DATE = "2009-01-01"
END_DATE = "2020-06-11"

OUT_PATH = Path(__file__).resolve().parents[1] / "data" / "historical_prices.json"


def _bars_for_frame(frame) -> list[dict[str, object]]:
    """Convert a single-ticker yfinance frame into mock-schema bars.

    Parameters
    ----------
    frame
        A :class:`pandas.DataFrame` indexed by date with the columns
        ``Open``/``High``/``Low``/``Close``/``Volume``.

    Returns
    -------
    list[dict]
        One dict per trading day, in chronological order, with the keys
        date/open/high/low/close/volume.  Rows with a missing close are
        dropped (yfinance pads a ticker's frame with NaNs for days it did
        not trade, e.g. TSLA before its 2010 IPO).
    """
    frame = frame.dropna(subset=["Close"]).sort_index()
    bars: list[dict[str, object]] = []
    for ts, row in frame.iterrows():
        bars.append(
            {
                "date": ts.strftime("%Y-%m-%d"),
                "open": round(float(row["Open"]), 4),
                "high": round(float(row["High"]), 4),
                "low": round(float(row["Low"]), 4),
                "close": round(float(row["Close"]), 4),
                "volume": int(row["Volume"]),
            }
        )
    return bars


def fetch(
    tickers: tuple[str, ...] = TICKERS,
    start: str = START_DATE,
    end: str = END_DATE,
) -> dict[str, object]:
    """Download daily bars and return the full mock-schema payload.

    Parameters
    ----------
    tickers
        Ticker symbols to download.
    start
        First date to include (inclusive, ``YYYY-MM-DD``).
    end
        Last date to include (inclusive, ``YYYY-MM-DD``).

    Returns
    -------
    dict
        ``{"metadata": {...}, "data": {ticker: [bar, ...]}}``.
    """
    import yfinance as yf

    # yfinance treats ``end`` as exclusive; bump it by a day to keep ``end``
    # inclusive, which is what the news CSV's last date implies.
    end_exclusive = (
        datetime.strptime(end, "%Y-%m-%d").toordinal() + 1
    )
    end_exclusive = datetime.fromordinal(end_exclusive).strftime("%Y-%m-%d")

    raw = yf.download(
        list(tickers),
        start=start,
        end=end_exclusive,
        interval="1d",
        auto_adjust=False,
        group_by="ticker",
        progress=False,
        threads=True,
    )
    if raw is None or raw.empty:
        raise SystemExit("yfinance returned no data (network or symbol issue)")

    bars = {ticker: _bars_for_frame(raw[ticker]) for ticker in tickers}
    n_periods = max((len(v) for v in bars.values()), default=0)

    metadata = {
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "frequency": "daily",
        "n_periods": n_periods,
        "tickers": list(tickers),
        "start_date": start,
        "end_date": end,
        "source": "yfinance",
        "model": "historical",
    }
    return {"metadata": metadata, "data": bars}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--start", default=START_DATE)
    parser.add_argument("--end", default=END_DATE)
    parser.add_argument("--out", type=Path, default=OUT_PATH)
    parser.add_argument(
        "--tickers",
        nargs="+",
        default=list(TICKERS),
        help="Ticker symbols to download.",
    )
    args = parser.parse_args()

    payload = fetch(tuple(args.tickers), args.start, args.end)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", encoding="utf-8") as fh:
        json.dump(payload, fh, indent=2)

    counts = ", ".join(
        f"{t}={len(b)}" for t, b in payload["data"].items()
    )
    print(f"Wrote {args.out} ({counts})")


if __name__ == "__main__":
    main()
