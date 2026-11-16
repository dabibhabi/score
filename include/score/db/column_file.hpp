#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace score::db {

// ---------------------------------------------------------------------------
// Column file format, version 1
//
//   offset  size  field
//   0       4     magic "SCOL"
//   4       2     format version, little-endian (currently 1)
//   6       1     ColumnType
//   7       9     reserved, must be zero
//   16      ...   values: fixed width, little-endian, row r at 16 + r * value_size
//
// The header is 16 bytes on purpose: values then start on an 8-byte boundary,
// so a memory-mapped file can be read as `const double*` without copying.
// ---------------------------------------------------------------------------

// The value bytes are written exactly as they sit in memory. That is only the
// documented little-endian format on a little-endian machine (x86-64, arm64).
static_assert(std::endian::native == std::endian::little, "column files assume a little-endian host");

enum class ColumnType : std::uint8_t {
    Int32   = 1,  // dates: days since 1970-01-01
    Int64   = 2,  // volume
    Float64 = 3,  // open, high, low, close
};

inline constexpr std::size_t                 kColumnHeaderSize    = 16;
inline constexpr std::array<std::uint8_t, 4> kColumnMagic         = {'S', 'C', 'O', 'L'};
inline constexpr std::uint16_t               kColumnFormatVersion = 1;

struct ColumnHeader {
    std::uint16_t version = kColumnFormatVersion;
    ColumnType    type    = ColumnType::Float64;

    bool operator==(const ColumnHeader&) const = default;
};

/// Owns a POSIX file descriptor and closes it on destruction. Provided
/// complete so the stubs and tests have something to hold descriptors with.
class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) : fd_(fd) {}
    ~UniqueFd() { reset(); }

    UniqueFd(UniqueFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    UniqueFd(const UniqueFd&)            = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    [[nodiscard]] int  get() const { return fd_; }
    [[nodiscard]] bool valid() const { return fd_ >= 0; }
    void               reset();

private:
    int fd_ = -1;
};

/// Width in bytes of one value: 4 for Int32, 8 for Int64 and Float64.
[[nodiscard]] std::size_t value_size(ColumnType type);

/// Serializes `header` into the 16-byte layout above.
[[nodiscard]] std::array<std::uint8_t, kColumnHeaderSize> encode_header(const ColumnHeader& header);

/// Parses a 16-byte header. Throws score::DomainError on wrong magic, an
/// unsupported version, an unknown ColumnType, or non-zero reserved bytes.
[[nodiscard]] ColumnHeader decode_header(std::span<const std::uint8_t, kColumnHeaderSize> bytes);

/// Opens the column file at `path`.
///  - Missing file and `writable`: creates it and writes a header for `type`.
///  - Missing file and read-only: throws std::system_error (ENOENT).
///  - Existing file: reads the header and throws score::DomainError if it is
///    invalid or its type differs from `type`.
[[nodiscard]] UniqueFd open_column_file(const std::filesystem::path& path, ColumnType type, bool writable);

/// Writes `values` so that they start at row `first_row`. `values.size()`
/// must be a multiple of value_size(type) (score::DomainError otherwise).
/// Must write every byte or throw std::system_error; a short write is not success.
void write_rows(int fd, ColumnType type, std::size_t first_row, std::span<const std::byte> values);

/// Reads `n_rows` values starting at row `first_row`. Throws score::DomainError
/// if the file holds fewer rows than requested.
[[nodiscard]] std::vector<std::byte> read_rows(int fd, ColumnType type, std::size_t first_row, std::size_t n_rows);

/// True if `symbol` is safe to use as a directory name: 1 to 32 characters
/// from [A-Za-z0-9._^=-], and not starting with '.'. This rejects "", ".",
/// "..", "a/b" and anything else that could escape the store's root directory.
[[nodiscard]] bool is_valid_symbol(std::string_view symbol);

}  // namespace score::db
