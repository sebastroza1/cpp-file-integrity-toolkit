#include "filetool/sha256.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#endif

namespace filetool {
namespace {
constexpr std::size_t kChunkSize = 1024 * 1024;

std::ifstream open_input(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open input file: " + path.string());
    }
    return input;
}

std::string to_hex(const unsigned char* bytes, std::size_t length) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < length; ++i) {
        output << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    return output.str();
}
}  // namespace

std::string sha256_file(const std::filesystem::path& path) {
    auto input = open_input(path);
    // A 1 MiB local array overflows the default Windows thread stack. Keep the
    // streaming buffer on the heap so chunk size is independent of stack limits.
    std::vector<char> buffer(kChunkSize);

#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD result_size = 0;
    std::vector<unsigned char> object;
    std::array<unsigned char, 32> digest{};

    auto check = [](NTSTATUS status, const char* operation) {
        if (status < 0) {
            throw std::runtime_error(std::string("Windows CNG failure: ") + operation);
        }
    };

    try {
        check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0),
              "open SHA-256 provider");
        check(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                                reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
                                &result_size, 0),
              "query hash object size");
        object.resize(object_size);
        check(BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0),
              "create hash");
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto count = input.gcount();
            if (count > 0) {
                check(BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                                     static_cast<ULONG>(count), 0),
                      "update hash");
            }
        }
        if (!input.eof()) {
            throw std::runtime_error("read failure: " + path.string());
        }
        check(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0),
              "finish hash");
    } catch (...) {
        if (hash != nullptr) BCryptDestroyHash(hash);
        if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0);
        throw;
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return to_hex(digest.data(), digest.size());
#else
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr) {
        throw std::runtime_error("OpenSSL could not allocate a digest context");
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_length = 0;
    try {
        if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
            throw std::runtime_error("OpenSSL could not initialize SHA-256");
        }
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto count = input.gcount();
            if (count > 0 &&
                EVP_DigestUpdate(context, buffer.data(), static_cast<std::size_t>(count)) != 1) {
                throw std::runtime_error("OpenSSL could not update SHA-256");
            }
        }
        if (!input.eof()) {
            throw std::runtime_error("read failure: " + path.string());
        }
        if (EVP_DigestFinal_ex(context, digest.data(), &digest_length) != 1) {
            throw std::runtime_error("OpenSSL could not finalize SHA-256");
        }
    } catch (...) {
        EVP_MD_CTX_free(context);
        throw;
    }
    EVP_MD_CTX_free(context);
    return to_hex(digest.data(), digest_length);
#endif
}

}  // namespace filetool

