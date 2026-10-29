#include "filetool/file_ops.hpp"

#include "filetool/sha256.hpp"

#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace filetool {
namespace {
constexpr std::size_t kChunkSize = 1024 * 1024;
constexpr std::uintmax_t kMaxBenchmarkSize = 512ULL * 1024ULL * 1024ULL;

void validate_manifest_path(const std::filesystem::path& path) {
    const auto text = path.generic_string();
    if (text.find('\t') != std::string::npos || text.find('\n') != std::string::npos ||
        text.find('\r') != std::string::npos) {
        throw std::runtime_error("manifest paths cannot contain tabs or newlines");
    }
    if (path.empty() || path.is_absolute()) {
        throw std::runtime_error("manifest path must be relative");
    }
    for (const auto& component : path.lexically_normal()) {
        if (component == "..") {
            throw std::runtime_error("manifest path cannot escape its directory");
        }
    }
}

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, '\t')) fields.push_back(field);
    return fields;
}

void write_pattern_file(const std::filesystem::path& path, std::uintmax_t size) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot create benchmark file: " + path.string());
    std::vector<char> buffer(kChunkSize);
    std::uint32_t state = 0xC0FFEEu;
    for (auto& byte : buffer) {
        state = state * 1664525u + 1013904223u;
        byte = static_cast<char>(state >> 24);
    }
    std::uintmax_t written = 0;
    while (written < size) {
        const auto count = static_cast<std::streamsize>(
            std::min<std::uintmax_t>(buffer.size(), size - written));
        output.write(buffer.data(), count);
        if (!output) throw std::runtime_error("benchmark write failure");
        written += static_cast<std::uintmax_t>(count);
    }
}
}  // namespace

void create_manifest(const std::filesystem::path& manifest,
                     const std::vector<std::filesystem::path>& files) {
    if (files.empty()) throw std::runtime_error("manifest requires at least one file");
    const auto parent = std::filesystem::absolute(manifest).parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        throw std::runtime_error("manifest parent directory does not exist");
    }
    const auto temporary = manifest.string() + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot create manifest temporary file");
    try {
        output << "FILETOOL-MANIFEST\t1\n";
        for (const auto& file : files) {
            if (!std::filesystem::is_regular_file(file)) {
                throw std::runtime_error("not a regular file: " + file.string());
            }
            auto relative = std::filesystem::relative(std::filesystem::absolute(file), parent);
            validate_manifest_path(relative);
            output << sha256_file(file) << '\t' << std::filesystem::file_size(file) << '\t'
                   << relative.generic_string() << '\n';
        }
        output.close();
        if (!output) throw std::runtime_error("failed to flush manifest");
        if (std::filesystem::exists(manifest)) {
            throw std::runtime_error("refusing to overwrite existing manifest");
        }
        std::filesystem::rename(temporary, manifest);
    } catch (...) {
        output.close();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

std::vector<VerificationResult> verify_manifest(const std::filesystem::path& manifest) {
    std::ifstream input(manifest, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open manifest: " + manifest.string());
    std::string line;
    if (!std::getline(input, line) || line != "FILETOOL-MANIFEST\t1") {
        throw std::runtime_error("unsupported or malformed manifest header");
    }
    std::vector<VerificationResult> results;
    const auto parent = std::filesystem::absolute(manifest).parent_path();
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        const auto fields = split_tabs(line);
        if (fields.size() != 3 || fields[0].size() != 64) {
            throw std::runtime_error("malformed manifest record");
        }
        validate_manifest_path(std::filesystem::path(fields[2]));
        const auto path = parent / std::filesystem::path(fields[2]);
        try {
            const auto expected_size = static_cast<std::uintmax_t>(std::stoull(fields[1]));
            if (!std::filesystem::is_regular_file(path)) {
                results.push_back({path, false, "missing or not a regular file"});
            } else if (std::filesystem::file_size(path) != expected_size) {
                results.push_back({path, false, "size mismatch"});
            } else if (sha256_file(path) != fields[0]) {
                results.push_back({path, false, "SHA-256 mismatch"});
            } else {
                results.push_back({path, true, "ok"});
            }
        } catch (const std::exception& error) {
            results.push_back({path, false, error.what()});
        }
    }
    if (!input.eof()) throw std::runtime_error("manifest read failure");
    return results;
}

void verified_copy(const std::filesystem::path& source,
                   const std::filesystem::path& destination,
                   std::optional<std::uintmax_t> interrupt_after) {
    if (!std::filesystem::is_regular_file(source)) {
        throw std::runtime_error("source is not a regular file");
    }
    if (std::filesystem::exists(destination)) {
        throw std::runtime_error("refusing to overwrite destination");
    }
    const auto parent = std::filesystem::absolute(destination).parent_path();
    if (!std::filesystem::is_directory(parent)) {
        throw std::runtime_error("destination parent directory does not exist");
    }
    const auto temporary = destination.string() + ".tmp." +
                           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::ifstream input(source, std::ios::binary);
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!input || !output) throw std::runtime_error("cannot open copy streams");
    std::vector<char> buffer(kChunkSize);
    std::uintmax_t copied = 0;
    try {
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto count = input.gcount();
            if (count <= 0) break;
            if (interrupt_after && copied + static_cast<std::uintmax_t>(count) > *interrupt_after) {
                throw std::runtime_error("simulated interrupted copy");
            }
            output.write(buffer.data(), count);
            if (!output) throw std::runtime_error("copy write failure");
            copied += static_cast<std::uintmax_t>(count);
        }
        if (!input.eof()) throw std::runtime_error("copy read failure");
        output.close();
        if (!output) throw std::runtime_error("failed to flush copied data");
        if (sha256_file(source) != sha256_file(temporary)) {
            throw std::runtime_error("copy checksum verification failed");
        }
        std::filesystem::rename(temporary, destination);
    } catch (...) {
        input.close();
        output.close();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

void run_benchmark(const std::filesystem::path& workdir,
                   std::uintmax_t size_bytes,
                   unsigned iterations,
                   std::ostream& output) {
    if (size_bytes > kMaxBenchmarkSize) throw std::runtime_error("benchmark size exceeds 512 MiB safety limit");
    if (iterations == 0 || iterations > 20) throw std::runtime_error("iterations must be between 1 and 20");
    std::filesystem::create_directories(workdir);
    const auto path = workdir / "filetool-benchmark.bin";
    write_pattern_file(path, size_bytes);
    output << "iteration,size_bytes,elapsed_ms,throughput_mib_s,sha256\n";
    try {
        for (unsigned iteration = 1; iteration <= iterations; ++iteration) {
            const auto start = std::chrono::steady_clock::now();
            const auto digest = sha256_file(path);
            const auto elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
            const auto seconds = elapsed / 1000.0;
            const auto throughput = seconds > 0.0
                                        ? (static_cast<double>(size_bytes) / 1048576.0) / seconds
                                        : 0.0;
            output << iteration << ',' << size_bytes << ',' << std::fixed << std::setprecision(3)
                   << elapsed << ',' << throughput << ',' << digest << '\n';
        }
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        throw;
    }
    std::filesystem::remove(path);
}

}  // namespace filetool

