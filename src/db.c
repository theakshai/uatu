#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>
#include <unistd.h>
#include "db.h"

// Helper function to get the current timestamp in milliseconds
int64_t current_timestamp_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)(ts.tv_sec) * 1000 + (ts.tv_nsec / 1000000);
}

#include <sys/stat.h>
#include <libgen.h>

// Helper to check if a directory contains .git
int has_git(const char *path) {
    char git_path[PATH_MAX];
    snprintf(git_path, sizeof(git_path), "%s/.git", path);
    struct stat sb;
    return (stat(git_path, &sb) == 0 && S_ISDIR(sb.st_mode));
}

char* uatu_find_project_root(const char *path) {
    static char root_path[PATH_MAX];
    char current_path[PATH_MAX];
    strncpy(current_path, path, PATH_MAX);

    // Safety check for root or empty
    if (strlen(current_path) == 0) return NULL;

    while (strlen(current_path) > 1) {
        if (has_git(current_path)) {
            strncpy(root_path, current_path, PATH_MAX);
            return root_path;
        }
        
        // Go up one level
        char *parent = dirname(current_path);
        // dirname might modify the string or return a pointer to static memory
        // safer to copy back
        char temp[PATH_MAX];
        strncpy(temp, parent, PATH_MAX);
        strncpy(current_path, temp, PATH_MAX);
    }
    
    // Check root / once more
    if (has_git("/")) {
        strcpy(root_path, "/");
        return root_path;
    }

    return NULL;
}

int uatu_db_init(UatuDB *ctx) {
    char *err_msg = 0;
    int rc = sqlite3_open(ctx->db_path, &ctx->db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(ctx->db));
        sqlite3_close(ctx->db);
        return 1;
    }

    // Enable WAL mode for better concurrency
    sqlite3_exec(ctx->db, "PRAGMA journal_mode=WAL;", 0, 0, 0);

    const char *sql = 
        "CREATE TABLE IF NOT EXISTS sessions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "path TEXT NOT NULL,"
        "project TEXT,"  // New column for project root
        "start_time INTEGER NOT NULL,"
        "last_heartbeat INTEGER NOT NULL,"
        "active INTEGER DEFAULT 1"
        ");";

    rc = sqlite3_exec(ctx->db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(ctx->db);
        return 1;
    }

    // Attempt to add 'project' column if it doesn't exist (migration)
    // We ignore errors here (e.g., duplicate column)
    sqlite3_exec(ctx->db, "ALTER TABLE sessions ADD COLUMN project TEXT;", 0, 0, 0);

    return 0;
}

int64_t uatu_db_start_session(UatuDB *ctx, const char *path, const char *project) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO sessions (path, project, start_time, last_heartbeat, active) VALUES (?, ?, ?, ?, 1);";
    
    if (sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(ctx->db));
        return -1;
    }

    int64_t now = current_timestamp_ms();
    
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    if (project) {
        sqlite3_bind_text(stmt, 2, project, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    sqlite3_bind_int64(stmt, 3, now);
    sqlite3_bind_int64(stmt, 4, now);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(ctx->db));
        sqlite3_finalize(stmt);
        return -1;
    }

    int64_t id = sqlite3_last_insert_rowid(ctx->db);
    sqlite3_finalize(stmt);
    return id;
}

int64_t uatu_db_heartbeat(UatuDB *ctx, int64_t session_id) {
    sqlite3_stmt *stmt;
    // Get the last_heartbeat, path, and project for the session
    const char *query_sql = "SELECT last_heartbeat, path, project FROM sessions WHERE id = ?;";
    
    if (sqlite3_prepare_v2(ctx->db, query_sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare query statement: %s\n", sqlite3_errmsg(ctx->db));
        return -1;
    }
    
    sqlite3_bind_int64(stmt, 1, session_id);
    
    int64_t last_heartbeat = 0;
    char path_buffer[1024];
    path_buffer[0] = '\0';
    char project_buffer[1024];
    project_buffer[0] = '\0';
    bool found = false;
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        last_heartbeat = sqlite3_column_int64(stmt, 0);
        const unsigned char *p = sqlite3_column_text(stmt, 1);
        if (p) strncpy(path_buffer, (const char*)p, sizeof(path_buffer) - 1);
        
        const unsigned char *proj = sqlite3_column_text(stmt, 2);
        if (proj) strncpy(project_buffer, (const char*)proj, sizeof(project_buffer) - 1);
        
        found = true;
    }
    sqlite3_finalize(stmt);
    
    if (!found) return -1;

    int64_t now = current_timestamp_ms();
    
    if ((now - last_heartbeat) > IDLE_THRESHOLD_MS) {
        uatu_db_stop_session(ctx, session_id);
        // Start NEW session, passing the existing project info
        return uatu_db_start_session(ctx, path_buffer, (strlen(project_buffer) > 0 ? project_buffer : NULL));
    } else {
        const char *update_sql = "UPDATE sessions SET last_heartbeat = ? WHERE id = ?;";
        if (sqlite3_prepare_v2(ctx->db, update_sql, -1, &stmt, 0) != SQLITE_OK) return -1;

        sqlite3_bind_int64(stmt, 1, now);
        sqlite3_bind_int64(stmt, 2, session_id);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return -1;
        }
        sqlite3_finalize(stmt);
        return session_id;
    }
}

