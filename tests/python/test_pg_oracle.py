"""Phase F: Postgres as the correctness oracle for ColumnStoreV1.

The same bars go into both stores; every date-range query must return the
same rows from each. Postgres is assumed correct, so any difference is a bug
in the column store.

Skipped unless both hold:
  * ``psycopg`` is installed            (``uv pip install "psycopg[binary]"``)
  * ``SCORE_PG_DSN`` names a database   (``export SCORE_PG_DSN=postgresql://localhost/score_test``)

The test creates and drops its own table, ``score_oracle_bars``; it touches
nothing else in that database. It fails with ``NativeNotImplementedError``
until Phases A-C of the storage engine are implemented.
"""

from __future__ import annotations

from datetime import date, timedelta
import os
import random

import pytest

psycopg = pytest.importorskip("psycopg")

DSN = os.environ.get("SCORE_PG_DSN")
pytestmark = pytest.mark.skipif(not DSN, reason="SCORE_PG_DSN is not set")

SYMBOLS = ["AAPL", "MSFT", "BRK.B", "GOOG", "AMZN"]
N_DAYS = 1500
FIRST_DAY = date(2015, 1, 1)
N_QUERIES = 200

Row = tuple[str, float, float, float, float, int]


def _history(seed: int) -> list[Row]:
    """A deterministic series with gaps, so range ends often fall between stored dates."""
    rng = random.Random(seed)
    rows: list[Row] = []
    price = 100.0 + seed
    for offset in range(N_DAYS):
        day = FIRST_DAY + timedelta(days=offset)
        if day.weekday() >= 5:
            continue
        price *= 1.0 + rng.uniform(-0.03, 0.03)
        rows.append((day.isoformat(), price, price * 1.01, price * 0.99, price, rng.randrange(1, 10**9)))
    return rows


@pytest.fixture()
def pg():
    with psycopg.connect(DSN, autocommit=True) as conn:
        conn.execute("DROP TABLE IF EXISTS score_oracle_bars")
        conn.execute(
            """
            CREATE TABLE score_oracle_bars (
                symbol text   NOT NULL,
                date   date   NOT NULL,
                open   float8 NOT NULL,
                high   float8 NOT NULL,
                low    float8 NOT NULL,
                close  float8 NOT NULL,
                volume bigint NOT NULL,
                PRIMARY KEY (symbol, date)
            )
            """
        )
        yield conn
        conn.execute("DROP TABLE IF EXISTS score_oracle_bars")


def test_scan_matches_postgres(pg, tmp_path) -> None:
    from score import Bar, ColumnStoreV1

    store = ColumnStoreV1(tmp_path / "store")
    for seed, symbol in enumerate(SYMBOLS):
        rows = _history(seed)
        store.append(symbol, [Bar(*row) for row in rows])
        with pg.cursor(
        ) as cur, cur.copy("COPY score_oracle_bars (symbol, date, open, high, low, close, volume) FROM STDIN") as copy:
            for row in rows:
                copy.write_row((symbol, *row))

    rng = random.Random(1234)
    for _ in range(N_QUERIES):
        symbol = rng.choice(SYMBOLS + ["NOSUCH"])
        # Range ends reach past both ends of the data and may be reversed.
        a = FIRST_DAY + timedelta(days=rng.randrange(-30, N_DAYS + 30))
        b = FIRST_DAY + timedelta(days=rng.randrange(-30, N_DAYS + 30))
        start, end = a.isoformat(), b.isoformat()

        expected = pg.execute(
            "SELECT date::text, open, high, low, close, volume FROM score_oracle_bars "
            "WHERE symbol = %s AND date BETWEEN %s AND %s ORDER BY date",
            (symbol, start, end),
        ).fetchall()
        got = [(b_.date, b_.open, b_.high, b_.low, b_.close, b_.volume) for b_ in store.scan(symbol, start, end)]

        # Exact equality on purpose: float8 round-trips a double bit for bit.
        assert got == expected, f"{symbol} [{start}, {end}]: {len(got)} rows vs {len(expected)} in Postgres"

    for symbol in SYMBOLS:
        (n,) = pg.execute("SELECT count(*) FROM score_oracle_bars WHERE symbol = %s", (symbol,)).fetchone()
        assert store.count(symbol) == n
