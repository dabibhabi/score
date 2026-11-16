#pragma once

// Shared helpers for the score::db tests. Everything here is complete; none
// of it depends on the functions you are implementing.

#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <score/core/exceptions.hpp>
#include <score/db/bar.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace score::db::testing {

/// A fresh, uniquely named directory that is deleted when the object dies.
class TempDir {
public:
    TempDir() {
        std::string pattern = (std::filesystem::temp_directory_path() / "score_db_XXXXXX").string();
        if (::mkdtemp(pattern.data()) == nullptr) { throw std::runtime_error("mkdtemp failed"); }
        path_ = pattern;
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

/// ISO date for a running index, strictly increasing in `i` and always a real
/// calendar date (12 months of 28 days per year, starting 2000-01-01).
inline std::string test_date(std::size_t i) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04zu-%02zu-%02zu", 2000 + i / 336, 1 + (i % 336) / 28, 1 + i % 28);
    return buf;
}

/// Bar number `i`: every field is a different function of `i`, so a test can
/// tell which row a value came from and catch columns that got out of step.
inline Bar test_bar(std::size_t i) {
    const auto x = static_cast<double>(i);
    return Bar{test_date(i), 100.0 + x, 200.0 + x, 50.0 + x, 150.0 + x, static_cast<std::int64_t>(1000 + i)};
}

/// Bars number first, first + 1, ..., first + n - 1.
inline std::vector<Bar> test_bars(std::size_t first, std::size_t n) {
    std::vector<Bar> bars;
    bars.reserve(n);
    for (std::size_t i = first; i < first + n; ++i) { bars.push_back(test_bar(i)); }
    return bars;
}

inline bool same_bar(const Bar& a, const Bar& b) {
    return a.date == b.date && a.open == b.open && a.high == b.high && a.low == b.low && a.close == b.close &&
           a.volume == b.volume;
}

/// Appends raw bytes to a file, bypassing the store. Used to fake the mess a
/// crashed writer leaves behind.
inline void append_raw(const std::filesystem::path& file, const std::string& bytes) {
    std::ofstream out(file, std::ios::binary | std::ios::app);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

inline std::string read_raw(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Exit codes used by run_in_child.
inline constexpr int kChildOk          = 0;
inline constexpr int kChildOtherError  = 2;
inline constexpr int kChildDomainError = 3;

/// Runs `body` in a forked child process and returns the child's pid. The
/// child never returns into the test framework: it exits with kChildOk,
/// kChildDomainError (body threw score::DomainError) or kChildOtherError.
template <typename F>
pid_t run_in_child(F&& body) {
    const pid_t pid = ::fork();
    if (pid != 0) { return pid; }
    int code = kChildOk;
    try {
        body();
    } catch (const score::DomainError&) { code = kChildDomainError; } catch (...) {
        code = kChildOtherError;
    }
    ::_exit(code);
}

/// Waits for a child started by run_in_child and returns its exit code (-1 if it was killed).
inline int wait_for_child(pid_t pid) {
    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) { return -1; }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/// Non-blocking check: true once the child has exited, with its exit code in `code`.
inline bool child_has_exited(pid_t pid, int& code) {
    int status = 0;
    if (::waitpid(pid, &status, WNOHANG) != pid) { return false; }
    code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return true;
}

}  // namespace score::db::testing
