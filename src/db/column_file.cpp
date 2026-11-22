#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <score/core/exceptions.hpp>
#include <score/db/column_file.hpp>

#include "fd_io.hpp"


#include <algorithm>
#include <cerrno>
#include <system_error>

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

namespace {

bool magic_matches(std::span<const std::uint8_t, kColumnHeaderSize> bytes) {
    return std::equal(kColumnMagic.begin(), kColumnMagic.end(), bytes.begin());
}

bool version_matches(std::span<const std::uint8_t, kColumnHeaderSize> bytes) {
    return bytes[4] == static_cast<std::uint8_t>(kColumnFormatVersion) &&
           bytes[5] == static_cast<std::uint8_t>(kColumnFormatVersion >> 8);
}

bool reserved_is_zero(std::span<const std::uint8_t, kColumnHeaderSize> bytes) {
    return std::all_of(bytes.begin() + 7, bytes.end(), [](std::uint8_t b) { return b == 0; });
}

}  // namespace

ColumnHeader decode_header(std::span<const std::uint8_t, kColumnHeaderSize> bytes) {

    if(!magic_matches(bytes)) {
        throw score::DomainError("score::db::decode_header: magic number mismatch");
    }
    if(!version_matches(bytes)) {
        throw score::DomainError("score::db::decode_header: version mismatch");
    }
    if(!reserved_is_zero(bytes)) {
        throw score::DomainError("score::db::decode_header: reserved bytes are not zero");
    }
    
    switch(static_cast<ColumnType>(bytes[6])) {
        case ColumnType::Int32: return ColumnHeader{kColumnFormatVersion, ColumnType::Int32};
        case ColumnType::Int64: return ColumnHeader{kColumnFormatVersion, ColumnType::Int64};
        case ColumnType::Float64: return ColumnHeader{kColumnFormatVersion, ColumnType::Float64};
    }
    throw score::DomainError("score::db::decode_header: unknown column type");
}

// Writes all of `bytes` at `offset`, looping over short writes and EINTR. Use it for every pwrite in write_rows.
void pwrite_all(int fd, std::span<const std::byte> bytes, off_t offset) {
    std::size_t done = 0;
    while(done < bytes.size()) {
        const ssize_t n = ::pwrite(fd, bytes.data() + done, bytes.size() - done, offset + static_cast<off_t>(done));
        if(n < 0) {
            if(errno == EINTR) { continue; }
            throw std::system_error(errno, std::generic_category(), "score::db: pwrite");
        }
        if(n == 0) {
            throw std::system_error(ENOSPC, std::generic_category(), "score::db: pwrite wrote nothing");
        }
        done += static_cast<std::size_t>(n);
    }
}

// A7, A8: fills all of `out` from `offset`, looping over short reads and EINTR. A file that ends early throws DomainError.
void pread_all(int fd, std::span<std::byte> out, off_t offset) {
    std::size_t done = 0;
    while(done < out.size()) {
        const ssize_t n = ::pread(fd, out.data() + done, out.size() - done, offset + static_cast<off_t>(done));
        if(n < 0) {
            if(errno == EINTR) { continue; }
            throw std::system_error(errno, std::generic_category(), "score::db: pread");
        }
        if(n == 0) {
            throw score::DomainError("score::db: file is shorter than expected");
        }
        done += static_cast<std::size_t>(n);
    }
}

namespace {


// Byte offset of row `first_row`. Use it for the start offset in write_rows (and read_rows later).
off_t row_offset(std::size_t first_row, ColumnType type) {
    return static_cast<off_t>(kColumnHeaderSize + first_row * value_size(type));
}


// A8 step 1: opens (and creates, if `writable`) the file with mode 0644 and O_CLOEXEC; failure throws system_error.
UniqueFd open_fd(const std::filesystem::path& path, bool writable) {
    const int flags = (writable ? (O_RDWR | O_CREAT) : O_RDONLY) | O_CLOEXEC;
    const int fd    = ::open(path.c_str(), flags, 0644);
    if(fd < 0) {
        throw std::system_error(errno, std::generic_category(), "score::db: open " + path.string());
    }
    return UniqueFd(fd);
}

// A8 step 2: current size of the open file in bytes.
std::size_t file_size_of(int fd) {
    struct stat st{};
    if(::fstat(fd, &st) < 0) {
        throw std::system_error(errno, std::generic_category(), "score::db: fstat");
    }
    return static_cast<std::size_t>(st.st_size);
}

// A8 step 2: writes a fresh header for `type` at offset 0 (new, empty files only).
void write_header(int fd, ColumnType type) {
    const auto header = encode_header(ColumnHeader{kColumnFormatVersion, type});
    pwrite_all(fd, std::as_bytes(std::span(header)), 0);
}

// A8 step 3: reads and validates the header at offset 0. Short or invalid files throw DomainError.
ColumnHeader read_header(int fd) {
    std::array<std::uint8_t, kColumnHeaderSize> bytes{};
    pread_all(fd, std::as_writable_bytes(std::span(bytes)), 0);
    return decode_header(bytes);
}

// A9: longest symbol is_valid_symbol accepts.
inline constexpr std::size_t kMaxSymbolLength = 32;

// A9: true for one character in [A-Za-z0-9._^=-]. Explicit ranges, not isalnum, so the result never depends on locale.
bool is_symbol_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '.' || c == '_' || c == '^' || c == '=' || c == '-';
}

}  // namespace

void write_rows(int fd, ColumnType type, std::size_t first_row, std::span<const std::byte> values) { // Not tested
    
    const std::size_t val_size = value_size(type);
    if(values.size() % val_size != 0) throw score::DomainError("score::db::write_rows: byte count is not a multiple of the value size");
    
    pwrite_all(fd , values , row_offset(first_row, type));
}

std::vector<std::byte> read_rows(int fd, ColumnType type, std::size_t first_row, std::size_t n_rows) { // NOT TESTED
    std::vector<std::byte> output(n_rows * value_size(type));
    pread_all(fd,output,row_offset(first_row,type));
    return output;
}

UniqueFd open_column_file(const std::filesystem::path& path, ColumnType type, bool writable) {
    UniqueFd fd = open_fd(path, writable);

    if(writable && file_size_of(fd.get()) == 0) {
        write_header(fd.get(), type);
        return fd;
    }
    if(read_header(fd.get()).type != type) {
        throw score::DomainError("score::db::open_column_file: column type mismatch");
    }
    return fd;
}

bool is_valid_symbol(std::string_view symbol) {
    if(symbol.empty() || symbol.size() > kMaxSymbolLength || symbol.front() == '.') return false;
    return std::all_of(symbol.begin(), symbol.end(), is_symbol_char);
}

}  // namespace score::db
