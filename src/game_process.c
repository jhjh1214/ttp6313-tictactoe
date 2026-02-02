#include "../include/ipc.h"
#include "../include/database.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define BUF_SIZE 1024
#define TURN_TIMEOUT_SEC 30

char board[9] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
int p1_fd, p2_fd;
char p1_user[64], p2_user[64];
int turn = 1;
int msg_queue_id;

void print_board(char *buf);

void send_to_both(const char *msg) {
    send(p1_fd, msg, strlen(msg), 0);
    send(p2_fd, msg, strlen(msg), 0);
}

void send_board() {
    char buf[BUF_SIZE] = "BOARD:\n";
    char board_str[256];
    print_board(board_str);
    strncat(buf, board_str, sizeof(buf) - strlen(buf) - 1);
    send_to_both(buf);
}

void send_turn(int player_fd) {
    char msg[64] = "YOUR_TURN\n";
    if (send(player_fd, msg, strlen(msg), 0) == -1) {
        perror("send turn failed");
    }
}

void print_board(char *buf) {
    sprintf(buf, " %c | %c | %c \n---+---+---\n %c | %c | %c \n---+---+---\n %c | %c | %c \n\n",
            board[0], board[1], board[2], 
            board[3], board[4], board[5], 
            board[6], board[7], board[8]);
}

int check_win(char c) {
    int w[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8},  // rows
        {0,3,6}, {1,4,7}, {2,5,8},  // columns
        {0,4,8}, {2,4,6}             // diagonals
    };
    for (int i = 0; i < 8; i++) {
        if (board[w[i][0]] == c && board[w[i][1]] == c && board[w[i][2]] == c) {
            return 1;
        }
    }
    return 0;
}

int board_full() {
    for (int i = 0; i < 9; i++) {
        if (board[i] == ' ') return 0;
    }
    return 1;
}

void send_game_notification(const char *msg) {
    struct game_msg notification;
    notification.mtype = 1;
    strncpy(notification.username, msg, sizeof(notification.username) - 1);
    notification.username[sizeof(notification.username) - 1] = '\0';
    msgsnd(msg_queue_id, &notification, sizeof(notification) - sizeof(long), IPC_NOWAIT);
    
    // Also write to FIFO if available
    int fifo_fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
    if (fifo_fd != -1) {
        write(fifo_fd, msg, strlen(msg));
        close(fifo_fd);
    }
}

