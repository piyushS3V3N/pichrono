#include "piqme.h"
#include "utils.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CHACHA20_KEY_SIZE (32)
#define CHACHA20_NONCE_SIZE (12)
#define CHACHA20_STATE_WORDS (16)
#define CHACHA20_BLOCK_SIZE (CHACHA20_STATE_WORDS * sizeof(uint32_t))

#define rotl32a(x, n) ((x) << (n)) | ((x) >> (32 - (n)))

#define Qround(a, b, c, d) \
  a += b; d ^= a; d = rotl32a(d, 16); \
  c += d; b ^= c; b = rotl32a(b, 12); \
  a += b; d ^= a; d = rotl32a(d, 8);  \
  c += d; b ^= c; b = rotl32a(b, 7);

static void initialize_state(uint32_t state[16], const uint8_t key[32], const uint8_t nonce[12], uint32_t counter) {
    state[0] = 0x61707865;
    state[1] = 0x3320646e;
    state[2] = 0x79622d32;
    state[3] = 0x6b206574;
    memcpy(&state[4], key, 32);
    state[12] = counter;
    memcpy(&state[13], nonce, 12);
}

static void core_block(const uint32_t start[16], uint32_t output[16]) {
    uint32_t x[16];
    memcpy(x, start, 64);
    for (int i = 0; i < 10; i++) {
        Qround(x[0], x[4], x[8],  x[12]);
        Qround(x[1], x[5], x[9],  x[13]);
        Qround(x[2], x[6], x[10], x[14]);
        Qround(x[3], x[7], x[11], x[15]);
        Qround(x[0], x[5], x[10], x[15]);
        Qround(x[1], x[6], x[11], x[12]);
        Qround(x[2], x[7], x[8],  x[13]);
        Qround(x[3], x[4], x[9],  x[14]);
    }
    for (int i = 0; i < 16; i++) output[i] = start[i] + x[i];
}

static void chacha20_xor_stream(uint8_t *dest, const uint8_t *source, size_t length, const uint8_t key[32], const uint8_t nonce[12], uint32_t counter) {
    uint32_t state[16], pad[16];
    size_t full_blocks = length / 64;
    initialize_state(state, key, nonce, counter);
    for (size_t b = 0; b < full_blocks; b++) {
        core_block(state, pad);
        state[12]++;
        for (int i = 0; i < 16; i++) {
            uint32_t s;
            memcpy(&s, source + b * 64 + i * 4, 4);
            s ^= pad[i];
            memcpy(dest + b * 64 + i * 4, &s, 4);
        }
    }
    size_t last = length % 64;
    if (last > 0) {
        core_block(state, pad);
        uint8_t *p8 = (uint8_t *)pad;
        for (size_t i = 0; i < last; i++) dest[full_blocks * 64 + i] = source[full_blocks * 64 + i] ^ p8[i];
    }
}

int piqme_encrypt(const char *in_data, size_t in_size, char **out_data, size_t *out_size) {
    char *compressed;
    size_t comp_size;
    if (compress_data(in_data, in_size, &compressed, &comp_size) != 0) return -1;

    uint8_t key[32] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0xde, 0xad, 0xbe, 0xef, 0xba, 0xad, 0xfa, 0xce, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
    uint8_t nonce[12] = {0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00};

    *out_data = malloc(sizeof(size_t) + comp_size);
    memcpy(*out_data, &comp_size, sizeof(size_t));
    chacha20_xor_stream((uint8_t *)(*out_data + sizeof(size_t)), (const uint8_t *)compressed, comp_size, key, nonce, 0);
    *out_size = sizeof(size_t) + comp_size;
    
    free(compressed);
    return 0;
}

int piqme_decrypt(const char *in_data, size_t in_size, char **out_data, size_t *out_size) {
    if (in_size < sizeof(size_t)) return -1;
    size_t comp_size;
    memcpy(&comp_size, in_data, sizeof(size_t));

    uint8_t key[32] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0xde, 0xad, 0xbe, 0xef, 0xba, 0xad, 0xfa, 0xce, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
    uint8_t nonce[12] = {0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00};

    char *decompressed_in = malloc(comp_size);
    chacha20_xor_stream((uint8_t *)decompressed_in, (const uint8_t *)(in_data + sizeof(size_t)), comp_size, key, nonce, 0);

    int res = decompress_data(decompressed_in, comp_size, out_data, out_size);
    free(decompressed_in);
    return res;
}
