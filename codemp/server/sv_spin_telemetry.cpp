#include "sv_spin_telemetry.h"
#include <time.h>
#include <math.h>

#define SPIN_TELEMETRY_LOG_PATH "ranked/spin_telemetry.log"
#define SPIN_OBSERVE_DURATION_MS 3500 // Observe for 3.5 seconds after !spin

typedef struct {
  qboolean isTracking;
  int triggerTime;
  int clientNum;
  char playerName[64];

  // Baseline snapshot
  int health;
  int maxHealth;
  int armor;
  int weaponsMask;
  int ammo[16];
  int saberIndex;
  int saberHolstered;
  int viewheight;
  int standheight;
  int crouchheight;
  int customRGBA[4];
  float speed;
  int gravity;
  vec3_t velocity;
  int forcePower;
  int forcePowersKnown;
  int forcePowerLevel[18];
  vec3_t entModelScale;
  int psModelScale;
  int stateModelScale;
  int pm_type;
  int pm_flags;
  int holdableItems;

  // Last checked / logged state
  int lastLoggedTime;
  int diffCount;
} spinTracker_t;

static spinTracker_t s_spinTrackers[MAX_CLIENTS];
static const char *s_wpNames[19] = {
    "WP_NONE (0)",
    "WP_STUN_BATON (1)",
    "WP_MELEE (2)",
    "WP_SABER (3)",
    "WP_BRYAR_PISTOL (4)",
    "WP_BLASTER (5)",
    "WP_DISRUPTOR (6)",
    "WP_BOWCASTER (7)",
    "WP_REPEATER (8)",
    "WP_DEMP2 (9)",
    "WP_FLECHETTE (10)",
    "WP_ROCKET_LAUNCHER (11)",
    "WP_THERMAL (12)",
    "WP_TRIP_MINE (13)",
    "WP_DET_PACK (14)",
    "WP_CONCUSSION (15)",
    "WP_BRYAR_OLD (16)",
    "WP_EMPLACED_GUN (17)",
    "WP_TURRET (18)"
};

static const char *s_fpNames[18] = {
    "FP_HEAL (0)",
    "FP_LEVITATION (1)",
    "FP_SPEED (2)",
    "FP_PUSH (3)",
    "FP_PULL (4)",
    "FP_TELEPATHY (5)",
    "FP_GRIP (6)",
    "FP_LIGHTNING (7)",
    "FP_RAGE (8)",
    "FP_PROTECT (9)",
    "FP_ABSORB (10)",
    "FP_TEAM_HEAL (11)",
    "FP_TEAM_FORCE (12)",
    "FP_DRAIN (13)",
    "FP_SEE (14)",
    "FP_SABER_OFFENSE (15)",
    "FP_SABER_DEFENSE (16)",
    "FP_SABERTHROW (17)"
};

static void SV_SpinTelemetry_Log(const char *fmt, ...) {
  char msg[1024];
  va_list argptr;
  va_start(argptr, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

  // Print to dedicated server console
  Com_Printf("%s\n", msg);

  if (!com_dedicated || !com_dedicated->integer) {
    return;
  }

  // Timestamp
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char timeBuf[32];
  strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", t);

  char logLine[1200];
  Com_sprintf(logLine, sizeof(logLine), "[%s] %s\n", timeBuf, msg);

  fileHandle_t f;
  FS_FOpenFileByMode(SPIN_TELEMETRY_LOG_PATH, &f, FS_APPEND);
  if (f) {
    FS_Write(logLine, (int)strlen(logLine), f);
    FS_FCloseFile(f);
  }
}

void SV_SpinTelemetry_Init(void) {
  memset(s_spinTrackers, 0, sizeof(s_spinTrackers));
  SV_SpinTelemetry_Log("^2[SPIN-TELEMETRY] Subsystem initialized. Monitoring for MB2 Wheel (!spin) events and telemetry changes.");
}

qboolean SV_SpinTelemetry_IsActive(void) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (s_spinTrackers[i].isTracking) {
      return qtrue;
    }
  }
  return qfalse;
}

