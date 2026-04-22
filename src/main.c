#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pichrono.h"

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        printf("Commands: init, add, commit, log, checkout, branch, graph, reflog, recover, serve\n");
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "init") == 0) {
        return pc_init();
    } else if (strcmp(cmd, "add") == 0) {
        if (argc < 3) {
            printf("Usage: %s add <file>\n", argv[0]);
            return 1;
        }
        return pc_add(argv[2]);
    } else if (strcmp(cmd, "commit") == 0) {
        if (argc < 4 || strcmp(argv[2], "-m") != 0) {
            printf("Usage: %s commit -m <message>\n", argv[0]);
            return 1;
        }
        return pc_commit(argv[3]);
    } else if (strcmp(cmd, "log") == 0) {
        return pc_log();
    } else if (strcmp(cmd, "checkout") == 0) {
        if (argc < 3) {
            printf("Usage: %s checkout <commit-sha|branch-name>\n", argv[0]);
            return 1;
        }
        return pc_checkout(argv[2]);
    } else if (strcmp(cmd, "branch") == 0) {
        return pc_branch(argc > 2 ? argv[2] : NULL);
    } else if (strcmp(cmd, "graph") == 0) {
        return pc_graph();
    } else if (strcmp(cmd, "reflog") == 0) {
        return pc_reflog();
    } else if (strcmp(cmd, "recover") == 0) {
        return pc_recover();
    } else if (strcmp(cmd, "serve") == 0) {
        int port = 8080;
        if (argc > 2) port = atoi(argv[2]);
        return pc_serve(port);
    } else if (strcmp(cmd, "sync") == 0) {
        if (argc < 3) {
            printf("Usage: %s sync <vcs-name>\n", argv[0]);
            return 1;
        }
        return pc_sync(argv[2]);
    } else {
        printf("Unknown command: %s\n", cmd);
        return 1;
    }

    return 0;
}
