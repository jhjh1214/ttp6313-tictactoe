#define _POSIX_C_SOURCE 200809L
#include "../include/database.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/file.h>
#include <unistd.h>

#define USER_DB "data/users.db"
#define STAT_DB "data/stats.db"

int user_exists(const char *user) {
    FILE *f = fopen(USER_DB, "r");
    if (!f) return 0;
    
    int fd = fileno(f);
    flock(fd, LOCK_SH);  // Shared lock for reading
    
    char line[128];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        char line_copy[128];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        
        char *u = strtok(line_copy, ":");
        if (u && strcmp(u, user) == 0) {
            found = 1;
            break;
        }
    }
    
    flock(fd, LOCK_UN);
    fclose(f);
    return found;
}

int validate_login(const char *user, const char *pass) {
    FILE *f = fopen(USER_DB, "r");
    if (!f) {
        perror("fopen users.db failed");
        return 0;
    }
    
    int fd = fileno(f);
    flock(fd, LOCK_SH);  // Shared lock for reading
    
    char line[128];
    int valid = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        char line_copy[128];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        
        char *u = strtok(line_copy, ":");
        char *p = strtok(NULL, ":");
        if (u && p && strcmp(u, user) == 0 && strcmp(p, pass) == 0) {
            valid = 1;
            break;
        }
    }
    
    flock(fd, LOCK_UN);
    fclose(f);
    return valid;
}

void register_user(const char *user, const char *pass) {
    if (user_exists(user)) {
        fprintf(stderr, "User '%s' already exists\n", user);
        return;
    }
    
    FILE *f = fopen(USER_DB, "a");
    if (!f) {
        perror("fopen users.db for append failed");
        return;
    }
    
    int fd = fileno(f);
    flock(fd, LOCK_EX);  // Exclusive lock for writing
    
    fprintf(f, "%s:%s\n", user, pass);
    
    flock(fd, LOCK_UN);
    fclose(f);
    
    printf("[DATABASE] User '%s' registered successfully\n", user);
}

void update_stats(const char *user, const char *result) {
    FILE *f = fopen(STAT_DB, "a");
    if (!f) {
        perror("fopen stats.db for append failed");
        return;
    }
    
    int fd = fileno(f);
    flock(fd, LOCK_EX);  // Exclusive lock for writing
    
    fprintf(f, "%s %s\n", user, result);
    
    flock(fd, LOCK_UN);
    fclose(f);
    
    printf("[DATABASE] Updated stats for '%s': %s\n", user, result);
}

void get_leaderboard(char *buf, size_t size) {
    FILE *f = fopen(STAT_DB, "r");
    if (!f) {
        strncpy(buf, "No statistics available yet.\n", size);
        buf[size - 1] = '\0';
        return;
    }
    
    int fd = fileno(f);
    flock(fd, LOCK_SH);  // Shared lock for reading
    
    // Dynamic structure to track all users
    struct LeaderEntry {
        char user[64];
        int wins;
        int losses;
        int draws;
    };
    
    struct LeaderEntry *leaders = NULL;
    int count = 0;
    int capacity = 10;
    leaders = malloc(capacity * sizeof(struct LeaderEntry));
    
    if (!leaders) {
        flock(fd, LOCK_UN);
        fclose(f);
        strncpy(buf, "Memory allocation error\n", size);
        buf[size - 1] = '\0';
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char line_copy[256];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        
        char *u = strtok(line_copy, " ");
        char *r = strtok(NULL, " ");
        
        if (!u || !r) continue;
        
        // Find or create user entry
        int found = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(leaders[i].user, u) == 0) {
                if (strcmp(r, "WIN") == 0) {
                    leaders[i].wins++;
                } else if (strcmp(r, "LOSS") == 0) {
                    leaders[i].losses++;
                } else if (strcmp(r, "DRAW") == 0) {
                    leaders[i].draws++;
                }
                found = 1;
                break;
            }
        }
        
        if (!found) {
            // Expand array if needed
            if (count >= capacity) {
                capacity *= 2;
                struct LeaderEntry *new_leaders = realloc(leaders, capacity * sizeof(struct LeaderEntry));
                if (!new_leaders) {
                    free(leaders);
                    flock(fd, LOCK_UN);
                    fclose(f);
                    strncpy(buf, "Memory allocation error\n", size);
                    buf[size - 1] = '\0';
                    return;
                }
                leaders = new_leaders;
            }
            
            // Add new user
            strncpy(leaders[count].user, u, sizeof(leaders[count].user) - 1);
            leaders[count].user[sizeof(leaders[count].user) - 1] = '\0';
            leaders[count].wins = (strcmp(r, "WIN") == 0) ? 1 : 0;
            leaders[count].losses = (strcmp(r, "LOSS") == 0) ? 1 : 0;
            leaders[count].draws = (strcmp(r, "DRAW") == 0) ? 1 : 0;
            count++;
        }
    }
    
    flock(fd, LOCK_UN);
    fclose(f);
    
    // Sort by wins (descending), then by win rate
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            int total_j = leaders[j].wins + leaders[j].losses + leaders[j].draws;
            int total_j1 = leaders[j+1].wins + leaders[j+1].losses + leaders[j+1].draws;
            
            double rate_j = (total_j > 0) ? (double)leaders[j].wins / total_j : 0;
            double rate_j1 = (total_j1 > 0) ? (double)leaders[j+1].wins / total_j1 : 0;
            
            // Primary: sort by wins, Secondary: sort by win rate
            if (leaders[j].wins < leaders[j+1].wins || 
                (leaders[j].wins == leaders[j+1].wins && rate_j < rate_j1)) {
                struct LeaderEntry temp = leaders[j];
                leaders[j] = leaders[j+1];
                leaders[j+1] = temp;
            }
        }
    }
    
    // Build output string
    buf[0] = '\0';
    strncat(buf, "==========================================\n", size - strlen(buf) - 1);
    strncat(buf, "           LEADERBOARD (Top 10)          \n", size - strlen(buf) - 1);
    strncat(buf, "==========================================\n", size - strlen(buf) - 1);
    
    if (count == 0) {
        strncat(buf, "No games played yet.\n", size - strlen(buf) - 1);
    } else {
        int display_count = (count < 10) ? count : 10;
        for (int i = 0; i < display_count; i++) {
            char entry[256];
            int total = leaders[i].wins + leaders[i].losses + leaders[i].draws;
            double win_rate = (total > 0) ? (double)leaders[i].wins / total * 100 : 0;
            
            snprintf(entry, sizeof(entry), 
                     "%2d. %-20s | W:%3d L:%3d D:%3d | Rate: %.1f%%\n",
                     i + 1, 
                     leaders[i].user,
                     leaders[i].wins,
                     leaders[i].losses,
                     leaders[i].draws,
                     win_rate);
            strncat(buf, entry, size - strlen(buf) - 1);
        }
    }
    strncat(buf, "==========================================\n", size - strlen(buf) - 1);
    
    free(leaders);
}