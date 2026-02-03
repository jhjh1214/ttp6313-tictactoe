# Multi-Player Online Tic-Tac-Toe Game System

A TCP/IP client-server application demonstrating advanced operating system concepts including socket programming, inter-process communication (IPC), process management, and concurrent programming.

## 📋 Table of Contents
- [Overview](#overview)
- [Features](#features)
- [System Architecture](#system-architecture)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Technical Implementation](#technical-implementation)
- [Testing](#testing)
- [Known Limitations](#known-limitations)
- [Contributors](#contributors)
- [License](#license)

## 🎯 Overview

This project implements a networked multiplayer Tic-Tac-Toe game where multiple users can play simultaneously across different machines. The system demonstrates:

- **Socket Programming**: TCP/IP communication between clients and server
- **Process Management**: Fork-based architecture with process isolation
- **Inter-Process Communication**: Message queues, FIFO, and semaphores
- **Concurrency Control**: File locking and synchronization mechanisms
- **I/O Multiplexing**: Efficient handling of multiple connections using select()

### What Problems Does It Solve?

1. **Remote Gameplay**: Enables players to compete across different machines/networks
2. **Concurrent Game Management**: Handles multiple simultaneous games without interference
3. **Real-Time Synchronization**: Ensures both players see board updates instantly
4. **Data Persistence**: Maintains user credentials and statistics across sessions
5. **Resource Management**: Prevents zombie processes and resource leaks

## ✨ Features

### 1. User Authentication
- User registration with unique username validation
- Login system with credential verification
- Duplicate login prevention
- Session management

### 2. Game Lobby
- Real-time online player list
- Player status tracking (available/in-game)
- Game invitation system
- Accept/decline invitation functionality
- Automatic lobby updates

### 3. Real-Time Gameplay
- Turn-based Tic-Tac-Toe mechanics
- Instant board synchronization
- Move validation (position, occupancy, range)
- 30-second turn timeout enforcement
- Win detection (8 patterns: 3 rows, 3 columns, 2 diagonals)
- Draw detection

### 4. Multi-Game Support
- Server handles multiple concurrent games
- Independent game processes (fork-based)
- Process isolation prevents interference
- Lobby remains accessible during games

### 5. Statistics and Leaderboard
- Win/Loss/Draw tracking per user
- Automatic statistics updates
- Top 10 leaderboard with rankings
- Win rate calculation
- Persistent storage

### 6. Notification System
- Message queue for structured events
- FIFO for real-time notifications
- Notifications for: invitations, moves, results, timeouts, disconnections

### 7. Connection Handling
- Graceful disconnect detection
- Automatic session cleanup
- Win-by-default for remaining player
- Server graceful shutdown (SIGINT/SIGTERM)

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────┐
│           CLIENT LAYER (Different VMs)              │
│  [Client 1]  [Client 2]  [Client 3]  [Client N]    │
└─────────────────┬───────────────────────────────────┘
                  │ TCP Port 5555
┌─────────────────▼───────────────────────────────────┐
│              SERVER PROCESS (Main)                   │
│  • socket/bind/listen/accept                        │
│  • select() I/O multiplexing                        │
│  • Authentication & Lobby management                │
│  • Signal handlers (SIGCHLD, SIGINT, SIGTERM)       │
└─────────────────┬───────────────────────────────────┘
                  │ fork() + execl()
┌─────────────────▼───────────────────────────────────┐
│           GAME PROCESSES (Independent)               │
│  Game Process 1 (PID: X)  │  Game Process 2 (PID: Y)│
│  alice vs bob             │  charlie vs david       │
└─────────────────┬───────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────┐
│                  IPC LAYER                           │
│  Message Queue  │  FIFO Pipe  │  Semaphores         │
│  (Key: 0x2222)  │  (/tmp/)    │  (Per-game)         │
└─────────────────┬───────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────┐
│            DATABASE LAYER (File-based)               │
│  users.db: username:password                        │
│  stats.db: username WIN/LOSS/DRAW                   │
│  (flock: LOCK_SH for read, LOCK_EX for write)       │
└─────────────────────────────────────────────────────┘
```

## 📦 Requirements

### System Requirements
- **OS**: Linux/Unix (Ubuntu 20.04+ recommended)
- **Compiler**: GCC 7.0+ with C99 support
- **Network**: TCP/IP connectivity between machines
- **Memory**: Minimum 512MB RAM
- **Disk**: ~10MB for installation

### Dependencies
- Standard C library (libc)
- POSIX threads library (pthread)
- No external libraries required

## 🚀 Installation

### 1. Clone Repository
```bash
git clone https://github.com/yourusername/tictactoe-multiplayer.git
cd tictactoe-multiplayer
```

### 2. Build Project
```bash
make clean
make all
```

This compiles three executables:
- `server` - Main server application
- `game_process` - Game logic handler
- `client` - Client application

### 3. Create Data Directory
```bash
mkdir -p data
```

## 💻 Usage

### Starting the Server

On the server machine:

```bash
./server
```

**Output:**
```
[IPC] FIFO created at /tmp/game_notify
[IPC] Message queue created (ID: 32768)
[SERVER] Running on port 5555
[SERVER] Press Ctrl+C to shutdown gracefully
```

**Get Server IP:**
```bash
hostname -I
# or
ip addr show
```

### Starting Clients

On client machines:

```bash
# Connect to localhost
./client

# Connect to remote server
./client <server_ip>

# Example
./client 192.168.1.100
```

### Client Commands

**Registration:**
```
Register or Login (R/L): R
Username: alice
Password: alice123
```

**Login:**
```
Register or Login (R/L): L
Username: alice
Password: alice123
```

**In Lobby:**
```
invite <username>     - Send game invitation
accept <username>     - Accept invitation
decline <username>    - Decline invitation
leaderboard           - View top players
help                  - Show commands
quit                  - Exit
```

**In Game:**
```
1-9                   - Make a move (position on board)
```

### Board Positions
```
 1 | 2 | 3
---+---+---
 4 | 5 | 6
---+---+---
 7 | 8 | 9
```

## 🔧 Technical Implementation

### Process Management
- **fork()**: Creates independent child processes for each game
- **execl()**: Replaces child with game_process executable
- **waitpid()**: Reaps terminated processes (prevents zombies)
- **Signal handling**: SIGCHLD, SIGINT, SIGTERM

### IPC Mechanisms

#### 1. Message Queue
```c
Key: 0x2222
Purpose: Structured event broadcasting
Operations: msgget(), msgsnd(), msgrcv()
```

#### 2. FIFO (Named Pipe)
```c
Path: /tmp/game_notify
Purpose: Asynchronous notifications
Operations: mkfifo(), open(), write(), close()
```

#### 3. Semaphore
```c
Unique key per game (based on PID)
Purpose: Board access synchronization
Operations: semget(), semctl(), semop()
```

### Socket Programming
```c
Protocol: TCP/IP (SOCK_STREAM)
Port: 5555
Server: INADDR_ANY (accepts from any interface)
Options: SO_REUSEADDR, TCP_NODELAY
I/O Multiplexing: select()
```

### Database
```c
Format: Plain text files
users.db: "username:password\n"
stats.db: "username RESULT\n"
Locking: flock(LOCK_SH) for reads, flock(LOCK_EX) for writes
```

## 🧪 Testing

See [TESTING.md](TESTING.md) for comprehensive test scenarios including:
- User authentication tests
- Lobby functionality tests
- Gameplay tests
- Concurrent game tests
- Error handling tests
- Stress tests

### Quick Test
```bash
# Terminal 1: Start server
./server

# Terminal 2: Client 1
./client
R
alice
alice123
invite bob

# Terminal 3: Client 2
./client
R
bob
bob456
accept alice

# Play game...
```

## ⚠️ Known Limitations

### Scalability
- Maximum 20 concurrent clients (configurable via MAX_CLIENTS)
- File-based database not optimized for thousands of users
- Process-per-game model limits total concurrent games

### Security
- Passwords stored in plaintext (educational project)
- No network encryption
- No authentication tokens

### Network
- LAN only (no NAT traversal)
- Single server instance
- Hardcoded port 5555

### Platform
- Linux/Unix only (POSIX APIs)
- Not portable to Windows without modifications

**Note**: These limitations are intentional for educational purposes, focusing on OS concepts rather than production deployment.

## 👥 Contributors

**Team Members:**
- **LIM JUN HONG** (1211110418) - Server architecture, socket communication
- **TAI KE YING DOROTHY** (1211111348) - Game process management, IPC
- **TAN JUN YAN** (1211109993) - Client application, UI
- **WONG XIAO YUN** (1211211150) - Database implementation, testing

**Course**: TTP6313 Operating Systems  
**Institution**: Multimedia University  
**Trimester**: 2, 2024/2025

## 📄 License

This project is developed for academic purposes as part of TTP6313 Operating Systems coursework.

## 🔗 Additional Resources

- [Testing Guide](TESTING.md)
- [Project Documentation](docs/)

---

**Project Status**: ✅ Complete and Ready for Submission

**Last Updated**: February 2026
