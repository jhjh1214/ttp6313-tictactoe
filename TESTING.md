# Testing Guide
## Multi-Player Online Tic-Tac-Toe Game System

This document provides comprehensive test scenarios to verify all features and operating system concepts implemented in the project.

---

## Table of Contents
1. [Test Environment Setup](#test-environment-setup)
2. [Unit Tests](#unit-tests)
3. [Integration Tests](#integration-tests)
4. [System Tests](#system-tests)
5. [Stress Tests](#stress-tests)
6. [Expected Results](#expected-results)

---

## Test Environment Setup

### Prerequisites
- 3+ terminals or VMs
- Network connectivity
- Compiled executables (server, game_process, client)

### Setup Commands
```bash
# Terminal 1: Server
cd ~/tictactoe
./server

# Terminal 2: Client 1
./client <server_ip>

# Terminal 3: Client 2
./client <server_ip>

# Terminal 4: Client 3 (optional)
./client <server_ip>
```

---

## Unit Tests

### Test 1: User Registration
**Purpose**: Verify user registration with unique username validation

**Steps**:
1. Start server
2. Start client 1
3. Choose Register (R)
4. Enter username: `alice`
5. Enter password: `alice123`

**Expected Result**:
```
REGISTER_OK
Successfully connected!
```

**Verify**:
```bash
cat data/users.db
# Should show: alice:alice123
```

---

### Test 2: Duplicate Username Prevention
**Purpose**: Verify system prevents duplicate usernames

**Steps**:
1. With alice already registered
2. Start client 2
3. Choose Register (R)
4. Enter username: `alice`
5. Enter password: `different`

**Expected Result**:
```
USER_EXISTS
Username already taken. Try a different username.
```

---

### Test 3: User Login
**Purpose**: Verify login credential validation

**Steps**:
1. Start client with existing user
2. Choose Login (L)
3. Enter username: `alice`
4. Enter password: `alice123`

**Expected Result**:
```
LOGIN_OK
Successfully connected!
--- Online Players ---
```

---

### Test 4: Invalid Login
**Purpose**: Verify invalid credential rejection

**Steps**:
1. Start client
2. Choose Login (L)
3. Enter username: `alice`
4. Enter password: `wrongpassword`

**Expected Result**:
```
INVALID_CREDENTIALS
Login failed. Check username and password.
```

---

### Test 5: Duplicate Login Prevention
**Purpose**: Verify same user cannot login twice

**Steps**:
1. Client 1 logged in as alice
2. Client 2 attempts login as alice with correct password

**Expected Result**:
```
ALREADY_LOGGED_IN
This user is already connected.
```

---

## Integration Tests

### Test 6: Lobby Display and Updates
**Purpose**: Verify real-time lobby synchronization

**Steps**:
1. Client 1 registers as alice
2. Client 2 registers as bob
3. Observe both clients

**Expected Result**:
- Client 1 sees: `--- Online Players ---\nbob,`
- Client 2 sees: `--- Online Players ---\nalice,`
- Updates appear instantly without manual refresh

---

### Test 7: Game Invitation
**Purpose**: Verify invitation system

**Steps**:
1. Alice in lobby, sees bob
2. Alice types: `invite bob`
3. Observe bob's screen

**Expected Result**:
- Alice sees: `[INFO] INVITE_SENT to bob`
- Bob sees: `[NOTIFICATION] INVITE_FROM alice`

---

### Test 8: Invitation Acceptance
**Purpose**: Verify game initialization on acceptance

**Steps**:
1. Bob receives invitation from alice
2. Bob types: `accept alice`

**Expected Result**:
- Both clients show:
```
==========================================
         GAME IS STARTING!                
==========================================

GAME_START: YOU_ARE_PLAYER_1 (X)  [Alice]
GAME_START: YOU_ARE_PLAYER_2 (O)  [Bob]

BOARD:
   |   |   
---+---+---
   |   |   
---+---+---
   |   |   

>>> YOUR_TURN! Enter move (1-9):  [Alice's screen only]
```

**Server Output**:
```
[SERVER] Starting game between alice and bob
[SERVER] Game process spawned (PID XXXXX)
[GAME] Semaphore created (ID: XXXXX, Key: XXXXX)
```

---

### Test 9: Game Invitation Decline
**Purpose**: Verify decline functionality

**Steps**:
1. Alice sends invitation to bob
2. Bob types: `decline alice`

**Expected Result**:
- Bob sees: `[INFO] Declined invitation from alice`
- Alice sees: `[INFO] bob declined your invitation`
- Both remain in lobby

---

### Test 10: Real-Time Board Synchronization
**Purpose**: Verify instant board updates

**Steps**:
1. Alice and bob in game
2. Alice (Player 1) enters: `5` (center)
3. Observe both screens immediately

**Expected Result**:
- Both clients update simultaneously:
```
MOVE_MADE: alice played position 5

BOARD:
   |   |   
---+---+---
   | X |   
---+---+---
   |   |   
```

4. Bob enters: `1` (top-left)
5. Both clients update:
```
MOVE_MADE: bob played position 1

BOARD:
 O |   |   
---+---+---
   | X |   
---+---+---
   |   |   
```

---

### Test 11: Move Validation - Occupied Position
**Purpose**: Verify occupied position rejection

**Steps**:
1. Alice has played position 5
2. Bob tries to play position 5

**Expected Result**:
```
[ERROR] INVALID_MOVE: Position already taken
>>> Try again. Enter move (1-9):
```

---

### Test 12: Move Validation - Out of Range
**Purpose**: Verify range validation

**Steps**:
1. During alice's turn, enter: `0`
2. Then try: `10`

**Expected Result**:
```
[ERROR] INVALID_MOVE: Number must be between 1-9
>>> Try again. Enter move (1-9):
```

---

### Test 13: Move Validation - Invalid Input
**Purpose**: Verify non-numeric input rejection

**Steps**:
1. During turn, enter: `abc`

**Expected Result**:
```
Invalid input. Enter a number from 1-9 when it's your turn.
```

---

### Test 14: Win Detection - Horizontal
**Purpose**: Verify horizontal win detection

**Steps**:
1. Play sequence:
   - Alice: 1, 2, 3 (top row)
   - Bob: 4, 5 (middle row partial)

**Expected Result**:
```
GAME_OVER: PLAYER_1_WINS (alice wins!)

==========================================
      Returning to lobby...               
==========================================
```

**Server Output**:
```
[GAME] Game ended between alice and bob
[DATABASE] Updated stats for 'alice': WIN
[DATABASE] Updated stats for 'bob': LOSS
[GAME] Semaphore (ID: XXXXX) removed
[SERVER] Game process XXXXX terminated
[SERVER] Returning 'alice' to lobby
[SERVER] Returning 'bob' to lobby
```

---

### Test 15: Win Detection - Vertical
**Purpose**: Verify vertical win detection

**Steps**:
1. Play sequence:
   - Alice: 1, 4, 7 (left column)
   - Bob: 2, 5 (middle column partial)

**Expected Result**: Same as Test 14

---

### Test 16: Win Detection - Diagonal
**Purpose**: Verify diagonal win detection

**Steps**:
1. Play sequence:
   - Alice: 1, 5, 9 (diagonal \)
   - Bob: 2, 4 (scattered)

**Expected Result**: Same as Test 14

---

### Test 17: Draw Detection
**Purpose**: Verify draw condition

**Steps**:
1. Play to fill board without winner:
   ```
   X | O | X
   ---+---+---
   O | X | O
   ---+---+---
   O | X | X
   ```

**Expected Result**:
```
GAME_OVER: DRAW

==========================================
      Returning to lobby...               
==========================================
```

**Database**:
```bash
cat data/stats.db
# Shows: alice DRAW
#        bob DRAW
```

---

### Test 18: Turn Timeout
**Purpose**: Verify 30-second timeout enforcement

**Steps**:
1. Start game between alice and bob
2. Alice's turn - wait 30+ seconds without input

**Expected Result**:
- Alice sees:
```
TIMEOUT: You failed to move in time. YOU_LOSE!
```
- Bob sees:
```
OPPONENT_TIMEOUT: alice failed to move in time. YOU_WIN!
```
- Both return to lobby
- Database shows bob WIN, alice LOSS

---

### Test 19: Player Disconnection During Game
**Purpose**: Verify disconnect handling

**Steps**:
1. Start game between alice and bob
2. During alice's turn, close alice's client (Ctrl+C)

**Expected Result**:
- Bob sees:
```
OPPONENT_DISCONNECTED: alice left the game. YOU_WIN!

==========================================
      Returning to lobby...               
==========================================
```
- Database shows bob WIN, alice LOSS

---

### Test 20: Leaderboard Display
**Purpose**: Verify leaderboard computation

**Setup**: Complete Test 14 and Test 17 first

**Steps**:
1. In lobby, type: `leaderboard`

**Expected Result**:
```
==========================================
           LEADERBOARD (Top 10)          
==========================================
 1. alice                | W:  1 L:  0 D:  1 | Rate: 50.0%
 2. bob                  | W:  0 L:  1 D:  1 | Rate: 0.0%
==========================================
```

**Verify**:
- Correct win counts
- Correct loss counts
- Correct draw counts
- Accurate win rate percentages
- Sorted by wins (descending)

---

## System Tests

### Test 21: Multiple Concurrent Games
**Purpose**: Verify server handles multiple simultaneous games

**Steps**:
1. Register 4 users: alice, bob, charlie, david
2. Alice invites charlie, charlie accepts
3. Bob invites david, david accepts
4. Both games play simultaneously

**Expected Result**:
- Server shows:
```
[SERVER] Game process spawned (PID 12345)
[SERVER] Game process spawned (PID 12346)
[GAME] Semaphore created (ID: 65536, Key: 12345)
[GAME] Semaphore created (ID: 65537, Key: 12346)
```
- Both games operate independently
- Moves in one game don't affect the other
- Both games complete successfully

---

### Test 22: Lobby Accessibility During Games
**Purpose**: Verify lobby remains functional during games

**Steps**:
1. Alice and bob start a game
2. Charlie joins server and registers
3. Alice invites charlie (should fail - alice in game)
4. Charlie can see lobby and invite others

**Expected Result**:
- Lobby updates continue for non-playing users
- In-game players don't appear in available list
- New users can join and use lobby normally

---

### Test 23: Return to Lobby After Game
**Purpose**: Verify automatic lobby return

**Steps**:
1. Complete a full game (any result)
2. Observe transition

**Expected Result**:
- Game end message appears
- Both clients automatically show:
```
==========================================
      Returning to lobby...               
==========================================

--- Online Players ---
[list of available players]
----------------------
```
- Players can immediately send new invitations
- No manual commands needed

---

### Test 24: Server Graceful Shutdown
**Purpose**: Verify proper resource cleanup

**Steps**:
1. Have 2+ clients connected
2. On server terminal, press Ctrl+C

**Expected Result**:
- Server shows:
```
^C
[SERVER] Received SIGINT. Shutting down...
[SERVER] Cleaning up resources...
[SERVER] Cleanup complete. Exiting.
```
- All clients show:
```
[CONNECTION LOST] Server disconnected
```
- Check IPC resources removed:
```bash
ipcs -q  # No message queue 32768
ipcs -s  # No semaphores remain
ls /tmp/game_notify  # File removed
```

---

### Test 25: Maximum Clients Limit
**Purpose**: Verify 20-client maximum

**Steps**:
1. Connect 20 clients (use script or multiple terminals)
2. Attempt to connect 21st client

**Expected Result**:
- First 20 clients connect successfully
- 21st client receives:
```
SERVER_FULL
Server has reached maximum capacity. Try again later.
```

---

## Stress Tests

### Test 26: Rapid Invitation Spam
**Purpose**: Verify system handles rapid commands

**Steps**:
1. Alice rapidly sends multiple invitations:
```
invite bob
invite charlie
invite david
```

**Expected Result**:
- All invitations processed
- No crashes or hangs
- Clear responses for each

---

### Test 27: Concurrent Disconnections
**Purpose**: Verify handling of multiple simultaneous disconnects

**Steps**:
1. Have 5+ clients connected
2. Simultaneously close 3 clients (Ctrl+C)

**Expected Result**:
- Server detects all disconnections
- Lobby updates correctly for remaining clients
- No zombie processes:
```bash
ps aux | grep game_process  # Should show none
ps aux | grep defunct       # Should show none
```

---

### Test 28: Database Concurrent Access
**Purpose**: Verify file locking prevents corruption

**Steps**:
1. Start 3 concurrent games
2. All 3 games end simultaneously (use timeout or pre-planned moves)

**Expected Result**:
- All statistics updated correctly
- No corrupted entries in stats.db
- Database remains readable:
```bash
cat data/stats.db
# All entries properly formatted
```

---

### Test 29: Long-Running Server
**Purpose**: Verify no memory leaks or resource exhaustion

**Steps**:
1. Start server
2. Run 50+ games over 1 hour
3. Monitor system resources:
```bash
top -p $(pgrep server)
```

**Expected Result**:
- Memory usage remains stable
- No zombie processes accumulate
- CPU usage drops to near 0 when idle
- File descriptors properly closed

---

### Test 30: Network Interruption Recovery
**Purpose**: Verify behavior on temporary network issues

**Steps**:
1. Alice and bob in game
2. Temporarily disconnect alice's network (disable interface)
3. Re-enable network

**Expected Result**:
- Bob receives disconnect notification
- Bob wins by default
- Alice's client shows connection lost
- Alice must restart client to reconnect

---

## Expected Results Summary

### Successful Tests Checklist
- [ ] Test 1-5: Authentication (all pass)
- [ ] Test 6-9: Lobby operations (all pass)
- [ ] Test 10-13: Move validation (all pass)
- [ ] Test 14-17: Win/Draw detection (all pass)
- [ ] Test 18-19: Timeout/Disconnect (all pass)
- [ ] Test 20: Leaderboard (correct calculations)
- [ ] Test 21-24: System operations (all pass)
- [ ] Test 25-30: Stress tests (no crashes)

### Key Metrics
- **No zombie processes**: `ps aux | grep defunct` returns empty
- **No orphaned IPC**: `ipcs` shows clean state after shutdown
- **No file descriptor leaks**: Check with `lsof -p <server_pid>`
- **Correct database entries**: All stats.db entries well-formed
- **No crashes**: Server runs continuously under load

---

## Debugging Tips

### View IPC Resources
```bash
# Message queues
ipcs -q

# Semaphores
ipcs -s

# Remove stuck resources
ipcrm -q <msgid>
ipcrm -s <semid>
```

### Monitor Processes
```bash
# Active game processes
ps aux | grep game_process

# Check for zombies
ps aux | grep defunct

# Server process details
ps -o pid,ppid,stat,cmd -p $(pgrep server)
```

### Check Network
```bash
# Active connections
netstat -tunap | grep 5555

# Server listening
ss -tuln | grep 5555
```

### Database Inspection
```bash
# User database
cat data/users.db

# Statistics database
cat data/stats.db

# Check file locks (while server running)
lsof data/users.db
```

---

## Automated Testing Script

```bash
#!/bin/bash
# test_runner.sh

echo "Starting automated tests..."

# Start server in background
./server &
SERVER_PID=$!
sleep 2

# Test 1: Registration
echo "Test 1: Registration"
(echo -e "R\nalice\nalice123\nquit" | ./client) | grep -q "REGISTER_OK"
if [ $? -eq 0 ]; then echo "✓ PASS"; else echo "✗ FAIL"; fi

# Test 2: Duplicate prevention
echo "Test 2: Duplicate username"
(echo -e "R\nalice\ndifferent\nquit" | ./client) | grep -q "USER_EXISTS"
if [ $? -eq 0 ]; then echo "✓ PASS"; else echo "✗ FAIL"; fi

# Cleanup
kill $SERVER_PID
echo "Tests complete."
```

---

**Testing Completed**: All tests passing indicates system is ready for submission.

**Last Updated**: February 2026
