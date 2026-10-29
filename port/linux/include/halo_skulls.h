/*
HALO_SKULLS.H

The campaign's skulls, as the Master Chief Collection's Halo: Combat Evolved
has them (port/linux/game/skulls.c). The game's sources ask skull_active;
the menus write game.skulls_active, and a co-op client takes the host's.
*/

#ifndef __HALO_SKULLS_H
#define __HALO_SKULLS_H

/* in the order of the skulls screen (tools/port_settings.py's SKULLS) */
enum skull
{
	_skull_anger = 0,
	_skull_armistice,
	_skull_assassins,
	_skull_bandanna,
	_skull_black_eye,
	_skull_blind,
	_skull_boom,
	_skull_catch,
	_skull_confetti_party,
	_skull_efficient,
	_skull_envy,
	_skull_eye_patch,
	_skull_famine,
	_skull_floor_is_lava,
	_skull_fog,
	_skull_foreign,
	_skull_ghost,
	_skull_give_and_take,
	_skull_grunt_birthday_party,
	_skull_grunt_funeral,
	_skull_iron,
	_skull_johnny_ammo,
	_skull_leadhead,
	_skull_m_rating,
	_skull_malfunction,
	_skull_mythic,
	_skull_need_glasses,
	_skull_outbreak,
	_skull_pinata,
	_skull_protected,
	_skull_recession,
	_skull_rip_and_tear,
	_skull_scarab,
	_skull_sputnik,
	_skull_streaking,
	_skull_swarm,
	_skull_thats_just_wrong,
	_skull_thunderstorm,
	_skull_tough_luck,
	_skull_woe_is_me,
	NUMBER_OF_SKULLS
};

struct skull_description
{
	char const *key;
	char const *label;
	char const *description;
};

extern struct skull_description const skull_descriptions[NUMBER_OF_SKULLS];

/* a set of skulls: a bit each, in words of 32 (in the saved games' and the
advertisement's bytes, little-endian, skull n bit n). As many as
SKULLS_MAXIMUM; more wants it raised (and the advertisement's bytes:
halo_port_limits.h's HALO_PORT_ADVERTISED_SKULLS_SIZE) */
#define SKULLS_MAXIMUM 64
#define SKULL_SET_WORDS ((SKULLS_MAXIMUM + 31) / 32)

struct skull_set
{
	unsigned long words[SKULL_SET_WORDS];
};

int skull_set_has(struct skull_set const *set, int skull);
void skull_set_add(struct skull_set *set, int skull);
int skull_set_is_empty(struct skull_set const *set);
struct skull_set skull_set_none(void);
struct skull_set skull_set_of(int skull);
/* every skull there is */
struct skull_set skull_set_all(void);
struct skull_set skull_set_union(struct skull_set a, struct skull_set b);
struct skull_set skull_set_intersection(struct skull_set a, struct skull_set b);
/* those of a not in b */
struct skull_set skull_set_difference(struct skull_set a, struct skull_set b);

/* whether the skull is on in the campaign game being played */
int skull_active(int skull);

/* how the skulls are had (game.skulls_unlock_mode): every one, or those
found playing, or not asked yet; and the set of those had */
enum
{
	_skulls_unlock_unset = 0,
	_skulls_unlock_all,
	_skulls_unlock_play
};
int skulls_unlock_mode(void);
struct skull_set skulls_unlocked_set(void);
/* whether the locked skulls are carried by enemies to be found: unlocking
by playing, without Settings > Gameplay's ALL SKULLS */
int skulls_drop(void);

/* game.skulls_active's words, to a set of the skulls and back; the text's
room (port_config.c writes values of up to 1024 characters) */
#define SKULLS_TEXT_LENGTH 1025
struct skull_set skulls_set_from_text(char const *text);
void skulls_text_from_set(struct skull_set set, char *text, unsigned int size);

/* the skulls' part of the game state (the skulls dropped and found in the
level, which a checkpoint keeps), made with the game's; and made new with
each map, and each start again of the level (game.c) */
void skulls_initialize(void);
void skulls_initialize_for_new_map(void);
/* the game state gone back to a checkpoint (or to save and quit): the
skulls found and not kept are lost (game_state.c) */
void skulls_game_state_reverted(void);
/* a saved game loaded (Continue): its lost skulls the level's (game_state.c) */
void skulls_game_state_loaded(void);
/* the skulls on in the level, their icons at the bottom right of the local
player's screen (hud.c) */
void skulls_hud_draw(short local_player_index);

