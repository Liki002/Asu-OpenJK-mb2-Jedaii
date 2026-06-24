# Jedaii Ranked OpenJK Fork
**Date**: 2026-06-13

This repository is a fork of [AsuTechio/OpenJK](https://github.com/AsuTechio/OpenJK) modified for the **Jedaii Ranked Duel** server.

## Added Features (Jedaii Ranked)

---

### Session: 2026-06-13 — Exempt NPCs from Duel Culling & Fix Bone Remapping

This session focused on fixing two major NPC-related issues: invisible NPCs and disappearing NPCs on spawning.

#### 1. Exempt NPCs from Duel Culling
- **NPC Culling Exemption**: Modified `DuelCull` in `codemp/server/duel_cull.cpp` to return `0` (do not cull, do not disable collision) if either entity is an NPC (`isNPC(flatten(ent)) || isNPC(flatten(touch))`).
- **Rationale**: Prior to this fix, when a player entered a duel, the server's snapshot duel culling would identify NPCs as non-dueling actors and filter them out of the dueling player's snapshots. This caused NPCs to disappear immediately after spawning or starting a duel. This fix keeps NPCs visible and interactive for all players, regardless of their dueling state.

#### 2. Revert Bone Remapping Commit
- **Reverted 31304c14**: Reverted the bone-remapping commit which corrupted skeleton structures on 72-bone humanoid models for dedicated servers, causing all NPCs to become invisible.

#### Files Modified
- `codemp/server/duel_cull.cpp` (Exempt NPCs from Duel Culling)
- `codemp/rd-dedicated/tr_model.cpp` (Revert bone remapping)
- `codemp/rd-rend2/tr_model.cpp` (Revert bone remapping)
- `codemp/rd-vanilla/tr_model.cpp` (Revert bone remapping)

---

### Session: 2026-05-13 — Mode Sync, Rival Tracking & Logic Hardening

This session focused on finalizing the engine's mode-detection synchronization, implementing the Rivalry social system, and hardening the "Hot Potato" and Duel resolution logic against command-parsing failures.

#### 1. Engine & Mode Synchronization
- **Triggered Mode Refresh**: Integrated `SV_Ranked_SaveConfig()` into `SV_SpawnServer()` in `sv_init.cpp`. The Ranked engine now re-queries `g_Authenticity` and `g_gametype` at the start of every map, ensuring the dashboard correctly reflects "Duel" vs "Open" mode immediately.
- **Initialization Cleanup**: Fixed a bug in `SV_Ranked_Init` that printed the active mode before CVars were synchronized, which previously caused misleading "Open" reports in the startup log.

#### 2. Rivalry & Persistence Enhancements
- **Rival Fields**: Added `rival_guid` and `rival_name` to the core account schema.
- **Automated Sync**: Updated `SV_Ranked_SyncTopLevelFields` to initialize and persist these fields for all accounts (defaulting to "None").
- **Social History**: Implemented `SV_Ranked_TrackRival` and `SV_Ranked_RecordRecentDuel` to maintain a persistent match history between players.

#### 3. Hot Potato Mode Overhaul
- **Messaging Pivot**: Changed all milestone broadcasts (30s, 1m, 2m streaks) from console `print` to global `chat` to ensure full in-game visibility.
- **Session MVP (Top Potato)**: 
    - Implemented session-based tracking for the longest single hold duration.
    - Added a global broadcast in `SV_Ranked_StopHotPotato` to announce the "Round MVP" when the event ends.
    - Example: `[Hot Potato] Round MVP: Liki held the potato for 12 ticks!`

#### 4. Robust Duel Logic & Parser
- **Parser Overhaul**: Completely rewrote the `@@@PLDUELACCEPT` extraction logic in `sv_gameapi.cpp`.
- **Command Prefix Handling**: The parser now correctly handles command words like `print` or `chat` at the start of the string and robustly extracts player names despite color codes or trailing punctuation.
- **Resolution Fix**: Fixed the "printLiki" bug where engine commands were being incorrectly included in resolved player names.

#### 5. Logging & Noise Reduction
- **Persistent Log Hardening**: Added comprehensive logging to `ranked/ranked.log`:
    - `DUEL_START`: `DUEL_START: Player1 vs Player2`
    - `DUEL_END`: `DUEL_END: Winner def. Loser (+Elo / -Elo) [Tie: 0, Disc: 0]`
    - `ELO`: Logs every individual FR change.
    - `LEVEL`: Logs every level-up event.
    - `WINMSG/WINSND`: Logs when custom rewards are triggered.
- **Spam Control**: Gated the frequent `DATABASE: Saved X accounts` log entry behind the `sv_ranked_debug` Cvar to keep the persistent log readable.

#### 6. UX Improvements
- **Domination Alerts**: Added a global chat broadcast for Domination streaks (5 kills on the same player) to accompany the centerprint.
- **Level-Up Visibility**: Global "Level Up" announcements now broadcast to `chat` instead of `print`, ensuring they are seen by all players.

#### Files Modified
- `codemp/server/sv_init.cpp` (Mode Sync Hook)
- `codemp/server/sv_gameapi.cpp` (Duel Parser Fixes)
- `codemp/server/sv_ranked_db.cpp` (Rival Fields & Logging Noise Control)
- `codemp/server/sv_ranked_logic.cpp` (Hot Potato MVP, Duel Logs, Chat Alerts)
- `codemp/server/sv_ranked_db.h` (Rival System Declarations)
- `codemp/server/sv_ranked_logic.h` (Internal logic updates)

---

### Session: 2026-05-13 — SQL Schema Alignment & Social Features

#### SQL-Compatible JSON Schema
- **Flattened Top-Level Fields**: Implemented `SV_Ranked_SyncTopLevelFields` to mirror the SQL leaderboard schema. Nested `modes` data is now aliased to top-level fields (`mmr`, `wins`, `losses`, `kills`, `deaths`, `clean_name`, `team`, `in_duel`) on every database save.
- **Three-Tier GUID System**:
  - `engine_guid`: Raw OpenJK client GUID.
  - `guid`: Internal `AUTH_`-prefixed unique ID.
  - `persistent_id`: Standard UUID for web tracking and social mapping.
- **Auto-Sync Persistence**: Logic integrated into `SV_Ranked_SaveAccounts` to ensure the JSON file is always ready for external SQL ingestion.

#### Server Messaging Standards
- **Standardized SV_SendServerCommand usage**:
  - `chat`: Primary for all game notifications and global announcements (Main Chat Area).
  - `print`: Console-only for detailed stats and technical data.
  - `cp`: High-priority CenterPrint alerts (Center of screen).

#### Rival System Implementation (Planned)
- Tracking match history counters between persistent IDs in JSON.
- Automated `rival_guid` and `rival_name` generation after 3+ encounters.

---

### Session: 2026-05-12 — System Finalization

#### Files Modified
- `codemp/server/sv_init.cpp`
- `codemp/server/sv_ranked_db.cpp`
- `codemp/server/sv_ranked_logic.cpp`
- `codemp/server/sv_ranked_cmds.cpp`

#### Global Ranked Toggle (`sv_ranked_enabled`)
- Registered new `sv_ranked_enabled` Cvar (default `1`, `CVAR_ARCHIVE`) in `sv_init.cpp`.
- Config sync: `SV_Ranked_LoadConfig` reads `"enabled"` from `ranked/config.json` and sets the Cvar; `SV_Ranked_SaveConfig` writes the current Cvar value back before saving.
- Toggle guards added to `SV_Ranked_ProcessKill`, `SV_Ranked_DuelStart` (logic disabled), and `SV_Ranked_ProcessCommand` (all `!` commands silently swallowed, except `!help` which notifies the player that ranked is offline).
- Disabling: `rcon sv_ranked_enabled 0` or set `"enabled": 0` in `ranked/config.json`.

#### Config Documentation (`_help` block)
- Added a `"_help"` JSON object to `ranked/config.json` with plain-English descriptions for every configurable field: `enabled`, `rank_titles`, `rank_thresholds`, all `elo_*` keys, `duel_win_credits`, `duel_loss_credits`, `roll_cooldown_ms`, `lucky_charm_bonus`.
- The help object is injected at first boot and preserved on subsequent loads so admins never need to read source code to understand a config value.

#### Achievement System Repair
- **Fixed `!achievements`**: Command now correctly iterates over JSON boolean keys (`item->string`) instead of expecting string values. Achievements now display properly even when dozens are earned.
- **Wired ignored stats**: `SV_Ranked_CheckKillAchievements` now grants:
  - `brawler` — 10 melee kills
  - `demolitionist` — 10 bomb kills
  - `dominator` — 5 dominations in a session
  - `kills_500`, `kills_1000` — additional kill milestones
  - `streak_20` — 20-kill streak
- **Economy trigger**: `UpdateAccountCredits` now calls `SV_Ranked_CheckEconomyAchievements` on every positive credit gain (bets, rolls, transfers) so the `credits_1000` achievement fires from any source, not just kills.

#### Daily Quest Milestone Notifications
- `SV_Ranked_ProgressQuest` now fires a private `print` notification when a player crosses a daily stat milestone (e.g., 10 kills, 25 saber kills, 5 dominations) and automatically awards a small credit bonus.
- Milestones are defined in `g_questMilestones[]` in `sv_ranked_db.cpp` for easy expansion.


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

---

---

## 2026-05-12 Update: Persistent Ranked System & Economy
Major overhaul of the server-side ranking and economy system, introducing a JSON-backed database and comprehensive player progression.

### 1. JSON Database Integration (`sv_ranked_db.cpp`, `cJSON.c`)
*   **Persistent Accounts**: Switched from transient name-based tracking to a persistent JSON database (`ranked/accounts.json`).
*   **GUID Anchoring**: Accounts are now linked to player GUIDs to prevent identity spoofing and name collisions.
*   **Configuration**: Centralized tuning via `ranked/config.json` (Elo K-factors, reward values, cooldowns).

### 2. Progression & Elo System (`sv_ranked_logic.cpp`)
*   **Dual Mode Elo**: Separate Force Rating (FR) buckets for **Duel** and **Open** modes.
*   **XP & Leveling**: 1000 XP per level system with titles (Youngling to Legend).
*   **Quest System**: Daily and long-term quests (e.g., "Century" - 100 kills, "Bounty Hunter" - 3 claims).
*   **Achievement System**: Multi-tier achievements for combat milestones and economy targets.

### 3. Combat & Social Features
*   **Kill Streaks**: Spree, Rampage, Godlike, and Legendary streaks with global broadcasts.
*   **Bounty System**: Automatic bounties on high streaks; manual bounty placement using credits.
*   **Multi-Kill Detection**: Double, Triple, Monster kills with FR/XP rewards.
*   **Jail/Probation**: New enforcement system to kick players who freekill while on probation.
*   **Gambling & Economy**: `!roll` command for credit gambling; `!send` for player-to-player transfers.
*   **Shop & Inventory**: Consumable boosts (XP/Credit/Elo) and items like the "Lucky Charm".

### 4. New Server-Side Commands (`sv_ranked_cmds.cpp`)
*   **User**: `!stats`, `!top`, `!rank`, `!bounty`, `!bet`, `!roll`, `!details`, `!buy`, `!inventory`, `!wanted`.
*   **Admin**: `!givecredits`, `!setelo`, `!setrank`, `!jail`, `!unjail`.
*   **Voting**: `!vote hotpotato` for initiating special game modes.

### Architectural Changes
*   Added `rankedMatchState_t` to `server.h` for per-client session tracking.
*   Modified `sv_client.cpp` to handle login/logout/connect events.
*   Integrated `cJSON` for all persistence needs.
*   Implemented `SV_Ranked_BroadcastOpen` to target messages away from private duels.

### Current Bugs & Open Tasks
*   **Win Message Overhaul**: Migrating participants to private centerprint broadcasts (In Progress).
*   **Audio Reliability**: Transitioning all `snd` calls to standard `play` command.
*   **GUID Spoofing**: Investigating further GUID validation to ensure client-side `ja_guid` is tamper-proof.

---

## 2026-05-12 Hotfix: Restore missing `sv_ranked_db.cpp` + verify legacy build

### Bug Fixes
* **Build break fix**: Re-created `codemp/server/sv_ranked_db.cpp` (it was referenced by `codemp/CMakeLists.txt` but missing in the working tree).
* **Linker fix**: Implemented the missing console command handlers required by `sv_ccmds.cpp`:
  * `ranked_resetpass` → `SV_RankedResetPass_f`
  * `ranked_clearaccounts` → `SV_RankedClearAccounts_f`
* **ODR/array size fix**: Standardized `sv_rankedPlayers` to `64` slots to match `sv_ranked_db.h` and avoid MSVC “redefinition; different subscripts” errors.

### New / Restored Functionality (Server-Side Ranked DB)
* **JSON persistence (cJSON)** for:
  * Accounts: `ranked/accounts.json`
  * Config/tuning: `ranked/config.json` (Elo constants, duel rewards, roll cooldown, lucky charm bonus)
* **Authentication flow**: `login <user> <pass>` registers or logs in; `logout` ends session (integrated via existing hooks in `sv_client.cpp`).
* **Minimal economy + progression helpers**:
  * Credits support (`UpdateAccountCredits`)
  * XP → level mapping (`SV_Ranked_CalculateLevel`)
  * Title mapping by FR (`SV_Ranked_GetTitle`)
* **Player-visible commands implemented** (called from `sv_ranked_cmds.cpp`):
  * `SV_Ranked_ShowStats`, `SV_Ranked_ShowTop`, `SV_Ranked_ShowRank`
  * `SV_Ranked_ShowTopCredits`, `SV_Ranked_ShowTopPotato`
  * `SV_Ranked_ShowCredits`
  * Shop stubs: `SV_Ranked_ShowShop`, `SV_Ranked_ShopBuy/Sell/Use`
  * Cosmetic/account settings: `SV_Ranked_Cmd_SetWinMsg`, `SV_Ranked_Cmd_SetWinSnd`
  * Online bounty list + placement: `SV_Ranked_ShowBountyList`, `SV_Ranked_SetBounty`
  * Daily quest + achievement persistence stubs (enough to unblock compilation)

### Build / Verification
* Followed `BUILD_INSTRUCTIONS.md` legacy pipeline: `build_legacy.sh` successfully produced an updated binary at `build-server/openjkded.i386`.

---

## 2026-05-12 Hotfix: Config-driven Rank Titles (and config source clarification)

### New Features
* **Configurable rank titles + thresholds**:
  * Added `rank_titles` and `rank_thresholds` support to `ranked/config.json`.
  * `SV_Ranked_GetTitle(int fr)` now reads from these arrays to determine the displayed title.
  * Defaults are auto-populated on boot if missing (preserves existing configs).

### Bug Fixes / UX Fixes
* **Config mismatch confusion resolved**:
  * Confirmed and documented that the native ranked system reads/writes **only**:
    * `ranked/config.json`
    * `ranked/accounts.json`
  * Legacy/other files like `ranked_config.json` / `accounts_v2.json` are not referenced by the current codebase and are not used by the dedicated binary built from this repo.

### Documentation
* Updated `IMPLEMENTATION_GUIDE_2026_05_11.md` with:
  * An explicit “which JSON does the engine use” section
  * A key-by-key explanation of config options
  * Logging notes (no dedicated `ranked.log` yet; uses standard console logging with `[RANKED]` prefix)

### Build / Verification
* Re-ran the legacy build pipeline (`build_legacy.sh`) successfully after the config/title changes and regenerated `build-server/openjkded.i386`.

---

## 2026-05-12 Update: Logging, Auto-Registration, Win Message & Admin Fixes

### New Features
* **Persistent Logging** (`sv_ranked_db.cpp`, `sv_ranked_db.h`):  
  - New `SV_Ranked_Log(fmt, ...)` function appends timestamped entries to `ranked/ranked.log`.  
  - Events logged: `LOGIN`, `REGISTER`, `AUTO-LOGIN`, `AUTO-REGISTER`, `LOGOUT`, `DISCONNECT`.
  - Path constant `RANKED_LOG_PATH = "ranked/ranked.log"` sits alongside `accounts.json` / `config.json`.

* **GUID-Based Auto-Registration** (`sv_ranked_db.cpp`, `sv_ranked_db.h`):  
  - New `SV_Ranked_AutoRegisterByGUID(client_t *cl)` called from `SV_Ranked_ClientConnect`.  
  - Returning players with a known GUID are **auto-logged in** transparently.  
  - New players with an unknown GUID get a fresh `ranked_XXXX` account with a random password delivered privately via `print`.  
  - Players without any GUID (pirated/non-JA+ clients) see a prompt to log in manually.

### Bug Fixes / UX
* **Audio Reliability** (`sv_ranked_logic.cpp` line 185):  
  - Replaced `snd sound/ushowdown/win.mp3` with `play sound/ushowdown/win.mp3` on Level-Up events.

* **Win Message Behavior** (`sv_ranked_logic.cpp` — `AnnounceSpecialDuelResults`):  
  - Verified: custom win messages (`!setwinmsg`) and custom sounds (`!setwinsnd`) are already sent via private `cp` and `play` to the winner and loser **only**. No change needed here.

* **`!setrank` Admin Command Fix** (`sv_ranked_cmds.cpp`):  
  - Replaced fragile `sscanf` parsing with a first-token/rest-of-string split.  
  - The first whitespace-delimited token is the player ID or partial name.  
  - Everything after is treated as the rank title — supporting multi-word titles like `Jedi Master` or `Grand Council Member`.
  - Now correctly returns an error on ambiguous matches (`-2`) instead of silently failing.

### Files Modified
* `AsuTechio_OpenJK/codemp/server/sv_ranked_db.h` — Added `SV_Ranked_Log` and `SV_Ranked_AutoRegisterByGUID` declarations.
* `AsuTechio_OpenJK/codemp/server/sv_ranked_db.cpp` — Implemented logging, auto-register/login, and disconnect log hooks.
* `AsuTechio_OpenJK/codemp/server/sv_ranked_logic.cpp` — `snd` → `play` for Level-Up sound.
* `AsuTechio_OpenJK/codemp/server/sv_ranked_cmds.cpp` — Rewrote `!setrank` argument parsing.

---

## 2026-05-13 Update: Native C++ Trivia, Kyle Boss Logic & Website Sync

This session focused on eliminating external dependencies (`trivia.js`) by porting all non-core game logic directly into the OpenJK C++ binary.

### 1. Native Trivia System (Architectural Port)
- **Data Persistence**: Created `sv_ranked_trivia.h` containing all 71 Star Wars trivia questions and their multi-variant answers in a static C struct.
- **Background Loop**: Implemented `SV_Ranked_Trivia_Frame` in `sv_ranked_logic.cpp`. It runs a timer and broadcasts a random question every ~4.6 minutes.
- **Secure Answering**: 
    - Modified `sv_client.cpp` and `sv_ranked_cmds.cpp` to intercept messages starting with the hash (`#`) prefix.
    - Trivia answers are hidden from public chat to prevent copying/cheating.
    - Implemented a 10-second per-player cooldown to prevent brute-forcing.
- **Rewards**: Correct answers automatically grant **100 XP, 50 Credits, and increment the `trivia_wins` counter** in the JSON database.

### 2. Kyle Boss Tracking & Rewards
- **Logic Hook**: Added detection for `kyle_boss_trainer` in the `SV_Ranked_ProcessKill` flow.
- **Automation**: When the boss is killed, the server broadcasts a global message and awards the slayer **250 XP, 100 Credits, and +1 `kyle_boss_kills` stat**.
- **Persistence**: These new stats are successfully serialized to `accounts.json` and persisted across map changes.

### 3. Server Mode Detection (MBII Fix)
- **Cvar Sync**: Updated `SV_Ranked_GetActiveMode` to check the `g_Authenticity` Cvar. 
    - `auth 3` now correctly maps to **Duel** mode stats.
    - Fixed a bug where the server would stay in "Open" mode despite being in a duel room.
- **Bridge Logic**: `SV_Ranked_SaveConfig` now writes `active_mode` to `config.json`. This allows external scripts (like the API or website) to know exactly which mode the server is running without complex RCON queries.

### 4. API & Web Dashboard Improvements
- **`ranked_api.py`**: Updated to read `config.json` for mode detection. It now correctly parses and serves `trivia_wins` and `kyle_boss_kills` stats.
- **`dbconnect.php`**: Resolved a critical PHP Parse Error (syntax error) caused by a missing semicolon on line 31. The website leaderboard is now functional again.

### 5. Build & Deployment
- Successfully rebuilt the server binary using the legacy pipeline: `build-server/openjkded.i386` is ready for deployment.
- **Files Modified**:
    - `codemp/server/sv_ranked_trivia.h` (New Question Database)
    - `codemp/server/sv_ranked_logic.cpp` (Trivia Loop & Boss Rewards)
    - `codemp/server/sv_ranked_cmds.cpp` (Chat Interception Logic)
    - `codemp/server/sv_ranked_db.cpp` (Config Mode Bridge)
    - `codemp/server/sv_client.cpp` (Expanded Command Hook)
    - `ranked_api.py` (Mode-Aware Stat Fetching)
    - `dbconnect.php` (Syntax Fix)

