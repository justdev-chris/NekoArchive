#include "nekoarchive/archive.h"
#include <fstream>
#include <filesystem>
#include <zstd.h>
#include <lzma.h>
#include <openssl/evp.h>
#include <xxhash.h>

namespace NekoArchive {

struct Archive::Impl {
    std::vector<FileEntry> entries;
    std::string password;
    CompressionMode mode;
    
    bool write_header(std::ofstream& file) {
        // Magic "NEKO"
        uint32_t magic = 0x4F4B454E;
        file.write(reinterpret_cast<const char*>(&magic), 4);
        
        // Version
        uint8_t version = 1;
        file.write(reinterpret_cast<const char*>(&version), 1);
        
        // Mode
        uint8_t mode_byte = static_cast<uint8_t>(mode);
        file.write(reinterpret_cast<const char*>(&mode_byte), 1);
        
        // File count
        uint64_t count = entries.size();
        file.write(reinterpret_cast<const char*>(&count), 8);
        
        return true;
    }
    
    bool read_header(std::ifstream& file) {
        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), 4);
        if (magic != 0x4F4B454E) return false;
        
        uint8_t version;
        file.read(reinterpret_cast<char*>(&version), 1);
        
        uint8_t mode_byte;
        file.read(reinterpret_cast<char*>(&mode_byte), 1);
        mode = static_cast<CompressionMode>(mode_byte);
        
        uint64_t count;
        file.read(reinterpret_cast<char*>(&count), 8);
        
        entries.resize(count);
        for (size_t i = 0; i < count; ++i) {
            uint16_t name_len;
            file.read(reinterpret_cast<char*>(&name_len), 2);
            entries[i].name.resize(name_len);
            file.read(&entries[i].name[0], name_len);
            file.read(reinterpret_cast<char*>(&entries[i].original_size), 8);
            file.read(reinterpret_cast<char*>(&entries[i].compressed_size), 8);
            file.read(reinterpret_cast<char*>(&entries[i].offset), 8);
            file.read(reinterpret_cast<char*>(&entries[i].crc32), 4);
        }
        
        return true;
    }
    
    std::vector<uint8_t> compress_data(const std::vector<uint8_t>& input) {
        if (input.empty()) return {};
        
        size_t bound = ZSTD_compressBound(input.size());
        std::vector<uint8_t> output(bound);
        
        size_t compressed_size = ZSTD_compress(
            output.data(), bound,
            input.data(), input.size(),
            3
        );
        
        if (ZSTD_isError(compressed_size)) {
            return {};
        }
        
        output.resize(compressed_size);
        return output;
    }
    
    std::vector<uint8_t> decompress_data(const std::vector<uint8_t>& input, size_t original_size) {
        if (input.empty() || original_size == 0) return {};
        
        std::vector<uint8_t> output(original_size);
        size_t decompressed_size = ZSTD_decompress(
            output.data(), original_size,
            input.data(), input.size()
        );
        
        if (ZSTD_isError(decompressed_size)) {
            return {};
        }
        
        output.resize(decompressed_size);
        return output;
    }
    
    uint32_t calculate_crc32(const std::vector<uint8_t>& data) {
        return XXH32(data.data(), data.size(), 0);
    }
};

Archive::Archive() : impl(std::make_unique<Impl>()) {}
Archive::~Archive() = default;

bool Archive::create(const std::string& output_path, const std::vector<std::string>& input_files) {
    std::ofstream file(output_path, std::ios::binary);
    if (!file) return false;
    
    impl->entries.clear();
    
    // Read and compress each file
    for (const auto& filepath : input_files) {
        std::ifstream input(filepath, std::ios::binary);
        if (!input) continue;
        
        input.seekg(0, std::ios::end);
        size_t size = input.tellg();
        input.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(size);
        input.read(reinterpret_cast<char*>(data.data()), size);
        
        std::vector<uint8_t> compressed = impl->compress_data(data);
        
        FileEntry entry;
        entry.name = std::filesystem::path(filepath).filename().string();
        entry.original_size = size;
        entry.compressed_size = compressed.size();
        entry.offset = 0; // Will be set after writing
        entry.crc32 = impl->calculate_crc32(data);
        
        impl->entries.push_back(entry);
    }
    
    // Write header (we'll come back to fix offsets)
    impl->write_header(file);
    
    // Write data
    uint64_t offset = 0;
    for (size_t i = 0; i < impl->entries.size(); ++i) {
        impl->entries[i].offset = offset;
        offset += impl->entries[i].compressed_size;
    }
    
    // Rewrite header with correct offsets
    file.seekp(0);
    impl->write_header(file);
    
    // Write compressed data
    for (const auto& entry : impl->entries) {
        // We need to re-compress and write
        // For now, placeholder
    }
    
    return true;
}

bool Archive::extract(const std::string& archive_path, const std::string& output_dir) {
    std::ifstream file(archive_path, std::ios::binary);
    if (!file) return false;
    
    if (!impl->read_header(file)) return false;
    
    for (const auto& entry : impl->entries) {
        file.seekg(entry.offset);
        std::vector<uint8_t> compressed(entry.compressed_size);
        file.read(reinterpret_cast<char*>(compressed.data()), entry.compressed_size);
        
        std::vector<uint8_t> decompressed = impl->decompress_data(compressed, entry.original_size);
        
        std::string output_path = output_dir + "/" + entry.name;
        std::ofstream output(output_path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
    }
    
    return true;
}

bool Archive::list(const std::string& archive_path, std::vector<FileEntry>& entries) {
    std::ifstream file(archive_path, std::ios::binary);
    if (!file) return false;
    
    if (!impl->read_header(file)) return false;
    
    entries = impl->entries;
    return true;
}

} // namespace NekoArchive
