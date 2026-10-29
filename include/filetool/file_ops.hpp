#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace filetool {

struct VerificationResult {
    std::filesystem::path path;
    bool ok;
    std::string detail;
};

void create_manifest(const std::filesystem::path& manifest,
                     const std::vector<std::filesystem::path>& files);
std::vector<VerificationResult> verify_manifest(const std::filesystem::path& manifest);
void verified_copy(const std::filesystem::path& source,
                   const std::filesystem::path& destination,
                   std::optional<std::uintmax_t> interrupt_after = std::nullopt);
void run_benchmark(const std::filesystem::path& workdir,
                   std::uintmax_t size_bytes,
                   unsigned iterations,
                   std::ostream& output);

}  // namespace filetool

