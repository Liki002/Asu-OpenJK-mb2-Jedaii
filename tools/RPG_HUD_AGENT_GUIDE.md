# 🎮 MovieBattles II Custom RPG HUD & Engine Extension Guide

This guide details the technical implementation of the custom **RPG HUD Overlay** and **Server-Client Data Sync** inside the OpenJK engine for MovieBattles II (`mbii.x86.exe`).

---

## 🏗️ Architecture & Rendering Layer

The RPG HUD is built **directly into the client engine refresh pipeline** (`codemp/client/cl_scrn.cpp`).

Because it operates at the client engine level rather than inside a dynamic VM module, the HUD:
- Renders **on top of all in-game graphics** regardless of game mode or mod DLLs.
- Operates standalone without requiring external `.pk3` asset archives.
- Is immune to closed-source `cgamex86.dll` / `uix86.dll` overrides.

```
┌─────────────────────────────────────────────────────────────┐
│ 1. CLIENT ENGINE OVERLAY (Built inside `mbii.x86.exe`)       │
│    - Custom RPG HUD (Avatar, Level, XP Bar)                 │
│    - Rendered via `SCR_DrawRPGHUDOverlay()` in `cl_scrn.cpp` │
└──────────────────────────────┬──────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────┐
│ 2. CGAME IN-GAME HUD (Loaded from `MBII/cgamex86.dll`)      │
│    - Standard MBII Health, Armor, and Force meters           │
└─────────────────────────────────────────────────────────────┘
```

---

## 📁 Modified Source Files

| Source File | Line / Location | Purpose |
|-------------|-----------------|---------|
| [`codemp/client/cl_scrn.cpp`](file:///codemp/client/cl_scrn.cpp) | `SCR_Init()`, `SCR_DrawRPGHUDOverlay()` | Declares RPG CVars, initializes default values, and renders the 2D panel, Avatar box, Level text, and dynamic XP bar. |
| [`codemp/client/cl_parse.cpp`](file:///codemp/client/cl_parse.cpp) | `CL_ParseCommandString()` | Intercepts server-sent `rpg_sync` commands to update client CVars in real time. |

---

## ⚙️ CVars Reference

| CVar | Default Value | Description |
|------|---------------|-------------|
| `cg_drawRPGHUD` | `1` | Master toggle (`1` = ON, `0` = OFF). |
| `cg_rpg_level` | `1` | Current player Level display. |
| `cg_rpg_xp` | `7500` | Current player XP numerical value. |
| `cg_rpg_xp_max` | `10000` | Required XP for next Level. |
| `cg_rpg_avatar` | `gfx/hud/avatar_default` | Shader path for the Avatar picture icon. |

---

## 📡 Server-to-Client Data Protocol

When a player kills an NPC, completes an objective, or gains XP on the server, the server issues a reliable network command:

```
rpg_sync <currentXP> <maxXP> <level>
```

### Example:
```cpp
// Server-side trigger (e.g. in g_combat.cpp upon NPC death):
trap_SendServerCommand( clientNum, "rpg_sync 8500 10000 15" );
```

When received by the client engine (`CL_ParseCommandString` in `cl_parse.cpp`), it updates `cg_rpg_xp`, `cg_rpg_xp_max`, and `cg_rpg_level`, instantly updating the on-screen progress bar and level badge.

---

## 🔨 Local Build Instructions (Windows MSVC)

To compile `mbii.x86.exe` locally in **10 seconds** using Visual Studio:

### 1. Configure CMake:
```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -B build -A Win32 '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
```

### 2. Compile Release Solution:
```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' build\OpenJK.slnx /p:Configuration=Release /m
```

### 3. Deploy Output Executable:
Copy `build\Release\openjk.x86.exe` to `mbii.x86.exe` inside your Jedi Academy `GameData` directory:
```
C:\Program Files (x86)\Steam\steamapps\common\Jedi Academy\GameData\mbii.x86.exe
```
