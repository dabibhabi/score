#pragma once

#include <sys/types.h>

#include <cstddef>
#include <span>

// Private to src/db: the retrying pread/pwrite loops shared by column_file.cpp and symbol_dir.cpp.
namespace score::db {

/// Writes all of `bytes` at `offset`, retrying short writes and EINTR. Throws std::system_error.
void pwrite_all(int fd, std::span<const std::byte> bytes, off_t offset);

/// Fills all of `out` from `offset`. A file that ends early throws score::DomainError.
void pread_all(int fd, std::span<std::byte> out, off_t offset);

}  // namespace score::db
