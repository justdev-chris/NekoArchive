// aes_custom.h
#ifndef AES_CUSTOM_H
#define AES_CUSTOM_H

#include <cstdint>
#include <vector>
#include <cstring>

namespace NekoArchive {
namespace AES {

// AES-128 block size is 16 bytes
constexpr int BLOCK_SIZE = 16;
constexpr int KEY_SIZE = 16;
constexpr int NUM_ROUNDS = 10;

// S-box for SubBytes
const uint8_t SBOX[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

// Inverse S-box for InvSubBytes
const uint8_t INV_SBOX[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

// Rcon for key expansion
const uint8_t RCON[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

class AESCipher {
private:
    uint8_t key[KEY_SIZE];
    uint8_t roundKeys[11][4][4];
    
    // Key expansion
    void expandKey() {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                roundKeys[0][i][j] = key[i * 4 + j];
            }
        }
        
        for (int i = 1; i <= NUM_ROUNDS; i++) {
            for (int j = 0; j < 4; j++) {
                if (j == 0) {
                    uint8_t temp[4] = {
                        roundKeys[i-1][0][1],
                        roundKeys[i-1][1][1],
                        roundKeys[i-1][2][1],
                        roundKeys[i-1][3][1]
                    };
                    
                    for (int k = 0; k < 4; k++) {
                        temp[k] = SBOX[temp[k]];
                    }
                    
                    roundKeys[i][0][0] = roundKeys[i-1][0][0] ^ temp[0] ^ RCON[i];
                    roundKeys[i][1][0] = roundKeys[i-1][1][0] ^ temp[1];
                    roundKeys[i][2][0] = roundKeys[i-1][2][0] ^ temp[2];
                    roundKeys[i][3][0] = roundKeys[i-1][3][0] ^ temp[3];
                } else {
                    roundKeys[i][0][j] = roundKeys[i-1][0][j] ^ roundKeys[i][0][j-1];
                    roundKeys[i][1][j] = roundKeys[i-1][1][j] ^ roundKeys[i][1][j-1];
                    roundKeys[i][2][j] = roundKeys[i-1][2][j] ^ roundKeys[i][2][j-1];
                    roundKeys[i][3][j] = roundKeys[i-1][3][j] ^ roundKeys[i][3][j-1];
                }
            }
        }
    }
    
    // SubBytes transformation
    void subBytes(uint8_t state[4][4]) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                state[i][j] = SBOX[state[i][j]];
            }
        }
    }
    
