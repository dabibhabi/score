#include <unistd.h>

#include <score/core/exceptions.hpp>
#include <score/db/column_file.hpp>

namespace score::db {

// Hints: tasks/code_snippets.md, "Phase A — fixed-width binary files".

void UniqueFd::reset() {
    if (fd_ >= 0) { ::close(fd_); }
    fd_ = -1;
}

std::size_t value_size(ColumnType type) {
    // TODO(A3): switch over ColumnType; throw score::DomainError for an unknown value.
    (void)type;
    throw score::NotImplementedError("score::db::value_size");
}

std::array<std::uint8_t, kColumnHeaderSize> encode_header(const ColumnHeader& header) {
    // TODO(A4):
    //  1. Start from an all-zero array (this zeroes the reserved bytes).
    //  2. Copy kColumnMagic into bytes 0..3.
    //  3. Store the version low byte first: bytes[4] = v & 0xFF, bytes[5] = v >> 8.
    //  4. Store the type in byte 6.
    (void)header;
    throw score::NotImplementedError("score::db::encode_header");
}

ColumnHeader decode_header(std::span<const std::uint8_t, kColumnHeaderSize> bytes) {
    // TODO(A5): the reverse of encode_header. Check, in this order: magic,
    // version == kColumnFormatVersion, type is one of the three enumerators,
    // reserved bytes are zero. Throw score::DomainError naming what was wrong.
    (void)bytes;
    throw score::NotImplementedError("score::db::decode_header");
}

void write_rows(int fd, ColumnType type, std::size_t first_row, std::span<const std::byte> values) {
    // TODO(A6):
    //  1. Reject values.size() % value_size(type) != 0 with score::DomainError.
    //  2. offset = kColumnHeaderSize + first_row * value_size(type).
    //  3. Loop on ::pwrite until every byte is written: pwrite may write fewer
    //     bytes than asked. Retry on EINTR; any other -1 -> std::system_error(errno, ...).
    (void)fd;
    (void)type;
    (void)first_row;
    (void)values;
    throw score::NotImplementedError("score::db::write_rows");
}

std::vector<std::byte> read_rows(int fd, ColumnType type, std::size_t first_row, std::size_t n_rows) {
    // TODO(A7): same offset arithmetic as write_rows, looping on ::pread.
    // pread returning 0 before the buffer is full means the file is too
    // short: throw score::DomainError.
    (void)fd;
    (void)type;
    (void)first_row;
    (void)n_rows;
    throw score::NotImplementedError("score::db::read_rows");
}

UniqueFd open_column_file(const std::filesystem::path& path, ColumnType type, bool writable) {
    // TODO(A8):
    //  1. ::open with O_RDWR | O_CREAT (mode 0644) when writable, O_RDONLY otherwise.
    //     Add O_CLOEXEC in both cases. -1 -> std::system_error(errno, ...).
    //  2. ::fstat to get the size. Size 0 and writable: pwrite encode_header at offset 0.
    //  3. Otherwise pread 16 bytes, decode_header, and compare the type.
    (void)path;
    (void)type;
    (void)writable;
    throw score::NotImplementedError("score::db::open_column_file");
}

bool is_valid_symbol(std::string_view symbol) {
    // TODO(A9): length 1..32, first character not '.', every character in
    // [A-Za-z0-9._^=-]. No std::regex needed: one loop is enough.
    (void)symbol;
    throw score::NotImplementedError("score::db::is_valid_symbol");
}

}  // namespace score::db
