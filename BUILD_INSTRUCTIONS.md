# Quick Build Guide for openjkded.i386

## Build Command
To rebuild the server binary, run this command from the `AsuTechio_OpenJK` directory:

```bash
wsl bash -c "echo '<YOUR_SUDO_PASSWORD>' | sudo -S bash build_legacy.sh"
```

## What It Does
- Uses Docker to compile for legacy Linux (GLIBC 2.31 / Debian Bullseye)
- Outputs the binary to: `build-server/openjkded.i386`
- Takes ~1-2 minutes to complete

## After Building
1. Stop your server
2. Replace the old `openjkded.i386` with the new one from `build-server/`
3. Start your server

## Modified Files (Jedaii Ranked)
- `codemp/server/server.h` - Session structures
- `codemp/server/sv_client.cpp` - Session logic, `AUTH_RESTORE` logging
- `codemp/server/sv_ccmds.cpp` - `sv_confirmlogin`, `sv_getplayer`, `sv_console` commands

## Key Features
- **Session Persistence**: IP + Name matching (1 hour expiry)
- **sv_console**: Raw console print (no prefix, no chat)
- **sv_getplayer**: JSON player info for bot
- **sv_confirmlogin**: Bot confirms login to create session
- **RPG HUD & Network Sync**: Synchronizes server account XP/Level to client HUD overlay (`rpg_sync` command)

---

# Client Build Guide (`mbii.x86.exe` / `openjk.x86.exe`)

To compile the custom client engine with the RPG HUD Overlay locally using Visual Studio:

1. **Configure CMake (Win32)**:
```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -B build -A Win32 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
```

2. **Compile Release Solution**:
```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' build\OpenJK.sln /p:Configuration=Release /m
```

3. **Deploy Output Executable**:
Copy `build\Release\openjk.x86.exe` (or `mbii.x86.exe`) to your desired directory or GameData folder.

