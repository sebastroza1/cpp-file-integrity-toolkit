#include "filetool/file_ops.hpp"
#include "filetool/sha256.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void usage() {
    std::cerr << "Usage:\n"
              << "  filetool hash <file>\n"
              << "  filetool manifest <manifest> <file>...\n"
              << "  filetool verify <manifest>\n"
              << "  filetool copy <source> <destination> [--interrupt-after <bytes>]\n"
              << "  filetool benchmark <workdir> <size-bytes> <iterations>\n";
}
}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 3) {
            usage();
            return 2;
        }
        const std::string command = argv[1];
        if (command == "hash" && argc == 3) {
            std::cout << filetool::sha256_file(argv[2]) << "  " << argv[2] << '\n';
            return 0;
        }
        if (command == "manifest" && argc >= 4) {
            std::vector<std::filesystem::path> files;
            for (int i = 3; i < argc; ++i) files.emplace_back(argv[i]);
            filetool::create_manifest(argv[2], files);
            std::cout << "created " << argv[2] << " with " << files.size() << " record(s)\n";
            return 0;
        }
        if (command == "verify" && argc == 3) {
            bool all_ok = true;
            for (const auto& result : filetool::verify_manifest(argv[2])) {
                std::cout << (result.ok ? "OK" : "FAIL") << '\t' << result.path.string()
                          << '\t' << result.detail << '\n';
                all_ok = all_ok && result.ok;
            }
            return all_ok ? 0 : 1;
        }
        if (command == "copy" && (argc == 4 || argc == 6)) {
            std::optional<std::uintmax_t> interrupt_after;
            if (argc == 6) {
                if (std::string(argv[4]) != "--interrupt-after") {
                    throw std::runtime_error("expected --interrupt-after <bytes>");
                }
                interrupt_after = std::stoull(argv[5]);
            }
            filetool::verified_copy(argv[2], argv[3], interrupt_after);
            std::cout << "verified copy complete\n";
            return 0;
        }
        if (command == "benchmark" && argc == 5) {
            filetool::run_benchmark(argv[2], std::stoull(argv[3]),
                                    static_cast<unsigned>(std::stoul(argv[4])), std::cout);
            return 0;
        }
        usage();
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}

