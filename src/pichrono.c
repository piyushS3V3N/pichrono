#include "pichrono.h"
#include "filehandler.h"
#include "utils.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pc_init(void) {
    if (filehandler_exists(".pichrono")) {
        printf("Already a pichrono repository.\n");
        return -1;
    }
    filehandler_mkdir_p(".pichrono/objects");
    filehandler_mkdir_p(".pichrono/refs/heads");
    filehandler_mkdir_p(".pichrono/logs");
    
    const char *head_content = "ref: refs/heads/master\n";
    filehandler_write_all(".pichrono/HEAD", head_content, strlen(head_content));
    filehandler_write_all(".pichrono/index", "", 0);
    printf("Initialized empty pichrono repository in .pichrono/\n");
    return 0;
}

void pc_reflog_append(const char *old_sha, const char *new_sha, const char *action) {
    FILE *f = fopen(".pichrono/logs/HEAD", "a");
    if (!f) return;
    fprintf(f, "%s %s %s\n", old_sha ? old_sha : "0000000000000000000000000000000000000000", new_sha, action);
    fclose(f);
}

static void write_object(const char *sha_hex, const char *data, size_t size) {
    char dir[128], path[256];
    snprintf(dir, sizeof(dir), ".pichrono/objects/%.2s", sha_hex);
    snprintf(path, sizeof(path), "%s/%s", dir, sha_hex + 2);
    filehandler_mkdir_p(dir);
    
    char *compressed;
    size_t comp_size;
    if (compress_data(data, size, &compressed, &comp_size) == 0) {
        filehandler_write_all(path, compressed, comp_size);
        free(compressed);
    }
}

char* read_object(const char *sha_hex, size_t *out_size) {
    char path[256];
    snprintf(path, sizeof(path), ".pichrono/objects/%.2s/%s", sha_hex, sha_hex + 2);
    size_t comp_size;
    char *compressed = filehandler_read_all(path, &comp_size);
    if (!compressed) return NULL;

    char *decompressed;
    if (decompress_data(compressed, comp_size, &decompressed, out_size) == 0) {
        free(compressed);
        return decompressed;
    }
    free(compressed);
    return NULL;
}

int pc_add(const char *path) {
    if (path == NULL || path[0] == '/' || strstr(path, "..")) {
        printf("fatal: invalid path '%s'\n", path ? path : "NULL");
        return -1;
    }

    size_t file_size;
    char *content = filehandler_read_all(path, &file_size);
    if (!content) {
        printf("fatal: pathspec '%s' did not match any files\n", path);
        return -1;
    }

    size_t obj_size = file_size + 32;
    char *obj_data = malloc(obj_size);
    int hdr_len = snprintf(obj_data, obj_size, "blob %zu", file_size);
    obj_data[hdr_len] = '\0';
    memcpy(obj_data + hdr_len + 1, content, file_size);
    size_t total_len = hdr_len + 1 + file_size;

    char sha_hex[41];
    compute_sha1(obj_data, total_len, sha_hex);
    write_object(sha_hex, obj_data, total_len);
    free(obj_data);
    free(content);

    size_t idx_size;
    char *idx = filehandler_read_all(".pichrono/index", &idx_size);
    if (!idx) idx = calloc(1, 1);

    char *new_idx = malloc(idx_size + 512);
    new_idx[0] = '\0';

    char *line = strtok(idx, "\n");
    int found = 0;
    while (line) {
        char old_sha[41], old_path[256];
        if (sscanf(line, "%40s %255s", old_sha, old_path) == 2) {
            if (strcmp(old_path, path) == 0) {
                sprintf(new_idx + strlen(new_idx), "%s %s\n", sha_hex, path);
                found = 1;
            } else {
                sprintf(new_idx + strlen(new_idx), "%s %s\n", old_sha, old_path);
            }
        }
        line = strtok(NULL, "\n");
    }
    if (!found) {
        sprintf(new_idx + strlen(new_idx), "%s %s\n", sha_hex, path);
    }
    filehandler_write_atomic(".pichrono/index", new_idx, strlen(new_idx));
    free(idx);
    free(new_idx);

    return 0;
}

char* get_current_branch(void) {
    size_t size;
    char *head = filehandler_read_all(".pichrono/HEAD", &size);
    if (!head) return NULL;
    char *branch = malloc(256);
    if (sscanf(head, "ref: %255s", branch) == 1) {
        free(head);
        return branch;
    }
    free(head);
    return NULL;
}

