#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace NekoArchive {

struct FileEntry {
    std::string name;
    uint64_t original_size;
    uint64_t compressed_size;
    uint64_t offset;
    uint32_t crc32;
};

class Archive {
public:
    Archive();
    ~Archive();

    bool create(const std::string& output_path,
                const std::vector<std::string>& input_files);

    bool extract(const std::string& archive_path,
                 const std::string& output_dir);

    bool list(const std::string& archive_path,
              std::vector<FileEntry>& entries);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace NekoArchive
