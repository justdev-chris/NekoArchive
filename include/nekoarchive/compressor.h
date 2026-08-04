#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace NekoArchive {

enum class CompressionMode : uint8_t {
    HARE = 0,
    CAT = 1,
    TIGER = 2
};

class Compressor {
public:
    Compressor(CompressionMode mode = CompressionMode::CAT);
    ~Compressor();

    void set_mode(CompressionMode mode);
    void set_password(const std::string& password);
    void set_thread_count(int threads);

    std::vector<uint8_t> compress(const std::vector<uint8_t>& input);
    std::vector<uint8_t> compress_file(const std::string& filepath);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace NekoArchive