int pc_commit(const char *message) {
    size_t idx_size;
    char *idx = filehandler_read_all(".pichrono/index", &idx_size);
    if (!idx || idx_size == 0) {
        printf("nothing to commit\n");
        if(idx) free(idx);
        return -1;
    }

    size_t tree_obj_size = idx_size + 32;
    char *tree_obj = malloc(tree_obj_size);
    int hdr_len = snprintf(tree_obj, tree_obj_size, "tree %zu", idx_size);
    tree_obj[hdr_len] = '\0';
    memcpy(tree_obj + hdr_len + 1, idx, idx_size);
    size_t tree_total_len = hdr_len + 1 + idx_size;

    char tree_sha[41];
    compute_sha1(tree_obj, tree_total_len, tree_sha);
    write_object(tree_sha, tree_obj, tree_total_len);
    free(tree_obj);
    free(idx);

    char *branch = get_current_branch();
    char ref_path[256];
    snprintf(ref_path, sizeof(ref_path), ".pichrono/%s", branch);
    
    char parent_sha[41] = {0};
    size_t psize;
    char *parent = filehandler_read_all(ref_path, &psize);
    if (parent) {
        sscanf(parent, "%40s", parent_sha);
        free(parent);
    }

    char commit_content[1024];
    int c_len;
    if (strlen(parent_sha) == 40) {
        c_len = snprintf(commit_content, sizeof(commit_content), 
            "tree %s\nparent %s\n\n%s\n", tree_sha, parent_sha, message);
    } else {
        c_len = snprintf(commit_content, sizeof(commit_content), 
            "tree %s\n\n%s\n", tree_sha, message);
    }

    size_t cobj_size = c_len + 32;
    char *cobj = malloc(cobj_size);
    hdr_len = snprintf(cobj, cobj_size, "commit %d", c_len);
    cobj[hdr_len] = '\0';
    memcpy(cobj + hdr_len + 1, commit_content, c_len);
    size_t c_total_len = hdr_len + 1 + c_len;

    char commit_sha[41];
    compute_sha1(cobj, c_total_len, commit_sha);
    write_object(commit_sha, cobj, c_total_len);
    free(cobj);

    char final_sha[42];
    sprintf(final_sha, "%s\n", commit_sha);
    filehandler_write_atomic(ref_path, final_sha, strlen(final_sha));
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "commit: %s", message);
    pc_reflog_append(strlen(parent_sha) == 40 ? parent_sha : NULL, commit_sha, log_msg);

    printf("[%s] %s\n", commit_sha, message);
    free(branch);
    return 0;
}

int pc_log(void) {
    char *branch = get_current_branch();
    char ref_path[256];
    snprintf(ref_path, sizeof(ref_path), ".pichrono/%s", branch);
    free(branch);

    size_t size;
    char *current = filehandler_read_all(ref_path, &size);
    if (!current) return 0;
    
    char curr_sha[41];
    sscanf(current, "%40s", curr_sha);
    free(current);

    while (strlen(curr_sha) == 40) {
        size_t obj_size;
        char *obj = read_object(curr_sha, &obj_size);
        if (!obj) break;
        
        char *content = obj + strlen(obj) + 1;
        printf("commit %s\n", curr_sha);
        
        char *msg_start = strstr(content, "\n\n");
        if (msg_start) {
            msg_start += 2;
        } else {
            msg_start = "";
        }

        char parent[41] = {0};
        char *parent_str = strstr(content, "parent ");
        if (parent_str && (!msg_start || parent_str < msg_start)) {
            sscanf(parent_str, "parent %40s", parent);
        }
        
        printf("\n    %s\n", msg_start);
        
        strcpy(curr_sha, parent);
        free(obj);
    }
    return 0;
}

static void create_parent_dirs(const char *path) {
    char *tmp = strdup(path);
    char *slash = strrchr(tmp, '/');
    if (slash) {
        *slash = '\0';
        filehandler_mkdir_p(tmp);
    }
    free(tmp);
}

