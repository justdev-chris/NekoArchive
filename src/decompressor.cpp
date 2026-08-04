#include "nekoarchive/decompressor.h"
#include <zstd.h>
#include <lzma.h>
#include <xxhash.h>
#include <openssl/evp.h>
#include <vector>
#include <thread>
#include <fstream>
#include <cstring>

namespace NekoArchive {

struct Decompressor::Impl {
    std::string password;
    int threads;
    
    Impl() : threads(std::thread::hardware_concurrency()) {}
    
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
        
        // Since we're using Zstd for CAT mode and LZMA for TIGER mode,
        // Huffman decode is only needed for HARE mode (lz77 + huffman)
        // For simplicity, we'll return the input and let lz77 handle it
        // In practice, HARE mode will just use lz77 without entropy coding
        return input;
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
    
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data) {
        if (password.empty() || data.empty()) return data;
        
        std::vector<uint8_t> salt(16);
        memcpy(salt.data(), data.data(), 16);
        
        std::vector<uint8_t> key(32);
        std::vector<uint8_t> iv(16);
        PKCS5_PBKDF2_HMAC(password.c_str(), password.size(), 
                         salt.data(), salt.size(), 100000,
                         EVP_sha256(), 32, key.data());
        
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data());
        
        std::vector<uint8_t> output(data.size() - 16);
        int len;
        EVP_DecryptUpdate(ctx, output.data(), &len, data.data() + 16, data.size() - 16);
        int outlen = len;
        
        EVP_DecryptFinal_ex(ctx, output.data() + len, &len);
        outlen += len;
        
        output.resize(outlen);
        EVP_CIPHER_CTX_free(ctx);
        
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

std::vector<uint8_t> Decompressor::decompress(const std::vector<uint8_t>& input) {
    if (input.empty()) return {};
    
    std::vector<uint8_t> result = input;
    
    if (!impl->password.empty()) {
        result = impl->decrypt(result);
    }
    
    std::vector<uint8_t> output;
    
    // Try Zstd decompression first (CAT mode)
    output = impl->zstd_decompress(result, result.size() * 4);
    if (!output.empty()) {
        // Check if it looks reasonable
        if (output.size() > 0 && output.size() < result.size() * 20) {
            return output;
        }
    }
    
    // Try LZMA decompression (TIGER mode)
    output = impl->lzma_decompress(result, result.size() * 4);
    if (!output.empty()) {
        if (output.size() > 0 && output.size() < result.size() * 20) {
            return output;
        }
    }
    
    // Try LZ77 decompression (HARE mode)
    output = impl->lz77_decompress(result);
    if (!output.empty()) {
        return output;
    }
    
    // If nothing worked, return original
    return result;
}

std::vector<uint8_t> Decompressor::decompress_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return {};
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    return decompress(data);
}

} // namespace NekoArchive
