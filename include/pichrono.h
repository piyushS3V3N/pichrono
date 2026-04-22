#ifndef PICHRONO_H
#define PICHRONO_H

#include <stddef.h>

int pc_init(void);
int pc_add(const char *path);
int pc_commit(const char *message);
int pc_log(void);
int pc_checkout(const char *target);
int pc_branch(const char *name);
int pc_graph(void);
int pc_reflog(void);
int pc_recover(void);
int pc_serve(int port);
int pc_sync(const char *vcs);
char* read_object(const char *sha_hex, size_t *out_size);
char* get_current_branch(void);
void pc_reflog_append(const char *old_sha, const char *new_sha, const char *action);

#endif
