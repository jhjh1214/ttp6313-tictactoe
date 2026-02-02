#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "../include/database.h"
#include "../include/ipc.h"

#define PORT 5555
#define MAX_CLIENTS 20
#define BUF_SIZE 1024

struct client {
    int fd;
    char username[64];
    int in_game;  // 0 = in lobby, 1 = in game
    pid_t game_pid;  // PID of game process if in_game == 1
};

struct client clients[MAX_CLIENTS];
int client_count = 0;
int listen_fd;
int msg_queue_id;

void return_players_to_lobby(pid_t game_pid) {
    // Find all players in this game and return them to lobby
    for (int i = 0; i < client_count; i++) {
        if (clients[i].in_game == 1 && clients[i].game_pid == game_pid) {
            printf("[SERVER] Returning '%s' to lobby after game (PID %d)\n", 
                   clients[i].username, game_pid);
            clients[i].in_game = 0;
            clients[i].game_pid = 0;
            send(clients[i].fd, "RETURN_TO_LOBBY\n", 16, 0);
            send_lobby(clients[i].fd);
        }
    }
    broadcast_lobby();
}

void sigchld_handler(int sig) {
    (void)sig;
    // Reap all terminated child processes
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[SERVER] Game process %d terminated\n", pid);
        // Return players from this game to lobby
        return_players_to_lobby(pid);
    }
}

void cleanup_resources() {
    printf("[SERVER] Cleaning up resources...\n");
    
    // Close all client connections
    for (int i = 0; i < client_count; i++) {
        close(clients[i].fd);
    }
    
    // Close listening socket
    if (listen_fd != -1) {
        close(listen_fd);
    }
    
    // Remove message queue
    if (msg_queue_id != -1) {
        msgctl(msg_queue_id, IPC_RMID, NULL);
    }
    
    // Remove FIFO
    unlink(FIFO_PATH);
    
    printf("[SERVER] Cleanup complete. Exiting.\n");
}

void sigint_handler(int sig) {
    (void)sig;
    printf("\n[SERVER] Received SIGINT. Shutting down...\n");
    cleanup_resources();
    exit(0);
}

void send_lobby(int fd) {
    char buf[BUF_SIZE] = "LOBBY:";
    int online_count = 0;
    
    for (int i = 0; i < client_count; i++) {
        if (clients[i].in_game == 0) {  // Only show available players
            strncat(buf, clients[i].username, sizeof(buf) - strlen(buf) - 1);
            strncat(buf, ",", sizeof(buf) - strlen(buf) - 1);
            online_count++;
        }
    }
    
    if (online_count > 0) {
        buf[strlen(buf) - 1] = '\n';  // Replace last comma with newline
    } else {
        strncat(buf, "No players available\n", sizeof(buf) - strlen(buf) - 1);
    }
    
    if (send(fd, buf, strlen(buf), 0) == -1) {
        perror("send lobby failed");
    }
}

void broadcast_lobby() {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].in_game == 0) {  // Only send to players in lobby
            send_lobby(clients[i].fd);
        }
    }
}

int find_client(const char *user) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].username, user) == 0) {
            return i;
        }
    }
    return -1;
}

void remove_client(int idx) {
    if (idx < 0 || idx >= client_count) return;
    
    printf("[SERVER] Removing client '%s' (fd %d)\n", clients[idx].username, clients[idx].fd);
    close(clients[idx].fd);
    
    // Shift remaining clients
    memmove(&clients[idx], &clients[idx + 1], 
            sizeof(struct client) * (client_count - idx - 1));
    client_count--;
    
    broadcast_lobby();
}

