#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/select.h>
#include <fcntl.h>
#include "../include/ipc.h"

#define PORT 5555
#define BUF_SIZE 1024

typedef enum {
    STATE_LOBBY,
    STATE_IN_GAME
} ClientState;

ClientState current_state = STATE_LOBBY;

void print_help() {
    printf("\n=== Available Commands ===\n");
    printf("In Lobby:\n");
    printf("  invite <username>   - Send game invitation to a player\n");
    printf("  accept <username>   - Accept game invitation from a player\n");
    printf("  decline <username>  - Decline game invitation from a player\n");
    printf("  leaderboard         - View top players\n");
    printf("  quit                - Exit the game\n");
    printf("\nIn Game:\n");
    printf("  1-9                 - Make a move (when it's your turn)\n");
    printf("=========================\n\n");
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server;
    char buf[BUF_SIZE], username[64], password[64];
    char *server_ip = "127.0.0.1";  // Default to localhost
    
    // Allow user to specify server IP
    if (argc >= 2) {
        server_ip = argv[1];
        printf("Connecting to server at %s...\n", server_ip);
    } else {
        printf("Usage: %s [server_ip]\n", argv[0]);
        printf("Connecting to localhost (127.0.0.1) by default...\n");
    }
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket failed");
        exit(1);
    }
    
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    if (inet_pton(AF_INET, server_ip, &server.sin_addr) <= 0) {
        perror("Invalid server IP address");
        exit(1);
    }
    
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("connect failed");
        exit(1);
    }
    
    printf("=== Tic-Tac-Toe Online Game ===\n");
    printf("Register or Login (R/L): ");
    char choice;
    scanf(" %c", &choice);
    
    printf("Username (max 63 chars): ");
    scanf("%63s", username);
    
    printf("Password (max 63 chars): ");
    scanf("%63s", password);
    
    // Clear input buffer
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    
    if (choice == 'R' || choice == 'r') {
        snprintf(buf, sizeof(buf), "REGISTER %s %s\n", username, password);
    } else {
        snprintf(buf, sizeof(buf), "LOGIN %s %s\n", username, password);
    }
    
    if (send(sock, buf, strlen(buf), 0) == -1) {
        perror("send login failed");
        close(sock);
        exit(1);
    }
    
    int bytes = recv(sock, buf, BUF_SIZE - 1, 0);
    if (bytes <= 0) {
        perror("recv login response failed");
        close(sock);
        exit(1);
    }
    buf[bytes] = '\0';
    printf("%s", buf);
    
    if (strstr(buf, "OK") == NULL) {
        printf("Login/Registration failed. Exiting.\n");
        close(sock);
        exit(1);
    }
    
    printf("\nSuccessfully connected!\n");
    print_help();
    
    // Main game loop
    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(sock, &rfds);
        int max_fd = sock > STDIN_FILENO ? sock : STDIN_FILENO;
        
        if (select(max_fd + 1, &rfds, NULL, NULL, NULL) == -1) {
            perror("select failed");
            break;
        }
        
        // Handle user input
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char input[BUF_SIZE];
            if (fgets(input, sizeof(input), stdin) == NULL) {
                break;
            }
            input[strcspn(input, "\n")] = '\0';  // Remove newline
            
            // Skip empty input
            if (strlen(input) == 0) {
                continue;
            }
            
            // Process command based on current state
            if (current_state == STATE_LOBBY) {
                if (strncmp(input, "invite ", 7) == 0) {
                    char target[64];
                    strncpy(target, input + 7, sizeof(target) - 1);
                    target[sizeof(target) - 1] = '\0';
                    snprintf(buf, sizeof(buf), "INVITE %s\n", target);
                } 
                else if (strncmp(input, "accept ", 7) == 0) {
                    char target[64];
                    strncpy(target, input + 7, sizeof(target) - 1);
                    target[sizeof(target) - 1] = '\0';
                    snprintf(buf, sizeof(buf), "ACCEPT %s\n", target);
                } 
                else if (strncmp(input, "decline ", 8) == 0) {
                    char target[64];
                    strncpy(target, input + 8, sizeof(target) - 1);
                    target[sizeof(target) - 1] = '\0';
                    snprintf(buf, sizeof(buf), "DECLINE %s\n", target);
                } 
                else if (strcmp(input, "leaderboard") == 0) {
                    strcpy(buf, "LEADERBOARD\n");
                } 
                else if (strcmp(input, "quit") == 0) {
                    strcpy(buf, "QUIT\n");
                    send(sock, buf, strlen(buf), 0);
                    printf("Goodbye!\n");
                    break;
                }
                else if (strcmp(input, "help") == 0) {
                    print_help();
                    continue;
                }
                else {
                    printf("Unknown command. Type 'help' for available commands.\n");
                    continue;
                }
            } 
            else {  // STATE_IN_GAME
                // Trim whitespace from input using a pointer
                char *trimmed = input;
                while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
                
                // Validate input is a single digit 1-9
                if (strlen(trimmed) >= 1 && trimmed[0] >= '1' && trimmed[0] <= '9') {
                    // Send just the digit
                    snprintf(buf, sizeof(buf), "%c\n", trimmed[0]);
                } else {
                    printf("Invalid input. Enter a number from 1-9 when it's your turn.\n");
                    continue;
                }
            }
            
            // Send the prepared command
            if (send(sock, buf, strlen(buf), 0) == -1) {
                perror("send failed");
                break;
            }
        }
        
        // Handle server messages
        if (FD_ISSET(sock, &rfds)) {
            int bytes = recv(sock, buf, BUF_SIZE - 1, 0);
            if (bytes <= 0) {
                printf("\n[CONNECTION LOST] Server disconnected\n");
                break;
            }
            buf[bytes] = '\0';
            
            // Parse server messages and update state
            if (strstr(buf, "GAME_START:") != NULL) {
                current_state = STATE_IN_GAME;
                printf("\n==========================================\n");
                printf("         GAME IS STARTING!                \n");
                printf("==========================================\n");
            }
            else if (strstr(buf, "RETURN_TO_LOBBY") != NULL) {
                current_state = STATE_LOBBY;
                printf("\n==========================================\n");
                printf("      Returning to lobby...               \n");
                printf("==========================================\n");
                print_help();
                continue;  // Don't print the message itself
            }
            else if (strstr(buf, "GAME_OVER:") != NULL) {
                printf("\n==========================================\n");
                printf("%s", buf);
                printf("==========================================\n");
            }
            else if (strstr(buf, "YOUR_TURN") != NULL) {
                printf("\n>>> YOUR TURN! Enter move (1-9): ");
                fflush(stdout);
                continue;  // Don't print the message itself
            }
            else if (strstr(buf, "WAITING:") != NULL) {
                printf("%s", buf);
                continue;
            }
            else if (strstr(buf, "INVALID_MOVE:") != NULL) {
                printf("\n[ERROR] %s", buf);
                printf(">>> Try again. Enter move (1-9): ");
                fflush(stdout);
            }
            else if (strstr(buf, "MOVE_MADE:") != NULL) {
                printf("%s", buf);
            }
            else if (strstr(buf, "BOARD:") != NULL) {
                printf("\n%s", buf);
            }
            else if (strstr(buf, "LOBBY:") != NULL) {
                if (current_state == STATE_LOBBY) {
                    printf("\n--- Online Players ---\n");
                    char *players = buf + 6;  // Skip "LOBBY:"
                    if (strstr(players, "No players") != NULL) {
                        printf("No other players online\n");
                    } else {
                        printf("%s", players);
                    }
                    printf("----------------------\n");
                }
            }
            else if (strstr(buf, "INVITE_FROM") != NULL) {
                printf("\n[NOTIFICATION] %s", buf);
                printf("Type 'accept <username>' or 'decline <username>'\n");
            }
            else if (strstr(buf, "INVITE_SENT") != NULL) {
                printf("[INFO] %s", buf);
            }
            else if (strstr(buf, "INVITE_DECLINED_BY") != NULL) {
                printf("\n[NOTIFICATION] %s", buf);
            }
            else if (strstr(buf, "PLAYER_NOT_AVAILABLE") != NULL) {
                printf("[ERROR] Player is not available or doesn't exist\n");
            }
            else if (strstr(buf, "OPPONENT_TIMEOUT") != NULL) {
                printf("\n[GAME RESULT] %s", buf);
            }
            else if (strstr(buf, "TIMEOUT:") != NULL) {
                printf("\n[GAME RESULT] %s", buf);
            }
            else if (strstr(buf, "OPPONENT_DISCONNECTED") != NULL) {
                printf("\n[GAME RESULT] %s", buf);
            }
            else if (strstr(buf, "Leaderboard") != NULL) {
                printf("\n%s", buf);
            }
            else if (strstr(buf, "GOODBYE") != NULL) {
                printf("Disconnected. Goodbye!\n");
                break;
            }
            else {
                // Print any other messages
                printf("%s", buf);
            }
        }
    }
    
    close(sock);
    return 0;
}