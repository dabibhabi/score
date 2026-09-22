#!/usr/bin/env python3
"""Download the historical stock-news dataset (Kaggle) into ``data/news/``.

Dataset: "Daily Financial News for 6000+ Stocks"
  https://www.kaggle.com/datasets/miguelaenlle/massive-stock-news-analysis-db-for-nlpbacktests

Only ``analyst_ratings_processed.csv`` is pulled (headline/date/ticker),
since the other files in the dataset aren't needed for the sentiment
pipeline and are much larger.

Requires a Kaggle API token, set via ``KAGGLE_API_TOKEN`` in the
environment or in a ``.env`` file at the repo root (never commit ``.env``).

Run::

    python scripts/fetch_news_data.py
"""

from __future__ import annotations

import os
from pathlib import Path

from dotenv import load_dotenv

DATASET = "miguelaenlle/massive-stock-news-analysis-db-for-nlpbacktests"
FILE_NAME = "analyst_ratings_processed.csv"
OUT_DIR = Path(__file__).resolve().parents[1] / "data" / "news"


def main() -> None:
    load_dotenv()  # picks up KAGGLE_API_TOKEN from repo-root .env, if present
    if not os.environ.get("KAGGLE_API_TOKEN"):
        raise SystemExit(
            "KAGGLE_API_TOKEN is not set (export it or put it in a .env file)"
        )

    # Imported lazily: kaggle reads its auth env vars at import time.
    from kaggle.api.kaggle_api_extended import KaggleApi

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    api = KaggleApi()
    api.authenticate()
    api.dataset_download_file(DATASET, FILE_NAME, path=str(OUT_DIR))

    # The API zips single-file downloads; unzip if needed.
    zip_path = OUT_DIR / f"{FILE_NAME}.zip"
    if zip_path.exists():
        import zipfile

        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(OUT_DIR)
        zip_path.unlink()

    print(f"Downloaded {FILE_NAME} -> {OUT_DIR / FILE_NAME}")


if __name__ == "__main__":
    main()
