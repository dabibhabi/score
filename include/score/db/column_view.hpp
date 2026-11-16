#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <score/db/column_file.hpp>
#include <span>
#include <utility>

namespace score::db {

/// A read-only window onto rows of one column file, backed by a memory
/// mapping that this object owns. The spans it hands out point straight into
/// the operating system's page cache: no copy is made, and they stay valid
/// until the ColumnView is destroyed or moved from.
///
/// A default-constructed ColumnView is empty (size() == 0) and owns nothing.
class ColumnView {
public:
    ColumnView() = default;

    /// Maps rows [first_row, first_row + n_rows) of the column file at `path`.
    /// `n_rows == 0` returns an empty view of the given type without mapping.
    /// Throws score::DomainError if the header is invalid, its type differs
    /// from `type`, or the file holds fewer rows than requested.
    [[nodiscard]] static ColumnView map(const std::filesystem::path& path, ColumnType type, std::size_t first_row,
                                        std::size_t n_rows);

    ~ColumnView();

    // Move operations are provided complete: they only swap the members
    // below. If you change the members, update swap().
    ColumnView(ColumnView&& other) noexcept { swap(other); }
    ColumnView& operator=(ColumnView&& other) noexcept {
        swap(other);
        return *this;
    }
    ColumnView(const ColumnView&)            = delete;
    ColumnView& operator=(const ColumnView&) = delete;

    [[nodiscard]] ColumnType  type() const { return type_; }
    [[nodiscard]] std::size_t size() const { return n_rows_; }
    [[nodiscard]] bool        empty() const { return n_rows_ == 0; }

    /// Typed access. Each throws score::DomainError if type() does not match.
    [[nodiscard]] std::span<const double>       f64() const;
    [[nodiscard]] std::span<const std::int64_t> i64() const;
    [[nodiscard]] std::span<const std::int32_t> i32() const;

private:
    void swap(ColumnView& other) noexcept {
        std::swap(map_base_, other.map_base_);
        std::swap(map_len_, other.map_len_);
        std::swap(data_, other.data_);
        std::swap(n_rows_, other.n_rows_);
        std::swap(type_, other.type_);
    }

    // mmap offsets must be multiples of the page size, so the mapping usually
    // starts before the first requested row: map_base_/map_len_ describe what
    // was mapped (and must be passed to munmap), data_ points at the first row.
    void*            map_base_ = nullptr;
    std::size_t      map_len_  = 0;
    const std::byte* data_     = nullptr;
    std::size_t      n_rows_   = 0;
    ColumnType       type_     = ColumnType::Float64;
};

}  // namespace score::db
