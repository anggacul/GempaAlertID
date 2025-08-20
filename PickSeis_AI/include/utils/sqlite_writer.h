#pragma once
#include <sqlite3.h>
 
int sqlite_init(const char* db_path);
void sqlite_close();
int sqlite_insert_pick(const char* station_id, double pick_time, float amplitude, float confidence); 