int uatu_db_stop_session(UatuDB *ctx, int64_t session_id) {
    // ... (existing implementation is fine, no changes needed for project tracking)
    // COPYING existing implementation for completeness of replacement
    sqlite3_stmt *stmt;
    
    const char *query_sql = "SELECT last_heartbeat FROM sessions WHERE id = ?;";
    if (sqlite3_prepare_v2(ctx->db, query_sql, -1, &stmt, 0) != SQLITE_OK) return 1;
    sqlite3_bind_int64(stmt, 1, session_id);
    
    int64_t last_heartbeat = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        last_heartbeat = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    int64_t now = current_timestamp_ms();
    int64_t end_time = last_heartbeat;
    if ((now - last_heartbeat) <= IDLE_THRESHOLD_MS) {
        end_time = now;
    }

    const char *sql = "UPDATE sessions SET active = 0, last_heartbeat = ? WHERE id = ?;";

    if (sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, 0) != SQLITE_OK) return 1;

    sqlite3_bind_int64(stmt, 1, end_time);
    sqlite3_bind_int64(stmt, 2, session_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

// Helper to format duration smartly: "1h 30m", "45m 10s", or "5s"
static void format_duration_smart(char *buf, size_t size, int64_t duration_ms) {
    int total_seconds = (int)(duration_ms / 1000);
    int h = total_seconds / 3600;
    int m = (total_seconds % 3600) / 60;
    int s = total_seconds % 60;

    if (h > 0) {
        snprintf(buf, size, "%dh %dm", h, m);
    } else if (m > 0) {
        snprintf(buf, size, "%dm %ds", m, s);
    } else {
        snprintf(buf, size, "%ds", s);
    }
}

int uatu_db_report(UatuDB *ctx, int verbose) {
    sqlite3_stmt *stmt;
    
    // Report grouped by PROJECT first, then loose directories
    const char *sql = 
        "SELECT COALESCE(project, path) as group_name, "
        "SUM(last_heartbeat - start_time) as duration, "
        "COUNT(*) as sessions "
        "FROM sessions "
        "GROUP BY group_name "
        "ORDER BY duration DESC;";

    if (sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(ctx->db));
        return 1;
    }

    if (verbose) {
        printf("%-60s %-15s %s\n", "Project / Directory", "Time Spent", "Sessions");
        printf("%-60s %-15s %s\n", "-------------------", "----------", "--------");
    } else {
        printf("%-60s %-15s %s\n", "Project / Directory", "Time", "Sessions");
        printf("%-60s %-15s %s\n", "-------------------", "----", "--------");
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(stmt, 0);
        int64_t duration_ms = sqlite3_column_int64(stmt, 1);
        int session_count = sqlite3_column_int(stmt, 2);
        
        if (duration_ms < 0) continue;

        char time_buf[64];
        if (verbose) {
            int total_seconds = (int)(duration_ms / 1000);
            int h = total_seconds / 3600;
            int m = (total_seconds % 3600) / 60;
            int s = total_seconds % 60;
            snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", h, m, s);
        } else {
            format_duration_smart(time_buf, sizeof(time_buf), duration_ms);
        }

        printf("%-60s %-15s %d\n", name, time_buf, session_count);
    }

    sqlite3_finalize(stmt);
    return 0;
}

int uatu_db_cleanup(UatuDB *ctx) {
    char *err_msg = 0;
    // Delete sessions with negative duration or < 1 second (noise)
    const char *sql = "DELETE FROM sessions WHERE (last_heartbeat - start_time) < 1000;";
    
    int rc = sqlite3_exec(ctx->db, sql, 0, 0, &err_msg);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cleanup failed: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    
    return sqlite3_changes(ctx->db);
}

void uatu_db_close(UatuDB *ctx) {
    if (ctx->db) {
        sqlite3_close(ctx->db);
        ctx->db = NULL;
    }
}
