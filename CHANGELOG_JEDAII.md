# Jedaii Ranked OpenJK Fork

This repository is a fork of [AsuTechio/OpenJK](https://github.com/AsuTechio/OpenJK) modified for the **Jedaii Ranked Duel** server.

## Added Features (Jedaii Ranked)

### 1. Secure Session Persistence ("Bully's Cookie")
Allows players to reconnect significantly faster without re-entering passwords.
*   **Mechanism**: Stores a session "cookie" (IP + Name) for 1 hour upon successful login.
*   **Safety**: Validates both **IP Address** and **Player Name** to prevent session hijacking on shared networks (LAN/WiFi).
*   **Auto-Restore**: Automatically restores session when a matching client connects (`AUTH_RESTORE` log).

### 2. New Server Commands (`sv_ccmds.cpp`)
Targeted commands for the Ranked Node.js Bot integration.

#### `sv_console <clientNum|all> "message"`
*   **Description**: Sends a raw text message to a client's console.
*   **Why**: Unlike `svtell` or `svprint`, this does **not** show in the chat box and has **no prefix**.
*   **Usage**: Perfect for sending login instructions or secret tokens.

#### `sv_getplayer <clientNum>`
*   **Description**: Returns detailed player information in **JSON format**.
*   **Output**: `PLAYER_INFO: { "id": 0, "name": "Name", "ip": "1.2.3.4", "ping": 50 }`
*   **Why**: reliable parsing for external bots (avoids regex errors with special characters in names).

#### `sv_confirmlogin <clientNum>`
*   **Description**: Used by the external Bot to confirm a successful login.
*   **Effect**: Triggers the server to create/update the session cookie for that player.

### External Script Requirement
This fork is designed to work in tandem with an **External Node.js Script (Bot)**.
*   **How it works**: The OpenJK server handles the game, while the Node.js script handles Authentication, Ranking, and Database storage.
*   **Setup**: The script usually runs on the same VPS (localhost) or a separate server, communicating via RCON.
*   **Flow**:
    1.  Server prints generic events (`AUTH_REQUEST`, `AUTH_RESTORE`).
    2.  Script reads logs -> checks database -> sends RCON commands (`sv_confirmlogin`, `sv_console`).

## Build Instructions (Legacy Support)
This fork includes a custom `build_legacy.sh` script and `Dockerfile` to compile the binary for older Linux environments (Debian Bullseye / GLIBC 2.31) while using modern OpenJK source.

### Files Modified
*   `codemp/server/server.h`: Added `session_t` struct.
*   `codemp/server/sv_client.cpp`: Session logic (`SV_AddSession`, `SV_HasSession`, `SV_DirectConnect`).
*   `codemp/server/sv_ccmds.cpp`: Added command handlers.
*   `build_legacy.sh`: Docker build wrapper.
