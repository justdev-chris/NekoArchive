#include "nekoarchive/decompressor.h"
#include "nekoarchive/aes_custom.h"
#include "nekoarchive/unicode.h"
#include <zstd.h>
#include <lzma.h>
#include <xxhash.h>
#include <vector>
#include <thread>
#include <fstream>
#include <cstring>
#include <queue>
#include <functional>
#include <unordered_map>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <codecvt>
#include <locale>

namespace NekoArchive {

struct Decompressor::Impl {
    std::string password;
    int threads;
    
    Impl() : threads(std::thread::hardware_concurrency()) {}
    
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data) {
        if (password.empty() || data.empty()) return data;
        AES::AESCipher cipher;
        cipher.setKey(password);
        return cipher.decrypt(data);
    }
    
    struct HuffmanNode {
        uint8_t symbol;
        int freq;
        HuffmanNode* left;
        HuffmanNode* right;
        HuffmanNode(uint8_t s, int f) : symbol(s), freq(f), left(nullptr), right(nullptr) {}
        ~HuffmanNode() { delete left; delete right; }
    };
    
    struct HuffmanCompare {
        bool operator()(HuffmanNode* a, HuffmanNode* b) {
            return a->freq > b->freq;
        }
    };
    
    std::vector<uint8_t> lz77_decompress(const std::vector<uint8_t>& input) {
        if (input.empty()) return {};
        
        std::vector<uint8_t> output;
        output.reserve(input.size() * 2);
        
        size_t i = 0;
        while (i < input.size()) {
            uint8_t marker = input[i++];
            
            if (marker == 0x00) {
                output.push_back(input[i++]);
            } else if (marker == 0xFF) {
                uint16_t dist = input[i] | (input[i+1] << 8);
                i += 2;
                uint8_t len = input[i++];
                
                size_t start = output.size() - dist;
                for (size_t j = 0; j < len; ++j) {
                    output.push_back(output[start + j]);
                }
            }
        }
        
        return output;
    }
    
    std::vector<uint8_t> huffman_decode(const std::vector<uint8_t>& input) {
        if (input.empty()) return {};
        
        size_t pos = 0;
        uint8_t num_symbols = input[pos++];
        if (num_symbols == 0) return {};
        
        int freq[256] = {0};
        for (int i = 0; i < num_symbols; ++i) {
            uint8_t symbol = input[pos++];
            uint32_t f = input[pos++] | (input[pos++] << 8) | (input[pos++] << 16) | (input[pos++] << 24);
            freq[symbol] = f;
        }
        
        std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, HuffmanCompare> pq;
        for (int i = 0; i < 256; ++i) {
            if (freq[i] > 0) {
                pq.push(new HuffmanNode(static_cast<uint8_t>(i), freq[i]));
            }
        }
        
        if (pq.empty()) return {};
        
        while (pq.size() > 1) {
            HuffmanNode* left = pq.top(); pq.pop();
            HuffmanNode* right = pq.top(); pq.pop();
            HuffmanNode* parent = new HuffmanNode(0, left->freq + right->freq);
            parent->left = left;
            parent->right = right;
            pq.push(parent);
        }
        
        HuffmanNode* root = pq.top();
        
        uint8_t padding = input[pos++];
        uint32_t bit_length = input[pos++] | (input[pos++] << 8) | (input[pos++] << 16) | (input[pos++] << 24);
        
        std::string bitstring;
        bitstring.reserve(bit_length);
        
        for (size_t i = pos; i < input.size() && bitstring.size() < bit_length; ++i) {
            uint8_t byte = input[i];
            for (int j = 7; j >= 0 && bitstring.size() < bit_length; --j) {
                bitstring += (byte & (1 << j)) ? '1' : '0';
            }
        }
        
        std::vector<uint8_t> output;
        output.reserve(bit_length);
        
        HuffmanNode* current = root;
        for (char c : bitstring) {
            if (c == '0') {
                current = current->left;
            } else {
                current = current->right;
            }
            
            if (!current->left && !current->right) {
                output.push_back(current->symbol);
                current = root;
            }
        }
        
        delete root;
        return output;
    }
    
    std::vector<uint8_t> zstd_decompress(const std::vector<uint8_t>& input, size_t original_size) {
        if (input.empty() || original_size == 0) return {};
        
        std::vector<uint8_t> output(original_size);
        size_t decompressed = ZSTD_decompress(
            output.data(), original_size,
            input.data(), input.size()
        );
        
        if (ZSTD_isError(decompressed)) {
            return {};
        }
        
        output.resize(decompressed);
        return output;
    }
    
    std::vector<uint8_t> lzma_decompress(const std::vector<uint8_t>& input, size_t original_size) {
        if (input.empty() || original_size == 0) return {};
        
        lzma_stream stream = LZMA_STREAM_INIT;
        lzma_ret ret = lzma_auto_decoder(&stream, UINT64_MAX, 0);
        if (ret != LZMA_OK) return {};
        
        std::vector<uint8_t> output;
        output.reserve(original_size);
        
        stream.next_in = input.data();
        stream.avail_in = input.size();
        
        uint8_t outbuf[1024];
        stream.next_out = outbuf;
        stream.avail_out = sizeof(outbuf);
        
        while (stream.avail_in > 0) {
            ret = lzma_code(&stream, LZMA_RUN);
            if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
                lzma_end(&stream);
                return {};
            }
            output.insert(output.end(), outbuf, outbuf + sizeof(outbuf) - stream.avail_out);
            stream.next_out = outbuf;
            stream.avail_out = sizeof(outbuf);
        }
        
        lzma_end(&stream);
        return output;
    }
};

Decompressor::Decompressor() : impl(std::make_unique<Impl>()) {}
Decompressor::~Decompressor() = default;

void Decompressor::set_password(const std::string& password) {
    impl->password = password;
}

void Decompressor::set_thread_count(int threads) {
    impl->threads = threads;
}

std::vector<uint8_t> Decompressor::decompress(const std::vector<uint8_t>& input, size_t original_size) {
    if (input.empty()) return {};
    
    std::vector<uint8_t> result = input;
    
    if (!impl->password.empty()) {
        result = impl->decrypt(result);
    }
    
    std::vector<uint8_t> output;
    
    // Try Zstd decompression (CAT mode) with correct original size
    if (original_size > 0) {
        output = impl->zstd_decompress(result, original_size);
        if (!output.empty()) {
            return output;
        }
    }
    
    // Try LZMA decompression (TIGER mode) with correct original size
    if (original_size > 0) {
        output = impl->lzma_decompress(result, original_size);
        if (!output.empty()) {
            return output;
        }
    }
    
    // Try Huffman + LZ77 (HARE mode)
    output = impl->huffman_decode(result);
    if (!output.empty()) {
        output = impl->lz77_decompress(output);
        if (!output.empty()) {
            return output;
        }
    }
    
    return result;
}

std::vector<uint8_t> Decompressor::decompress_file(const std::string& filepath) {
    std::vector<uint8_t> data;
    
#ifdef _WIN32
    // Use Windows wide API for Unicode paths
    std::wstring wpath = utf8_to_wstring(filepath);
    
    HANDLE hFile = CreateFileW(
        wpath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return {};
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return {};
    }
    
    size_t size = static_cast<size_t>(fileSize.QuadPart);
    data.resize(size);
    
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, data.data(), static_cast<DWORD>(size), &bytesRead, NULL) || bytesRead != size) {
        CloseHandle(hFile);
        return {};
    }
    CloseHandle(hFile);
#else
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return {};
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    data.resize(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
#endif
    
    return decompress(data, 0);
}

} // namespace NekoArchive
