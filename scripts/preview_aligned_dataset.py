#!/usr/bin/env python3
"""Build the aligned price+news table and print/save a small preview.

Sanity check before any model is added.  The full aligned table is only held
in memory; just a small per-ticker preview (``data/aligned_preview_<TICKER>.csv``)
is written to disk, to keep the repo light.

Run::

    python scripts/preview_aligned_dataset.py [--ticker AAPL] [--rows 10]
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))

from score.news import align_news_to_prices, load_news, load_price_bars  # noqa: E402

OUT_DIR = Path(__file__).resolve().parents[1] / "data"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ticker", default="AAPL")
    parser.add_argument("--rows", type=int, default=10)
    args = parser.parse_args()

    news = load_news()
    bars = load_price_bars()
    aligned = align_news_to_prices(bars, news, [t for t in bars])

    print(f"headlines kept: {len(news.headlines)} (dropped {news.n_dropped} "
          f"rows with unparseable dates)")
    print(f"aligned rows: {len(aligned)}")
    print(aligned.groupby("ticker").agg(
        days=("date", "size"),
        first=("date", "min"),
        last=("date", "max"),
        headlines=("news_count", "sum"),
        days_with_news=("news_count", lambda s: int((s > 0).sum())),
    ))

    subset = aligned[aligned["ticker"] == args.ticker]
    print(f"\nfirst {args.rows} aligned rows for {args.ticker}:")
    print(subset.head(args.rows).to_string(index=False))

    print(f"\nfirst {args.rows} rows for {args.ticker} that have news:")
    print(subset[subset["news_count"] > 0].head(args.rows).to_string(index=False))

    out = OUT_DIR / f"aligned_preview_{args.ticker}.csv"
    subset.head(args.rows).to_csv(out, index=False)
    print(f"\nWrote {out}")


if __name__ == "__main__":
    main()