void start_game(int p1_idx, int p2_idx) {
    if (p1_idx < 0 || p2_idx < 0 || p1_idx >= client_count || p2_idx >= client_count) {
        fprintf(stderr, "[SERVER ERROR] Invalid player indices\n");
        return;
    }
    
    int p1_fd = clients[p1_idx].fd;
    int p2_fd = clients[p2_idx].fd;
    
    printf("[SERVER] Starting game between %s (fd %d) and %s (fd %d)\n",
           clients[p1_idx].username, p1_fd,
           clients[p2_idx].username, p2_fd);
    
    int pid = fork();
    if (pid == 0) {
        // Child process: game_process
        
        // Close listening socket
        close(listen_fd);
        
        // Close all other client sockets except the two players
        for (int i = 0; i < client_count; i++) {
            if (clients[i].fd != p1_fd && clients[i].fd != p2_fd) {
                close(clients[i].fd);
            }
        }
        
        // Prepare arguments
        char fd1_str[16], fd2_str[16];
        char u1[64], u2[64];
        char pid_str[16];
        snprintf(fd1_str, sizeof(fd1_str), "%d", p1_fd);
        snprintf(fd2_str, sizeof(fd2_str), "%d", p2_fd);
        snprintf(pid_str, sizeof(pid_str), "%d", getpid());
        strncpy(u1, clients[p1_idx].username, sizeof(u1) - 1);
        u1[sizeof(u1) - 1] = '\0';
        strncpy(u2, clients[p2_idx].username, sizeof(u2) - 1);
        u2[sizeof(u2) - 1] = '\0';
        
        // Execute game process
        execl("./game_process", "game_process", fd1_str, fd2_str, u1, u2, NULL);
        perror("execl failed");
        exit(1);
    } else if (pid > 0) {
        // Parent process: mark players as in-game and store game PID
        clients[p1_idx].in_game = 1;
        clients[p1_idx].game_pid = pid;
        clients[p2_idx].in_game = 1;
        clients[p2_idx].game_pid = pid;
        
        printf("[SERVER] Game process spawned (PID %d)\n", pid);
        
        // Send notification via message queue
        struct game_msg notification;
        notification.mtype = 1;
        notification.player = 0;
        notification.move = 0;
        // Truncate message if too long to fit
        char game_msg_str[128];
        snprintf(game_msg_str, sizeof(game_msg_str), "Game: %s vs %s", 
                 clients[p1_idx].username, clients[p2_idx].username);
        strncpy(notification.username, game_msg_str, sizeof(notification.username) - 1);
        notification.username[sizeof(notification.username) - 1] = '\0';
        msgsnd(msg_queue_id, &notification, sizeof(notification) - sizeof(long), IPC_NOWAIT);
        
        // Update lobby for remaining players
        broadcast_lobby();
    } else {
        perror("fork failed");
    }
}

void handle_client_disconnect(int idx) {
    printf("[SERVER] Client '%s' disconnected\n", clients[idx].username);
    
    if (clients[idx].in_game) {
        // Client was in a game - the game_process will handle this
        printf("[SERVER] Client was in game - game_process will handle cleanup\n");
    }
    
    remove_client(idx);
}

