#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#define UATU_DB_NAME ".uatu.db"
#define IDLE_THRESHOLD_MS (5 * 60 * 1000) // 5 minutes

typedef struct {
    sqlite3 *db;
    const char *db_path;
} UatuDB;

// Initialize the database connection and schema if needed.
// Returns 0 on success, non-zero on failure.
int uatu_db_init(UatuDB *ctx);

// Find the project root (git root) for a given path.
// Returns a pointer to a static buffer containing the project name/path,
// or NULL if not in a project.
char* uatu_find_project_root(const char *path);

// Start a session for a given path and project.
// Returns the session ID on success, -1 on failure.
int64_t uatu_db_start_session(UatuDB *ctx, const char *path, const char *project);

// Update the last heartbeat timestamp for a given session ID.
// If idle time > threshold, closes old session and starts a new one.
// Returns the active session ID (which might be new), or -1 on failure.
int64_t uatu_db_heartbeat(UatuDB *ctx, int64_t session_id);

// Mark a session as inactive (stopped).
// Returns 0 on success, non-zero on failure.
int uatu_db_stop_session(UatuDB *ctx, int64_t session_id);

// Generate a report of time spent per directory.
// Prints directly to stdout.
// verbose: 1 for HH:MM:SS, 0 for human-readable (e.g. 1h 30m).
// Returns 0 on success, non-zero on failure.
int uatu_db_report(UatuDB *ctx, int verbose);

// Clean up invalid sessions (e.g. negative duration, zero duration).
// Returns the number of deleted rows.
int uatu_db_cleanup(UatuDB *ctx);

// Close the database connection.
void uatu_db_close(UatuDB *ctx);

#endif // DB_H
