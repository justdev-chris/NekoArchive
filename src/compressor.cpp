#include "nekoarchive/compressor.h"
#include "nekoarchive/aes_custom.h"
#include <zstd.h>
#include <lzma.h>
#include <xxhash.h>
#include <vector>
#include <cstring>
#include <thread>
#include <queue>
#include <functional>
#include <unordered_map>
#include <fstream>
#include <algorithm>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <codecvt>
#include <locale>

// Convert UTF-8 string to wide string (Windows UTF-16)
std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}
#endif

namespace NekoArchive {

struct Compressor::Impl {
    CompressionMode mode;
    std::string password;
    int threads;
    size_t dict_size;
    
    Impl() : mode(CompressionMode::CAT), threads(std::thread::hardware_concurrency()), dict_size(1 << 20) {}
    
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data) {
        if (password.empty() || data.empty()) return data;
        AES::AESCipher cipher;
        cipher.setKey(password);
        return cipher.encrypt(data);
    }
    
    std::vector<uint8_t> lz77_compress(const std::vector<uint8_t>& input) {
        if (input.empty()) return {};
        
        std::vector<uint8_t> output;
        output.reserve(input.size() * 1.1);
        
        size_t i = 0;
        const size_t window_size = 32768;
        const size_t lookahead = 258;
        
        while (i < input.size()) {
            size_t match_len = 0;
            size_t match_pos = 0;
            size_t window_start = (i > window_size) ? i - window_size : 0;
            
            for (size_t j = window_start; j < i; ++j) {
                size_t len = 0;
                while (i + len < input.size() && 
                       j + len < i && 
                       input[j + len] == input[i + len] && 
                       len < lookahead) {
                    ++len;
                }
                
                if (len >= 3 && len > match_len) {
                    match_len = len;
                    match_pos = i - j;
                }
            }
            
            if (match_len >= 3) {
                output.push_back(0xFF);
                uint16_t dist = static_cast<uint16_t>(match_pos);
                output.push_back(dist & 0xFF);
                output.push_back((dist >> 8) & 0xFF);
                output.push_back(static_cast<uint8_t>(match_len));
                i += match_len;
            } else {
                output.push_back(0x00);
                output.push_back(input[i]);
                ++i;
            }
        }
        
        return output;
    }
    
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
    
