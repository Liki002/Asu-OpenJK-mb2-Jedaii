=================================================================
  RPG HUD STANDALONE - SOUND FILES - pk3_ranked_rpg\sound\rpg\
=================================================================

Drop real sound files in this folder, replacing these 11 placeholders
with actual .wav files of the same names. Then rebuild the
zzzz_rpgstandalone.pk3 using the build script.

Supported format: 16-bit PCM .wav, 22050 or 44100 Hz, Mono or Stereo
(Quake/MB2 engine standard). Max ~2 seconds recommended for notifs.

Filenames + purpose (matches engine code cl_xp_profile.cpp s_rpgSoundPaths):

 1. multikill_double.wav   -> "DOUBLE KILL!"   (2 kills in 3s chain)
 2. multikill_triple.wav   -> "TRIPLE KILL!"   (3)
 3. multikill_overkill.wav -> "OVERKILL!"      (4)
 4. multikill_monster.wav  -> "MONSTER KILL!"  (5)
 5. multikill_ultra.wav    -> "ULTRA KILL!"    (6 or more)
 6. levelup.wav            -> Level up chime / fanfare
 7. perfect_duel.wav       -> "PERFECT DUEL!"  (0 HP delta + 0 hits taken)
 8. quickdraw.wav          -> "QUICK DRAW!"    (Perfect + <3s duration)
 9. killstreak.wav         -> Kill streak milestone (5/10/25 kills, duel streaks)
10. achievement.wav        -> Milestones (100 kills, 50 duels, max level, endurance)
11. xp_gain.wav            -> Every kill / NPC kill / XP tick subtle blip

TIP: Classic CS / Quake / Unreal Tournament announcer packs work great for
multi-kills (double/triple/overkill/monster/ultra). If you leave any of these
as 0-byte placeholders, the engine will silently skip them - no crash, no sound.

Rebuilding pk3 after updating sounds:
  Run the build script or use Compress-Archive (zip) on the pk3_ranked_rpg\
  folder contents, rename .zip -> zzzz_rpgstandalone.pk3, and drop into
  MBII/GameData/MBII/ folder.

Optional CVars (set in console, saved to q3config.cfg):
  /cg_rpg_notify_sounds 0|1   -> all sounds off/on (default 1 on)
  /cg_rpg_notify_popups 0|1   -> HUD cards off/on (default 1 on)
  /cg_rpg_multikill_enabled   -> multi-kill detection off/on (1)
  /cg_rpg_streak_enabled      -> kill streak detection off/on (1)
=================================================================
