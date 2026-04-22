#include "filehandler.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

bool filehandler_exists(const char *path)
{
    struct stat st;
    return path != NULL && stat(path, &st) == 0;
}

long filehandler_get_size(const char *path)
{
    struct stat st;
    if (path == NULL || stat(path, &st) != 0) {
        return -1;
    }
    return (long)st.st_size;
}

char *filehandler_read_all(const char *path, size_t *out_size)
{
    FILE *fp = NULL;
    char *buffer = NULL;
    size_t size;
    long file_size;

    if (path == NULL || out_size == NULL) {
        return NULL;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    size = (size_t)file_size;
    buffer = malloc(size + 1);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }

    if (fread(buffer, 1, size, fp) != size) {
        free(buffer);
        fclose(fp);
        return NULL;
    }

    buffer[size] = '\0';
    *out_size = size;

    fclose(fp);
    return buffer;
}

int filehandler_write_all(const char *path, const void *data, size_t size)
{
    FILE *fp = NULL;
    if (path == NULL || data == NULL) {
        return -1;
    }
    fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }
    if (size > 0 && fwrite(data, 1, size, fp) != size) {
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        return -1;
    }
    return 0;
}

int filehandler_write_atomic(const char *path, const void *data, size_t size)
{
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    
    if (filehandler_write_all(tmp_path, data, size) != 0) {
        return -1;
    }
    
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return -1;
    }
    
    return 0;
}

int filehandler_append_all(const char *path, const void *data, size_t size)
{
    FILE *fp = NULL;
    if (path == NULL || data == NULL) {
        return -1;
    }
    fp = fopen(path, "ab");
    if (fp == NULL) {
        return -1;
    }
    if (size > 0 && fwrite(data, 1, size, fp) != size) {
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        return -1;
    }
    return 0;
}

int filehandler_mkdir(const char *path)
{
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

int filehandler_mkdir_p(const char *path) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if(tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for(p = tmp + 1; *p; p++)
        if(*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    mkdir(tmp, 0755);
    return 0;
}
