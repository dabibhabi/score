# C++ Kernel Migration Notes

`DescriptiveStats::variance()` now keeps its central-moment computation next
to its empty-series policy, using standard C++ algorithms:

```cpp
const double sum_sq = std::accumulate(series_.cbegin(), series_.cend(), 0.0,
                                      [m](double sum, double value) {
                                          const double d = value - m;
                                          return sum + d * d;
                                      });
```

This replaces the deleted private C kernel and avoids a second language layer
without changing the C ABI.

---

# Storage engine v1 — snippets and reading

Companion to the "Storage engine v1" plan in [tasks/todo.md](todo.md). One section per phase.
Each section has the technique in its smallest generic form, the mistakes people usually make with
it, and two or three things to read. The snippets are deliberately **not** the bodies of the stubs
in [include/score/db/](../include/score/db/): they show the mechanism on a toy case, and fitting it
to the stub's contract is the exercise.

Every C++ block below compiles on its own with `clang++ -std=c++23`. Read one section, write the
functions of that phase, then move on. There is no need to read ahead.

## Phase A — dates as day counts

Used by: [src/db/date_codec.cpp](../src/db/date_codec.cpp).

`std::chrono` already knows the calendar. A `year_month_day` is three fields; a `sys_days` is a
count of days since 1970-01-01. Converting between them is the whole trick.

```cpp
#include <chrono>
#include <cstdint>

std::int32_t to_day_count(int y, unsigned m, unsigned d) {
    using namespace std::chrono;
    const year_month_day ymd{year{y}, month{m}, day{d}};
    // ymd.ok() is false for dates that do not exist, such as February 30.
    return static_cast<std::int32_t>(sys_days{ymd}.time_since_epoch().count());
}

std::chrono::year_month_day from_day_count(std::int32_t n) {
    using namespace std::chrono;
    return year_month_day{sys_days{days{n}}};
}
// int{ymd.year()}, unsigned{ymd.month()}, unsigned{ymd.day()} read the fields back out.
```

What goes wrong:

- Converting a `year_month_day` that is not `ok()` gives a number, not an error. Check first.
- `std::stoi` accepts `" 12"`, `"+1"` and `"12abc"`. `std::from_chars` does not skip whitespace, and
  its returned pointer tells you where parsing stopped, so you can demand that it consumed every character.
- Formatting needs zero padding: `std::format("{:04}-{:02}-{:02}", y, m, d)`.

Read:

