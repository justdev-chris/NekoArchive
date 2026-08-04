#include "nekoarchive/compressor.h"
#include <zstd.h>
#include <lzma.h>
#include <xxhash.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <vector>
#include <cstring>
#include <thread>

namespace NekoArchive {

struct Compressor::Impl {
    CompressionMode mode;
    std::string password;
    int threads;
    size_t dict_size;
    
    Impl() : mode(CompressionMode::CAT), threads(std::thread::hardware_concurrency()), dict_size(1 << 20) {}
    
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
    
    std::vector<uint8_t> huffman_encode(const std::vector<uint8_t>& input) {
        if (input.empty()) return {};
        
        int freq[256] = {0};
        for (uint8_t c : input) {
            freq[c]++;
        }
        
        struct Node {
            uint8_t symbol;
            int freq;
            Node* left;
            Node* right;
            Node(uint8_t s, int f) : symbol(s), freq(f), left(nullptr), right(nullptr) {}
            ~Node() { delete left; delete right; }
        };
        
        auto cmp = [](Node* a, Node* b) { return a->freq > b->freq; };
        std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> pq(cmp);
        
        for (int i = 0; i < 256; ++i) {
            if (freq[i] > 0) {
                pq.push(new Node(i, freq[i]));
            }
        }
        
        if (pq.empty()) return {};
        if (pq.size() == 1) {
            Node* leaf = pq.top();
            pq.pop();
            Node* parent = new Node(0, leaf->freq);
            parent->left = leaf;
            pq.push(parent);
        }
        
        while (pq.size() > 1) {
            Node* left = pq.top(); pq.pop();
            Node* right = pq.top(); pq.pop();
            Node* parent = new Node(0, left->freq + right->freq);
            parent->left = left;
            parent->right = right;
            pq.push(parent);
        }
        
        Node* root = pq.top();
        std::unordered_map<uint8_t, std::string> codes;
        std::function<void(Node*, std::string)> generate_codes = [&](Node* node, std::string code) {
            if (!node->left && !node->right) {
                codes[node->symbol] = code;
                return;
            }
            if (node->left) generate_codes(node->left, code + "0");
            if (node->right) generate_codes(node->right, code + "1");
        };
        generate_codes(root, "");
        
        std::string bitstring;
        bitstring.reserve(input.size() * 8);
        for (uint8_t c : input) {
            bitstring += codes[c];
        }
        
        std::vector<uint8_t> output;
        output.push_back(static_cast<uint8_t>(bitstring.size() % 8));
        
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
        
        return {}; // Placeholder - full implementation would rebuild tree from freq
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
    
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data) {
        if (password.empty() || data.empty()) return data;
        
        std::vector<uint8_t> salt(16);
        RAND_bytes(salt.data(), salt.size());
        
        std::vector<uint8_t> key(32);
        std::vector<uint8_t> iv(16);
        PKCS5_PBKDF2_HMAC(password.c_str(), password.size(), 
                         salt.data(), salt.size(), 100000,
                         EVP_sha256(), 32, key.data());
        
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data());
        
        std::vector<uint8_t> output(data.size() + 16 + 16 + salt.size());
        size_t offset = 0;
        
        memcpy(output.data(), salt.data(), salt.size());
        offset += salt.size();
        
        int len;
        EVP_EncryptUpdate(ctx, output.data() + offset, &len, data.data(), data.size());
        offset += len;
        
        EVP_EncryptFinal_ex(ctx, output.data() + offset, &len);
        offset += len;
        
        output.resize(offset);
        EVP_CIPHER_CTX_free(ctx);
        
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
    
    std::vector<uint8_t> result = input;
    
    // Apply compression based on mode
    if (impl->mode == CompressionMode::HARE) {
        result = impl->lz77_compress(result);
    } else if (impl->mode == CompressionMode::CAT) {
        result = impl->zstd_compress(result);
    } else if (impl->mode == CompressionMode::TIGER) {
        result = impl->lzma_compress(result);
    }
    
    // Apply entropy coding
    result = impl->huffman_encode(result);
    
    // Apply encryption if password is set
    if (!impl->password.empty()) {
        result = impl->encrypt(result);
    }
    
    return result;
}

std::vector<uint8_t> Compressor::compress_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return {};
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    return compress(data);
}

} // namespace NekoArchive
