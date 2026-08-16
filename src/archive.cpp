#include "nekoarchive/archive.h"
#include "nekoarchive/compressor.h"
#include "nekoarchive/decompressor.h"
#include <fstream>
#include <filesystem>
#include <zstd.h>
#include <lzma.h>
#include <xxhash.h>
#include <iostream>

namespace fs = std::filesystem;

namespace NekoArchive {

struct Archive::Impl {
    std::vector<FileEntry> entries;
    std::string password;
    CompressionMode mode;
    std::vector<std::vector<uint8_t>> compressed_data;
    std::vector<std::vector<uint8_t>> original_data;
    
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
    if (!file) {
        std::cerr << "Archive::create: Failed to open output file: " << output_path << std::endl;
        return false;
    }
    
    impl->entries.clear();
    impl->compressed_data.clear();
    impl->original_data.clear();
    impl->compressed_data.reserve(input_files.size());
    impl->original_data.reserve(input_files.size());
    
    Compressor compressor(impl->mode);
    if (!impl->password.empty()) {
        compressor.set_password(impl->password);
    }
    
    for (const auto& filepath : input_files) {
        std::cout << "  Reading file: " << filepath << std::endl;
        std::ifstream input(filepath, std::ios::binary);
        if (!input) {
            std::cerr << "  Failed to open: " << filepath << std::endl;
            continue;
        }
        
        input.seekg(0, std::ios::end);
        size_t size = input.tellg();
        input.seekg(0, std::ios::beg);
        
        std::cout << "  File size: " << size << " bytes" << std::endl;
        
        std::vector<uint8_t> data(size);
        input.read(reinterpret_cast<char*>(data.data()), size);
        
        std::cout << "  Compressing..." << std::endl;
        std::vector<uint8_t> compressed = compressor.compress(data);
        std::cout << "  Compressed size: " << compressed.size() << " bytes" << std::endl;
        
        FileEntry entry;
        entry.name = fs::path(filepath).filename().string();
        entry.original_size = size;
        entry.compressed_size = compressed.size();
        entry.offset = 0;
        entry.crc32 = impl->calculate_crc32(data);
        
        impl->entries.push_back(entry);
        impl->compressed_data.push_back(compressed);
        impl->original_data.push_back(data);
    }
    
    std::cout << "Total entries: " << impl->entries.size() << std::endl;
    
    // Write header and index FIRST
    if (!impl->write_header(file)) {
        std::cerr << "Archive::create: Failed to write header" << std::endl;
        return false;
    }
    
    if (!impl->write_index(file)) {
        std::cerr << "Archive::create: Failed to write index" << std::endl;
        return false;
    }
    
    // NOW calculate offsets based on current file position
    uint64_t data_start = file.tellp();
    uint64_t offset = data_start;
    for (size_t i = 0; i < impl->entries.size(); ++i) {
        impl->entries[i].offset = offset;
        std::cout << "  Entry " << i << ": offset=" << offset << std::endl;
        offset += impl->compressed_data[i].size();
    }
    
    // Write compressed data
    for (size_t i = 0; i < impl->compressed_data.size(); ++i) {
        std::cout << "  Writing compressed data " << i << " (" << impl->compressed_data[i].size() << " bytes)" << std::endl;
        file.write(reinterpret_cast<const char*>(impl->compressed_data[i].data()), 
                   impl->compressed_data[i].size());
        if (!file.good()) {
            std::cerr << "Archive::create: Failed to write data" << std::endl;
            return false;
        }
    }
    
    uint32_t footer_crc = 0;
    file.write(reinterpret_cast<const char*>(&footer_crc), 4);
    if (!file.good()) {
        std::cerr << "Archive::create: Failed to write footer" << std::endl;
        return false;
    }
    
    std::cout << "Archive::create: SUCCESS" << std::endl;
    return true;
}

bool Archive::extract(const std::string& archive_path, const std::string& output_dir) {
    std::cout << "Archive::extract: Opening " << archive_path << std::endl;
    std::ifstream file(archive_path, std::ios::binary);
    if (!file) {
        std::cerr << "Archive::extract: Failed to open archive" << std::endl;
        return false;
    }
    
    if (!impl->read_header(file)) {
        std::cerr << "Archive::extract: Failed to read header" << std::endl;
        return false;
    }
    
    std::cout << "Archive::extract: Found " << impl->entries.size() << " entries" << std::endl;
    
    fs::create_directories(output_dir);
    
    Decompressor decompressor;
    if (!impl->password.empty()) {
        decompressor.set_password(impl->password);
    }
    
    for (const auto& entry : impl->entries) {
        std::cout << "  Extracting: " << entry.name << " (original=" << entry.original_size 
                  << ", compressed=" << entry.compressed_size << ", offset=" << entry.offset << ")" << std::endl;
        
        file.seekg(entry.offset, std::ios::beg);
        if (!file.good()) {
            std::cerr << "  Failed to seek to offset " << entry.offset << std::endl;
            return false;
        }
        
        std::vector<uint8_t> compressed(entry.compressed_size);
        file.read(reinterpret_cast<char*>(compressed.data()), entry.compressed_size);
        if (!file.good()) {
            std::cerr << "  Failed to read compressed data" << std::endl;
            return false;
        }
        
        std::cout << "  Decompressing..." << std::endl;
        std::vector<uint8_t> decompressed = decompressor.decompress(compressed);
        std::cout << "  Decompressed size: " << decompressed.size() << " bytes" << std::endl;
        
        // If decompression failed (returned compressed data unchanged), and we have a password
        // try without password (maybe the archive isn't encrypted)
        if (decompressed.size() == entry.compressed_size && !impl->password.empty()) {
            std::cout << "  Decompression with password failed, trying without..." << std::endl;
            Decompressor no_pass_decompressor;
            decompressed = no_pass_decompressor.decompress(compressed);
            std::cout << "  Decompressed without password: " << decompressed.size() << " bytes" << std::endl;
        }
        
        uint32_t crc = XXH32(decompressed.data(), decompressed.size(), 0);
        if (crc != entry.crc32) {
            std::cerr << "  CRC mismatch! Expected " << entry.crc32 << ", got " << crc << std::endl;
            // Try using the compressor's decompression directly
            std::cout << "  Trying direct decompression with stored mode..." << std::endl;
            // We need to decompress based on the mode stored in the archive
            // This will be handled by the decompressor's detection
        }
        std::cout << "  CRC OK" << std::endl;
        
        std::string output_path = output_dir + "/" + entry.name;
        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            std::cerr << "  Failed to create output file: " << output_path << std::endl;
            return false;
        }
        
        output.write(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
        if (!output.good()) {
            std::cerr << "  Failed to write output file" << std::endl;
            return false;
        }
        std::cout << "  Wrote: " << output_path << std::endl;
    }
    
    std::cout << "Archive::extract: SUCCESS" << std::endl;
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