- [cppreference: `std::chrono::year_month_day`](https://en.cppreference.com/w/cpp/chrono/year_month_day):
  the `ok()` member and the conversion to `sys_days`. Five minutes.
- [Howard Hinnant, "chrono-Compatible Low-Level Date Algorithms"](https://howardhinnant.github.io/date_algorithms.html):
  only the `days_from_civil` section. This is the arithmetic `sys_days` does for you; read it to
  see that a date-to-integer conversion is about ten lines with no tables.

## Phase A — fixed-width binary files

Used by: [src/db/column_file.cpp](../src/db/column_file.cpp).

Two ideas. First, a file format is defined in bytes, so a header is built byte by byte instead of
by dumping a struct (struct layout has padding and depends on the compiler). Second, `pwrite` can
write fewer bytes than you asked for, so it always sits in a loop.

```cpp
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <system_error>

// A 32-bit value as four little-endian bytes: lowest byte first.
std::array<std::uint8_t, 4> put_u32_le(std::uint32_t v) {
    return {static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v >> 8), static_cast<std::uint8_t>(v >> 16),
            static_cast<std::uint8_t>(v >> 24)};
}

std::uint32_t get_u32_le(const std::array<std::uint8_t, 4>& b) {
    return std::uint32_t{b[0]} | std::uint32_t{b[1]} << 8 | std::uint32_t{b[2]} << 16 | std::uint32_t{b[3]} << 24;
}

// Writes all `len` bytes at `offset`, or throws. Never returns after a partial write.
void pwrite_all(int fd, const std::byte* data, std::size_t len, off_t offset) {
    while (len > 0) {
        const ssize_t n = ::pwrite(fd, data, len, offset);
        if (n < 0) {
            if (errno == EINTR) { continue; }  // interrupted by a signal: just try again
            throw std::system_error(errno, std::generic_category(), "pwrite");
        }
        data += n;
        len -= static_cast<std::size_t>(n);
        offset += n;
    }
}
```

What goes wrong:

- Treating a short write as success. The file is then silently missing its tail.
- `pread` has one more case than `pwrite`: a return of `0` means end of file. In a "read exactly
  n bytes" loop that is an error, not something to retry.
- `write`/`read` move a shared file position; `pwrite`/`pread` take the offset as an argument and
  move nothing. With several threads or processes on one file, only the second pair is safe.
- Why the values themselves can be written with a plain `pwrite` of the array: on a little-endian
  machine the bytes in memory already are the file format. The `static_assert` in
  `column_file.hpp` is what makes that assumption explicit.
- Fixed width is what makes row `r` live at `16 + r * width`. No index is needed to find a row.

Read:

- [`pread(2)` / `pwrite(2)` manual page](https://man7.org/linux/man-pages/man2/pwrite.2.html):
  the "RETURN VALUE" section, for the short-write and end-of-file rules.
- [Apache Parquet: file format](https://parquet.apache.org/docs/file-format/): look at the diagram
  only. It is the industrial version of what you are building (magic number, column chunks, a
  footer with metadata). Notice what it has that v1 does not: everything in one file, and a footer
  instead of a header.

## Phase B — making an append survive a crash

Used by: [src/db/symbol_dir.cpp](../src/db/symbol_dir.cpp) and `ColumnStoreV1::append`.

A `write` only reaches the operating system's cache. Power loss before the cache is flushed loses
the data, and can leave six column files with six different lengths. The fix has two parts:
`fsync` forces the data to the device, and one small file (`COUNT`) says how many rows are real.
`COUNT` is replaced with a rename, which the file system guarantees is all-or-nothing.

The snippet is the general "atomically replace a small file" recipe, shown for a text file.

```cpp
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <string_view>
#include <system_error>

[[noreturn]] void fail(const char* what) { throw std::system_error(errno, std::generic_category(), what); }

void replace_file_atomically(const std::filesystem::path& target, std::string_view contents) {
    const std::filesystem::path tmp = target.string() + ".tmp";

    // 1. Write the new contents to a temporary file next to the target.
    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) { fail("open"); }
    if (::write(fd, contents.data(), contents.size()) != static_cast<ssize_t>(contents.size())) { fail("write"); }

    // 2. Force those bytes to the device BEFORE the rename. Otherwise a crash
    //    can leave the new name pointing at a file with no contents.
    if (::fsync(fd) != 0) { fail("fsync"); }
    ::close(fd);

    // 3. The commit point. Readers see the old file or the new one, never a mix.
    if (::rename(tmp.c_str(), target.c_str()) != 0) { fail("rename"); }

    // 4. The rename changed the directory, so the directory needs its own fsync.
    const int dir = ::open(target.parent_path().c_str(), O_RDONLY | O_CLOEXEC);
    if (dir < 0) { fail("open dir"); }
    if (::fsync(dir) != 0) { fail("fsync dir"); }
    ::close(dir);
}
```

(The snippet leaks `fd` when it throws and treats a short `write` as an error; use `UniqueFd` and
your write loop in the real code.)

What goes wrong:

- **Order.** Data files first, `fsync` them, and only then replace `COUNT`. If `COUNT` is durable
  before the rows it counts, a crash leaves a count that points at garbage.
- **macOS.** On macOS `fsync()` hands the data to the drive but does not ask the drive to empty its
  own write cache. `fcntl(fd, F_FULLFSYNC)` does. That is what `full_fsync` is for; on Linux the
  same function is a plain `fsync`.
- **Forgetting the directory.** Creating or renaming a file edits the directory. Without the
  directory `fsync` the file can vanish after a crash even though its contents were flushed.
- **Stale `COUNT.tmp`.** A crash between steps 1 and 3 leaves the temp file behind. It must never
  be read, and the next commit must be able to overwrite it (`O_TRUNC`).
- **Recovery is not a special mode.** There is no "repair on startup" step: every append begins by
  truncating the column files to `COUNT` rows, which is a no-op when nothing went wrong.

Read:

- [Dan Luu, "Files are hard"](https://danluu.com/file-consistency/): the opening and the
  "Filesystem semantics" section. It explains why every step above exists.
- [LWN, "Ensuring data reaches disk"](https://lwn.net/Articles/457667/): the parts about `fsync`
  and about newly created files. This is the recipe above, from the kernel side.
- [Apple `fsync(2)` manual page](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fsync.2.html):
  the paragraphs about `F_FULLFSYNC`.
- Optional, later: [SQLite, "Atomic Commit"](https://www.sqlite.org/atomiccommit.html) shows the
  same problem solved with a journal, which is where a write-ahead log would take this project.

## Phase C — reading without copying

Used by: [src/db/column_view.cpp](../src/db/column_view.cpp), `ColumnStoreV1::find_rows`, `scan`, `scan_column`.

`mmap` makes a file's bytes appear at an address. Nothing is read until you touch a page, and
nothing is copied into your own buffers: the span you return points at the operating system's cache.

```cpp
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <span>
#include <system_error>

// Maps a whole file read-only. Assumes the file is not empty: mmap rejects a length of 0.
class MappedFile {
public:
    explicit MappedFile(const char* path) {
        const int fd = ::open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) { throw std::system_error(errno, std::generic_category(), "open"); }
        struct stat st {};
        ::fstat(fd, &st);
        len_  = static_cast<std::size_t>(st.st_size);
        base_ = ::mmap(nullptr, len_, PROT_READ, MAP_PRIVATE, fd, /*offset=*/0);
        ::close(fd);  // the mapping stays valid after the descriptor is closed
        if (base_ == MAP_FAILED) { throw std::system_error(errno, std::generic_category(), "mmap"); }
    }
    ~MappedFile() { ::munmap(base_, len_); }
    MappedFile(const MappedFile&)            = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    std::span<const std::byte> bytes() const { return {static_cast<const std::byte*>(base_), len_}; }

private:
    void*       base_ = nullptr;
    std::size_t len_  = 0;
};
```

Finding a date range in a sorted column is two binary searches. `lower_bound` finds the first
element not less than the start; `upper_bound` finds the first element greater than the end. That
pair gives an inclusive range with no special cases for dates that are not stored.

```cpp
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

// Index of the first element in [lo, hi] and how many there are. `sorted` must be ascending.
std::pair<std::size_t, std::size_t> inclusive_range(std::span<const std::int32_t> sorted, std::int32_t lo,
                                                     std::int32_t hi) {
    const auto first = std::ranges::lower_bound(sorted, lo);
    const auto last  = std::ranges::upper_bound(sorted, hi);
    if (last <= first) { return {0, 0}; }
    return {static_cast<std::size_t>(first - sorted.begin()), static_cast<std::size_t>(last - first)};
}
```

What goes wrong:

- **The offset must be a multiple of the page size** (`sysconf(_SC_PAGESIZE)`: 4 KiB on x86-64,
  16 KiB on Apple silicon). To map from byte 54,000 you map from the page boundary below it and
  add the difference to the returned pointer. `munmap` needs the original pointer and length, which
  is why `ColumnView` stores `map_base_` separately from `data_`.
- **Alignment.** Reading the bytes as `const double*` needs an address divisible by 8. The mapping
  starts on a page boundary and the header is 16 bytes, so row 0 is aligned; that is the reason the
  header is not 7 or 10 bytes.
- **Length 0** is an error (`EINVAL`). An empty range must return an empty view without calling `mmap`.
- **A file that shrinks under a mapping** kills the process with `SIGBUS` on the next read. v1 is
  safe because committed rows are never removed: writers only truncate rows past `COUNT`, and
  readers never map past the `COUNT` they read.
- **Read `COUNT` once per query.** Reading it twice can give two different answers if an append
  commits in between, and the columns of one result would then disagree.

Read:

- [`mmap(2)` manual page](https://man7.org/linux/man-pages/man2/mmap.2.html): the description of
  `offset`, `MAP_PRIVATE` against `MAP_SHARED`, and what it says about `SIGBUS`.
- [cppreference: `std::lower_bound`](https://en.cppreference.com/w/cpp/algorithm/lower_bound): the
  definition and the example; `upper_bound` is linked from it.
- [Crotty, Leis and Pavlo, "Are You Sure You Want to Use MMAP in Your Database Management System?"](https://db.cs.cmu.edu/mmap-cidr2022/):
  watch the video on that page or read the paper linked from it. It lists the reasons real engines avoid `mmap`. Then
  decide for yourself which of them apply to a store that is read-only through its mappings and
  never updates in place. That question goes in the Review section of the todo.

## Phase D — one writer per symbol

Used by: `SymbolLock` in [src/db/symbol_dir.cpp](../src/db/symbol_dir.cpp), and the fork-based tests.

`flock` puts an advisory lock on an open file. "Advisory" means it stops only other code that also
asks for the lock, which is all the store needs.

```cpp
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cerrno>
#include <system_error>

// Runs `critical_section` while holding an exclusive lock on `lock_path`.
template <typename F>
void with_file_lock(const char* lock_path, F&& critical_section) {
    const int fd = ::open(lock_path, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd < 0) { throw std::system_error(errno, std::generic_category(), "open"); }

    while (::flock(fd, LOCK_EX) != 0) {  // blocks until no one else holds it
        if (errno != EINTR) {
            ::close(fd);
            throw std::system_error(errno, std::generic_category(), "flock");
        }
    }
    critical_section();
    ::close(fd);  // closing the descriptor releases the lock
}
```

(If `critical_section` throws, this version never closes `fd`. An RAII class fixes that, and is
what `SymbolLock` should be.)

What goes wrong:

- **The lock belongs to the open file, not to the process or thread.** Two separate `open` calls
  on the same path conflict with each other, even inside one process. That is what makes one
  `SymbolLock` per append correct for threads as well as processes. A descriptor copied with `dup`
  or inherited through `fork` shares the lock instead of conflicting, so take the lock after
  forking, never before.
- **The process dying releases the lock**, because the kernel closes its descriptors. A crashed
  writer therefore cannot block the store forever. No lock file needs deleting, and deleting `LOCK`
  while someone holds it would break the exclusion.
- **Checking before locking.** "Read `COUNT`, then take the lock, then write" lets two writers both
  read the old count. Everything that depends on `COUNT` happens inside the lock.
- **Network file systems.** `flock` over NFS is unreliable. v1 assumes a local disk.
- In the tests, the child process must leave with `_exit`, never by returning into Catch2;
  `run_in_child` in [tests/cpp/db_test_util.hpp](../tests/cpp/db_test_util.hpp) does this for you.

Read:

- [`flock(2)` manual page](https://man7.org/linux/man-pages/man2/flock.2.html): the
  paragraphs about duplicated descriptors and `fork`.
- [Victor Gaydov, "File locking in Linux"](https://gavv.net/articles/file-locks/): the "Common
  features", "Differing features" and "BSD locks (flock)" sections. It explains why `flock` was chosen over `fcntl` locks,
  whose release-on-any-close rule is a well-known trap.

## Phase E — handing mapped memory to NumPy

Used by: the `scan_column` binding in [src/bindings/nanobindings.cpp](../src/bindings/nanobindings.cpp).

A NumPy array can point at memory it does not own, as long as something keeps that memory alive.
For `scan_column` the owner is the `ColumnView`: move it to the heap and give NumPy a capsule that
deletes it when the array is garbage-collected. The snippet shows the pattern with a plain vector.

```cpp
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>

#include <cstddef>
#include <vector>

namespace nb = nanobind;

nb::ndarray<nb::numpy, const double, nb::ndim<1>> make_view(std::size_t n) {
    // The object that owns the memory, moved to the heap so it can outlive this call.
    auto* owner_object = new std::vector<double>(n, 1.5);

    // The capsule destroys it when Python drops the last reference to the array.
    nb::capsule owner(owner_object, [](void* p) noexcept { delete static_cast<std::vector<double>*>(p); });

    return nb::ndarray<nb::numpy, const double, nb::ndim<1>>(owner_object->data(), {owner_object->size()}, owner);
}

NB_MODULE(snippet_example, m) { m.def("make_view", &make_view); }
```

What goes wrong:

- **Returning a view of a local.** If the array points into a `ColumnView` that is destroyed when
  the binding returns, Python reads unmapped memory and the interpreter crashes.
- **Writable arrays.** The mapping is `PROT_READ`; writing through the array would crash. The
  element type here is `const double`. Add a Python test that the returned array has
  `flags.writeable == False`, and that assigning to an element raises.
- **Holding the GIL during a slow scan.** Not a problem at daily-bar sizes; noted for later.
- The risk functions are bound by function pointer today. After they change to
  `std::span<const double>` (Phase C), nanobind has no automatic conversion for `std::span`, so
  those bindings need a small lambda taking `const std::vector<double>&`.

Read:

- [nanobind: the `nb::ndarray<..>` class](https://nanobind.readthedocs.io/en/latest/ndarray.html):
  the sections "Returning arrays from C++ to Python" and "Data ownership". The `nb::capsule`
  example there is the same pattern as above.

## Phase F — Postgres as oracle and baseline

Used by: [tests/python/test_pg_oracle.py](../tests/python/test_pg_oracle.py) and
[tests/cpp/bench_db.cpp](../tests/cpp/bench_db.cpp).

Postgres plays two roles. As an oracle it is assumed correct: load the same bars, ask the same
question, and any difference is a bug in the column store. As a baseline it is the number to
compare against, on stock settings, with the obvious schema.

These statements were written from the Postgres documentation and have **not** been run here
(no Postgres server is installed on this machine). Run them once by hand before trusting them.

```sql
CREATE TABLE bars (
    symbol text   NOT NULL,
    date   date   NOT NULL,
    open   float8 NOT NULL,
    high   float8 NOT NULL,
    low    float8 NOT NULL,
    close  float8 NOT NULL,
    volume bigint NOT NULL,
    PRIMARY KEY (symbol, date)   -- a B-tree: finds one symbol's date range directly
);

-- Bulk load from a CSV file on the client (psql meta-command; far faster than INSERT per row).
\copy bars FROM 'bars.csv' WITH (FORMAT csv, HEADER true)
ANALYZE bars;

-- Time a query and see how it was executed.
\timing on
EXPLAIN (ANALYZE, BUFFERS)
SELECT close FROM bars
WHERE symbol = 'SYM7' AND date BETWEEN '2009-01-01' AND '2009-12-31'
ORDER BY date;

-- The cross-section, which should favour Postgres over one-directory-per-symbol.
EXPLAIN (ANALYZE, BUFFERS)
SELECT symbol, close FROM bars WHERE date = '2009-12-31';
```

Making the comparison fair:

- **Same question.** `scan_column` returns one column; compare it with `SELECT close`, not `SELECT *`.
- **Cold and warm.** Run each query once after emptying the operating system's cache (`sudo purge`
  on macOS; `sync; echo 3 | sudo tee /proc/sys/vm/drop_caches` on Linux) and restarting Postgres,
  then several more times warm. Report both. A warm column scan is mostly a memory copy.
- **Durability on both sides.** The store's ingest number includes `F_FULLFSYNC`. Postgres with
  default settings also flushes on commit. Loading everything in one transaction versus one per
  symbol changes the result more than any tuning would; say which one you measured.
- **Client overhead.** A Postgres query pays for a socket round trip and row serialization. That is
  a real cost of the architecture, not an unfair handicap, but it is worth measuring separately with
  `EXPLAIN ANALYZE` (server time) against the client's wall clock.
- **Run benchmarks from a Release build.** `make build` already configures `CMAKE_BUILD_TYPE=Release`.

Read:

- [PostgreSQL manual: "Populating a Database"](https://www.postgresql.org/docs/current/populate.html):
  one page; why `COPY` in one transaction, and why `ANALYZE` afterwards.
- [Catch2: authoring benchmarks](https://github.com/catchorg/Catch2/blob/devel/docs/benchmarks.md):
  `BENCHMARK` against `BENCHMARK_ADVANCED`, and how returning a value from the benchmark body keeps
  the optimizer from deleting the work.
- [PostgreSQL manual: BRIN indexes](https://www.postgresql.org/docs/current/brin.html): the
  introduction only. A BRIN index keeps a min and max per block range, which is the same idea as
  "the date column is sorted, so binary search it". Worth knowing as the tuning option you chose
  not to pursue.
- For the oracle test's loader: [psycopg 3: using COPY](https://www.psycopg.org/psycopg3/docs/basic/copy.html).