void SV_SpinTelemetry_OnSpinCommand(client_t *cl) {
  if (!cl || cl->state < CS_ACTIVE) return;
  int cNum = (int)(cl - svs.clients);
  if (cNum < 0 || cNum >= MAX_CLIENTS) return;

  playerState_t *ps = SV_GameClientNum(cNum);
  sharedEntity_t *ent = SV_GentityNum(cNum);
  if (!ps) return;

  spinTracker_t *tr = &s_spinTrackers[cNum];
  memset(tr, 0, sizeof(spinTracker_t));

  tr->isTracking = qtrue;
  tr->triggerTime = svs.time;
  tr->clientNum = cNum;
  Q_strncpyz(tr->playerName, cl->name, sizeof(tr->playerName));

  // Capture baseline
  tr->health = ps->stats[STAT_HEALTH];
  tr->maxHealth = ps->stats[STAT_MAX_HEALTH];
  tr->armor = ps->stats[STAT_ARMOR];
  tr->weaponsMask = ps->stats[STAT_WEAPONS];
  for (int w = 0; w < 16; w++) tr->ammo[w] = ps->ammo[w];
  tr->saberIndex = ps->saberIndex;
  tr->saberHolstered = ps->saberHolstered;
  tr->viewheight = ps->viewheight;
  tr->standheight = ps->standheight;
  tr->crouchheight = ps->crouchheight;
  for (int c = 0; c < 4; c++) tr->customRGBA[c] = ps->customRGBA[c];
  tr->speed = ps->speed;
  tr->gravity = ps->gravity;
  VectorCopy(ps->velocity, tr->velocity);
  tr->forcePower = ps->fd.forcePower;
  tr->forcePowersKnown = ps->fd.forcePowersKnown;
  for (int fp = 0; fp < 18; fp++) tr->forcePowerLevel[fp] = ps->fd.forcePowerLevel[fp];
  tr->psModelScale = ps->iModelScale;
  if (ent) {
    VectorCopy(ent->modelScale, tr->entModelScale);
    tr->stateModelScale = ent->s.iModelScale;
  }
  tr->pm_type = ps->pm_type;
  tr->pm_flags = ps->pm_flags;
  tr->holdableItems = ps->stats[STAT_HOLDABLE_ITEMS];
  tr->lastLoggedTime = svs.time;
  tr->diffCount = 0;

  SV_SpinTelemetry_Log("^5========================================================================^7");
  SV_SpinTelemetry_Log("^3[SPIN-TELEMETRY] >>> SPIN TRIGGERED by '%s' (Client #%d) at svs.time %d <<<", cl->name, cNum, svs.time);
  SV_SpinTelemetry_Log("  ^7[Baseline] HP: ^2%d/%d^7 | Armor: ^5%d^7 | WeaponsMask: ^60x%04X^7 | SaberIndex: ^3%d",
                       tr->health, tr->maxHealth, tr->armor, tr->weaponsMask, tr->saberIndex);
  SV_SpinTelemetry_Log("  ^7[Baseline] Viewheight: ^2%d^7 (Stand:%d, Crouch:%d) | Speed: ^5%.1f^7 | Gravity: ^3%d^7 | Vel: (^5%.1f, %.1f, %.1f^7)",
                       tr->viewheight, tr->standheight, tr->crouchheight, tr->speed, tr->gravity, tr->velocity[0], tr->velocity[1], tr->velocity[2]);
  SV_SpinTelemetry_Log("  ^7[Baseline] ModelScale: ent(^2%.2f, %.2f, %.2f^7) ps->iModelScale: ^5%d%%^7 | s.iModelScale: ^5%d%%",
                       tr->entModelScale[0], tr->entModelScale[1], tr->entModelScale[2],
                       tr->psModelScale, tr->stateModelScale);
  SV_SpinTelemetry_Log("  ^7[Baseline] Force: ^5%d^7 | ForcePowersKnown: ^60x%04X^7 | PM_Type: ^3%d^7 | PM_Flags: ^30x%04X",
                       tr->forcePower, tr->forcePowersKnown, tr->pm_type, tr->pm_flags);
  SV_SpinTelemetry_Log("^5========================================================================^7");
}