    // InvSubBytes transformation
    void invSubBytes(uint8_t state[4][4]) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                state[i][j] = INV_SBOX[state[i][j]];
            }
        }
    }
    
    // ShiftRows transformation
    void shiftRows(uint8_t state[4][4]) {
        uint8_t temp[4];
        
        // Row 1
        temp[0] = state[0][1];
        temp[1] = state[1][1];
        temp[2] = state[2][1];
        temp[3] = state[3][1];
        state[0][1] = temp[1];
        state[1][1] = temp[2];
        state[2][1] = temp[3];
        state[3][1] = temp[0];
        
        // Row 2
        temp[0] = state[0][2];
        temp[1] = state[1][2];
        temp[2] = state[2][2];
        temp[3] = state[3][2];
        state[0][2] = temp[2];
        state[1][2] = temp[3];
        state[2][2] = temp[0];
        state[3][2] = temp[1];
        
        // Row 3
        temp[0] = state[0][3];
        temp[1] = state[1][3];
        temp[2] = state[2][3];
        temp[3] = state[3][3];
        state[0][3] = temp[3];
        state[1][3] = temp[0];
        state[2][3] = temp[1];
        state[3][3] = temp[2];
    }
    
    // InvShiftRows transformation
    void invShiftRows(uint8_t state[4][4]) {
        uint8_t temp[4];
        
        // Row 1
        temp[0] = state[0][1];
        temp[1] = state[1][1];
        temp[2] = state[2][1];
        temp[3] = state[3][1];
        state[0][1] = temp[3];
        state[1][1] = temp[0];
        state[2][1] = temp[1];
        state[3][1] = temp[2];
        
        // Row 2
        temp[0] = state[0][2];
        temp[1] = state[1][2];
        temp[2] = state[2][2];
        temp[3] = state[3][2];
        state[0][2] = temp[2];
        state[1][2] = temp[3];
        state[2][2] = temp[0];
        state[3][2] = temp[1];
        
        // Row 3
        temp[0] = state[0][3];
        temp[1] = state[1][3];
        temp[2] = state[2][3];
        temp[3] = state[3][3];
        state[0][3] = temp[1];
        state[1][3] = temp[2];
        state[2][3] = temp[3];
        state[3][3] = temp[0];
    }
    
    // MixColumns transformation
    uint8_t gf_mul(uint8_t a, uint8_t b) {
        uint8_t result = 0;
        while (b) {
            if (b & 1) result ^= a;
            if (a & 0x80) {
                a = (a << 1) ^ 0x1B;
            } else {
                a <<= 1;
            }
            b >>= 1;
        }
        return result;
    }
    
    void mixColumns(uint8_t state[4][4]) {
        for (int i = 0; i < 4; i++) {
            uint8_t s0 = state[i][0];
            uint8_t s1 = state[i][1];
            uint8_t s2 = state[i][2];
            uint8_t s3 = state[i][3];
            
            state[i][0] = gf_mul(s0, 2) ^ gf_mul(s1, 3) ^ s2 ^ s3;
            state[i][1] = s0 ^ gf_mul(s1, 2) ^ gf_mul(s2, 3) ^ s3;
            state[i][2] = s0 ^ s1 ^ gf_mul(s2, 2) ^ gf_mul(s3, 3);
            state[i][3] = gf_mul(s0, 3) ^ s1 ^ s2 ^ gf_mul(s3, 2);
        }
    }
    
    // InvMixColumns transformation
    void invMixColumns(uint8_t state[4][4]) {
        for (int i = 0; i < 4; i++) {
            uint8_t s0 = state[i][0];
            uint8_t s1 = state[i][1];
            uint8_t s2 = state[i][2];
            uint8_t s3 = state[i][3];
            
            state[i][0] = gf_mul(s0, 0x0E) ^ gf_mul(s1, 0x0B) ^ gf_mul(s2, 0x0D) ^ gf_mul(s3, 0x09);
            state[i][1] = gf_mul(s0, 0x09) ^ gf_mul(s1, 0x0E) ^ gf_mul(s2, 0x0B) ^ gf_mul(s3, 0x0D);
            state[i][2] = gf_mul(s0, 0x0D) ^ gf_mul(s1, 0x09) ^ gf_mul(s2, 0x0E) ^ gf_mul(s3, 0x0B);
            state[i][3] = gf_mul(s0, 0x0B) ^ gf_mul(s1, 0x0D) ^ gf_mul(s2, 0x09) ^ gf_mul(s3, 0x0E);
        }
    }
    
    // AddRoundKey transformation
    void addRoundKey(uint8_t state[4][4], int round) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                state[i][j] ^= roundKeys[round][i][j];
            }
        }
    }
    
    // Encrypt a single block (16 bytes)
    void encryptBlock(uint8_t* block) {
        uint8_t state[4][4];
        
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                state[i][j] = block[i * 4 + j];
            }
        }
        
        addRoundKey(state, 0);
        
        for (int round = 1; round <= NUM_ROUNDS - 1; round++) {
            subBytes(state);
            shiftRows(state);
            mixColumns(state);
            addRoundKey(state, round);
        }
        
        subBytes(state);
        shiftRows(state);
        addRoundKey(state, NUM_ROUNDS);
        
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                block[i * 4 + j] = state[i][j];
            }
        }
    }
    
    // Decrypt a single block (16 bytes)
    void decryptBlock(uint8_t* block) {
        uint8_t state[4][4];
        
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                state[i][j] = block[i * 4 + j];
            }
        }
        
        addRoundKey(state, NUM_ROUNDS);
        
        for (int round = NUM_ROUNDS - 1; round >= 1; round--) {
            invSubBytes(state);
            invShiftRows(state);
            invMixColumns(state);
            addRoundKey(state, round);
        }
        
        invSubBytes(state);
        invShiftRows(state);
        addRoundKey(state, 0);
        
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                block[i * 4 + j] = state[i][j];
            }
        }
    }
    
public:
    AESCipher() = default;
    
    void setKey(const uint8_t* keyData, size_t keySize = 16) {
        memset(key, 0, KEY_SIZE);
        size_t copySize = (keySize < KEY_SIZE) ? keySize : KEY_SIZE;
        memcpy(key, keyData, copySize);
        expandKey();
    }
    
    void setKey(const std::string& password) {
        setKey(reinterpret_cast<const uint8_t*>(password.c_str()), password.size());
    }
    
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext) {
        if (plaintext.empty()) return {};
        
        // PKCS7 padding
        size_t paddedSize = ((plaintext.size() + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
        std::vector<uint8_t> padded(paddedSize);
        memcpy(padded.data(), plaintext.data(), plaintext.size());
        
        uint8_t padValue = static_cast<uint8_t>(paddedSize - plaintext.size());
        for (size_t i = plaintext.size(); i < paddedSize; i++) {
            padded[i] = padValue;
        }
        
        // Encrypt each block
        for (size_t i = 0; i < paddedSize; i += BLOCK_SIZE) {
            encryptBlock(padded.data() + i);
        }
        
        return padded;
    }
    
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext) {
        if (ciphertext.empty() || ciphertext.size() % BLOCK_SIZE != 0) return {};
        
        std::vector<uint8_t> decrypted = ciphertext;
        
        // Decrypt each block
        for (size_t i = 0; i < decrypted.size(); i += BLOCK_SIZE) {
            decryptBlock(decrypted.data() + i);
        }
        
        // Remove PKCS7 padding
        if (!decrypted.empty()) {
            uint8_t padValue = decrypted.back();
            if (padValue > 0 && padValue <= BLOCK_SIZE) {
                decrypted.resize(decrypted.size() - padValue);
            }
        }
        
        return decrypted;
    }
};

} // namespace AES
} // namespace NekoArchive

#endif // AES_CUSTOM_H
