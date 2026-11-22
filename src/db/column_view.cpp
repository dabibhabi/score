#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <score/core/exceptions.hpp>
#include <score/db/column_view.hpp>
#include <system_error>

#include "fd_io.hpp"

namespace score::db {

// Hints: tasks/code_snippets.md, "Phase C — reading without copying".

namespace {

// C1: size of one memory page in bytes (4096 on x86-64, 16384 on Apple silicon).
std::size_t page_size() { return static_cast<std::size_t>(::sysconf(_SC_PAGESIZE)); }

// C1: the part of the file to mmap for a row range. mmap offsets must be multiples of the page size,
// so the window starts at the page boundary at or below the first row and `lead_bytes` skips the gap.
struct MappingWindow {
    off_t       aligned_offset;  // multiple of the page size
    std::size_t lead_bytes;      // the first row sits this many bytes into the mapping
    std::size_t length;          // bytes to map, including lead_bytes
};

// C1 step 4: the window for rows [first_row, first_row + n_rows) of a column of `type`.
MappingWindow mapping_window(ColumnType type, std::size_t first_row, std::size_t n_rows) {
    const std::size_t row_width   = value_size(type);
    const std::size_t byte_offset = kColumnHeaderSize + first_row * row_width;
    const std::size_t aligned     = byte_offset - byte_offset % page_size();
    const std::size_t lead_bytes  = byte_offset - aligned;
    return {static_cast<off_t>(aligned), lead_bytes, lead_bytes + n_rows * row_width};
}

// C1 step 3: the smallest file size that still contains rows [0, first_row + n_rows).
std::size_t required_file_size(ColumnType type, std::size_t first_row, std::size_t n_rows) {
    return kColumnHeaderSize + (first_row + n_rows) * value_size(type);
}

// C1 step 3: throws DomainError if the open file is shorter than `required_size`.
void require_file_size(int fd, std::size_t required_size) {
    if(file_size_of(fd) < required_size) {
        throw score::DomainError("score::db::ColumnView::map: file holds fewer rows than requested");
    }
}

// C1 step 5: read-only, private mapping of `window` over `fd`. Failure throws system_error.
void* map_read_only(int fd, const MappingWindow& window) {
    void* base = ::mmap(nullptr, window.length, PROT_READ, MAP_PRIVATE, fd, window.aligned_offset);
    if(base == MAP_FAILED) {
        throw std::system_error(errno, std::generic_category(), "score::db::ColumnView: mmap");
    }
    return base;
}

// C3 to C5: the mapped bytes as `n_rows` values of type T. Throws DomainError unless `actual` is `expected`.
// The cast is safe: the mapping is page aligned and the 16-byte header keeps every row aligned for T.
template <typename T>
std::span<const T> typed_span(const std::byte* data, std::size_t n_rows, ColumnType actual, ColumnType expected) {
    if(actual != expected) {
        throw score::DomainError("score::db::ColumnView: column type mismatch");
    }
    return {reinterpret_cast<const T*>(data), n_rows};
}

}  // namespace

ColumnView ColumnView::map(const std::filesystem::path& path, ColumnType type, std::size_t first_row,
                           std::size_t n_rows) {
    UniqueFd fd = open_column_file(path, type, false);  // validates the header and the type

    ColumnView view;
    view.type_ = type;
    if(n_rows == 0) { return view; }  // mmap rejects a length of 0

    require_file_size(fd.get(), required_file_size(type, first_row, n_rows));

    const MappingWindow window = mapping_window(type, first_row, n_rows);
    view.map_base_ = map_read_only(fd.get(), window);
    view.map_len_  = window.length;
    view.data_     = static_cast<const std::byte*>(view.map_base_) + window.lead_bytes;
    view.n_rows_   = n_rows;
    return view;  // fd closes here; the mapping stays valid
}

ColumnView::~ColumnView() {
    if(map_base_ != nullptr) { ::munmap(map_base_, map_len_); }
}

std::span<const double> ColumnView::f64() const {
    return typed_span<double>(data_, n_rows_, type_, ColumnType::Float64);
}

std::span<const std::int64_t> ColumnView::i64() const {
    return typed_span<std::int64_t>(data_, n_rows_, type_, ColumnType::Int64);
}

std::span<const std::int32_t> ColumnView::i32() const {
    return typed_span<std::int32_t>(data_, n_rows_, type_, ColumnType::Int32);
}

}  // namespace score::db
