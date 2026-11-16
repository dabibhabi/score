#include <score/core/exceptions.hpp>
#include <score/db/column_view.hpp>

namespace score::db {

// Hints: tasks/code_snippets.md, "Phase C — reading without copying".

ColumnView ColumnView::map(const std::filesystem::path& path, ColumnType type, std::size_t first_row,
                           std::size_t n_rows) {
    // TODO(C1):
    //  1. open_column_file(path, type, /*writable=*/false) validates the header.
    //  2. n_rows == 0: return an empty view with type_ set. mmap of length 0 is an error.
    //  3. ::fstat; throw score::DomainError if the file is shorter than
    //     kColumnHeaderSize + (first_row + n_rows) * value_size(type).
    //  4. byte_offset = kColumnHeaderSize + first_row * value_size(type).
    //     page = ::sysconf(_SC_PAGESIZE); aligned = byte_offset rounded down to a page.
    //  5. ::mmap(nullptr, (byte_offset - aligned) + n_rows * value_size(type),
    //            PROT_READ, MAP_PRIVATE, fd, aligned). MAP_FAILED -> std::system_error.
    //  6. Fill map_base_, map_len_, data_ = base + (byte_offset - aligned), n_rows_, type_.
    //     The descriptor can be closed now: the mapping keeps the file alive.
    (void)path;
    (void)type;
    (void)first_row;
    (void)n_rows;
    throw score::NotImplementedError("score::db::ColumnView::map");
}

ColumnView::~ColumnView() {
    // TODO(C2): if (map_base_ != nullptr) ::munmap(map_base_, map_len_).
    // Until this is written every view leaks its mapping.
}

std::span<const double> ColumnView::f64() const {
    // TODO(C3): throw score::DomainError unless type_ == ColumnType::Float64, then
    // return {reinterpret_cast<const double*>(data_), n_rows_}. Why the cast is
    // safe here (alignment, object lifetime) is covered in the snippet notes.
    throw score::NotImplementedError("score::db::ColumnView::f64");
}

std::span<const std::int64_t> ColumnView::i64() const {
    // TODO(C4): as f64(), for ColumnType::Int64.
    throw score::NotImplementedError("score::db::ColumnView::i64");
}

std::span<const std::int32_t> ColumnView::i32() const {
    // TODO(C5): as f64(), for ColumnType::Int32.
    throw score::NotImplementedError("score::db::ColumnView::i32");
}

}  // namespace score::db