int main() {
    struct sockaddr_in addr;
    fd_set rfds;
    int max_fd;
    
    // Set up signal handlers
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
    // Create IPC resources
    if (create_fifo() == -1) {
        fprintf(stderr, "[SERVER] Warning: FIFO creation failed\n");
    }
    msg_queue_id = create_msg_queue();
    
    // Create data directory
    mkdir("data", 0755);
    
    // Create listening socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket failed");
        exit(1);
    }
    
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
    }
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        exit(1);
    }
    
    if (listen(listen_fd, 10) == -1) {
        perror("listen failed");
        exit(1);
    }
    
    printf("[SERVER] Running on port %d\n", PORT);
    printf("[SERVER] Press Ctrl+C to shutdown gracefully\n");
    
    while (1) {
        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);
        max_fd = listen_fd;
        
        // Only monitor clients in lobby (not in-game)
        for (int i = 0; i < client_count; i++) {
            if (clients[i].in_game == 0) {  // Only lobby clients
                FD_SET(clients[i].fd, &rfds);
                if (clients[i].fd > max_fd) max_fd = clients[i].fd;
            }
        }
        
        if (select(max_fd + 1, &rfds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) continue;
            perror("select failed");
            break;
        }
        
        // Handle new connections
        if (FD_ISSET(listen_fd, &rfds)) {
            int new_fd = accept(listen_fd, NULL, NULL);
            if (new_fd == -1) {
                perror("accept failed");
                continue;
            }
            printf("[SERVER] New connection accepted, fd = %d\n", new_fd);
            
            char buf[BUF_SIZE];
            int bytes = recv(new_fd, buf, BUF_SIZE - 1, 0);
            if (bytes <= 0) {
                printf("[SERVER] Connection closed early (bytes = %d), closing fd %d\n", bytes, new_fd);
                close(new_fd);
                continue;
            }
            buf[bytes] = '\0';
            
            // Remove trailing newline/whitespace
            buf[strcspn(buf, "\r\n")] = '\0';
            
            printf("[SERVER] Received: '%s'\n", buf);
            
            char *command = strtok(buf, " ");
            char *user = strtok(NULL, " ");
            char *pass = strtok(NULL, " ");
            
            if (!command || !user || !pass) {
                printf("[SERVER] Invalid format → INVALID_FORMAT\n");
                send(new_fd, "INVALID_FORMAT\n", 15, 0);
                close(new_fd);
                continue;
            }
            
            // Validate username and password length
            if (strlen(user) >= 64 || strlen(pass) >= 64) {
                send(new_fd, "USERNAME_OR_PASSWORD_TOO_LONG\n", 30, 0);
                close(new_fd);
                continue;
            }
            
            if (strcmp(command, "REGISTER") == 0) {
                printf("[SERVER] REGISTER request for '%s'\n", user);
                if (user_exists(user)) {
                    printf("[SERVER] User '%s' exists → USER_EXISTS\n", user);
                    send(new_fd, "USER_EXISTS\n", 12, 0);
                    close(new_fd);
                } else {
                    register_user(user, pass);
                    printf("[SERVER] User '%s' registered\n", user);
                    send(new_fd, "REGISTER_OK\n", 12, 0);
                    
                    if (client_count < MAX_CLIENTS) {
                        clients[client_count].fd = new_fd;
                        strncpy(clients[client_count].username, user, 
                                sizeof(clients[client_count].username) - 1);
                        clients[client_count].username[sizeof(clients[client_count].username) - 1] = '\0';
                        clients[client_count].in_game = 0;
                        client_count++;
                        printf("[SERVER] Added '%s' to lobby (fd %d, total %d)\n", 
                               user, new_fd, client_count);
                        broadcast_lobby();
                    } else {
                        printf("[SERVER] Server full → SERVER_FULL\n");
                        send(new_fd, "SERVER_FULL\n", 12, 0);
                        close(new_fd);
                    }
                }
            }
            else if (strcmp(command, "LOGIN") == 0) {
                printf("[SERVER] LOGIN request for '%s'\n", user);
                if (validate_login(user, pass)) {
                    if (find_client(user) != -1) {
                        printf("[SERVER] User '%s' already logged in\n", user);
                        send(new_fd, "ALREADY_LOGGED_IN\n", 18, 0);
                        close(new_fd);
                        continue;
                    }
                    
                    send(new_fd, "LOGIN_OK\n", 9, 0);
                    printf("[SERVER] Login successful for '%s'\n", user);
                    
                    if (client_count < MAX_CLIENTS) {
                        clients[client_count].fd = new_fd;
                        strncpy(clients[client_count].username, user,
                                sizeof(clients[client_count].username) - 1);
                        clients[client_count].username[sizeof(clients[client_count].username) - 1] = '\0';
                        clients[client_count].in_game = 0;
                        client_count++;
                        printf("[SERVER] Added '%s' to lobby (fd %d, total %d)\n",
                               user, new_fd, client_count);
                        broadcast_lobby();
                    } else {
                        printf("[SERVER] Server full → SERVER_FULL\n");
                        send(new_fd, "SERVER_FULL\n", 12, 0);
                        close(new_fd);
                    }
                } else {
                    printf("[SERVER] Invalid credentials for '%s'\n", user);
                    send(new_fd, "INVALID_LOGIN\n", 14, 0);
                    close(new_fd);
                }
            }
            else {
                printf("[SERVER] Unknown command '%s'\n", command);
                send(new_fd, "INVALID_COMMAND\n", 16, 0);
                close(new_fd);
            }
        }
        
        // Handle messages from lobby clients only
        for (int i = 0; i < client_count; ) {
            // Skip in-game clients - they're handled by game_process
            if (clients[i].in_game == 1) {
                i++;
                continue;
            }
            
            if (FD_ISSET(clients[i].fd, &rfds)) {
                char buf[BUF_SIZE];
                int bytes = recv(clients[i].fd, buf, BUF_SIZE - 1, 0);
                
                if (bytes <= 0) {
                    handle_client_disconnect(i);
                    continue;  // Don't increment i
                }
                
                buf[bytes] = '\0';
                buf[strcspn(buf, "\r\n")] = '\0';  // Remove newline
                
                // Handle lobby commands
                printf("[SERVER] From '%s': '%s'\n", clients[i].username, buf);
                
                char *command = strtok(buf, " ");
                
                if (strcmp(command, "INVITE") == 0) {
                    char *target = strtok(NULL, "\n");
                    if (!target) {
                        send(clients[i].fd, "INVALID_INVITE_FORMAT\n", 22, 0);
                    } else {
                        int target_idx = find_client(target);
                        if (target_idx != -1 && clients[target_idx].in_game == 0) {
                            char notify[BUF_SIZE];
                            snprintf(notify, sizeof(notify), "INVITE_FROM %s\n", 
                                    clients[i].username);
                            send(clients[target_idx].fd, notify, strlen(notify), 0);
                            
                            snprintf(notify, sizeof(notify), "INVITE_SENT to %s\n", target);
                            send(clients[i].fd, notify, strlen(notify), 0);
                        } else {
                            send(clients[i].fd, "PLAYER_NOT_AVAILABLE\n", 21, 0);
                        }
                    }
                }
                else if (strcmp(command, "ACCEPT") == 0) {
                    char *target = strtok(NULL, "\n");
                    if (!target) {
                        send(clients[i].fd, "INVALID_ACCEPT_FORMAT\n", 22, 0);
                    } else {
                        int target_idx = find_client(target);
                        if (target_idx != -1 && clients[target_idx].in_game == 0) {
                            start_game(target_idx, i);  // Inviter is player 1
                            continue;  // Don't increment i, clients array changed
                        } else {
                            send(clients[i].fd, "PLAYER_NOT_AVAILABLE\n", 21, 0);
                        }
                    }
                }
                else if (strcmp(command, "DECLINE") == 0) {
                    char *target = strtok(NULL, "\n");
                    if (!target) {
                        send(clients[i].fd, "INVALID_DECLINE_FORMAT\n", 23, 0);
                    } else {
                        int target_idx = find_client(target);
                        if (target_idx != -1) {
                            char notify[BUF_SIZE];
                            snprintf(notify, sizeof(notify), "INVITE_DECLINED_BY %s\n",
                                    clients[i].username);
                            send(clients[target_idx].fd, notify, strlen(notify), 0);
                        }
                    }
                }
                else if (strcmp(command, "LEADERBOARD") == 0) {
                    char lb[BUF_SIZE];
                    get_leaderboard(lb, sizeof(lb));
                    send(clients[i].fd, lb, strlen(lb), 0);
                }
                else if (strcmp(command, "QUIT") == 0) {
                    send(clients[i].fd, "GOODBYE\n", 8, 0);
                    handle_client_disconnect(i);
                    continue;  // Don't increment i
                }
                else {
                    send(clients[i].fd, "UNKNOWN_COMMAND\n", 16, 0);
                }
            }
            i++;
        }
    }
    
    cleanup_resources();
    return 0;
}