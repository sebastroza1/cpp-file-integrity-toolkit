#pragma once

#include <filesystem>
#include <string>

namespace filetool {

std::string sha256_file(const std::filesystem::path& path);

}  // namespace filetool

