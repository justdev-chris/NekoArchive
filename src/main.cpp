#include "nekoarchive/archive.h"
#include "nekoarchive/compressor.h"
#include "nekoarchive/decompressor.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

void print_usage() {
    std::cout << "NekoArchive v0.1 - Purr-fect compression\n\n";
    std::cout << "Usage:\n";
    std::cout << "  nekocli -c <input> -o <output>    Compress\n";
    std::cout << "  nekocli -x <archive> -o <dir>     Extract\n";
    std::cout << "  nekocli -l <archive>              List contents\n";
    std::cout << "  nekocli -h                        Show help\n";
    std::cout << "\nModes:\n";
    std::cout << "  -m hare    Fast compression\n";
    std::cout << "  -m cat     Balanced (default)\n";
    std::cout << "  -m tiger   Maximum compression\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string command = argv[1];
    
    try {
        if (command == "-c" || command == "--compress") {
            if (argc < 5) {
                std::cerr << "Error: -c <input> -o <output>\n";
                return 1;
            }
            
            std::string input = argv[2];
            std::string output = argv[4];
            
            std::vector<std::string> files;
            if (std::filesystem::is_directory(input)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(input)) {
                    if (!entry.is_directory()) {
                        files.push_back(entry.path().string());
                    }
                }
            } else {
                files.push_back(input);
            }
            
            NekoArchive::Archive archive;
            std::cout << "🐱 Compressing " << files.size() << " files...\n";
            
            if (archive.create(output, files)) {
                std::cout << "✅ Compressed to " << output << "\n";
            } else {
                std::cerr << "❌ Compression failed\n";
                return 1;
            }
            
        } else if (command == "-x" || command == "--extract") {
            if (argc < 5) {
                std::cerr << "Error: -x <archive> -o <dir>\n";
                return 1;
            }
            
            std::string archive_path = argv[2];
            std::string output_dir = argv[4];
            
            NekoArchive::Archive archive;
            std::cout << "🐱 Extracting " << archive_path << "...\n";
            
            if (archive.extract(archive_path, output_dir)) {
                std::cout << "✅ Extracted to " << output_dir << "\n";
            } else {
                std::cerr << "❌ Extraction failed\n";
                return 1;
            }
            
        } else if (command == "-l" || command == "--list") {
            if (argc < 3) {
                std::cerr << "Error: -l <archive>\n";
                return 1;
            }
            
            std::string archive_path = argv[2];
            NekoArchive::Archive archive;
            std::vector<NekoArchive::FileEntry> entries;
            
            if (archive.list(archive_path, entries)) {
                std::cout << "📦 Contents of " << archive_path << ":\n\n";
                std::cout << "  Name                    Size      Ratio\n";
                std::cout << "  ----------------------------------------\n";
                for (const auto& entry : entries) {
                    double ratio = (double)entry.compressed_size / entry.original_size * 100;
                    std::cout << "  " << entry.name << "  " 
                              << entry.original_size << "  " 
                              << ratio << "%\n";
                }
            } else {
                std::cerr << "❌ Failed to read archive\n";
                return 1;
            }
            
        } else if (command == "-h" || command == "--help") {
            print_usage();
            
        } else {
            std::cerr << "Unknown command: " << command << "\n";
            print_usage();
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
