#include <unistd.h>

#include <score/core/exceptions.hpp>
#include <score/db/column_file.hpp>


#include <algorithm>

namespace score::db {

// Hints: tasks/code_snippets.md, "Phase A — fixed-width binary files".

void UniqueFd::reset() {
    if (fd_ >= 0) { ::close(fd_); }
    fd_ = -1;
}

std::size_t value_size(ColumnType type) {

    switch(type) {
        case ColumnType::Int32: return 4;
        case ColumnType::Int64: return 8;
        case ColumnType::Float64: return 8;
    }
    throw score::DomainError("db::score Unknown column type!");
}

std::array<std::uint8_t, kColumnHeaderSize> encode_header(const ColumnHeader& header) {
   
    
    std::array<std::uint8_t,kColumnHeaderSize> output{}; // using array instead of vector to avoid reallocation

    std::copy(kColumnMagic.begin(),kColumnMagic.end(),output.begin());

    output[4] = static_cast<std::uint8_t>(header.version & 0xFF);
    output[5] = static_cast<std::uint8_t>(header.version >> 8);
    output[6] = static_cast<std::uint8_t>(header.type);
    
    return output;
}

ColumnHeader decode_header(std::span<const std::uint8_t, kColumnHeaderSize> bytes) {
    // TODO(A5): the reverse of encode_header. Check, in this order: magic,
    // version == kColumnFormatVersion, type is one of the three enumerators,
    // reserved bytes are zero. Throw score::DomainError naming what was wrong.
    // start decode
    if(bytes[0] != kColumnMagic[0] || bytes[1] != kColumnMagic[1] || bytes[2] != kColumnMagic[2] || bytes[3] != kColumnMagic[3]) {
        throw score::DomainError("score::db::decode_header: magic number mismatch");
    }
    if(bytes[4] != static_cast<std::uint8_t>(kColumnFormatVersion)) {
        throw score::DomainError("score::db::decode_header: version mismatch");
    }
    if(bytes[5] != static_cast<std::uint8_t>(kColumnFormatVersion >> 8)) {
        throw score::DomainError("score::db::decode_header: version mismatch");
    }
    if(bytes[7] != 0) {
        throw score::DomainError("score::db::decode_header: reserved bytes are not zero");
    }
    ColumnType type = static_cast<ColumnType>(bytes[6]);
    switch(type) {
        case static_cast<std::uint8_t>(ColumnType::Int32):
            return ColumnHeader{kColumnFormatVersion, ColumnType::Int32};
        case static_cast<std::uint8_t>(ColumnType::Int64):
            return ColumnHeader{kColumnFormatVersion, ColumnType::Int64};
        case static_cast<std::uint8_t>(ColumnType::Float64):
            return ColumnHeader{kColumnFormatVersion, ColumnType::Float64};
    }


    // start decode


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