/* a level loaded: the skulls it is played with, taken now (the host's for
a client), and Grunt Birthday Party's sound added to its tags; the level
going (before the randomizer's tags go, which were added before it) */
void skulls_level_loaded(long scenario_tag_index);
void skulls_level_unloaded(void);
/* the level won: the skulls found in it kept (main.c's main_won_map_private) */
void skulls_level_won(void);

/* the sound cache's samples of the birthday party's sound, from memory
(xbox_sound_cache.c); FALSE for any other sound */
struct sound_permutation;
unsigned char skulls_sound_read(struct sound_permutation const *permutation, void *buffer);

/* the advertisement's reserved bytes: the skulls flag and set, when the
host plays a campaign game with skulls, and the drops flag when its enemies
drop skulls to be found; the client's copy of them (the advertisement's
flags, and its skulls' bytes) */
void skulls_advertisement(unsigned char *reserved);
void skulls_set_remote(unsigned char flags, unsigned char const *bytes);
/* ... a client's, as it was set (a replay a client records keeps it:
network_replay.c): the flags' skulls bits and the skulls' bytes
(HALO_PORT_ADVERTISED_SKULLS_SIZE) */
void skulls_remote_get(unsigned char *flags, unsigned char *bytes);
/* the level's tags every machine of a co-op game has alike, as it loaded:
their count and fingerprint (a replay's playback compares them with the
recording's) */
void skulls_shared_tags(long *count, unsigned long *hash);
/* whether the game this machine hosts is a campaign game with skulls, or
whose enemies drop skulls (one skulls_advertisement marks) */
int skulls_hosted(void);
/* whether an advertisement's skulls take tags from the skulls' pack: the skull
the enemies drop, Outbreak's Flood, Scarab's projectiles */
int skulls_advertised_need_pack(unsigned char flags, unsigned char const *bytes);

/* a co-op game's skulls to be found (the host's, kept the same on its
clients): the state sent to a client that is ready (NONE: to every
client, when it changed), and the host's message handled on a client
(network_distributed.c) */
void skulls_send_state(long machine_index);
int skulls_handles_message(unsigned short const *message, unsigned short size);
void skulls_handle_message(long machine_index, unsigned short const *message, unsigned short size);
/* a co-op host's Outbreak bursts, which its clients play
(skulls_outbreak.c, network_distributed.c) */
int skulls_outbreak_handles_message(unsigned short const *message, unsigned short size);
/* a modded co-op game's level tags (their count and fingerprint), the
host's sent to a client that is ready, which checks its own against them
(skulls.c, network_distributed.c) */
void skulls_send_tags(long machine_index);
int skulls_tags_handles_message(unsigned short const *message, unsigned short size);
void skulls_tags_handle_message(long machine_index, unsigned short const *message, unsigned short size);
void skulls_outbreak_handle_message(long machine_index, unsigned short const *message, unsigned short size);

/* whether the unit (or an object's team) is the players' enemy */
int skulls_unit_is_enemy(long unit_index);
int skulls_team_is_enemy(short team_index);
/* whether the unit is a grunt, by its tag's name (randomizer.c's classes) */
int skulls_unit_is_grunt(long unit_index);
/* whether the weapon tag is a Covenant weapon (randomizer.c's) */
int skulls_weapon_is_covenant(long weapon_definition_index);

/* whether the object is a player's unit */
int skulls_unit_is_player(long object_index);

/* a melee hit on the victim, which the host (or a game of one machine)
deals: Pinata's grenades and Black Eye's shield */
void skulls_melee_damage(long victim_index, long attacker_object_index, long attacker_player_index);

/* Boom: what an explosion's radii are multiplied by */
float skulls_boom_scale(void);

/* a unit's killing blow, by the player (NONE: another's), and its death
(Grunt Funeral's); on every machine, the effects' damage the host's. The
blow: Grunt Birthday Party's celebration of a headshot, and a skull to be
found dropped by an enemy */
void skulls_unit_killed(long unit_index, int headshot, long killer_player_index);
/* a blow to a unit, before its aftermath (and its killing blow's
skulls_unit_killed): the kind it was, and whether it killed. A kill's
odds of dropping a skull: a headshot, a punch from behind, an instant kill
(none before it), a group's (one blow, several), a vehicle's. (The
direction is a real_vector3d: this header comes before real_math.h) */
void skulls_unit_blow(long unit_index, long definition_index, long owner_object_index,
	void const *direction, int melee, int vehicle, int lethal);
void skulls_unit_died(long unit_index);

