#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include "db.h"

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <command> [args]\n", prog_name);
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  start <path>     Start a tracking session for a directory\n");
    fprintf(stderr, "  heartbeat <id>   Update the heartbeat for a session\n");
    fprintf(stderr, "  stop <id>        Stop a tracking session\n");
    fprintf(stderr, "  report           Show time spent per directory/project\n");
    fprintf(stderr, "  cleanup          Remove short (<1s) or invalid sessions\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *command = argv[1];
    UatuDB db_ctx;
    
    // Construct database path: ~/.uatu/uatu.db
    char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Error: HOME environment variable not set.\n");
        return 1;
    }

    char db_path[PATH_MAX];
    snprintf(db_path, sizeof(db_path), "%s/.uatu/uatu.db", home);
    db_ctx.db_path = db_path;

    // Ensure the .uatu directory exists
    char dir_path[PATH_MAX];
    snprintf(dir_path, sizeof(dir_path), "%s/.uatu", home);
    
    // Auto-Initialization: Ensure directory and DB exist for ANY command
    // This makes the tool "just work" without a manual init step.
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        char mkdir_cmd[PATH_MAX + 10];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", dir_path);
        system(mkdir_cmd);
    }
    
    if (uatu_db_init(&db_ctx) != 0) {
        return 1;
    }

    if (strcmp(command, "start") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s start <path>\n", argv[0]);
            return 1;
        }
        char *path = argv[2];
        // Resolve absolute path
        char real_path[PATH_MAX];
        if (realpath(path, real_path) == NULL) {
             // If path doesn't exist, maybe track it anyway? 
             // Ideally we only track existing dirs.
             // Fallback to provided path if realpath fails (e.g. deleted dir)
             strncpy(real_path, path, PATH_MAX);
        }
        
        char *project = uatu_find_project_root(real_path);
        
        int64_t id = uatu_db_start_session(&db_ctx, real_path, project);
        if (id != -1) {
            printf("%lld\n", id);
        } else {
            return 1;
        }

    } else if (strcmp(command, "heartbeat") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s heartbeat <id>\n", argv[0]);
            return 1;
        }
        int64_t id = atoll(argv[2]);
        int64_t new_id = uatu_db_heartbeat(&db_ctx, id);
        
        if (new_id != -1) {
            printf("%lld\n", new_id);
            return 0;
        } else {
            return 1;
        }

    } else if (strcmp(command, "stop") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s stop <id>\n", argv[0]);
            return 1;
        }
        int64_t id = atoll(argv[2]);
        if (uatu_db_stop_session(&db_ctx, id) != 0) {
            return 1;
        }

    } else if (strcmp(command, "report") == 0) {
        if (uatu_db_report(&db_ctx) != 0) {
            return 1;
        }

    } else if (strcmp(command, "cleanup") == 0) {
        int deleted = uatu_db_cleanup(&db_ctx);
        if (deleted >= 0) {
            printf("Removed %d invalid or short (<1s) sessions.\n", deleted);
        } else {
            return 1;
        }

    } else {
        print_usage(argv[0]);
        return 1;
    }

    uatu_db_close(&db_ctx);
    return 0;
}
