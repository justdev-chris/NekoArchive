#include "nekoarchive/archive.h"
#include "nekoarchive/compressor.h"
#include <fstream>
#include <filesystem>
#include <zstd.h>
#include <lzma.h>
#include <openssl/evp.h>
#include <xxhash.h>

namespace fs = std::filesystem;

namespace NekoArchive {

struct Archive::Impl {
    std::vector<FileEntry> entries;
    std::string password;
    CompressionMode mode;
    std::vector<std::vector<uint8_t>> compressed_data; // Store compressed data for writing
    std::vector<std::vector<uint8_t>> original_data;   // Store original data for CRC
    
    bool write_header(std::ofstream& file) {
        uint32_t magic = 0x4F4B454E;
        file.write(reinterpret_cast<const char*>(&magic), 4);
        if (!file.good()) return false;
        
        uint8_t version = 1;
        file.write(reinterpret_cast<const char*>(&version), 1);
        if (!file.good()) return false;
        
        uint8_t mode_byte = static_cast<uint8_t>(mode);
        file.write(reinterpret_cast<const char*>(&mode_byte), 1);
        if (!file.good()) return false;
        
        uint64_t count = entries.size();
        file.write(reinterpret_cast<const char*>(&count), 8);
        if (!file.good()) return false;
        
        return true;
    }
    
    bool write_index(std::ofstream& file) {
        for (const auto& entry : entries) {
            uint16_t name_len = static_cast<uint16_t>(entry.name.size());
            file.write(reinterpret_cast<const char*>(&name_len), 2);
            if (!file.good()) return false;
            
            file.write(entry.name.c_str(), name_len);
            if (!file.good()) return false;
            
            file.write(reinterpret_cast<const char*>(&entry.original_size), 8);
            if (!file.good()) return false;
            
            file.write(reinterpret_cast<const char*>(&entry.compressed_size), 8);
            if (!file.good()) return false;
            
            file.write(reinterpret_cast<const char*>(&entry.offset), 8);
            if (!file.good()) return false;
            
            file.write(reinterpret_cast<const char*>(&entry.crc32), 4);
            if (!file.good()) return false;
        }
        return true;
    }
    
    bool read_header(std::ifstream& file) {
        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), 4);
        if (!file.good() || magic != 0x4F4B454E) return false;
        
        uint8_t version;
        file.read(reinterpret_cast<char*>(&version), 1);
        if (!file.good()) return false;
        
        uint8_t mode_byte;
        file.read(reinterpret_cast<char*>(&mode_byte), 1);
        if (!file.good()) return false;
        mode = static_cast<CompressionMode>(mode_byte);
        
        uint64_t count;
        file.read(reinterpret_cast<char*>(&count), 8);
        if (!file.good()) return false;
        
        entries.resize(count);
        for (size_t i = 0; i < count; ++i) {
            uint16_t name_len;
            file.read(reinterpret_cast<char*>(&name_len), 2);
            if (!file.good()) return false;
            
            entries[i].name.resize(name_len);
            file.read(&entries[i].name[0], name_len);
            if (!file.good()) return false;
            
            file.read(reinterpret_cast<char*>(&entries[i].original_size), 8);
            if (!file.good()) return false;
            
            file.read(reinterpret_cast<char*>(&entries[i].compressed_size), 8);
            if (!file.good()) return false;
            
            file.read(reinterpret_cast<char*>(&entries[i].offset), 8);
            if (!file.good()) return false;
            
            file.read(reinterpret_cast<char*>(&entries[i].crc32), 4);
            if (!file.good()) return false;
        }
        
        return true;
    }
    
    uint32_t calculate_crc32(const std::vector<uint8_t>& data) {
        return XXH32(data.data(), data.size(), 0);
    }
};

Archive::Archive() : impl(std::make_unique<Impl>()) {
    impl->mode = CompressionMode::CAT;
}

Archive::~Archive() = default;

bool Archive::create(const std::string& output_path, const std::vector<std::string>& input_files) {
    std::ofstream file(output_path, std::ios::binary);
    if (!file) return false;
    
    impl->entries.clear();
    impl->compressed_data.clear();
    impl->original_data.clear();
    impl->compressed_data.reserve(input_files.size());
    impl->original_data.reserve(input_files.size());
    
    Compressor compressor(impl->mode);
    if (!impl->password.empty()) {
        compressor.set_password(impl->password);
    }
    
    // Read and compress each file
    for (const auto& filepath : input_files) {
        std::ifstream input(filepath, std::ios::binary);
        if (!input) continue;
        
        input.seekg(0, std::ios::end);
        size_t size = input.tellg();
        input.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(size);
        input.read(reinterpret_cast<char*>(data.data()), size);
        
        // Compress the data
        std::vector<uint8_t> compressed = compressor.compress(data);
        
        FileEntry entry;
        entry.name = fs::path(filepath).filename().string();
        entry.original_size = size;
        entry.compressed_size = compressed.size();
        entry.offset = 0; // Will be set after all files are processed
        entry.crc32 = impl->calculate_crc32(data);
        
        impl->entries.push_back(entry);
        impl->compressed_data.push_back(compressed);
        impl->original_data.push_back(data);
    }
    
    // Calculate offsets
    uint64_t offset = 0;
    for (size_t i = 0; i < impl->entries.size(); ++i) {
        impl->entries[i].offset = offset;
        offset += impl->compressed_data[i].size();
    }
    
    // Write header
    if (!impl->write_header(file)) {
        return false;
    }
    
    // Write index
    if (!impl->write_index(file)) {
        return false;
    }
    
    // Write all compressed data
    for (size_t i = 0; i < impl->compressed_data.size(); ++i) {
        file.write(reinterpret_cast<const char*>(impl->compressed_data[i].data()), 
                   impl->compressed_data[i].size());
        if (!file.good()) return false;
    }
    
    // Write footer checksum (CRC of entire archive)
    uint32_t footer_crc = 0;
    file.write(reinterpret_cast<const char*>(&footer_crc), 4);
    if (!file.good()) return false;
    
    return true;
}

bool Archive::extract(const std::string& archive_path, const std::string& output_dir) {
    std::ifstream file(archive_path, std::ios::binary);
    if (!file) return false;
    
    if (!impl->read_header(file)) return false;
    
    // Create output directory if it doesn't exist
    fs::create_directories(output_dir);
    
    Decompressor decompressor;
    if (!impl->password.empty()) {
        decompressor.set_password(impl->password);
    }
    
    for (const auto& entry : impl->entries) {
        // Seek to data offset
        file.seekg(entry.offset, std::ios::beg);
        if (!file.good()) return false;
        
        // Read compressed data
        std::vector<uint8_t> compressed(entry.compressed_size);
        file.read(reinterpret_cast<char*>(compressed.data()), entry.compressed_size);
        if (!file.good()) return false;
        
        // Decompress
        std::vector<uint8_t> decompressed = decompressor.decompress(compressed);
        
        // Verify CRC32
        uint32_t crc = XXH32(decompressed.data(), decompressed.size(), 0);
        if (crc != entry.crc32) {
            // CRC mismatch - file corrupt
            return false;
        }
        
        // Write output file
        std::string output_path = output_dir + "/" + entry.name;
        std::ofstream output(output_path, std::ios::binary);
        if (!output) return false;
        
        output.write(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
        if (!output.good()) return false;
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
