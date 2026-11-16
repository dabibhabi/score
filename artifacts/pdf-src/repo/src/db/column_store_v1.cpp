#include <score/core/exceptions.hpp>
#include <score/db/column_store_v1.hpp>
#include <utility>

namespace score::db {

ColumnStoreV1::ColumnStoreV1(std::filesystem::path root_dir) : root_dir_(std::move(root_dir)) {}

void ColumnStoreV1::append(const std::string& /*symbol*/, const std::vector<Bar>& /*bars*/) {
    // TODO: implement
    throw score::NotImplementedError("score::db::ColumnStoreV1::append");
}

std::vector<Bar> ColumnStoreV1::scan(const std::string& /*symbol*/, const std::string& /*start_date*/,
                                      const std::string& /*end_date*/) const {
    // TODO: implement
    throw score::NotImplementedError("score::db::ColumnStoreV1::scan");
}

std::vector<std::string> ColumnStoreV1::symbols() const {
    // TODO: implement
    throw score::NotImplementedError("score::db::ColumnStoreV1::symbols");
}

std::size_t ColumnStoreV1::count(const std::string& /*symbol*/) const {
    // TODO: implement
    throw score::NotImplementedError("score::db::ColumnStoreV1::count");
}

}  // namespace score::db
