#ifndef PIQME_H
#define PIQME_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encrypts data using PiQME (Zstd + ChaCha20 via Metal).
 * @param in_data Input data.
 * @param in_size Input data size.
 * @param out_data Output pointer (allocated by function).
 * @param out_size Output data size.
 * @return 0 on success, -1 on failure.
 */
int piqme_encrypt(const char *in_data, size_t in_size, char **out_data, size_t *out_size);

/**
 * Decrypts data using PiQME.
 * @param in_data Input data.
 * @param in_size Input data size.
 * @param out_data Output pointer (allocated by function).
 * @param out_size Output data size.
 * @return 0 on success, -1 on failure.
 */
int piqme_decrypt(const char *in_data, size_t in_size, char **out_data, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif
