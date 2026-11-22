// Phase E. Passes once ColumnStoreV1::append and count work (Phase B).

#include <catch2/catch_test_macros.hpp>
#include <score/db/column_store_v1.hpp>
#include <score/db/ingestion.hpp>
#include <string>

#include "db_test_util.hpp"

using namespace score::db;
using namespace score::db::testing;

namespace {

/// Produces `n` deterministic bars per symbol; stands in for yfinance or the mock generator.
class FakeSource : public IngestionSource {
public:
    explicit FakeSource(std::size_t n) : n_(n) {}

    [[nodiscard]] std::string name() const override { return "fake"; }

    std::size_t ingest(ColumnStore& store, const std::string& symbol) override {
        store.append(symbol, test_bars(0, n_));
        return n_;
    }

private:
    std::size_t n_;
};

}  // namespace

TEST_CASE("IngestionSource: writes through the ColumnStore interface", "[db][E][ingestion]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    FakeSource    source(25);

    REQUIRE(source.name() == "fake");
    REQUIRE(source.ingest(store, "AAPL") == 25);
    REQUIRE(source.ingest(store, "MSFT") == 25);
    REQUIRE(store.count("AAPL") == 25);
    REQUIRE(store.count("MSFT") == 25);
}

TEST_CASE("IngestionSource: ingesting the same data twice is rejected by the store", "[db][E][ingestion]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    FakeSource    source(5);

    REQUIRE(source.ingest(store, "AAPL") == 5);
    REQUIRE_THROWS_AS(source.ingest(store, "AAPL"), score::DomainError);
    REQUIRE(store.count("AAPL") == 5);
}
