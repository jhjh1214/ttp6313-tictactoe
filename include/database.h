#ifndef DATABASE_H
#define DATABASE_H

#include <stddef.h>     // for size_t

// User authentication functions
int user_exists(const char *user);
int validate_login(const char *user, const char *pass);
void register_user(const char *user, const char *pass);

// Statistics tracking functions
void update_stats(const char *user, const char *result); // "WIN", "LOSS", "DRAW"
void get_leaderboard(char *buf, size_t size);

#endif