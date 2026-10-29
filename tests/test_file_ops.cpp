#include "filetool/file_ops.hpp"
#include "filetool/sha256.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

template <typename Function>
void expect_throws(Function function, const std::string& message) {
    try {
        function();
        expect(false, message);
    } catch (const std::exception&) {
    }
}

void write_file(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output) throw std::runtime_error("test fixture write failed");
}
}  // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "filetool tests with spaces";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root / "special [files]");
    try {
        const auto empty = root / "empty.bin";
        const auto abc = root / "special [files]" / "abc data.txt";
        write_file(empty, "");
        write_file(abc, "abc");
        expect(filetool::sha256_file(empty) ==
                   "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
               "empty-file SHA-256 known vector");
        expect(filetool::sha256_file(abc) ==
                   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
               "abc SHA-256 known vector");
        expect_throws([&] { filetool::sha256_file(root / "missing"); },
                      "missing input must fail");

        const auto large = root / "large.bin";
        write_file(large, std::string(3 * 1024 * 1024 + 17, 'x'));
        expect(filetool::sha256_file(large).size() == 64, "large file streams successfully");

        const auto manifest = root / "manifest.tsv";
        filetool::create_manifest(manifest, {empty, abc, large});
        const auto initial = filetool::verify_manifest(manifest);
        expect(initial.size() == 3, "manifest has three records");
        for (const auto& item : initial) expect(item.ok, "initial manifest verification succeeds");
        write_file(abc, "abd");
        const auto modified = filetool::verify_manifest(manifest);
        expect(!modified[1].ok && modified[1].detail == "SHA-256 mismatch",
               "same-size modification is detected");
        write_file(large, "short");
        const auto truncated = filetool::verify_manifest(manifest);
        expect(!truncated[2].ok && truncated[2].detail == "size mismatch",
               "truncation is detected");

        const auto unsafe_manifest = root / "unsafe.tsv";
        write_file(unsafe_manifest,
                   "FILETOOL-MANIFEST\t1\n"
                   "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\t0\t../escape\n");
        expect_throws([&] { filetool::verify_manifest(unsafe_manifest); },
                      "manifest traversal is rejected");

        write_file(abc, "abc");
        const auto destination = root / "copied file.bin";
        filetool::verified_copy(abc, destination);
        expect(filetool::sha256_file(abc) == filetool::sha256_file(destination),
               "verified copy preserves content");
        expect_throws([&] { filetool::verified_copy(abc, destination); },
                      "copy refuses overwrite");

        const auto interrupted = root / "interrupted.bin";
        expect_throws([&] { filetool::verified_copy(large, interrupted, 1); },
                      "simulated interruption fails");
        expect(!std::filesystem::exists(interrupted), "interruption leaves no destination");
        bool temp_found = false;
        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            if (entry.path().filename().string().find("interrupted.bin.tmp.") == 0) temp_found = true;
        }
        expect(!temp_found, "interruption removes temporary copy");
        expect_throws([&] { filetool::verified_copy(abc, root / "missing-parent" / "x"); },
                      "missing destination parent fails");

        std::ostringstream benchmark;
        filetool::run_benchmark(root / "bench", 1024 * 1024, 2, benchmark);
        expect(benchmark.str().find("throughput_mib_s") != std::string::npos,
               "bounded benchmark emits CSV");
        expect(!std::filesystem::exists(root / "bench" / "filetool-benchmark.bin"),
               "benchmark removes its fixture");
    } catch (const std::exception& error) {
        ++failures;
        std::cerr << "UNEXPECTED: " << error.what() << '\n';
    }
    std::filesystem::remove_all(root, ignored);
    if (failures == 0) std::cout << "All filetool tests passed\n";
    return failures == 0 ? 0 : 1;
}

