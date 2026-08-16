#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace NekoArchive {

class Decompressor {
public:
    Decompressor();
    ~Decompressor();

    void set_password(const std::string& password);
    void set_thread_count(int threads);

    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input, size_t original_size);
    std::vector<uint8_t> decompress_file(const std::string& filepath);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace NekoArchive
