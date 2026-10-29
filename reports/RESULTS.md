# Validation results

Executed on 2026-10-29 on Windows 11 (build 22631), an AMD Ryzen 5 3550H, and
MSVC 19.29.30159. CMake selected Windows SDK 10.0.19041.0. These are local
results; GitHub Actions and the Linux/OpenSSL backend were not executed here.

## Build and tests

```text
cmake -S . -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

1/1 Test #1: filetool_tests ... Passed
100% tests passed, 0 tests failed out of 1
```

The test executable covers known SHA-256 vectors, a file spanning multiple
chunks, missing files, modification and truncation detection, paths with spaces
and brackets, overwrite refusal, path traversal rejection, copy interruption
cleanup, missing destination directories, and benchmark cleanup. A meaningful
permission-denied fixture was not forced on this Windows host because ACL changes
would make the test environment-dependent.

During initial validation, the 1 MiB stream buffer was allocated on the stack and
exceeded the default Windows thread stack. The failed test exposed the issue; the
buffer was moved to heap storage and the full suite then passed. This is retained
as engineering evidence rather than hidden from the validation record.

## SHA-256 benchmark

Release build, deterministic input, three iterations per size:

| Size | Iteration 1 | Iteration 2 | Iteration 3 |
| ---: | ---: | ---: | ---: |
| 1 MiB | 74.786 MiB/s | 506.355 MiB/s | 501.580 MiB/s |
| 16 MiB | 396.544 MiB/s | 632.039 MiB/s | 631.615 MiB/s |
| 64 MiB | 364.127 MiB/s | 633.758 MiB/s | 596.445 MiB/s |

Every iteration for a given size produced the same digest. The slower first
iteration and faster subsequent passes are consistent with startup and operating-
system cache effects; the data does not establish raw storage throughput. The
tool deleted each benchmark fixture after its run.
