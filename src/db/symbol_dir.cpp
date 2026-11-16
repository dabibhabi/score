#include <score/core/exceptions.hpp>
#include <score/db/symbol_dir.hpp>

namespace score::db {

// Hints: tasks/code_snippets.md, "Phase B — making an append survive a crash"
// and "Phase D — one writer per symbol".

void full_fsync(int fd) {
    // TODO(B1): #if defined(__APPLE__) use ::fcntl(fd, F_FULLFSYNC), else ::fsync(fd).
    // -1 -> std::system_error(errno, std::generic_category(), "fsync").
    (void)fd;
    throw score::NotImplementedError("score::db::full_fsync");
}

void fsync_directory(const std::filesystem::path& dir) {
    // TODO(B2): ::open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC), then ::fsync, then close
    // (hold it in a UniqueFd). Plain fsync is correct for a directory, also on macOS.
    (void)dir;
    throw score::NotImplementedError("score::db::fsync_directory");
}

std::size_t read_committed_count(const std::filesystem::path& symbol_dir) {
    // TODO(B3):
    //  1. ::open(symbol_dir / kCountFileName, O_RDONLY). ENOENT -> return 0.
    //  2. pread 8 bytes; anything other than exactly 8 -> score::DomainError.
    //  3. Assemble the uint64 from little-endian bytes and return it.
    (void)symbol_dir;
    throw score::NotImplementedError("score::db::read_committed_count");
}

void commit_count(const std::filesystem::path& symbol_dir, std::size_t n_rows) {
    // TODO(B4): the order is the whole point; do not reorder.
    //  1. Create/truncate symbol_dir / kCountTmpFileName (O_WRONLY | O_CREAT | O_TRUNC).
    //  2. Write the 8 little-endian bytes of n_rows.
    //  3. full_fsync the temp file, then close it.
    //  4. std::filesystem::rename(tmp, symbol_dir / kCountFileName)  <- the commit point.
    //  5. fsync_directory(symbol_dir) so the rename itself survives a crash.
    (void)symbol_dir;
    (void)n_rows;
    throw score::NotImplementedError("score::db::commit_count");
}

void truncate_to_rows(int fd, ColumnType type, std::size_t n_rows) {
    // TODO(B5): ::ftruncate(fd, kColumnHeaderSize + n_rows * value_size(type)).
    (void)fd;
    (void)type;
    (void)n_rows;
    throw score::NotImplementedError("score::db::truncate_to_rows");
}

SymbolLock::SymbolLock(const std::filesystem::path& symbol_dir) {
    // TODO(D1):
    //  1. ::open(symbol_dir / kLockFileName, O_RDWR | O_CREAT | O_CLOEXEC, 0644) into fd_.
    //  2. ::flock(fd_.get(), LOCK_EX), retrying on EINTR. This blocks until the lock is free.
    (void)symbol_dir;
    throw score::NotImplementedError("score::db::SymbolLock");
}

SymbolLock::~SymbolLock() {
    // TODO(D2): nothing to write if you rely on fd_: closing the descriptor
    // releases the flock. Read the snippet notes on why, then delete this comment.
}

}  // namespace score::db