int pc_checkout(const char *target) {
    char old_sha[41] = {0};
    char *curr_branch = get_current_branch();
    if (curr_branch) {
        char path[256];
        snprintf(path, sizeof(path), ".pichrono/%s", curr_branch);
        size_t s;
        char *content = filehandler_read_all(path, &s);
        if (content) {
            sscanf(content, "%40s", old_sha);
            free(content);
        }
        free(curr_branch);
    } else {
        size_t s;
        char *head = filehandler_read_all(".pichrono/HEAD", &s);
        if (head) {
            sscanf(head, "%40s", old_sha);
            free(head);
        }
    }

    char branch_path[256], actual_sha[41];
    int is_branch = 0;
    snprintf(branch_path, sizeof(branch_path), ".pichrono/refs/heads/%s", target);
    
    if (filehandler_exists(branch_path)) {
        size_t s;
        char *content = filehandler_read_all(branch_path, &s);
        if (content) {
            sscanf(content, "%40s", actual_sha);
            free(content);
            is_branch = 1;
        }
    } else {
        strncpy(actual_sha, target, 40);
        actual_sha[40] = '\0';
    }

    size_t obj_size;
    char *cobj = read_object(actual_sha, &obj_size);
    if (!cobj) {
        printf("fatal: reference is not a tree: %s\n", target);
        return -1;
    }

    char *content = cobj + strlen(cobj) + 1;
    char tree_sha[41];
    if (sscanf(content, "tree %40s", tree_sha) != 1) {
        free(cobj);
        return -1;
    }

    size_t tree_obj_size;
    char *tree_obj = read_object(tree_sha, &tree_obj_size);
    if (!tree_obj) {
        free(cobj);
        return -1;
    }

    char *tree_content = tree_obj + strlen(tree_obj) + 1;
    char *idx_backup = strdup(tree_content);

    char *line = strtok(tree_content, "\n");
    while (line) {
        char blob_sha[41], path[256];
        if (sscanf(line, "%40s %255s", blob_sha, path) == 2) {
            size_t blob_size;
            char *blob = read_object(blob_sha, &blob_size);
            if (blob) {
                char *blob_data = blob + strlen(blob) + 1;
                size_t header_len = strlen(blob);
                size_t actual_size = blob_size - (header_len + 1);
                create_parent_dirs(path);
                filehandler_write_all(path, blob_data, actual_size);
                free(blob);
            }
        }
        line = strtok(NULL, "\n");
    }

    filehandler_write_atomic(".pichrono/index", idx_backup, strlen(idx_backup));
    
    if (is_branch) {
        char head_content[256];
        snprintf(head_content, sizeof(head_content), "ref: refs/heads/%s\n", target);
        filehandler_write_atomic(".pichrono/HEAD", head_content, strlen(head_content));
    } else {
        // Detached HEAD
        char head_content[64];
        snprintf(head_content, sizeof(head_content), "%s\n", actual_sha);
        filehandler_write_atomic(".pichrono/HEAD", head_content, strlen(head_content));
    }

    free(idx_backup);
    free(tree_obj);
    free(cobj);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "checkout: moving to %s", target);
    pc_reflog_append(strlen(old_sha) == 40 ? old_sha : NULL, actual_sha, log_msg);

    printf("Switched to %s '%s'\n", is_branch ? "branch" : "commit", target);
    return 0;
}

int pc_branch(const char *name) {
    if (!name) {
        // List branches
        DIR *d = opendir(".pichrono/refs/heads");
        if (!d) return -1;
        
        char *current = get_current_branch();
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_name[0] == '.') continue;
            char ref_full[256];
            snprintf(ref_full, sizeof(ref_full), "refs/heads/%s", dir->d_name);
            if (current && strcmp(current, ref_full) == 0) {
                printf("* %s\n", dir->d_name);
            } else {
                printf("  %s\n", dir->d_name);
            }
        }
        closedir(d);
        if (current) free(current);
        return 0;
    }

    // Create branch
    char *curr_branch = get_current_branch();
    if (!curr_branch) {
        printf("fatal: not on a branch, cannot create new branch\n");
        return -1;
    }

    char ref_path[256];
    snprintf(ref_path, sizeof(ref_path), ".pichrono/%s", curr_branch);
    size_t s;
    char *sha = filehandler_read_all(ref_path, &s);
    if (!sha) {
        printf("fatal: current branch has no commits\n");
        free(curr_branch);
        return -1;
    }

    char new_path[256];
    snprintf(new_path, sizeof(new_path), ".pichrono/refs/heads/%s", name);
    filehandler_write_atomic(new_path, sha, strlen(sha));
    
    printf("Branch '%s' created at %s", name, sha);
    free(curr_branch);
    free(sha);
    return 0;
}

typedef struct {
    char sha[41];
    char name[128];
} BranchRef;

