#pragma once

#include <cstddef>
#include <filesystem>
#include <score/db/column_file.hpp>

namespace score::db {

// ---------------------------------------------------------------------------
// Per-symbol directory
//
//   root/<SYMBOL>/date.col  open.col  high.col  low.col  close.col  volume.col
//   root/<SYMBOL>/COUNT       8 bytes, little-endian uint64: committed row count
//   root/<SYMBOL>/COUNT.tmp   exists only while a commit is in progress
//   root/<SYMBOL>/LOCK        empty file; writers hold an exclusive flock on it
//
// COUNT is the single source of truth. Rows at index >= COUNT in any column
// file are garbage left by an append that did not finish; readers never look
// at them and the next writer truncates them away.
// ---------------------------------------------------------------------------

inline constexpr const char* kCountFileName    = "COUNT";
inline constexpr const char* kCountTmpFileName = "COUNT.tmp";
inline constexpr const char* kLockFileName     = "LOCK";

/// Flushes the file's data all the way to the storage device. On macOS plain
/// fsync() stops at the drive's write cache, so this must use
/// fcntl(fd, F_FULLFSYNC); elsewhere it is fsync(). Throws std::system_error.
void full_fsync(int fd);

/// fsyncs the directory itself, which makes a rename or file creation inside
/// it durable. Throws std::system_error.
void fsync_directory(const std::filesystem::path& dir);

/// Returns the committed row count: the value in `COUNT`, or 0 if `COUNT`
/// does not exist. Never reads `COUNT.tmp`. Throws score::DomainError if
/// `COUNT` exists but is not exactly 8 bytes.
[[nodiscard]] std::size_t read_committed_count(const std::filesystem::path& symbol_dir);

/// Atomically replaces `COUNT` with `n_rows`. After this returns, a crash at
/// any moment leaves either the old or the new count on disk, never a mix.
void commit_count(const std::filesystem::path& symbol_dir, std::size_t n_rows);

/// Shrinks or extends the column file to hold exactly `n_rows` values
/// (file size = kColumnHeaderSize + n_rows * value_size(type)).
void truncate_to_rows(int fd, ColumnType type, std::size_t n_rows);

/// Exclusive inter-process lock on one symbol directory, held for the
/// lifetime of the object. Construction blocks until the lock is free.
/// Works between processes and between threads that each construct their own
/// SymbolLock (each one opens its own file descriptor).
class SymbolLock {
public:
    explicit SymbolLock(const std::filesystem::path& symbol_dir);
    ~SymbolLock();

    SymbolLock(const SymbolLock&)            = delete;
    SymbolLock& operator=(const SymbolLock&) = delete;

private:
    UniqueFd fd_;
};

}  // namespace score::db
