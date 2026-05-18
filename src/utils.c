#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zstd.h>

#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonDigest.h>

int compress_data(const char *in_data, size_t in_size, char **out_data, size_t *out_size) {
    size_t dest_capacity = ZSTD_compressBound(in_size);
    *out_data = malloc(dest_capacity);
    if (!*out_data) return -1;

    size_t const c_size = ZSTD_compress(*out_data, dest_capacity, in_data, in_size, 1);
    if (ZSTD_isError(c_size)) {
        free(*out_data);
        return -1;
    }
    *out_size = c_size;
    return 0;
}

int decompress_data(const char *in_data, size_t in_size, char **out_data, size_t *out_size) {
    unsigned long long const r_size = ZSTD_getFrameContentSize(in_data, in_size);
    if (r_size == ZSTD_CONTENTSIZE_ERROR || r_size == ZSTD_CONTENTSIZE_UNKNOWN) return -1;

    *out_data = malloc((size_t)r_size + 1);
    if (!*out_data) return -1;

    size_t const d_size = ZSTD_decompress(*out_data, (size_t)r_size, in_data, in_size);
    if (ZSTD_isError(d_size)) {
        free(*out_data);
        return -1;
    }
    *out_size = d_size;
    (*out_data)[d_size] = '\0';
    return 0;
}

void compute_sha1(const char *data, size_t size, char *out_hex) {
    unsigned char hash[CC_SHA1_DIGEST_LENGTH];
    CC_SHA1(data, (CC_LONG)size, hash);
    for (int i = 0; i < CC_SHA1_DIGEST_LENGTH; i++) {
        sprintf(out_hex + (i * 2), "%02x", hash[i]);
    }
    out_hex[40] = '\0';
}
