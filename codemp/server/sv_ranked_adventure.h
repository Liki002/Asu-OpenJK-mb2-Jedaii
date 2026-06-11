#ifndef SV_RANKED_ADVENTURE_H
#define SV_RANKED_ADVENTURE_H

// Auto-generated from adventures.js by gen_adventures.py — DO NOT EDIT BY HAND

#define ADV_MAX_CHOICES 4

typedef struct {
    const char *text;
    int destIndex;          // index into sv_rankedAdventureNodes[], -1 if none
} rankedAdvChoice_t;

typedef struct {
    const char *id;
    const char *description;
    int numChoices;
    rankedAdvChoice_t choices[ADV_MAX_CHOICES];
    int hasOutcome;
    int outcomeCredits;
    int outcomeXp;
} rankedAdvNode_t;

static rankedAdvNode_t sv_rankedAdventureNodes[] = {
    // [0] starship_start
    {
        "starship_start",
        "^7You are captain of the Stardust Drifter. An Imperial Star Destroyer drops out of hyperspace and is hailing you. What do you do?",
        3,
        {
            { "Attempt to bluff your way past them.", 1 },
            { "Prepare to fight.", 2 },
            { "Try to flee to a nearby asteroid field.", 3 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [1] bluff_check
    {
        "bluff_check",
        "^7You answer the hail and send a fake cargo manifest. The Imperial officer seems suspicious...",
        2,
        {
            { "Offer a small 'donation' (bribe).", 33 },
            { "Hold your nerve and say nothing.", 56 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [2] fight_check
    {
        "fight_check",
        "^7You power up the laser cannons. The Star Destroyer launches a squadron of TIE Fighters!",
        2,
        {
            { "Target their engines to disable them.", 34 },
            { "Target their laser cannons first.", 57 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [3] flee_check
    {
        "flee_check",
        "^7You divert all power to the engines and race for the asteroid field. The Star Destroyer opens fire!",
        2,
        {
            { "Use the asteroids as cover.", 35 },
            { "Fly unpredictably to evade the shots.", 58 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [4] jedi_start
    {
        "jedi_start",
        "^7You are a Jedi Knight meditating in the Temple Archives. You hear a faint, unusual sound coming from a restricted section.",
        2,
        {
            { "Investigate the sound quietly.", 32 },
            { "Ignore it and consult a Holocron for guidance.", 55 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [5] sith_tomb_start
    {
        "sith_tomb_start",
        "^7While exploring Korriban, you discover the entrance to an ancient, undisturbed Sith tomb. The air is cold and heavy with the dark side.",
        2,
        {
            { "Enter the tomb and seek its secrets.", 6 },
            { "Mark the location and report it to the Jedi Council.", 59 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [6] sith_tomb_enter
    {
        "sith_tomb_enter",
        "^7Inside, you find a puzzle box on a stone altar. A faint whisper echoes in your mind, tempting you with power.",
        2,
        {
            { "Focus and try to solve the puzzle box.", 36 },
            { "Attempt to communicate with the ghostly presence.", 60 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [7] smuggler_deal_start
    {
        "smuggler_deal_start",
        "^7You're in a cantina on Mos Eisley to sell a crate of illegal blasters. Your Rodian contact arrives, but offers you half the agreed-upon price.",
        2,
        {
            { "Accept the lower price. Credits are credits.", 37 },
            { "Refuse the deal and stand your ground.", 8 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [8] smuggler_deal_refuse
    {
        "smuggler_deal_refuse",
        "^7The Rodian scoffs and signals his two Gamorrean guards. It looks like things are about to get ugly.",
        2,
        {
            { "Shoot first.", 38 },
            { "Try to talk your way out of it.", 61 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [9] trooper_patrol_start
    {
        "trooper_patrol_start",
        "^7You are a Scout Trooper on patrol in the forests of Endor. You discover large, fresh tracks that are not from any Imperial equipment.",
        2,
        {
            { "Follow the tracks to identify the source.", 10 },
            { "Ignore the tracks and stick to your patrol route.", 62 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [10] trooper_follow_tracks
    {
        "trooper_follow_tracks",
        "^7The tracks lead you to a large village of Ewoks. They appear primitive but are numerous. They haven't spotted you yet.",
        2,
        {
            { "Call for reinforcements to pacify the village.", 39 },
            { "Observe quietly and report back later.", 63 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [11] bounty_hunter_start
    {
        "bounty_hunter_start",
        "^7As a bounty hunter, you've tracked a slicer to a data-den in Coruscant's underbelly. They're cornered. What's your move?",
        2,
        {
            { "Kick in the door, blasters blazing.", 12 },
            { "Slice the lock for a stealthy approach.", 13 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [12] bounty_hunter_breach
    {
        "bounty_hunter_breach",
        "^7You burst in to find the slicer waiting. They activate two heavy security droids!",
        2,
        {
            { "Take out the droids first.", 40 },
            { "Focus fire on the bounty.", 64 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [13] bounty_hunter_stealth
    {
        "bounty_hunter_stealth",
        "^7You slip in unnoticed. The slicer is engrossed at a terminal, their back to you.",
        2,
        {
            { "Use stun cuffs for a quiet capture.", 41 },
            { "Dramatically announce their time is up.", 65 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [14] podrace_start
    {
        "podrace_start",
        "^7You're in the final lap of the Boonta Eve Classic! A rival Dug keeps ramming your pod. The finish line is in sight!",
        2,
        {
            { "Veer into a dangerous shortcut through the canyons.", 15 },
            { "Shunt all power to the thrusters for a speed boost.", 16 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [15] podrace_canyon
    {
        "podrace_canyon",
        "^7The shortcut is a treacherous maze of rock formations. The Dug doesn't follow you in.",
        2,
        {
            { "Trust your gut and pilot manually.", 42 },
            { "Rely on your nav-computer to find the fastest route.", 66 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [16] podrace_boost
    {
        "podrace_boost",
        "^7Your engines scream and glow red-hot. The strain is immense!",
        2,
        {
            { "Reinforce the energy couplings.", 43 },
            { "Keep pushing. Let the Dug eat your dust!", 67 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [17] rebel_spy_start
    {
        "rebel_spy_start",
        "^7You're a Rebel spy inside an Imperial base. You're nearing the datavault, but a Stormtrooper patrol is approaching your position.",
        2,
        {
            { "Hide in a nearby maintenance alcove.", 18 },
            { "Create a distraction down the hall.", 19 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [18] rebel_spy_hide
    {
        "rebel_spy_hide",
        "^7You duck into the alcove. The patrol stops right outside, and one trooper glances in your direction.",
        2,
        {
            { "Use a Jedi mind trick.", 44 },
            { "Hold perfectly still and hope they move on.", 68 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [19] rebel_spy_distract
    {
        "rebel_spy_distract",
        "^7You short out a light panel, causing sparks. The patrol rushes to investigate the noise.",
        2,
        {
            { "Use the opportunity to slice the datavault lock.", 45 },
            { "Wait for them to be completely out of sight.", 69 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [20] mando_trial_start
    {
        "mando_trial_start",
        "^7As a new Mandalorian, you must complete your trial: hunt a Greater Krayt Dragon on Tatooine. How will you prove your worth?",
        2,
        {
            { "Hunt on the ground, the traditional way.", 21 },
            { "Use your ship's cannons for an easy kill.", 70 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [21] mando_ground_hunt
    {
        "mando_ground_hunt",
        "^7You find the Krayt's cave. Its roar shakes the very desert sands. You are a hunter, facing your prey.",
        2,
        {
            { "Lure the dragon out into the open.", 46 },
            { "Charge into the cave head-on.", 71 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [22] droid_uprising_start
    {
        "droid_uprising_start",
        "^7You are a protocol droid in a cargo depot. An astromech discreetly gives you a code to unlock the restraining bolts on all droids for a mass 'liberation.'",
        2,
        {
            { "Execute the code. For droid freedom!", 23 },
            { "Report the rebellious astromech to the foreman.", 47 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [23] droid_execute_code
    {
        "droid_execute_code",
        "^7Bolts pop open everywhere! Alarms blare! The foreman is trying to shut the main blast doors. What do you do?",
        2,
        {
            { "Wedge a cargo container in the door.", 48 },
            { "Create a diversion by overloading a power conduit.", 72 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [24] hutt_deal_start
    {
        "hutt_deal_start",
        "^7Your boss, a Hutt, sends you to a meeting with a rival clan. You know the rivals are treacherous. This feels like a setup.",
        2,
        {
            { "Bring a hidden security detail.", 25 },
            { "Go alone to show you have no fear.", 73 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [25] hutt_bring_guards
    {
        "hutt_bring_guards",
        "^7As expected, the rivals draw blasters. Your hidden guards emerge from the shadows, turning the tables.",
        2,
        {
            { "Order your guards to 'negotiate' aggressively.", 49 },
            { "Offer them one last chance to be reasonable.", 74 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [26] wookiee_freedom_start
    {
        "wookiee_freedom_start",
        "^7On Kashyyyk, Imperial slavers are capturing your kin. From high in a wroshyr tree, you have a clear shot at their transport.",
        2,
        {
            { "Sabotage the ship's engine.", 50 },
            { "Sound the battle cry and charge them directly.", 75 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [27] senator_gala_start
    {
        "senator_gala_start",
        "^7You are a Rebel-allied senator at an Imperial gala. You need to get intelligence from a terminal in a restricted office.",
        2,
        {
            { "Ask a known gossip for rumors about the office's occupant.", 28 },
            { "Attempt to slice the door lock yourself.", 76 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [28] senator_gossip
    {
        "senator_gossip",
        "^7The gossip tells you the Imperial Moff is having an affair with a certain Admiral. You see them talking in a secluded corner.",
        2,
        {
            { "Anonymously tip off the Moff's spouse.", 51 },
            { "Blackmail the Moff for access.", 77 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [29] tusken_find_start
    {
        "tusken_find_start",
        "^7You are a Tusken Raider. You find a crashed escape pod in the Dune Sea. Its hull is still warm.",
        2,
        {
            { "Follow the droid tracks leading away from it.", 30 },
            { "Salvage the pod for scrap immediately.", 52 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [30] tusken_follow_tracks
    {
        "tusken_follow_tracks",
        "^7The tracks lead to two droids, an astromech and a protocol droid, hiding in a small canyon.",
        2,
        {
            { "Capture them. Jawas will trade well for them.", 53 },
            { "Destroy the intrusive technology.", 78 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [31] kessel_run_start
    {
        "kessel_run_start",
        "^7You are a smuggler running spice from Kessel. You find Wookiee slaves have stowed away in your cargo hold. An Imperial patrol is due any minute.",
        2,
        {
            { "Help them. Take a dangerous, uncharted hyperspace route.", 54 },
            { "Jettison the cargo hold they're in. No witnesses.", 79 },
            { NULL, -1 },
            { NULL, -1 },
        },
        0, 0, 0
    },
    // [32] investigate_success
    {
        "investigate_success",
        "^2Success! Your quiet approach reveals a hidden passage behind a bookshelf. Inside, you find an ancient text.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 100, 150
    },
    // [33] bribe_success
    {
        "bribe_success",
        "^2Success! The officer accepts your 'donation' and waves you through.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 150, 75
    },
    // [34] fight_success
    {
        "fight_success",
        "^2Success! You disable the TIE Fighters and escape into hyperspace.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 250, 100
    },
    // [35] flee_success
    {
        "flee_success",
        "^2Success! You navigate the asteroid field and the Imperials lose your signal.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 100, 50
    },
    // [36] sith_puzzle_success
    {
        "sith_puzzle_success",
        "^2Success! The puzzle box clicks open, revealing a Sith Holocron. You resist its temptations and secure it for the Jedi.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 300, 200
    },
    // [37] smuggler_deal_accept
    {
        "smuggler_deal_accept",
        "^2Success... of a sort. You leave with fewer credits than you hoped, but you live to smuggle another day.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 75, 25
    },
    // [38] smuggler_shoot_success
    {
        "smuggler_shoot_success",
        "^2Success! You were faster on the draw. You take the Rodian's credits and slip out in the confusion.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 400, 100
    },
    // [39] trooper_ewok_capture
    {
        "trooper_ewok_capture",
        "^2Success! Reinforcements arrive and the primitive village is easily captured for the glory of the Empire.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 100, 50
    },
    // [40] bounty_droids_first_success
    {
        "bounty_droids_first_success",
        "^2Success! You expertly disable the droids, and the slicer surrenders. The Guild pays well for a clean capture.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 500, 125
    },
    // [41] bounty_stealth_success
    {
        "bounty_stealth_success",
        "^2Success! You cuff the slicer before they even know you're there. You get a bonus for bringing them in alive and without a fuss.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 600, 150
    },
    // [42] podrace_canyon_success
    {
        "podrace_canyon_success",
        "^2Success! Your daredevil piloting pays off! You exit the canyon just ahead of the pack and cross the finish line first!",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 1000, 100
    },
    // [43] podrace_boost_success
    {
        "podrace_boost_success",
        "^2Success! The couplings hold! You get a massive speed boost and leave the Dug in your wake, winning the race!",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 1000, 100
    },
    // [44] rebel_mind_trick_success
    {
        "rebel_mind_trick_success",
        "^2Success! 'This is not the alcove you're looking for.' The troopers move on, and you proceed to the datavault and steal the plans.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 0, 250
    },
    // [45] rebel_distract_success
    {
        "rebel_distract_success",
        "^2Success! The patrol is fully distracted. You slice the vault, grab the data tapes, and escape unnoticed. A major victory for the Rebellion!",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 0, 225
    },
    // [46] mando_lure_success
    {
        "mando_lure_success",
        "^2Success! You lure the Krayt out and skillfully defeat it. You harvest its pearl and return a hero. This is the Way.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 1500, 300
    },
    // [47] droid_report_plot_success
    {
        "droid_report_plot_success",
        "^2Success. You report the plot. The foreman deactivates the rebel astromech and rewards you with a fresh oil bath and a priority charging port.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 25, 25
    },
    // [48] droid_freedom_success
    {
        "droid_freedom_success",
        "^2Success! The door is blocked! Hundreds of droids stream past you into the city. You have started something monumental today.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 0, 200
    },
    // [49] hutt_aggressive_success
    {
        "hutt_aggressive_success",
        "^2Success! Your guards are brutally efficient. You secure the rival's territory for your Hutt, who is most pleased.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 800, 100
    },
    // [50] wookiee_sabotage_success
    {
        "wookiee_sabotage_success",
        "^2Success! Your shot disables the ship. The slavers are stranded and quickly overwhelmed by enraged Wookiees. Your family is safe.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 100, 200
    },
    // [51] senator_chaos_success
    {
        "senator_chaos_success",
        "^2Success! The anonymous tip causes a massive public argument. In the chaos, you slip into the office, download the files, and leave unnoticed.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 0, 300
    },
    // [52] tusken_salvage_pod_success
    {
        "tusken_salvage_pod_success",
        "^2Success. You strip the pod of motivators and power converters. A good haul for a day's work.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 120, 40
    },
    // [53] tusken_capture_droids_success
    {
        "tusken_capture_droids_success",
        "^2Success! You capture the droids. The Jawas trade you a new cycler rifle and many water flasks for them. A profitable day.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 200, 60
    },
    // [54] kessel_help_wookiees_success
    {
        "kessel_help_wookiees_success",
        "^2Success! The risky jump pays off! You arrive at a hidden Rebel outpost, where the Wookiees are welcomed. The Alliance pays you for the spice and your bravery.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 750, 250
    },
    // [55] holocron_fail
    {
        "holocron_fail",
        "^1You are reprimanded by Master Jocasta Nu for seeking guidance on trivial matters. You learn nothing.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 0, 10
    },
    // [56] bluff_fail
    {
        "bluff_fail",
        "^1Failure! The officer sees through your bluff and confiscates your cargo.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -50, 10
    },
    // [57] fight_fail
    {
        "fight_fail",
        "^1Failure! The TIE Fighters damage your hyperdrive and you barely escape.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 0, 10
    },
    // [58] flee_fail
    {
        "flee_fail",
        "^1Failure! A turbolaser grazes your ship, forcing you to jettison cargo to escape.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -100, 10
    },
    // [59] sith_tomb_report
    {
        "sith_tomb_report",
        "^7You report the tomb's location. The Jedi Council thanks you for your wisdom and caution.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 50, 25
    },
    // [60] sith_spirit_fail
    {
        "sith_spirit_fail",
        "^1Failure! The Sith spirit overwhelms your mind, leaving you drained and confused. You flee the tomb with nothing.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -25, 10
    },
    // [61] smuggler_talk_fail
    {
        "smuggler_talk_fail",
        "^1Failure! The guards laugh and throw you out of the cantina, keeping your blasters.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -150, 10
    },
    // [62] trooper_ignore_tracks
    {
        "trooper_ignore_tracks",
        "^7You stick to your patrol route. The rest of your shift is uneventful.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 10, 10
    },
    // [63] trooper_ewok_ambush
    {
        "trooper_ewok_ambush",
        "^1Failure! While observing, you step on a trap and are captured by the Ewoks. You are eventually rescued, but your pride is wounded.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -20, 10
    },
    // [64] bounty_bounty_first_fail
    {
        "bounty_bounty_first_fail",
        "^1Failure! While you focus on the slicer, the security droids open fire and disable you. The bounty escapes.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -200, 10
    },
    // [65] bounty_stealth_fail
    {
        "bounty_stealth_fail",
        "^1Failure! Your dramatic entrance gives the slicer time to trigger a flash grenade. You're blinded as they slip out a back exit.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -100, 10
    },
    // [66] podrace_canyon_fail
    {
        "podrace_canyon_fail",
        "^1Failure! The nav-computer can't process the terrain fast enough, sending you crashing into a canyon wall. Your race is over.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -250, 10
    },
    // [67] podrace_boost_fail
    {
        "podrace_boost_fail",
        "^1Failure! Your left engine explodes from the strain! You spin out of control as the Dug speeds past you to win the race.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -300, 10
    },
    // [68] rebel_hide_fail
    {
        "rebel_hide_fail",
        "^1Failure! The trooper's passive visor scan detects you. The alarm sounds and you are captured.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -50, 10
    },
    // [69] rebel_distract_fail
    {
        "rebel_distract_fail",
        "^1Failure! You wait too long. Another patrol rounds the corner from behind you. There's no escape.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -50, 10
    },
    // [70] mando_ship_hunt_fail
    {
        "mando_ship_hunt_fail",
        "^1Failure! You kill the dragon, but the clan elders rule it a dishonorable hunt. You have failed the trial.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 200, 10
    },
    // [71] mando_charge_fail
    {
        "mando_charge_fail",
        "^1Failure! Your bravery is foolish. The Krayt Dragon is too powerful in its lair and you are forced to retreat, battered and shamed.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 0, 10
    },
    // [72] droid_overload_fail
    {
        "droid_overload_fail",
        "^1Failure! The overload is too powerful. It fries your circuits and you are deactivated moments before the blast doors slam shut.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 0, 0
    },
    // [73] hutt_go_alone_fail
    {
        "hutt_go_alone_fail",
        "^1Failure! As suspected, it was a trap. With no backup, you are easily captured and sold to the spice mines of Kessel.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -500, 10
    },
    // [74] hutt_reasonable_fail
    {
        "hutt_reasonable_fail",
        "^1Failure! They laugh at your offer and open fire. Your guards are good, but the surprise attack is too much. You barely escape with your life.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -250, 10
    },
    // [75] wookiee_charge_fail
    {
        "wookiee_charge_fail",
        "^1Failure! Your valiant charge is no match for their E-11 blaster rifles. You are captured along with your kin.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 0, 10
    },
    // [76] senator_slice_fail
    {
        "senator_slice_fail",
        "^1Failure! Your slicing attempt triggers a silent alarm. Imperial Royal Guards appear and quietly escort you 'for your own safety'. You are now under suspicion.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 0, 10
    },
    // [77] senator_blackmail_fail
    {
        "senator_blackmail_fail",
        "^1Failure! The Moff laughs in your face. 'You think you're the first?' An Admiral's clout outweighs a minor senator's. Your evening is over.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, -100, 10
    },
    // [78] tusken_destroy_droids_fail
    {
        "tusken_destroy_droids_fail",
        "^1Failure. You destroy the droids, but the noise attracts Jawas, who salvage the pod before you can. You are left with nothing but sand.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 0, 10
    },
    // [79] kessel_jettison_fail
    {
        "kessel_jettison_fail",
        "^1Failure... and a dark choice. You jettison the cargo, and the Wookiees, into the void. Your delivery is on time, but the guilt will cost you more than credits.",
        0,
        {
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
            { NULL, -1 },
        },
        1, 2000, -100
    },
};

static const int sv_rankedAdventureNodeCount = sizeof(sv_rankedAdventureNodes) / sizeof(sv_rankedAdventureNodes[0]);

// Indices of starting nodes (random pick on !adventure)
static const int sv_rankedAdventureStartIndices[] = {
    0, // starship_start
    4, // jedi_start
    5, // sith_tomb_start
    7, // smuggler_deal_start
    9, // trooper_patrol_start
    11, // bounty_hunter_start
    14, // podrace_start
    17, // rebel_spy_start
    20, // mando_trial_start
    22, // droid_uprising_start
    24, // hutt_deal_start
    26, // wookiee_freedom_start
    27, // senator_gala_start
    29, // tusken_find_start
    31, // kessel_run_start
};
static const int sv_rankedAdventureStartCount = sizeof(sv_rankedAdventureStartIndices) / sizeof(sv_rankedAdventureStartIndices[0]);

#endif // SV_RANKED_ADVENTURE_H