void SV_SpinTelemetry_OnGameServerCommand(int clientNum, const char *text) {
  if (!text || !text[0]) return;

  qboolean isSpinRelevant = qfalse;
  if (SV_SpinTelemetry_IsActive()) {
    isSpinRelevant = qtrue;
  } else if (strstr(text, "spin") || strstr(text, "Spin") || strstr(text, "wheel") || strstr(text, "Wheel") ||
             strstr(text, "fortune") || strstr(text, "Fortune") || strstr(text, "reward") || strstr(text, "penalty") ||
             strstr(text, "punishment") || strstr(text, "prize")) {
    isSpinRelevant = qtrue;
  }

  if (isSpinRelevant) {
    SV_SpinTelemetry_Log("^6[SPIN-TELEMETRY GVM-CMD] Target: %d | ServerCommand: %s^7", clientNum, text);
  }
}

void SV_SpinTelemetry_Frame(void) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    spinTracker_t *tr = &s_spinTrackers[i];
    if (!tr->isTracking) continue;

    if (svs.time - tr->triggerTime > SPIN_OBSERVE_DURATION_MS) {
      SV_SpinTelemetry_Log("^3[SPIN-TELEMETRY] Observation window completed for '%s' (Client #%d). Total diffs captured: ^2%d^7",
                           tr->playerName, tr->clientNum, tr->diffCount);
      tr->isTracking = qfalse;
      continue;
    }

    client_t *cl = &svs.clients[i];
    if (cl->state < CS_ACTIVE) {
      tr->isTracking = qfalse;
      continue;
    }

    playerState_t *ps = SV_GameClientNum(i);
    sharedEntity_t *ent = SV_GentityNum(i);
    if (!ps) continue;

    // Check for Weapon additions / removals
    if (ps->stats[STAT_WEAPONS] != tr->weaponsMask) {
      int oldMask = tr->weaponsMask;
      int newMask = ps->stats[STAT_WEAPONS];
      for (int w = 0; w < 19; w++) {
        int bit = (1 << w);
        if (!(oldMask & bit) && (newMask & bit)) {
          SV_SpinTelemetry_Log("^2[SPIN-DIFF: WEAPON ADDED] + %s (bit 0x%04X) | Ammo: %d | TotalMask: 0x%04X",
                               s_wpNames[w], bit, (w < 16 ? ps->ammo[w] : 0), newMask);
          tr->diffCount++;
        } else if ((oldMask & bit) && !(newMask & bit)) {
          SV_SpinTelemetry_Log("^1[SPIN-DIFF: WEAPON REMOVED] - %s (bit 0x%04X) | TotalMask: 0x%04X",
                               s_wpNames[w], bit, newMask);
          tr->diffCount++;
        }
      }
      tr->weaponsMask = newMask;
    }

    // Check Ammo changes
    for (int w = 0; w < 16; w++) {
      if (ps->ammo[w] != tr->ammo[w]) {
        if (abs(ps->ammo[w] - tr->ammo[w]) >= 10 || tr->ammo[w] == 0) {
          SV_SpinTelemetry_Log("^5[SPIN-DIFF: AMMO] Weapon %s Ammo changed: %d -> %d",
                               (w < 19 ? s_wpNames[w] : "WP_UNKNOWN"), tr->ammo[w], ps->ammo[w]);
          tr->diffCount++;
        }
        tr->ammo[w] = ps->ammo[w];
      }
    }

    // Check Saber Index / Holster state
    if (ps->saberIndex != tr->saberIndex || ps->saberHolstered != tr->saberHolstered) {
      SV_SpinTelemetry_Log("^3[SPIN-DIFF: SABER STATE] SaberIndex: %d -> %d | SaberHolstered: %d -> %d",
                           tr->saberIndex, ps->saberIndex, tr->saberHolstered, ps->saberHolstered);
      tr->saberIndex = ps->saberIndex;
      tr->saberHolstered = ps->saberHolstered;
      tr->diffCount++;
    }

    // Check Entity / Model Scale
    if (ps->iModelScale != tr->psModelScale) {
      SV_SpinTelemetry_Log("^6[SPIN-DIFF: MODEL SCALE (ps->iModelScale)] %d%% -> %d%% (Scale factor: %.2fx)",
                           tr->psModelScale, ps->iModelScale, (float)ps->iModelScale / 100.0f);
      tr->psModelScale = ps->iModelScale;
      tr->diffCount++;
    }

    if (ent) {
      if (fabsf(ent->modelScale[0] - tr->entModelScale[0]) > 0.02f ||
          fabsf(ent->modelScale[1] - tr->entModelScale[1]) > 0.02f ||
          fabsf(ent->modelScale[2] - tr->entModelScale[2]) > 0.02f) {
        SV_SpinTelemetry_Log("^6[SPIN-DIFF: MODEL SCALE (ent->modelScale)] (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)",
                             tr->entModelScale[0], tr->entModelScale[1], tr->entModelScale[2],
                             ent->modelScale[0], ent->modelScale[1], ent->modelScale[2]);
        VectorCopy(ent->modelScale, tr->entModelScale);
        tr->diffCount++;
      }

      if (ent->s.iModelScale != tr->stateModelScale) {
        SV_SpinTelemetry_Log("^6[SPIN-DIFF: MODEL SCALE (ent->s.iModelScale)] %d%% -> %d%%",
                             tr->stateModelScale, ent->s.iModelScale);
        tr->stateModelScale = ent->s.iModelScale;
        tr->diffCount++;
      }
    }

    // Check Viewheight & Stand/Crouch height (Size change)
    if (ps->viewheight != tr->viewheight || ps->standheight != tr->standheight || ps->crouchheight != tr->crouchheight) {
      SV_SpinTelemetry_Log("^3[SPIN-DIFF: HEIGHTS] ps->viewheight: %d -> %d | Stand: %d -> %d | Crouch: %d -> %d",
                           tr->viewheight, ps->viewheight,
                           tr->standheight, ps->standheight,
                           tr->crouchheight, ps->crouchheight);
      tr->viewheight = ps->viewheight;
      tr->standheight = ps->standheight;
      tr->crouchheight = ps->crouchheight;
      tr->diffCount++;
    }

    // Check Custom RGBA (Color / Glow / Effect tint)
    if (ps->customRGBA[0] != tr->customRGBA[0] || ps->customRGBA[1] != tr->customRGBA[1] ||
        ps->customRGBA[2] != tr->customRGBA[2] || ps->customRGBA[3] != tr->customRGBA[3]) {
      SV_SpinTelemetry_Log("^5[SPIN-DIFF: CUSTOM RGBA TINT] (%d, %d, %d, %d) -> (%d, %d, %d, %d)",
                           tr->customRGBA[0], tr->customRGBA[1], tr->customRGBA[2], tr->customRGBA[3],
                           ps->customRGBA[0], ps->customRGBA[1], ps->customRGBA[2], ps->customRGBA[3]);
      for (int c = 0; c < 4; c++) tr->customRGBA[c] = ps->customRGBA[c];
      tr->diffCount++;
    }

    // Check Health / MaxHealth / Armor
    if (ps->stats[STAT_HEALTH] != tr->health || ps->stats[STAT_MAX_HEALTH] != tr->maxHealth || ps->stats[STAT_ARMOR] != tr->armor) {
      SV_SpinTelemetry_Log("^2[SPIN-DIFF: STATS] HP: %d -> %d | MaxHP: %d -> %d | Armor: %d -> %d",
                           tr->health, ps->stats[STAT_HEALTH],
                           tr->maxHealth, ps->stats[STAT_MAX_HEALTH],
                           tr->armor, ps->stats[STAT_ARMOR]);
      tr->health = ps->stats[STAT_HEALTH];
      tr->maxHealth = ps->stats[STAT_MAX_HEALTH];
      tr->armor = ps->stats[STAT_ARMOR];
      tr->diffCount++;
    }

    // Check Speed / Gravity / Large Velocity Spike (Slap / Yeet)
    if (fabsf(ps->speed - tr->speed) > 2.0f) {
      SV_SpinTelemetry_Log("^5[SPIN-DIFF: SPEED] ps->speed changed: %.1f -> %.1f", tr->speed, ps->speed);
      tr->speed = ps->speed;
      tr->diffCount++;
    }
    if (ps->gravity != tr->gravity) {
      SV_SpinTelemetry_Log("^5[SPIN-DIFF: GRAVITY] ps->gravity changed: %d -> %d", tr->gravity, ps->gravity);
      tr->gravity = ps->gravity;
      tr->diffCount++;
    }
    if (fabsf(ps->velocity[2] - tr->velocity[2]) > 400.0f ||
        fabsf(ps->velocity[0] - tr->velocity[0]) > 400.0f ||
        fabsf(ps->velocity[1] - tr->velocity[1]) > 400.0f) {
      SV_SpinTelemetry_Log("^1[SPIN-DIFF: VELOCITY IMPULSE] ps->velocity impulse: (%.1f, %.1f, %.1f) -> (%.1f, %.1f, %.1f)",
                           tr->velocity[0], tr->velocity[1], tr->velocity[2],
                           ps->velocity[0], ps->velocity[1], ps->velocity[2]);
      VectorCopy(ps->velocity, tr->velocity);
      tr->diffCount++;
    }

    // Check Force Powers
    if (ps->fd.forcePowersKnown != tr->forcePowersKnown) {
      SV_SpinTelemetry_Log("^6[SPIN-DIFF: FORCE POWERS KNOWN] 0x%04X -> 0x%04X", tr->forcePowersKnown, ps->fd.forcePowersKnown);
      for (int fp = 0; fp < 18; fp++) {
        int bit = (1 << fp);
        if (!(tr->forcePowersKnown & bit) && (ps->fd.forcePowersKnown & bit)) {
          SV_SpinTelemetry_Log("  ^2+ Force Power Added: %s (Level %d)", s_fpNames[fp], ps->fd.forcePowerLevel[fp]);
        } else if ((tr->forcePowersKnown & bit) && !(ps->fd.forcePowersKnown & bit)) {
          SV_SpinTelemetry_Log("  ^1- Force Power Removed: %s", s_fpNames[fp]);
        }
      }
      tr->forcePowersKnown = ps->fd.forcePowersKnown;
      tr->diffCount++;
    }
    for (int fp = 0; fp < 18; fp++) {
      if (ps->fd.forcePowerLevel[fp] != tr->forcePowerLevel[fp]) {
        SV_SpinTelemetry_Log("^6[SPIN-DIFF: FORCE POWER LEVEL] %s Level: %d -> %d",
                             s_fpNames[fp], tr->forcePowerLevel[fp], ps->fd.forcePowerLevel[fp]);
        tr->forcePowerLevel[fp] = ps->fd.forcePowerLevel[fp];
        tr->diffCount++;
      }
    }

    // Check PM_Type / PM_Flags
    if (ps->pm_type != tr->pm_type || ps->pm_flags != tr->pm_flags) {
      SV_SpinTelemetry_Log("^3[SPIN-DIFF: PMOVE] ps->pm_type: %d -> %d | ps->pm_flags: 0x%04X -> 0x%04X",
                           tr->pm_type, ps->pm_type, tr->pm_flags, ps->pm_flags);
      tr->pm_type = ps->pm_type;
      tr->pm_flags = ps->pm_flags;
      tr->diffCount++;
    }

    // Check Holdable Items
    if (ps->stats[STAT_HOLDABLE_ITEMS] != tr->holdableItems) {
      SV_SpinTelemetry_Log("^2[SPIN-DIFF: HOLDABLE ITEMS] 0x%04X -> 0x%04X",
                           tr->holdableItems, ps->stats[STAT_HOLDABLE_ITEMS]);
      tr->holdableItems = ps->stats[STAT_HOLDABLE_ITEMS];
      tr->diffCount++;
    }
  }
}