/* the skull the enemies carry, to be found (unlocking by playing): a
multiplayer map's oddball, copied into the level as it loads
(skulls_tag_import, from watanoge_skulls.pack). Whether a level loading
wants it: the skulls on, unlocking by playing, a campaign game (a co-op
client's: the host's drops, by its advertisement) */
#define SKULLS_CARRIED_TAG "weapons\\ball\\ball"
int skulls_carried_wanted(void);
/* each tick of the game: the skulls lying about, heard */
void skulls_tick(void);

/* Outbreak (port/linux/game/skulls_outbreak.c): a player's kill may let
loose Flood spores that run to the bodies about and raise them as Flood.
The Flood's tags come from Two Betrayals (c40), copied into the level as it
loads (skulls_tag_import, from watanoge_skulls.pack): the roots below, and what they need.
Whether a level loading wants them: Outbreak on, or to be found (in co-op,
by the host's advertisement alone, so every machine copies the same) */
#define SKULLS_FLOOD_DONOR "c40"
#define SKULLS_OUTBREAK_SPORE_TAG "characters\\flood_infection\\flood_infection"
#define SKULLS_OUTBREAK_CARRIER_TAG "characters\\floodcarrier\\floodcarrier"
#define SKULLS_OUTBREAK_BURST_TAG "characters\\floodcarrier\\effects\\body destroyed"
#define SKULLS_OUTBREAK_ELITE_TAGS \
	"characters\\floodcombat elite\\floodcombat elite unarmed", \
	"characters\\floodcombat elite\\floodcombat elite plasma rifle", \
	"characters\\floodcombat elite\\floodcombat elite needler"
#define SKULLS_OUTBREAK_HUMAN_TAGS \
	"characters\\floodcombat_human\\floodcombat_human unarmed", \
	"characters\\floodcombat_human\\floodcombat_human assault rifle", \
	"characters\\floodcombat_human\\floodcombat_human pistol"
int skulls_outbreak_wanted(void);
/* the skulls' tag pack (port/assets/tag_packs/watanoge_skulls.toml), and
the map it has the skull the enemies carry from (and Scarab's rocket) */
#define SKULLS_PACK "watanoge_skulls"
#define SKULLS_CARRIED_DONOR "wizard"
/* whether its pack is wanted: skulls to be found, or Outbreak's Flood
(game.skulls_active's, the skulls on) */
int skulls_pack_wanted(void);
/* the tags of the skulls on (carried: the skull the enemies carry to be
found; Outbreak's Flood; Scarab's projectiles) added to the level's import
(randomizer_tags_loaded: tag_import.c) */
void skulls_tag_import(int carried, int outbreak, int scarab);
/* the Outbreak's part of the game state (its spores and the Flood they
raised), made with the game's, and new with each map */
void skulls_outbreak_initialize(void);
void skulls_outbreak_initialize_for_new_map(void);
/* the level's: its Flood's tags found (skulls_level_loaded) */
void skulls_outbreak_level_loaded(void);
/* a unit killed by a player (skulls_unit_killed), and each tick */
void skulls_outbreak_kill(long unit_index, long killer_player_index);
void skulls_outbreak_tick(void);
/* whether the unit is one of the Flood a spore raised: it carries no skull,
lets loose no spores, and its death is not counted in the skulls' odds */
int skulls_outbreak_raised(long unit_index);
/* while set, an effect made makes no sound, or no damage; and its parts and
particles are of this size (1, else): the burst a raised Flood appears in,
its particles alone, and a killed enemy's as its spores go loose, half the
size, harmless (effects.c) */
extern int skulls_effect_quiet;
extern int skulls_effect_harmless;
extern float skulls_effect_size;
/* while set, an effect made makes no objects but garbage, which every
machine keeps for itself (Floodfection's burst and bits, played by each
machine of a multiplayer game for itself: infection.c) */
extern int skulls_effect_no_objects;
/* Floodfection's bits, thrown where a zombie dies (Two Betrayals', copied in
from watanoge_gamemodes.pack with the burst: infection_tag_import) */
#define FLOODFECTION_BITS_TAGS \
	"effects\\flood bit limb explode", \
	"effects\\flood bit h-head explode", \
	"effects\\flood bit skin explode"
/* a skull to be found lying about: its name ("IRON SKULL"), else NULL; and
it taken by a player's action button: on for the level, to be kept once
the level is finished without dying. FALSE for another item. (The name is
a wchar_t string, of the 16-bit wchar_t every build has: this header comes
before <stddef.h>) */
unsigned short const *skulls_item_name(long object_index);
/* (damage.c's object_damage_aftermath) how far a blast throws the item: a
skull still falling from its enemy a fifth as far (one landed, not at all:
its item's does-not-accelerate flag), any other item as far as ever */
float skulls_item_push_scale(long object_index);
int skulls_item_take(short local_player_index, long unit_index, long object_index);

