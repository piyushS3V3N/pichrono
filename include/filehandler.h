#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool filehandler_exists(const char *path);
long filehandler_get_size(const char *path);
char *filehandler_read_all(const char *path, size_t *out_size);
int filehandler_write_all(const char *path, const void *data, size_t size);
int filehandler_write_atomic(const char *path, const void *data, size_t size);
int filehandler_append_all(const char *path, const void *data, size_t size);
int filehandler_mkdir(const char *path);
int filehandler_mkdir_p(const char *path);

#endif // FILEHANDLER_H
