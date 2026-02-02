#ifndef IPC_H
#define IPC_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MSG_KEY 0x2222
#define SEM_KEY 0x3333
#define FIFO_PATH "/tmp/game_notify"

struct game_msg {
    long mtype;
    int player;           // 1 or 2
    int move;             // 1-9
    char username[64];    // For notifications and additional info
};

// Semaphore functions
int create_semaphore();
void sem_lock(int semid);
void sem_unlock(int semid);

// Message queue functions
int create_msg_queue();

// FIFO functions
int create_fifo();
void send_notification(int fd, const char *msg);
int receive_notification(int fd, char *buf, size_t size);

#endif