#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <score/core/exceptions.hpp>
#include <score/db/symbol_dir.hpp>
#include <span>
#include <system_error>

#include "fd_io.hpp"
#include "score/db/column_file.hpp"

namespace score::db {

// Hints: tasks/code_snippets.md, "Phase B — making an append survive a crash"
// and "Phase D — one writer per symbol".

namespace {

inline constexpr std::size_t kCountBytes = 8;

// B2, B3, B4: opens `path` with raw open(2) flags (O_CLOEXEC is added); failure throws system_error.
UniqueFd open_path(const std::filesystem::path& path, int flags) {
    const int fd = ::open(path.c_str(), flags | O_CLOEXEC, 0644);
    if(fd < 0) {
        throw std::system_error(errno, std::generic_category(), "score::db: open " + path.string());
    }
    return UniqueFd(fd);
}

// B4: `n` as 8 little-endian bytes. Byte i is floor(n / 256^i) mod 256.
std::array<std::byte, kCountBytes> encode_count(std::size_t n) {
    std::array<std::byte, kCountBytes> out{};
    for(std::size_t i = 0; i < kCountBytes; ++i) {
        out[i] = static_cast<std::byte>((static_cast<std::uint64_t>(n) >> (8 * i)) & 0xFF);
    }
    return out;
}

// B3: the inverse of encode_count.
std::size_t decode_count(std::span<const std::byte, kCountBytes> bytes) {
    std::uint64_t n = 0;
    for(std::size_t i = 0; i < kCountBytes; ++i) {
        n |= static_cast<std::uint64_t>(bytes[i]) << (8 * i);
    }
    return static_cast<std::size_t>(n);
}

// B1 to B5: throws system_error carrying the current errno. Call it right after the failing system call.
[[noreturn]] void throw_errno(const char* what) {
    throw std::system_error(errno, std::generic_category(), what);
}

// B4: creates or truncates `path`, writes all of `bytes`, and fsyncs the file before returning.
void write_file_durably(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    UniqueFd file_fd = open_path(path, O_WRONLY | O_CREAT | O_TRUNC);
    pwrite_all(file_fd.get(), bytes, 0);
    full_fsync(file_fd.get());
}

// D1: runs `system_call` again while it fails with EINTR (a signal interrupted it); returns the final result.
template <typename SystemCall>
int retry_on_eintr(SystemCall system_call) {
    int result = 0;
    do {
        result = system_call();
    } while(result == -1 && errno == EINTR);
    return result;
}

// D1: blocks until this descriptor holds an exclusive flock. The lock belongs to the open file, so two
// separate open() calls on the same path exclude each other, even inside one process.
void flock_exclusive(int fd) {
    const int result = retry_on_eintr([fd] { return ::flock(fd, LOCK_EX); });
    if(result != 0) { throw_errno("flock"); }
}

}  // namespace

void full_fsync(int fd) {
#if defined(__APPLE__)
    const int result = ::fcntl(fd, F_FULLFSYNC);
#else
    const int result = ::fsync(fd);
#endif
    if(result != 0) { throw_errno("fsync"); }
}

void fsync_directory(const std::filesystem::path& dir) {
    UniqueFd directory_fd = open_path(dir, O_RDONLY | O_DIRECTORY);
    if(::fsync(directory_fd.get()) != 0) { throw_errno("fsync directory"); }
}

std::size_t read_committed_count(const std::filesystem::path& symbol_dir) {
    const auto count_path = symbol_dir / kCountFileName;

    if(!std::filesystem::exists(count_path)) { return 0; }
    if(std::filesystem::file_size(count_path) != kCountBytes) {
        throw score::DomainError("score::db::read_committed_count: COUNT is not 8 bytes");
    }

    UniqueFd count_fd = open_path(count_path, O_RDONLY);
    std::array<std::byte, kCountBytes> count_bytes{};
    pread_all(count_fd.get(), count_bytes, 0);
    return decode_count(count_bytes);
}

void commit_count(const std::filesystem::path& symbol_dir, std::size_t n_rows) {
    const auto tmp_path    = symbol_dir / kCountTmpFileName;
    const auto count_path  = symbol_dir / kCountFileName;
    const auto count_bytes = encode_count(n_rows);

    write_file_durably(tmp_path, count_bytes);
    std::filesystem::rename(tmp_path, count_path);  // the commit point: readers see the old count or the new one
    fsync_directory(symbol_dir);                    // makes the rename itself survive a crash
}

void truncate_to_rows(int fd, ColumnType type, std::size_t n_rows) {
    const auto file_size = static_cast<off_t>(kColumnHeaderSize + n_rows * value_size(type));
    if(::ftruncate(fd, file_size) != 0) { throw_errno("ftruncate"); }
}

SymbolLock::SymbolLock(const std::filesystem::path& symbol_dir) {
    fd_ = open_path(symbol_dir / kLockFileName, O_RDWR | O_CREAT);
    flock_exclusive(fd_.get());
}

SymbolLock::~SymbolLock() = default;  // the UniqueFd member closes the descriptor, which releases the flock

}  // namespace score::db