void end_game(const char *result, int winner) {
    char msg[128];
    
    if (strcmp(result, "WIN") == 0) {
        snprintf(msg, sizeof(msg), "GAME_OVER: PLAYER_%d_WINS (%s wins!)\n", 
                 winner, (winner == 1) ? p1_user : p2_user);
        send_to_both(msg);
        
        update_stats((winner == 1) ? p1_user : p2_user, "WIN");
        update_stats((winner == 1) ? p2_user : p1_user, "LOSS");
        
        snprintf(msg, sizeof(msg), "Game ended: %s defeats %s", 
                 (winner == 1) ? p1_user : p2_user,
                 (winner == 1) ? p2_user : p1_user);
        send_game_notification(msg);
    } else {
        send_to_both("GAME_OVER: DRAW\n");
        update_stats(p1_user, "DRAW");
        update_stats(p2_user, "DRAW");
        
        snprintf(msg, sizeof(msg), "Game ended: %s vs %s - Draw", p1_user, p2_user);
        send_game_notification(msg);
    }
    
    printf("[GAME] Game ended between %s and %s\n", p1_user, p2_user);
    
    // Exit game process - server will detect via SIGCHLD and return players to lobby
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s p1_fd p2_fd p1_user p2_user\n", argv[0]);
        exit(1);
    }
    
    p1_fd = atoi(argv[1]);
    p2_fd = atoi(argv[2]);
    
    strncpy(p1_user, argv[3], sizeof(p1_user) - 1);
    p1_user[sizeof(p1_user) - 1] = '\0';
    strncpy(p2_user, argv[4], sizeof(p2_user) - 1);
    p2_user[sizeof(p2_user) - 1] = '\0';
    
    // Disable Nagle's algorithm for immediate message delivery
    int flag = 1;
    setsockopt(p1_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    setsockopt(p2_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    
    // Get message queue for notifications
    msg_queue_id = msgget(MSG_KEY, 0666);
    if (msg_queue_id == -1) {
        perror("msgget failed - notifications disabled");
    }
    
    printf("[GAME] Starting game: %s (P1) vs %s (P2)\n", p1_user, p2_user);
    
    // Send game start notifications
    char start_msg[256];
    snprintf(start_msg, sizeof(start_msg), 
             "Game started: %s vs %s", p1_user, p2_user);
    send_game_notification(start_msg);
    
    // Inform players of their roles
    send(p1_fd, "GAME_START: YOU_ARE_PLAYER_1 (X)\n", 33, 0);
    send(p2_fd, "GAME_START: YOU_ARE_PLAYER_2 (O)\n", 33, 0);
    
    sleep(1);  // Brief pause for readability
    
    send_board();
    send_turn(p1_fd);
    
    while (1) {
        int current_fd = (turn == 1) ? p1_fd : p2_fd;
        int other_fd = (turn == 1) ? p2_fd : p1_fd;
        
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(current_fd, &rfds);
        
        struct timeval timeout;
        timeout.tv_sec = TURN_TIMEOUT_SEC;
        timeout.tv_usec = 0;
        
        int ret = select(current_fd + 1, &rfds, NULL, NULL, &timeout);
        
        if (ret == -1) {
            perror("select failed");
            break;
        } else if (ret == 0) {
            // Timeout occurred
            char win_msg[128];
            char lose_msg[128];
            
            snprintf(win_msg, sizeof(win_msg), 
                     "OPPONENT_TIMEOUT: %s failed to move in time. YOU_WIN!\n",
                     (turn == 1) ? p1_user : p2_user);
            snprintf(lose_msg, sizeof(lose_msg), 
                     "TIMEOUT: You failed to move in time. YOU_LOSE!\n");
            
            send(other_fd, win_msg, strlen(win_msg), 0);
            send(current_fd, lose_msg, strlen(lose_msg), 0);
            
            update_stats((turn == 1) ? p2_user : p1_user, "WIN");
            update_stats((turn == 1) ? p1_user : p2_user, "LOSS");
            
            snprintf(win_msg, sizeof(win_msg), 
                     "Game timeout: %s wins by default", 
                     (turn == 1) ? p2_user : p1_user);
            send_game_notification(win_msg);
            
            exit(0);
        }
        
        // Receive move from current player
        char buf[64];
        int bytes = recv(current_fd, buf, sizeof(buf) - 1, 0);
        
        if (bytes <= 0) {
            // Player disconnected
            char msg[128];
            snprintf(msg, sizeof(msg), 
                     "OPPONENT_DISCONNECTED: %s left the game. YOU_WIN!\n",
                     (turn == 1) ? p1_user : p2_user);
            send(other_fd, msg, strlen(msg), 0);
            
            update_stats((turn == 1) ? p2_user : p1_user, "WIN");
            update_stats((turn == 1) ? p1_user : p2_user, "LOSS");
            
            snprintf(msg, sizeof(msg), 
                     "Player disconnect: %s wins by default",
                     (turn == 1) ? p2_user : p1_user);
            send_game_notification(msg);
            
            exit(0);
        }
        
        buf[bytes] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';  // Remove newline
        
        printf("[GAME] Player %d (%s) move: '%s'\n", 
               turn, (turn == 1) ? p1_user : p2_user, buf);
        
        // Parse move
        char *endptr;
        long move = strtol(buf, &endptr, 10);
        
        if (endptr == buf || (*endptr != '\0')) {
            send(current_fd, "INVALID_MOVE: Must be a number 1-9\n", 36, 0);
            send_turn(current_fd);
            continue;
        }
        
        int pos = (int)move - 1;
        
        if (pos < 0 || pos > 8) {
            send(current_fd, "INVALID_MOVE: Number must be between 1-9\n", 42, 0);
            send_turn(current_fd);
            continue;
        }
        
        if (board[pos] != ' ') {
            send(current_fd, "INVALID_MOVE: Position already taken\n", 38, 0);
            send_turn(current_fd);
            continue;
        }
        
        // Valid move - update board
        board[pos] = (turn == 1) ? 'X' : 'O';
        
        // Announce the move to both players
        char move_msg[128];
        snprintf(move_msg, sizeof(move_msg), 
                 "\nMOVE_MADE: %s played position %d\n",
                 (turn == 1) ? p1_user : p2_user, (int)move);
        send_to_both(move_msg);
        
        // Small delay to ensure move message is received
        usleep(50000);  // 50ms
        
        // Send updated board to BOTH players immediately
        send_board();
        
        // Small delay to ensure board is received
        usleep(50000);  // 50ms
        
        // Check for win
        if (check_win(board[pos])) {
            end_game("WIN", turn);
            // end_game exits, so this won't be reached
        }
        
        // Check for draw
        if (board_full()) {
            end_game("DRAW", 0);
            // end_game exits, so this won't be reached
        }
        
        // Switch turns
        turn = (turn == 1) ? 2 : 1;
        
        // Send turn notification to next player
        send_turn((turn == 1) ? p1_fd : p2_fd);
    }
    
    return 0;
}