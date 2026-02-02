#include "../include/ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int create_semaphore() {
    int semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget failed");
        return -1;
    }
    
    union semun u;
    u.val = 1;
    if (semctl(semid, 0, SETVAL, u) == -1) {
        perror("semctl SETVAL failed");
        return -1;
    }
    
    printf("[IPC] Semaphore created (ID: %d)\n", semid);
    return semid;
}

void sem_lock(int semid) {
    if (semid == -1) return;
    
    struct sembuf sb = {0, -1, 0};
    if (semop(semid, &sb, 1) == -1) {
        perror("semop lock failed");
    }
}

void sem_unlock(int semid) {
    if (semid == -1) return;
    
    struct sembuf sb = {0, 1, 0};
    if (semop(semid, &sb, 1) == -1) {
        perror("semop unlock failed");
    }
}

int create_msg_queue() {
    int qid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (qid == -1) {
        perror("msgget failed");
        return -1;
    }
    
    printf("[IPC] Message queue created (ID: %d)\n", qid);
    return qid;
}

int create_fifo() {
    // Remove existing FIFO if it exists
    unlink(FIFO_PATH);
    
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo failed");
            return -1;
        }
    }
    
    printf("[IPC] FIFO created at %s\n", FIFO_PATH);
    return 0;
}

void send_notification(int fd, const char *msg) {
    if (fd == -1) {
        fprintf(stderr, "Invalid FIFO fd\n");
        return;
    }
    
    ssize_t written = write(fd, msg, strlen(msg));
    if (written == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("write notification failed");
        }
    }
}

int receive_notification(int fd, char *buf, size_t size) {
    if (fd == -1) {
        fprintf(stderr, "Invalid FIFO fd\n");
        return -1;
    }
    
    int bytes = read(fd, buf, size);
    if (bytes == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read notification failed");
        }
        return -1;
    }
    
    return bytes;
}