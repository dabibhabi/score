"""Pandas-facing wrappers around the native risk engine.

This file is INTENTIONALLY scaffolded with stubs, in the same spirit as
:mod:`score.analysis`.  Each function has a complete signature and
docstring; the bodies raise :class:`NotImplementedError` until they are
filled in.  ``tasks/risk_engine_todo.md`` has the order of work and
reading material.

Division of labour
------------------
Every formula lives in C++ under ``score::risk``.  This module only
reshapes pandas objects into :class:`score.Series` (and back), walks
rolling windows, and stacks per-ticker results into tidy frames.  If a
function here starts computing a statistic itself, it is in the wrong
file.

None of the native symbols these wrappers need exist yet, which is why
nothing is imported from :mod:`score._core` at module scope; add those
imports as the corresponding C++ lands.

Conventions
-----------
* Inputs are **returns**, not prices, except the drawdown helpers.
* VaR and Expected Shortfall are positive loss magnitudes: 0.031 means
  "a 3.1% loss is exceeded (1 - confidence) of the time".
* Every rolling statistic at day *t* uses observations up to and
  including *t*, and must be shifted before being joined to anything
  forward-looking.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Literal, Sequence

import pandas as pd

#: Which estimator a VaR/ES call should use.
VaRMethod = Literal["historical", "gaussian", "cornish_fisher", "student_t", "evt", "monte_carlo"]

#: Component blocks of the composite score.
COMPONENTS = ("volatility", "tail", "factor", "drawdown", "regime", "news")


# ---------------------------------------------------------------------------
# Containers
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class RollingSpec:
    """How a rolling estimator walks the panel.

    Attributes
    ----------
    window
        Number of trailing observations per estimate.
    min_periods
        Minimum observations before an estimate is emitted.
    refit_every
        Trading days between full re-fits for estimators with a fitting
        step (GARCH, EVT, Student-t).  Parameters are held constant
        between re-fits while the cheap recursion still updates daily.
        Re-fitting every day is ~20x the cost for little gain.
    """

    window: int = 252
    min_periods: int = 200
    refit_every: int = 21


@dataclass(frozen=True)
class CompositeWeights:
    """Weights over the component blocks of the composite score.

    These are a prior, not a result.  Report the backtest for these, for
    equal weights, and for weights fit on a training split, and let the
    evidence decide.

    Attributes
    ----------
    volatility, tail, factor, drawdown, regime, news
        Non-negative weights, conventionally summing to one.
    """

    volatility: float = 0.30
    tail: float = 0.20
    factor: float = 0.15
    drawdown: float = 0.20
    regime: float = 0.10
    news: float = 0.05

    def normalized(self, tol: float = 1e-9) -> bool:
        """Return whether the weights sum to one within ``tol``."""
        raise NotImplementedError("CompositeWeights.normalized: sum and compare")


# ---------------------------------------------------------------------------
# Conversion helpers
# ---------------------------------------------------------------------------
def to_series(values: pd.Series):
    """Convert a pandas Series into a native :class:`score.Series`.

    Parameters
    ----------
    values
        Float-valued series; its index is carried across as the native
        date labels when it is a DatetimeIndex.

    Returns
    -------
    score.Series
        The native series.
    """
    raise NotImplementedError("to_series: hand the native constructor a contiguous float buffer")


def from_series(native, index: pd.Index | None = None) -> pd.Series:
    """Convert a native :class:`score.Series` back into pandas.

    Parameters
    ----------
    native
        A :class:`score.Series`.
    index
        Optional index to attach; when omitted the native date labels
        are used if present.

    Returns
    -------
    pandas.Series
        The values as a pandas series.
    """
    raise NotImplementedError("from_series: read the native buffer once, not element by element")


# ---------------------------------------------------------------------------
# Volatility
# ---------------------------------------------------------------------------
def log_returns(prices: pd.Series) -> pd.Series:
    """Log returns of a price series, via ``StockAnalyzer::log_returns``.

    Parameters
    ----------
    prices
        Close or adjusted-close prices, chronologically ordered.

    Returns
    -------
    pandas.Series
        Length ``len(prices) - 1``.
    """
    raise NotImplementedError("log_returns: delegate to the native StockAnalyzer")


def ewma_volatility(
    returns: pd.Series,
    lam: float = 0.94,
    periods_per_year: int = 252,
) -> pd.Series:
    """RiskMetrics EWMA volatility.

    Parameters
    ----------
    returns
        Return series.
    lam
        Decay factor; 0.94 is the RiskMetrics daily convention.
    periods_per_year
        Annualization factor.

    Returns
    -------
    pandas.Series
        Annualized conditional volatility, aligned to ``returns``.
    """
    raise NotImplementedError("ewma_volatility: delegate to score::risk::ewma_volatility")


def rolling_volatility(
    returns: pd.Series,
    window: int = 252,
    periods_per_year: int = 252,
) -> pd.Series:
    """Trailing realized volatility.

    This is the **baseline** the composite score has to beat; keep it
    available from the start rather than computing it at the end.

    Parameters
    ----------
    returns
        Return series.
    window
        Trailing observations per estimate.
    periods_per_year
        Annualization factor.

    Returns
    -------
    pandas.Series
        Annualized realized volatility.
    """
    raise NotImplementedError("rolling_volatility: delegate to the native rolling estimator")


def garch_features(returns: pd.Series, spec: RollingSpec | None = None) -> pd.DataFrame:
    """Rolling GARCH(1,1) fits and their one-step volatility forecast.

    Re-fits on ``spec.refit_every`` and filters daily in between.
    Non-converged fits must be reported, not silently scored on.

    Parameters
    ----------
    returns
        Return series.
    spec
        Rolling/refit schedule; defaults to :class:`RollingSpec`.

    Returns
    -------
    pandas.DataFrame
        Columns ``omega``, ``alpha``, ``beta``, ``persistence``,
        ``cond_vol``, ``forecast_vol``, ``converged``.
    """
    raise NotImplementedError("garch_features: walk refit dates, filter the recursion daily")


# ---------------------------------------------------------------------------
# Tail risk
# ---------------------------------------------------------------------------
def value_at_risk(
    returns: pd.Series,
    confidence: float = 0.99,
    method: VaRMethod = "historical",
    window: int | None = None,
) -> pd.DataFrame:
    """Rolling VaR and Expected Shortfall by one estimator.

    Parameters
    ----------
    returns
        Return series.
    confidence
        Confidence level, e.g. 0.99.
    method
        Which native estimator to call.
    window
        Trailing window; ``None`` uses the whole history expanding.

    Returns
    -------
    pandas.DataFrame
        Columns ``var``, ``expected_shortfall``, ``method``, both as
        positive loss magnitudes.
    """
    raise NotImplementedError("value_at_risk: dispatch on method to the native estimator")


def evt_features(
    returns: pd.Series,
    spec: RollingSpec | None = None,
    threshold_quantile: float = 0.95,
) -> pd.DataFrame:
    """Rolling peaks-over-threshold fits of the loss tail.

    A 250-day window yields roughly a dozen exceedances at the 95%
    threshold, which is far too few for a stable shape parameter; use a
    long window (1000+ days) or pool exceedances across a sector.

    Parameters
    ----------
    returns
        Return series; the fit runs on ``-returns``.
    spec
        Rolling/refit schedule.
    threshold_quantile
        Quantile of the loss distribution used as the threshold.

    Returns
    -------
    pandas.DataFrame
        Columns ``xi``, ``beta``, ``threshold``, ``n_exceedances``,
        ``evt_var``, ``evt_es``, ``converged``.
    """
    raise NotImplementedError("evt_features: rolling GPD fit over the exceedances")


def drawdown_features(prices: pd.Series, periods_per_year: int = 252) -> pd.DataFrame:
    """Drawdown state of a price path.

    Parameters
    ----------
    prices
        Price series (not returns).
    periods_per_year
        Annualization factor for the Calmar ratio.

    Returns
    -------
    pandas.DataFrame
        Columns ``drawdown``, ``max_drawdown``, ``underwater_days``,
        ``ulcer_index``, ``calmar``.
    """
    raise NotImplementedError("drawdown_features: delegate to score::risk::drawdown_stats")


# ---------------------------------------------------------------------------
# Factor and cross-sectional structure
# ---------------------------------------------------------------------------
def factor_exposures(
    excess_returns: pd.Series,
    factors: pd.DataFrame,
    spec: RollingSpec | None = None,
) -> pd.DataFrame:
    """Rolling multi-factor regression of one name on the factor set.

    Parameters
    ----------
    excess_returns
        Return series already net of the risk-free rate.
    factors
        Factor returns indexed by date, e.g. the Fama-French five plus
        momentum.
    spec
        Rolling schedule.

    Returns
    -------
    pandas.DataFrame
        Columns ``alpha``, one ``beta_<factor>`` per column of
        ``factors``, ``idiosyncratic_vol``, ``r_squared``.
    """
    raise NotImplementedError("factor_exposures: rolling native OLS against the factor matrix")


def covariance_matrix(
    returns: pd.DataFrame,
    lam: float | None = None,
) -> pd.DataFrame:
    """Covariance of a cross-section of return series.

    Parameters
    ----------
    returns
        Wide frame, one column per ticker, on a common calendar with no
        missing values.
    lam
        When given, use an EWMA covariance with this decay instead of
        the equal-weighted sample covariance.

    Returns
    -------
    pandas.DataFrame
        Square, symmetric, labelled by ticker.
    """
    raise NotImplementedError("covariance_matrix: delegate to the native covariance routine")


def absorption_ratio(
    returns: pd.DataFrame,
    window: int = 252,
    n_components: int = 5,
) -> pd.Series:
    """Rolling share of cross-sectional variance in the top components.

    A market-wide fragility indicator: when a handful of components
    absorb most of the variance, diversification has quietly stopped
    working.

    Parameters
    ----------
    returns
        Wide frame of returns, one column per ticker.
    window
        Trailing window for each covariance estimate.
    n_components
        How many leading components to count.

    Returns
    -------
    pandas.Series
        Values in ``[0, 1]``, indexed by date.
    """
    raise NotImplementedError("absorption_ratio: rolling covariance, then the native eigensolver")


def regime_features(market: pd.DataFrame) -> pd.DataFrame:
    """Market-wide risk-regime features, identical across tickers.

    Parameters
    ----------
    market
        Output of :func:`score.panel.load_market_context`.

    Returns
    -------
    pandas.DataFrame
        Columns ``vix``, ``vix_change``, ``vix_percentile``, indexed by
        date.
    """
    raise NotImplementedError("regime_features: expanding percentile rank of the VIX level")


# ---------------------------------------------------------------------------
# Composite score
# ---------------------------------------------------------------------------
def build_feature_panel(
    prices: pd.DataFrame,
    factors: pd.DataFrame,
    market: pd.DataFrame,
    spec: RollingSpec | None = None,
) -> pd.DataFrame:
    """Compute every risk feature for every ``(ticker, day)``.

    The expensive entry point: it walks each ticker once and stacks the
    per-ticker frames from the functions above.  Cache the fitted GARCH
    and EVT parameters keyed by ``(ticker, refit_date)`` so rebuilding
    features does not mean re-fitting.

    Parameters
    ----------
    prices
        Output of :func:`score.panel.build_prices_panel`.
    factors
        Output of :func:`score.panel.load_ff_factors`.
    market
        Output of :func:`score.panel.load_market_context`.
    spec
        Rolling schedule shared by the fitted estimators.

    Returns
    -------
    pandas.DataFrame
        One row per ``(ticker, date)``, one column per raw feature.
    """
    raise NotImplementedError("build_feature_panel: per-ticker feature assembly")


def cross_sectional_zscore(
    features: pd.DataFrame,
    columns: Sequence[str] | None = None,
    robust: bool = True,
    winsor_quantiles: tuple[float, float] = (0.01, 0.99),
) -> pd.DataFrame:
    """Standardize features within each day, across the cross-section.

    Within a day only: using a full-sample mean or standard deviation
    leaks the future into every past observation.  Robust scaling
    (median and MAD) is the default because equity risk features are
    right-skewed enough that one blown-up name would otherwise compress
    everyone else toward zero.

    Parameters
    ----------
    features
        Output of :func:`build_feature_panel`.
    columns
        Which columns to standardize; ``None`` means all numeric ones.
    robust
        Use median/MAD rather than mean/standard deviation.
    winsor_quantiles
        Cross-sectional clipping bounds applied before scaling.

    Returns
    -------
    pandas.DataFrame
        Same shape as ``features``, standardized, signed so that higher
        always means riskier.
    """
    raise NotImplementedError("cross_sectional_zscore: group by date, winsorize, then scale")


def composite_score(
    z_features: pd.DataFrame,
    weights: CompositeWeights | None = None,
) -> pd.Series:
    """Blend standardized components into a 0-100 risk score.

    Components that are unavailable for a ``(ticker, day)`` must be
    dropped from the weighted average and the remaining weights
    renormalized.  Treating a missing component as zero silently scores
    it as exactly average, which for the sparsely-covered news features
    is most of the panel.

    Parameters
    ----------
    z_features
        Output of :func:`cross_sectional_zscore`.
    weights
        Component weights; defaults to :class:`CompositeWeights`.

    Returns
    -------
    pandas.Series
        Scores in ``[0, 100]``, indexed by ``(ticker, date)``.
    """
    raise NotImplementedError("composite_score: mask-aware weighted blend, then map to 0-100")


# ---------------------------------------------------------------------------
# Evaluation
# ---------------------------------------------------------------------------
def forward_targets(prices: pd.DataFrame, horizon: int = 21) -> pd.DataFrame:
    """Realized outcomes over the ``horizon`` days *after* each day.

    Strictly ``t+1 .. t+horizon``.  An off-by-one here makes every
    downstream number meaningless in the most flattering direction.

    Parameters
    ----------
    prices
        Output of :func:`score.panel.build_prices_panel`.
    horizon
        Forward window in trading days.

    Returns
    -------
    pandas.DataFrame
        Columns ``fwd_realized_vol``, ``fwd_max_drawdown``,
        ``fwd_worst_return``.
    """
    raise NotImplementedError("forward_targets: forward-looking rolling aggregates")


def information_coefficient(
    scores: pd.Series,
    targets: pd.Series,
) -> pd.DataFrame:
    """Per-day rank correlation between score and realized outcome.

    Overlapping forward windows make naive t-statistics roughly four
    times too large; the summary needs an autocorrelation-consistent
    standard error.

    Parameters
    ----------
    scores
        Output of :func:`composite_score`.
    targets
        One column of :func:`forward_targets`.

    Returns
    -------
    pandas.DataFrame
        Per-day Spearman correlation, plus the summary statistics.
    """
    raise NotImplementedError("information_coefficient: per-day Spearman, corrected t-stat")


def decile_table(
    scores: pd.Series,
    targets: pd.DataFrame,
    n_buckets: int = 10,
) -> pd.DataFrame:
    """Mean forward outcome per score bucket.

    The headline result: does the top decile actually realize worse
    drawdowns than the bottom, and is the relationship monotone in
    between?

    Parameters
    ----------
    scores
        Output of :func:`composite_score`.
    targets
        Output of :func:`forward_targets`.
    n_buckets
        Number of equal-count buckets per day.

    Returns
    -------
    pandas.DataFrame
        One row per bucket, one column per target, with the top-minus-
        bottom spread appended.
    """
    raise NotImplementedError("decile_table: daily bucketing, then average within bucket")


def var_backtest(
    returns: pd.Series,
    var_forecasts: pd.Series,
    confidence: float = 0.99,
) -> pd.DataFrame:
    """Kupiec and Christoffersen tests of a VaR forecast series.

    Unconditional coverage asks whether the breach *rate* is right;
    independence asks whether breaches cluster.  A model can pass either
    alone while being unusable, so report both and the joint test.

    Parameters
    ----------
    returns
        Realized returns.
    var_forecasts
        VaR forecast for the same day, as a positive loss magnitude,
        made using only prior data.
    confidence
        The confidence level the forecasts were produced at.

    Returns
    -------
    pandas.DataFrame
        Breach counts and rates, the three likelihood-ratio statistics
        and their p-values.
    """
    raise NotImplementedError("var_backtest: delegate to the native backtest routines")