/* Envy: a player's unit's flashlight button pressed: TRUE when it is the
skull's active camo instead (turned on or off, as the flashlight is); and
each tick, its charge (the flashlight's battery) used or filled, TRUE when
the skull has the battery, not the light (units.c); Swarm: whether an
object's definition is a hunter's */
int skulls_envy(long unit_index);
int skulls_envy_update(long unit_index);
int skulls_object_is_hunter(long definition_index);

/* Give & Take: a player's unit fired a shot (a miss, unless an enemy is hit
soon); damage of a player's dealt to an object (a hit, if an enemy) */
void skulls_shot_fired(long unit_index);
void skulls_damage_dealt(long object_index, long owner_player_index);
/* Protected: an enemy's body without a shield given one; a hit on such a
shield (broken by it, or not), shown as the model cannot show it */
void skulls_protect(long unit_index);
void skulls_given_shield_hit(long object_index, int broken);

/* Famine and Efficient: the weapon an actor drops, with half its
ammunition, or twice it */
void skulls_famine_weapon(long weapon_index);

/* ---------- the port's own skulls (port/linux/game/skulls_custom.c) */

/* Woe Is Me and Armistice: a pair of teams' enmity as the skulls have it:
TRUE, enemies, FALSE, not, NONE (-1), as the game has it; and their
alliance (game_allegiance.c). Taken each tick (skulls_custom_tick) */
int skulls_team_enmity(short our_team_index, short other_team_index);
int skulls_team_alliance(short our_team_index, short other_team_index);
/* M Rating: how many more of a particle the effects make (1, else) */
float skulls_particle_count_scale(long particle_definition_index);
/* Need Glasses?: whether a unit is cloaked by the skull (Assassins' too) */
int skulls_unit_cloaked(long unit_index);
/* Scarab: the projectile a unit's shot of the weapon fires instead, a
player's: the rocket of a human weapon, the fuel rod of a Covenant one;
NONE, its own (weapons.c) */
long skulls_scarab_projectile(long weapon_definition_index, long unit_index);
/* ... the projectile the weapon's shots are swapped for, whoever fires it
(NONE: Scarab off, or not in the level): the damage a co-op host takes from
a client's reports of them (network_damage.c) */
long skulls_scarab_weapon_projectile(long weapon_definition_index);
/* Rip & Tear: whether the unit is a player's that can only punch (its
triggers and grenades held: units.c), and the shield a unit recharges to
(1, a player's 3: damage.c); a player's punch's damage, doubled */
int skulls_rip_and_tear(long unit_index);
float skulls_shield_cap(long object_index);
/* Floor Is Lava: a biped's footing, each tick it moves: airborne, or on the
object (NONE: the level's floor) (bipeds.c) */
void skulls_biped_footing(long biped_index, int airborne, long support_object_index);
/* Johnny Ammo: whether a reload of a magazine throws the rounds left in it
away: a player's weapon (its owner's), a magazine reloaded whole, not round
by round (weapons.c) */
int skulls_johnny_ammo(long owner_object_index, int reloads_whole);
/* the skulls' each tick (skulls_tick), the level's (skulls_level_loaded) */
void skulls_custom_tick(void);
void skulls_custom_level_loaded(void);
/* Scarab's projectiles: the rocket (Wizard's) and the fuel rod (c40's,
SKULLS_FLOOD_DONOR's), copied into the level as it loads; whether a level
loading wants them (Scarab on, or to be found; in co-op, by the host's
advertisement alone) */
#define SKULLS_SCARAB_ROCKET_TAG "weapons\\rocket launcher\\rocket"
#define SKULLS_SCARAB_FUEL_ROD_TAG "weapons\\fuel rod gun\\fuel rod"
int skulls_scarab_wanted(void);

/* Malfunction: whether the local player's HUD element is broken. The
elements are rolled again each time the player has a new body */
enum
{
	_skull_malfunction_shield = 0,
	_skull_malfunction_health,
	_skull_malfunction_motion_sensor,
	_skull_malfunction_crosshair,
	_skull_malfunction_weapon,
	_skull_malfunction_grenades,
	NUMBER_OF_SKULL_MALFUNCTIONS
};
int skulls_hud_broken(short local_player_index, long unit_index, int element);

#endif /* __HALO_SKULLS_H */
