#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonDigest.h>

int compress_data(const char *in_data, size_t in_size, char **out_data, size_t *out_size) {
    uLongf dest_len = compressBound((uLong)in_size);
    *out_data = malloc(dest_len);
    if (!*out_data) return -1;

    int res = compress((Bytef*)*out_data, &dest_len, (const Bytef*)in_data, (uLong)in_size);
    if (res != Z_OK) {
        free(*out_data);
        return -1;
    }
    *out_size = (size_t)dest_len;
    return 0;
}

int decompress_data(const char *in_data, size_t in_size, char **out_data, size_t *out_size) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = (uInt)in_size;
    strm.next_in = (Bytef *)in_data;

    if (inflateInit(&strm) != Z_OK) return -1;

    size_t out_capacity = in_size * 2 + 1024;
    *out_data = malloc(out_capacity);
    if (!*out_data) {
        inflateEnd(&strm);
        return -1;
    }

    strm.avail_out = (uInt)out_capacity;
    strm.next_out = (Bytef *)*out_data;

    int ret;
    do {
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_BUF_ERROR || (ret == Z_OK && strm.avail_out == 0)) {
            size_t new_capacity = out_capacity * 2;
            char *new_data = realloc(*out_data, new_capacity);
            if (!new_data) {
                free(*out_data);
                inflateEnd(&strm);
                return -1;
            }
            strm.next_out = (Bytef *)(new_data + (out_capacity - strm.avail_out));
            strm.avail_out += (uInt)(new_capacity - out_capacity);
            *out_data = new_data;
            out_capacity = new_capacity;
        } else if (ret != Z_OK && ret != Z_STREAM_END) {
            free(*out_data);
            inflateEnd(&strm);
            return -1;
        }
    } while (ret != Z_STREAM_END);

    *out_size = strm.total_out;
    (*out_data)[*out_size] = '\0';
    inflateEnd(&strm);
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