int pc_graph(void) {
    BranchRef branches[64];
    int branch_count = 0;

    DIR *d = opendir(".pichrono/refs/heads");
    if (!d) return -1;
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL && branch_count < 64) {
        if (dir->d_name[0] == '.') continue;
        char path[256];
        snprintf(path, sizeof(path), ".pichrono/refs/heads/%s", dir->d_name);
        size_t s;
        char *content = filehandler_read_all(path, &s);
        if (content) {
            sscanf(content, "%40s", branches[branch_count].sha);
            strncpy(branches[branch_count].name, dir->d_name, 127);
            branch_count++;
            free(content);
        }
    }
    closedir(d);

    char *curr_branch = get_current_branch();
    char curr_sha[41] = {0};
    if (curr_branch) {
        char path[256];
        snprintf(path, sizeof(path), ".pichrono/%s", curr_branch);
        size_t s;
        char *content = filehandler_read_all(path, &s);
        if (content) {
            sscanf(content, "%40s", curr_sha);
            free(content);
        }
    }

    // Basic graph traversal (linear for now as we don't have merges)
    char walk_sha[41];
    strcpy(walk_sha, curr_sha);

    while (strlen(walk_sha) == 40) {
        size_t obj_size;
        char *obj = read_object(walk_sha, &obj_size);
        if (!obj) break;

        char *content = obj + strlen(obj) + 1;
        printf("* ");
        
        // Print branch labels
        for (int i = 0; i < branch_count; i++) {
            if (strcmp(branches[i].sha, walk_sha) == 0) {
                printf("[%s] ", branches[i].name);
            }
        }

        char *msg_start = strstr(content, "\n\n");
        if (msg_start) {
            msg_start += 2;
            char *msg_end = strchr(msg_start, '\n');
            if (msg_end) *msg_end = '\0';
            printf("%s (%s)\n", msg_start, walk_sha);
        } else {
            printf("(%s)\n", walk_sha);
        }

        char parent[41] = {0};
        char *parent_str = strstr(content, "parent ");
        if (parent_str) {
            sscanf(parent_str, "parent %40s", parent);
            printf("| \n");
        }
        
        strcpy(walk_sha, parent);
        free(obj);
    }

    if (curr_branch) free(curr_branch);
    return 0;
}

int pc_reflog(void) {
    size_t s;
    char *content = filehandler_read_all(".pichrono/logs/HEAD", &s);
    if (!content) {
        printf("Reflog is empty.\n");
        return 0;
    }
    printf("%s", content);
    free(content);
    return 0;
}

int pc_recover(void) {
    DIR *d = opendir(".pichrono/objects");
    if (!d) return -1;

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (strlen(dir->d_name) != 2) continue;

        char subdir_path[256];
        snprintf(subdir_path, sizeof(subdir_path), ".pichrono/objects/%s", dir->d_name);
        DIR *sd = opendir(subdir_path);
        if (!sd) continue;

        struct dirent *sdir;
        while ((sdir = readdir(sd)) != NULL) {
            if (sdir->d_name[0] == '.') continue;

            char full_sha[41];
            snprintf(full_sha, sizeof(full_sha), "%s%s", dir->d_name, sdir->d_name);

            size_t obj_size;
            char *obj = read_object(full_sha, &obj_size);
            if (obj && strncmp(obj, "commit ", 7) == 0) {
                char *content = obj + strlen(obj) + 1;
                char *msg_start = strstr(content, "\n\n");
                if (msg_start) {
                    msg_start += 2;
                    char *msg_end = strchr(msg_start, '\n');
                    if (msg_end) *msg_end = '\0';
                    printf("Found commit %s: %s\n", full_sha, msg_start);
                } else {
                    printf("Found commit %s\n", full_sha);
                }
            }
            if (obj) free(obj);
        }
        closedir(sd);
    }
    closedir(d);
    return 0;
}

int pc_sync(const char *vcs) {
    if (strcmp(vcs, "git") != 0) {
        printf("fatal: unsupported VCS '%s'\n", vcs);
        return -1;
    }

    if (!filehandler_exists(".git")) {
        printf("fatal: .git directory not found\n");
        return -1;
    }

    // 1. Get latest Git commit message
    FILE *fp_msg = popen("git log -1 --pretty=%B", "r");
    if (!fp_msg) return -1;
    char git_msg[1024] = {0};
    fgets(git_msg, sizeof(git_msg), fp_msg);
    pclose(fp_msg);
    
    // Trim newline
    size_t len = strlen(git_msg);
    if (len > 0 && git_msg[len-1] == '\n') git_msg[len-1] = '\0';

    char sync_msg[2048];
    snprintf(sync_msg, sizeof(sync_msg), "[Git Sync] %s", strlen(git_msg) > 0 ? git_msg : "Snapshot from Git");

    // 2. Get all tracked files and add them
    FILE *fp_files = popen("git ls-files", "r");
    if (!fp_files) return -1;
    char file_path[256];
    int count = 0;
    while (fgets(file_path, sizeof(file_path), fp_files)) {
        size_t flen = strlen(file_path);
        if (flen > 0 && file_path[flen-1] == '\n') file_path[flen-1] = '\0';
        if (pc_add(file_path) == 0) count++;
    }
    pclose(fp_files);

    if (count == 0) {
        printf("nothing to sync\n");
        return 0;
    }

    // 3. Create Pichrono commit
    return pc_commit(sync_msg);
}
