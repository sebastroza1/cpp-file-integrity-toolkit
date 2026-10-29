# C++ File Integrity Toolkit

A cross-platform C++20 command-line toolkit for streaming SHA-256 verification,
portable manifests, failure-safe copies, and bounded integrity benchmarks. It is
designed as a small, inspectable storage-engineering artifact: data is processed
in 1 MiB chunks, destinations are never silently overwritten, partial copies are
removed, and a copied file is hashed before its temporary name is atomically
renamed within the destination filesystem.

## Engineering evidence

- Uses trusted platform cryptography: Windows CNG (`BCrypt`) or OpenSSL EVP on Linux.
- Handles empty, multi-chunk, missing, modified, and truncated files.
- Supports spaces and common special characters in paths.
- Detects simulated mid-copy interruption and removes partial output.
- Builds with CMake and runs without third-party argument or test frameworks.
- Includes Windows and Ubuntu CI definitions.

## Build and test

Windows (Visual Studio generator):

```powershell
cmake -S . -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Linux:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Linux requires OpenSSL development headers. Windows links to `bcrypt.lib`, part
of the Windows SDK.

## Commands

```text
filetool hash <file>
filetool manifest <manifest> <file>...
filetool verify <manifest>
filetool copy <source> <destination> [--interrupt-after <bytes>]
filetool benchmark <workdir> <size-bytes> <iterations>
```

`verify` exits with code 1 when any record fails. `copy` writes a unique temporary
file beside the destination, verifies SHA-256, then renames it. `--interrupt-after`
is an explicit fault-injection option for demonstrating cleanup behavior.

The manifest is a deliberately simple, versioned UTF-8 TSV format:

```text
FILETOOL-MANIFEST<TAB>1
<sha256><TAB><size-bytes><TAB><path-relative-to-manifest>
```

Tabs and newlines in file names are rejected because they are record delimiters.
Absolute and parent-traversal paths are also rejected, so verification cannot
escape the manifest directory.

## Benchmark safety and interpretation

The benchmark creates deterministic data, hashes it 1–20 times, emits CSV, and
removes its fixture. Input size is capped at 512 MiB. Results measure end-to-end
hashing through the operating-system file cache; they are not raw-disk benchmarks
and should not be compared across machines without controlling cache, power, and
filesystem conditions. See [reports/RESULTS.md](reports/RESULTS.md) for the locally
executed evidence.

## Failure model and limitations

- Rename atomicity is assumed only within the same destination filesystem.
- `flush()`/close errors are checked, but this tool does not issue an OS-level
  durable `fsync`/`FlushFileBuffers`; power-loss durability is therefore not claimed.
- Symlink and concurrent-source-mutation semantics are not hardened for hostile use.
- Windows and Linux crypto backends should produce identical SHA-256 output, but
  the repository's recorded benchmark is environment-specific.
- This is a portfolio utility, not a replacement for filesystem snapshots, signed
  manifests, or an end-to-end storage protocol.

## Repository layout

```text
include/filetool/  Public interfaces
src/               CLI, streaming crypto, and file operations
tests/             Dependency-free CTest executable
scripts/           Repeatable local build entry points
reports/           Executed benchmark and validation record
.github/workflows/ Cross-platform CI definition
```

## License

MIT

