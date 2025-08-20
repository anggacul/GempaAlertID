#include "utils/sqlite_writer.h"
#include <stdio.h>

static sqlite3* db = NULL;

int sqlite_init(const char* db_path) {
    if (sqlite3_open(db_path, &db) != SQLITE_OK) return 0;
    const char* sql = "CREATE TABLE IF NOT EXISTS picks ("
                      "station_id TEXT,"
                      "pick_time REAL,"
                      "amplitude REAL,"
                      "confidence REAL,"
                      "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                      "PRIMARY KEY (station_id, pick_time)"
                      ");";
    char* err = NULL;
    if (sqlite3_exec(db, sql, 0, 0, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return 0;
    }
    return 1;
}

void sqlite_close() {
    if (db) sqlite3_close(db);
    db = NULL;
}

int sqlite_insert_pick(const char* station_id, double pick_time, float amplitude, float confidence) {
    if (!db) return 0;
    const char* sql = "INSERT INTO picks (station_id, pick_time, amplitude, confidence) VALUES (?, ?, ?, ?) "
                      "ON CONFLICT(station_id, pick_time) DO UPDATE SET "
                      "amplitude=excluded.amplitude, "
                      "confidence=excluded.confidence, "
                      "created_at=CURRENT_TIMESTAMP;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, station_id, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, pick_time);
    sqlite3_bind_double(stmt, 3, amplitude);
    sqlite3_bind_double(stmt, 4, confidence);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
} 