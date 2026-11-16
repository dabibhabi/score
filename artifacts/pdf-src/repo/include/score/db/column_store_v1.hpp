#pragma once

#include <filesystem>
#include <score/db/column_store.hpp>

namespace score::db {

/// First concrete ColumnStore. On-disk byte layout is not designed yet
/// (TODO) — this class exists to establish the module boundary so callers
/// can code against ColumnStore without waiting on that design.
class ColumnStoreV1 : public ColumnStore {
public:
    explicit ColumnStoreV1(std::filesystem::path root_dir);

    void append(const std::string& symbol, const std::vector<Bar>& bars) override;
    [[nodiscard]] std::vector<Bar> scan(const std::string& symbol, const std::string& start_date,
                                         const std::string& end_date) const override;
    [[nodiscard]] std::vector<std::string> symbols() const override;
    [[nodiscard]] std::size_t count(const std::string& symbol) const override;

private:
    std::filesystem::path root_dir_;
};

}  // namespace score::db
