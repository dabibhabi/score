// Phase B (count, fsync, truncate) and Phase D (SymbolLock).
// Remove "[.todo]" from one TEST_CASE at a time, top to bottom.

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <filesystem>
#include <score/core/exceptions.hpp>
#include <score/db/symbol_dir.hpp>
#include <string>
#include <system_error>

#include "db_test_util.hpp"

using namespace score::db;
using score::db::testing::append_raw;
using score::db::testing::read_raw;
using score::db::testing::TempDir;

TEST_CASE("full_fsync and fsync_directory", "[db][B][symbol_dir]") {
    TempDir    tmp;
    const auto path = tmp.path() / "file";
    append_raw(path, "data");

    UniqueFd fd(::open(path.c_str(), O_RDWR));
    REQUIRE(fd.valid());
    REQUIRE_NOTHROW(full_fsync(fd.get()));
    REQUIRE_NOTHROW(fsync_directory(tmp.path()));

    REQUIRE_THROWS_AS(full_fsync(-1), std::system_error);
    REQUIRE_THROWS_AS(fsync_directory(tmp.path() / "no_such_dir"), std::system_error);
}

TEST_CASE("read_committed_count: no COUNT file means zero rows", "[db][B][symbol_dir]") {
    TempDir tmp;
    REQUIRE(read_committed_count(tmp.path()) == 0);
}

TEST_CASE("commit_count then read_committed_count", "[db][B][symbol_dir]") {
    TempDir tmp;
    commit_count(tmp.path(), 5);
    REQUIRE(read_committed_count(tmp.path()) == 5);
    commit_count(tmp.path(), 12);
    REQUIRE(read_committed_count(tmp.path()) == 12);
    REQUIRE_FALSE(std::filesystem::exists(tmp.path() / kCountTmpFileName));  // the rename consumed it
}

TEST_CASE("commit_count: COUNT is 8 little-endian bytes", "[db][B][symbol_dir]") {
    TempDir tmp;
    commit_count(tmp.path(), 258);  // 0x0102
    REQUIRE(read_raw(tmp.path() / kCountFileName) == std::string("\x02\x01\x00\x00\x00\x00\x00\x00", 8));
}

TEST_CASE("read_committed_count: a leftover COUNT.tmp is ignored", "[db][B][symbol_dir]") {
    TempDir tmp;
    // A writer crashed after creating COUNT.tmp but before the rename.
    append_raw(tmp.path() / kCountTmpFileName, std::string("\x63\x00\x00\x00\x00\x00\x00\x00", 8));
    REQUIRE(read_committed_count(tmp.path()) == 0);

    commit_count(tmp.path(), 5);
    append_raw(tmp.path() / kCountTmpFileName, "garbage");
    REQUIRE(read_committed_count(tmp.path()) == 5);

    commit_count(tmp.path(), 6);  // a later commit must cope with the stale temp file
    REQUIRE(read_committed_count(tmp.path()) == 6);
}

TEST_CASE("read_committed_count: a COUNT of the wrong size is corruption", "[db][B][symbol_dir]") {
    TempDir tmp;
    append_raw(tmp.path() / kCountFileName, "abc");
    REQUIRE_THROWS_AS(read_committed_count(tmp.path()), score::DomainError);
}

TEST_CASE("truncate_to_rows: shrinks and extends", "[db][B][symbol_dir]") {
    TempDir    tmp;
    const auto path = tmp.path() / "close.col";
    UniqueFd   fd   = open_column_file(path, ColumnType::Float64, true);

    truncate_to_rows(fd.get(), ColumnType::Float64, 10);
    REQUIRE(std::filesystem::file_size(path) == kColumnHeaderSize + 80);
    truncate_to_rows(fd.get(), ColumnType::Float64, 3);
    REQUIRE(std::filesystem::file_size(path) == kColumnHeaderSize + 24);
    truncate_to_rows(fd.get(), ColumnType::Float64, 0);
    REQUIRE(std::filesystem::file_size(path) == kColumnHeaderSize);
    REQUIRE(open_column_file(path, ColumnType::Float64, false).valid());  // header survived
}

// --- Phase D -----------------------------------------------------------------

namespace {

// Tries to take the symbol lock without blocking, on a descriptor of its own.
// Returns true if the lock was free (and releases it again).
bool lock_is_free(const std::filesystem::path& dir) {
    UniqueFd fd(::open((dir / kLockFileName).c_str(), O_RDWR | O_CREAT, 0644));
    if (!fd.valid()) { return false; }
    return ::flock(fd.get(), LOCK_EX | LOCK_NB) == 0;
}

}  // namespace

TEST_CASE("SymbolLock: released when the object dies", "[.todo][db][D][symbol_dir]") {
    TempDir tmp;
    {
        SymbolLock lock(tmp.path());
        REQUIRE(std::filesystem::exists(tmp.path() / kLockFileName));
        REQUIRE_FALSE(lock_is_free(tmp.path()));
    }
    REQUIRE(lock_is_free(tmp.path()));
}

TEST_CASE("SymbolLock: excludes another process", "[.todo][db][D][symbol_dir]") {
    TempDir tmp;
    int     child_ready[2];
    int     parent_done[2];
    REQUIRE(::pipe(child_ready) == 0);
    REQUIRE(::pipe(parent_done) == 0);

    const pid_t pid = score::db::testing::run_in_child([&] {
        ::close(child_ready[0]);
        ::close(parent_done[1]);
        SymbolLock lock(tmp.path());
        char       byte = 'L';
        (void)!::write(child_ready[1], &byte, 1);  // "I hold the lock"
        (void)!::read(parent_done[0], &byte, 1);   // wait until the parent has looked
    });
    ::close(child_ready[1]);
    ::close(parent_done[0]);

    char byte = 0;
    // read() returns 0 (end of file) if the child died before taking the lock.
    REQUIRE(::read(child_ready[0], &byte, 1) == 1);
    const bool free_while_child_holds_it = lock_is_free(tmp.path());

    ::close(parent_done[1]);  // lets the child exit, which releases the lock
    REQUIRE(score::db::testing::wait_for_child(pid) == score::db::testing::kChildOk);
    ::close(child_ready[0]);

    REQUIRE_FALSE(free_while_child_holds_it);
    REQUIRE(lock_is_free(tmp.path()));
}
