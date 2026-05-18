#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int compress_data(const char *in_data, size_t in_size, char **out_data, size_t *out_size);
int decompress_data(const char *in_data, size_t in_size, char **out_data, size_t *out_size);
void compute_sha1(const char *data, size_t size, char *out_hex);

#ifdef __cplusplus
}
#endif

#endif