    std::vector<uint8_t> huffman_encode(const std::vector<uint8_t>& input) {
        if (input.empty()) return {};
        
        int freq[256] = {0};
        for (uint8_t c : input) {
            freq[c]++;
        }
        
        std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, HuffmanCompare> pq;
        int num_symbols = 0;
        for (int i = 0; i < 256; ++i) {
            if (freq[i] > 0) {
                pq.push(new HuffmanNode(static_cast<uint8_t>(i), freq[i]));
                num_symbols++;
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
        
        std::unordered_map<uint8_t, std::string> codes;
        std::function<void(HuffmanNode*, std::string)> generate_codes = [&](HuffmanNode* node, std::string code) {
            if (!node->left && !node->right) {
                codes[node->symbol] = code;
                return;
            }
            if (node->left) generate_codes(node->left, code + "0");
            if (node->right) generate_codes(node->right, code + "1");
        };
        generate_codes(root, "");
        
        std::vector<uint8_t> output;
        output.reserve(input.size() + 1024);
        
        output.push_back(static_cast<uint8_t>(num_symbols));
        
        for (int i = 0; i < 256; ++i) {
            if (freq[i] > 0) {
                output.push_back(static_cast<uint8_t>(i));
                uint32_t f = static_cast<uint32_t>(freq[i]);
                output.push_back(f & 0xFF);
                output.push_back((f >> 8) & 0xFF);
                output.push_back((f >> 16) & 0xFF);
                output.push_back((f >> 24) & 0xFF);
            }
        }
        
        std::string bitstring;
        bitstring.reserve(input.size() * 8);
        for (uint8_t c : input) {
            bitstring += codes[c];
        }
        
        uint8_t padding = static_cast<uint8_t>((8 - (bitstring.size() % 8)) % 8);
        output.push_back(padding);
        
        uint32_t bit_length = static_cast<uint32_t>(bitstring.size());
        output.push_back(bit_length & 0xFF);
        output.push_back((bit_length >> 8) & 0xFF);
        output.push_back((bit_length >> 16) & 0xFF);
        output.push_back((bit_length >> 24) & 0xFF);
        
        for (size_t i = 0; i < bitstring.size(); i += 8) {
            uint8_t byte = 0;
            for (size_t j = 0; j < 8 && i + j < bitstring.size(); ++j) {
                if (bitstring[i + j] == '1') {
                    byte |= (1 << (7 - j));
                }
            }
            output.push_back(byte);
        }
        
        delete root;
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
    
    std::vector<uint8_t> zstd_compress(const std::vector<uint8_t>& input) {
        if (input.empty()) return {};
        
        int level = (mode == CompressionMode::HARE) ? 1 : 
                   (mode == CompressionMode::TIGER) ? 12 : 3;
        
        size_t bound = ZSTD_compressBound(input.size());
        std::vector<uint8_t> output(bound);
        
        size_t compressed = ZSTD_compress(
            output.data(), bound,
            input.data(), input.size(),
            level
        );
        
        if (ZSTD_isError(compressed)) return {};
        
        output.resize(compressed);
        return output;
    }
    
    std::vector<uint8_t> zstd_decompress(const std::vector<uint8_t>& input, size_t original_size) {
        if (input.empty() || original_size == 0) return {};
        
        std::vector<uint8_t> output(original_size);
        size_t decompressed = ZSTD_decompress(
            output.data(), original_size,
            input.data(), input.size()
        );
        
        if (ZSTD_isError(decompressed)) return {};
        
        output.resize(decompressed);
        return output;
    }
    
    std::vector<uint8_t> lzma_compress(const std::vector<uint8_t>& input) {
        if (input.empty()) return {};
        
        lzma_stream stream = LZMA_STREAM_INIT;
        lzma_ret ret = lzma_easy_encoder(&stream, 9, LZMA_CHECK_CRC64);
        if (ret != LZMA_OK) return {};
        
        std::vector<uint8_t> output;
        output.reserve(input.size());
        
        stream.next_in = input.data();
        stream.avail_in = input.size();
        
        uint8_t outbuf[1024];
        stream.next_out = outbuf;
        stream.avail_out = sizeof(outbuf);
        
        while (stream.avail_in > 0 || stream.avail_out == 0) {
            ret = lzma_code(&stream, stream.avail_in > 0 ? LZMA_RUN : LZMA_FINISH);
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

Compressor::Compressor(CompressionMode mode) : impl(std::make_unique<Impl>()) {
    impl->mode = mode;
}

Compressor::~Compressor() = default;

void Compressor::set_mode(CompressionMode mode) {
    impl->mode = mode;
}

void Compressor::set_password(const std::string& password) {
    impl->password = password;
}

void Compressor::set_thread_count(int threads) {
    impl->threads = threads;
}

std::vector<uint8_t> Compressor::compress(const std::vector<uint8_t>& input) {
    if (input.empty()) return {};
    
    std::vector<uint8_t> result;
    
    if (impl->mode == CompressionMode::HARE) {
        result = impl->lz77_compress(input);
        result = impl->huffman_encode(result);
    } else if (impl->mode == CompressionMode::CAT) {
        result = impl->zstd_compress(input);
    } else if (impl->mode == CompressionMode::TIGER) {
        result = impl->lzma_compress(input);
    }
    
    if (!impl->password.empty()) {
        result = impl->encrypt(result);
    }
    
    return result;
}

std::vector<uint8_t> Compressor::compress_file(const std::string& filepath) {
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
    
    return compress(data);
}

} // namespace NekoArchive
