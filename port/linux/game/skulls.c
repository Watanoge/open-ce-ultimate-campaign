/*
SKULLS.C

The campaign's skulls, as the Master Chief Collection's Halo: Combat Evolved
has them: each makes the campaign harder or stranger, and the game's sources
ask skull_active where it changes what they do (each marked "port: skull").

The skulls are taken when a level loads: game.skulls_active's, while
game.skulls is on; a co-op client's are those the host advertised, not its
own. Only a campaign game has them (a solo scenario, no multiplayer game
engine). The menus' screen (menu_functions.c) writes game.skulls_active
after a level's difficulty.

Only the host runs the AI and deals damage (port/linux/NETCODE.md), so the
skulls that change those take effect there; those of the HUD, the camera
and the pickups take effect on each machine, which the advertised mask
makes the same.

In co-op the host's enemies drop the host's locked skulls to be found, as
in its own game (a modded game: halo_port_limits.h). Every machine copies
in the skull they carry; the host decides the drops and the pickups, and
tells its clients of the skulls dropped, taken, unlocked and lost
(_distributed_message_skulls_state), which each machine plays and keeps by
the host's rules (INSTANT UNLOCK, LOSE ON DEATH). A skull a player already
has is not kept again.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/errors.h"
#include "effects/effect_definitions.h"
#include "effects/effects.h"
#include "effects/particles.h"
#include "cutscene/cinematics.h"
#include "interface/hud_messaging.h"
#include "interface/ui_widget.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_group_lookup.h"
#include "render/render.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "saved games/game_state.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "math/real_math.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "sound/game_sound.h"
#include "sound/sound_definitions.h"
#include "sound/sound_manager.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "units/units.h"
#include "game/players.h"

#include "halo_port_limits.h"
#include "custom_edition_cache.h"
#include "randomizer.h"
#include "network_distributed.h"

void platform_log(char const *format, ...);

extern int config_boolean(char const *name);
extern long config_integer(char const *name);
extern char const *config_string(char const *name);
/* cseries_windows.h's */
unsigned long system_milliseconds(void);

/* the Master Chief Collection's skulls (Halopedia's MCC:Skulls), their help
on the skulls screen's help line, which wraps it (88 characters at most) */
struct skull_description const skull_descriptions[NUMBER_OF_SKULLS] =
{
	{ "anger", "ANGER", "Enemies fire faster." },
	{ "armistice", "ARMISTICE", "All enemies stop fighting each other." },
	{ "assassins", "ASSASSINS", "All enemies are permanently cloaked." },
	{ "bandanna", "BANDANNA", "Infinite ammo and grenades, never overheat." },
	{ "black_eye", "BLACK EYE", "Your shield only recharges when you punch an enemy." },
	{ "blind", "BLIND", "No HUD and no weapon on screen." },
	{ "boom", "BOOM", "Explosions reach twice as far." },
	{ "catch", "CATCH", "Enemies throw grenades more often. Also drop more." },
	{ "confetti_party", "CONFETTI PARTY", "Every enemy killed by a headshot goes out with a celebration." },
	{ "efficient", "EFFICIENT", "Enemies drop their weapons with twice the ammo." },
	{ "envy", "ENVY", "Your flashlight is active camo instead." },
	{ "eye_patch", "EYE PATCH", "No aim assist: no magnetism, no red reticle." },
	{ "famine", "FAMINE", "Weapons dropped by enemies have half\ntheir ammo." },
	{ "floor_is_lava", "FLOOR IS LAVA", "Touching the floor burns you. Stand on anything else." },
	{ "fog", "FOG", "No motion tracker." },
	{ "foreign", "FOREIGN", "You can't pick up Covenant weapons." },
	{ "ghost", "GHOST", "Enemies don't flinch when you hit them." },
	{ "give_and_take", "GIVE & TAKE", "Shooting damages the player, dealing damage recharges it." },
	{ "grunt_birthday_party", "GRUNT BIRTHDAY", "A grunt killed by a headshot goes out with a celebration." },
	{ "grunt_funeral", "GRUNT FUNERAL", "Dying grunts blow up like plasma grenades." },
	{ "iron", "IRON", "Dying reverts to checkpoint. Single player restarts the level." },
	{ "johnny_ammo", "JOHNNY AMMO", "Reloading throws away the rounds left in the magazine." },
	{ "leadhead", "LEADHEAD", "No headshots: a hit to the head is like any other." },
	{ "m_rating", "M RATING", "Enemies bleed much, much more." },
	{ "malfunction", "MALFUNCTION", "Each time you spawn, one part of your HUD stops working." },
	{ "mythic", "MYTHIC", "Enemies have twice the health and shields." },
	{ "need_glasses", "NEED GLASSES?", "Everyone is cloaked: enemies, allies and vehicles." },
	{ "outbreak", "OUTBREAK", "Kills can release flood spores that infect nearby bodies." },
	{ "pinata", "PINATA", "Punching an enemy knocks a grenade loose." },
	{ "protected", "PROTECTED", "All enemies have shields." },
	{ "recession", "RECESSION", "Every shot uses twice the ammo." },
	{ "rip_and_tear", "RIP & TEAR", "Overshields, melee only with double damage." },
	{ "scarab", "SCARAB", "Human weapons fire rockets, Covenant fuel rods." },
	{ "sputnik", "SPUTNIK", "Hits and blasts throw bodies, vehicles and weapons 3x as far." },
	{ "streaking", "STREAKING", "Your shields drain over time. Kill to recharge." },
	{ "swarm", "SWARM", "Hunters are much stronger." },
	{ "thats_just_wrong", "JUST... WRONG", "Enemies see and hear you from twice as far away." },
	{ "thunderstorm", "THUNDERSTORM", "Enemies are promoted." },
	{ "tough_luck", "TOUGH LUCK", "Enemies always go berserk, dive from grenades, and never flee." },
	{ "woe_is_me", "WOE IS ME", "All AI is against you. You have no allies." },
};

#define SKULLS_LOCAL_PLAYERS 4

static struct skull_set level_set;
static int remote_on;
static struct skull_set remote_set;
/* a co-op client's: the host's enemies drop skulls (the advertisement's
drops flag); the host's rules and its count of those left to find, as
its state last said (skulls_handle_message) */
static int remote_drops;
static int remote_instant;
static int remote_lose_on_death;
static short remote_left;
/* a co-op host's: its skulls' state sent since the level loaded
(skulls_send_state); a client's: the host's heard since (the first only
taken, not played: what was taken and unlocked before is not this
machine's) */
static boolean skulls_sent_state_valid;
static boolean skulls_state_heard;
/* the level's tags every machine of a co-op game has alike (the map's, the
randomizer's and the skulls' copies: before this machine's own menus' and
mods'), as the level loaded: their count and fingerprint
(skulls_send_tags) */
static long shared_tag_count;
static unsigned long shared_tag_hash;

/* ---------- sets of skulls */

/* (the skull set's bytes, and NUMBER_OF_SKULLS within it) */
typedef char skull_set_holds_the_skulls[NUMBER_OF_SKULLS <= SKULLS_MAXIMUM ? 1 : -1];
typedef char skull_set_fits_the_advertisement[
	SKULL_SET_WORDS * 4 <= HALO_PORT_ADVERTISED_SKULLS_SIZE ? 1 : -1];

int skull_set_has(struct skull_set const *set, int skull)
{
	return skull >= 0 && skull < NUMBER_OF_SKULLS && ((set->words[skull >> 5] >> (skull & 31)) & 1) != 0;
}

void skull_set_add(struct skull_set *set, int skull)
{
	if (skull >= 0 && skull < NUMBER_OF_SKULLS)
		set->words[skull >> 5] |= 1UL << (skull & 31);
}

int skull_set_is_empty(struct skull_set const *set)
{
	short word;

	for (word = 0; word < SKULL_SET_WORDS; word++)
	{
		if (set->words[word])
			return 0;
	}
	return 1;
}

struct skull_set skull_set_none(void)
{
	struct skull_set set;

	csmemset(&set, 0, sizeof(set));
	return set;
}

struct skull_set skull_set_of(int skull)
{
	struct skull_set set = skull_set_none();

	skull_set_add(&set, skull);
	return set;
}

struct skull_set skull_set_all(void)
{
	struct skull_set set = skull_set_none();
	int skull;

	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
		skull_set_add(&set, skull);
	return set;
}

struct skull_set skull_set_union(struct skull_set a, struct skull_set b)
{
	short word;

	for (word = 0; word < SKULL_SET_WORDS; word++)
		a.words[word] |= b.words[word];
	return a;
}

struct skull_set skull_set_intersection(struct skull_set a, struct skull_set b)
{
	short word;

	for (word = 0; word < SKULL_SET_WORDS; word++)
		a.words[word] &= b.words[word];
	return a;
}

struct skull_set skull_set_difference(struct skull_set a, struct skull_set b)
{
	short word;

	for (word = 0; word < SKULL_SET_WORDS; word++)
		a.words[word] &= ~b.words[word];
	return a;
}

/* Malfunction's broken element, for each local player's body, and the
game time it was last looked at: a checkpoint's revert, or the level
started again, brings back the same body (the same unit index) at an
earlier time, which rolls again too */
static struct
{
	long unit_index;
	long game_time;
	int element;
} malfunction[SKULLS_LOCAL_PLAYERS] = { { NONE, NONE, NONE }, { NONE, NONE, NONE }, { NONE, NONE, NONE },
	{ NONE, NONE, NONE } };
static unsigned long malfunction_roll;

static void malfunction_reset(void)
{
	int index;

	/* (the element kept, so the next body's is another) */
	for (index = 0; index < SKULLS_LOCAL_PLAYERS; index++)
	{
		malfunction[index].unit_index = NONE;
		malfunction[index].game_time = NONE;
	}
}

/* an element, not the one broken before, nor the motion sensor while Fog
has it off already: from the clock, as the game's own local seed starts the
same each time a level loads */
static int malfunction_new_element(int previous)
{
	int choices[NUMBER_OF_SKULL_MALFUNCTIONS];
	int count = 0;
	int element;

	for (element = 0; element < NUMBER_OF_SKULL_MALFUNCTIONS; element++)
	{
		if (element == _skull_malfunction_motion_sensor && skull_active(_skull_fog))
			continue;
		if (element == previous)
			continue;
		choices[count++] = element;
	}
	malfunction_roll = malfunction_roll * 1664525UL + 1013904223UL + system_milliseconds();
	return choices[(malfunction_roll >> 16) % (unsigned long)count];
}

struct skull_set skulls_set_from_text(char const *text)
{
	struct skull_set set = skull_set_none();
	char word[32];
	unsigned int length = 0;

	for (;; text++)
	{
		char character = text ? *text : 0;

		if (character == ',' || character == ' ' || character == 0)
		{
			if (length)
			{
				int skull;

				word[length] = 0;
				for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
				{
					if (!_stricmp(word, skull_descriptions[skull].key))
						skull_set_add(&set, skull);
				}
			}
			length = 0;
			if (!character)
				break;
			continue;
		}
		if (length + 1 < sizeof(word))
			word[length++] = character;
	}

	return set;
}

void skulls_text_from_set(struct skull_set set, char *text, unsigned int size)
{
	unsigned int length = 0;
	int skull;

	if (!size)
		return;
	text[0] = 0;
	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		if (!skull_set_has(&set, skull))
			continue;
		length += (unsigned int)snprintf(text + length, size - length, "%s%s", length ? "," : "",
			skull_descriptions[skull].key);
		if (length >= size)
		{
			text[size - 1] = 0;
			return;
		}
	}
}

extern int config_write(char const *name, char const *value);

/* the question answered: unlocking by playing, ALL SKULLS on or off (the
answer "all" ALL SKULLS on, and so is a setting of "all" from before,
the first time it is read, so that ALL SKULLS can turn it off later) */
int skulls_unlock_mode(void)
{
	char const *mode = config_string("game.skulls_unlock_mode");

	if (mode && !_stricmp(mode, "all"))
	{
		if (config_write("game.skulls_unlock_mode", "play") && config_write("game.skulls_all_unlocked", "true"))
			error(_error_silent, "skulls: game.skulls_unlock_mode \"all\" is now \"play\", with ALL SKULLS on");
		return _skulls_unlock_play;
	}
	if (mode && !_stricmp(mode, "play"))
		return _skulls_unlock_play;
	return _skulls_unlock_unset;
}

/* the skulls of the set kept for good (game.skulls_unlocked) */
static void skulls_keep(struct skull_set set)
{
	char text[SKULLS_TEXT_LENGTH];

	skulls_text_from_set(skull_set_union(skulls_set_from_text(config_string("game.skulls_unlocked")), set), text,
		sizeof(text));
	if (!config_write("game.skulls_unlocked", text))
		error(_error_silent, "skulls: could not write game.skulls_unlocked");
}

struct skull_set skulls_unlocked_set(void)
{
	/* (ALL SKULLS on: every one, those found kept for when it is off) */
	if (skulls_unlock_mode() != _skulls_unlock_play || config_boolean("game.skulls_all_unlocked"))
		return skull_set_all();
	/* (Grunt Birthday Party the one had from the start) */
	return skull_set_union(skulls_set_from_text(config_string("game.skulls_unlocked")),
		skull_set_of(_skull_grunt_birthday_party));
}

int skulls_drop(void)
{
	return skulls_unlock_mode() == _skulls_unlock_play && !config_boolean("game.skulls_all_unlocked");
}

/* whether the enemies of the game this machine runs (its own, or one it
hosts) drop its locked skulls: unlocking by playing, the skulls on, a
campaign game */
static int drops_hosted(void)
{
	struct network_game *game = network_game_get_game();

	if (!config_boolean("game.skulls") || !skulls_drop() || network_game_distributed_client())
		return 0;
	return !game || game->variant.game_engine_index == game_engine_none;
}

int skulls_carried_wanted(void)
{
	struct network_game *game = network_game_get_game();

	/* (a co-op client: the host's, which every machine copies alike) */
	if (network_game_distributed_client())
		return remote_drops && (!game || game->variant.game_engine_index == game_engine_none);
	return drops_hosted();
}


/* the host's (or a game of one machine's) skulls, from its settings: those
turned on that it has */
static struct skull_set config_set(void)
{
	struct network_game *game = network_game_get_game();

	if (!config_boolean("game.skulls"))
		return skull_set_none();
	if (game && game->variant.game_engine_index != game_engine_none)
		return skull_set_none();

	return skull_set_intersection(skulls_set_from_text(config_string("game.skulls_active")), skulls_unlocked_set());
}

/* whether a level loading wants the tags a skull needs copied in: the
skull on, or still to be found in the level (picked up, it is on there).
In co-op every machine copies the same, by the host's advertisement alone
(the skulls on, and any skull with its drops: one picked up is on), so
that the tags are at the same indices on each */
static int skull_tags_wanted(int skull)
{
	struct network_game *game = network_game_get_game();
	struct skull_set set;
	struct skull_set unlocked;

	if (game && game->variant.game_engine_index != game_engine_none)
		return 0;
	if (network_game_distributed_client())
		return (remote_on && skull_set_has(&remote_set, skull)) || remote_drops;
	set = config_set();
	if (game_connection() == _game_connection_network_server)
		return skull_set_has(&set, skull) || drops_hosted();
	unlocked = skulls_unlocked_set();
	return skull_set_has(&set, skull) || (skulls_carried_wanted() && !skull_set_has(&unlocked, skull));
}

int skulls_outbreak_wanted(void)
{
	return skull_tags_wanted(_skull_outbreak);
}

int skulls_scarab_wanted(void)
{
	return skull_tags_wanted(_skull_scarab);
}

int skulls_pack_wanted(void)
{
	struct skull_set chosen;

	/* (the question of how they are had not answered yet: written before it
	is asked, as either answer may want it, menu_functions.c's
	skulls_setup_poll; with the skulls off too, as a daily challenge or a
	co-op host's skulls apply whatever SKULLS says) */
	if (skulls_unlock_mode() == _skulls_unlock_unset)
		return 1;
	if (!config_boolean("game.skulls"))
		return 0;
	chosen = skull_set_intersection(skulls_set_from_text(config_string("game.skulls_active")),
		skulls_unlocked_set());
	return skulls_drop() || skull_set_has(&chosen, _skull_outbreak) || skull_set_has(&chosen, _skull_scarab);
}

/* the game's own sounds (below) */
enum
{
	_own_sound_party = 0,
	_own_sound_skull_pickup,
	_own_sound_skull_carrier,
	_own_sound_skull_floor,
	NUMBER_OF_OWN_SOUNDS
};

static void own_sounds_add(unsigned long wanted);
static void own_sounds_remove(void);
static void finding_reset(void);
static struct skull_set found_set(void);

/* the level's carried skull's weapon tag (randomizer.c copies it in), or
NONE when its enemies carry none */
static long carried_definition = NONE;

static void running_skulls_reset(void);
static void give_and_take_reset(void);

void skulls_level_loaded(long scenario_tag_index)
{
	skulls_sent_state_valid = FALSE;
	skulls_state_heard = FALSE;
	malfunction_reset();
	running_skulls_reset();
	if (network_game_distributed_client())
		level_set = remote_on ? remote_set : skull_set_none();
	else
		level_set = config_set();
	if (!skull_set_is_empty(&level_set))
	{
		char text[SKULLS_TEXT_LENGTH];

		skulls_text_from_set(level_set, text, sizeof(text));
		error(_error_silent, "skulls: %s", text);
	}
	/* (the skull the enemies carry: a campaign level's, copied in by now) */
	carried_definition = NONE;
	if (scenario_tag_index != NONE && scenario_definition_get(scenario_tag_index)->type == _scenario_type_solo &&
		skulls_carried_wanted())
	{
		carried_definition = tag_loaded(WEAPON_DEFINITION_TAG, SKULLS_CARRIED_TAG);
		error(_error_silent, "skulls: the enemies %s", carried_definition != NONE ? "carry skulls" :
			"carry no skull (it could not be copied)");
	}
	/* (the sounds: Grunt Birthday Party's cheer, a campaign level's; those
	of the skulls to be found) */
	if (scenario_tag_index != NONE && scenario_definition_get(scenario_tag_index)->type == _scenario_type_solo)
	{
		own_sounds_add((skull_set_has(&level_set, _skull_grunt_birthday_party) ||
			skull_set_has(&level_set, _skull_confetti_party) ?
			1UL << _own_sound_party : 0) |
			(carried_definition != NONE ? (1UL << _own_sound_skull_pickup) | (1UL << _own_sound_skull_carrier) |
				(1UL << _own_sound_skull_floor) : 0));
	}
	/* (the port's own skulls': their tags, once the skulls on are known) */
	skulls_outbreak_level_loaded();
	skulls_custom_level_loaded();
	/* (the tags shared with a co-op game's other machines, all there now:
	the menus' and the mods' come after, cache_files.c) */
	randomizer_tag_table_fingerprint(&shared_tag_count, &shared_tag_hash);
	error(_error_silent, "skulls: the level's shared tags: %ld, fingerprint %08lx", shared_tag_count, shared_tag_hash);
}

void skulls_level_unloaded(void)
{
	carried_definition = NONE;
	own_sounds_remove();
	level_set = skull_set_none();
	malfunction_reset();
}

int skull_active(int skull)
{
	/* (those turned on, and those found in the level, as the game state has
	them: a revert to before one was taken takes it back) */
	struct skull_set set = skull_set_union(level_set, found_set());

	if (!skull_set_has(&set, skull))
		return 0;
	/* (a campaign's scenario: not the main menu's, nor a multiplayer map's) */
	return global_scenario && global_scenario->type == _scenario_type_solo;
}

static void skull_set_to_bytes(struct skull_set set, unsigned char *bytes);

void skulls_advertisement(unsigned char *reserved)
{
	struct skull_set set;

	if (!reserved || !network_game_get_game())
		return;
	/* (the enemies drop the host's locked skulls: every machine copies in
	the skull they carry) */
	if (drops_hosted())
	{
		reserved[HALO_PORT_ADVERTISED_FLAGS_OFFSET] =
			(unsigned char)(reserved[HALO_PORT_ADVERTISED_FLAGS_OFFSET] | HALO_PORT_ADVERTISED_SKULL_DROPS_FLAG);
	}
	set = config_set();
	if (skull_set_is_empty(&set))
		return;
	reserved[HALO_PORT_ADVERTISED_FLAGS_OFFSET] =
		(unsigned char)(reserved[HALO_PORT_ADVERTISED_FLAGS_OFFSET] | HALO_PORT_ADVERTISED_SKULLS_FLAG);
	skull_set_to_bytes(set, reserved + HALO_PORT_ADVERTISED_SKULLS_OFFSET);
}

int skulls_hosted(void)
{
	struct skull_set set;

	if (!network_game_get_game())
		return 0;
	set = config_set();
	return !skull_set_is_empty(&set) || drops_hosted();
}

/* a set to its eight bytes (little-endian, skull n bit n) and back, as the
advertisement and the state message carry them */
static void skull_set_to_bytes(struct skull_set set, unsigned char *bytes)
{
	short byte_index;

	for (byte_index = 0; byte_index < HALO_PORT_ADVERTISED_SKULLS_SIZE; byte_index++)
	{
		bytes[byte_index] = byte_index / 4 < SKULL_SET_WORDS ?
			(unsigned char)(set.words[byte_index / 4] >> ((byte_index % 4) * 8)) : 0;
	}
}

static struct skull_set skull_set_from_bytes(unsigned char const *bytes)
{
	struct skull_set set = skull_set_none();
	short byte_index;

	for (byte_index = 0; byte_index < SKULL_SET_WORDS * 4; byte_index++)
		set.words[byte_index / 4] |= (unsigned long)bytes[byte_index] << ((byte_index % 4) * 8);
	/* (none past those there are) */
	return skull_set_intersection(set, skull_set_all());
}

int skulls_advertised_need_pack(unsigned char flags, unsigned char const *bytes)
{
	struct skull_set set;

	if (flags & HALO_PORT_ADVERTISED_SKULL_DROPS_FLAG)
		return 1;
	if (!(flags & HALO_PORT_ADVERTISED_SKULLS_FLAG) || !bytes)
		return 0;
	set = skull_set_from_bytes(bytes);
	return skull_set_has(&set, _skull_outbreak) || skull_set_has(&set, _skull_scarab);
}

void skulls_set_remote(unsigned char flags, unsigned char const *bytes)
{
	remote_on = (flags & HALO_PORT_ADVERTISED_SKULLS_FLAG) && bytes;
	remote_drops = (flags & HALO_PORT_ADVERTISED_SKULL_DROPS_FLAG) != 0;
	remote_set = remote_on ? skull_set_from_bytes(bytes) : skull_set_none();
}

void skulls_remote_get(unsigned char *flags, unsigned char *bytes)
{
	*flags = (unsigned char)((remote_on ? HALO_PORT_ADVERTISED_SKULLS_FLAG : 0) |
		(remote_drops ? HALO_PORT_ADVERTISED_SKULL_DROPS_FLAG : 0));
	skull_set_to_bytes(remote_set, bytes);
}

void skulls_shared_tags(long *count, unsigned long *hash)
{
	*count = shared_tag_count;
	*hash = shared_tag_hash;
}

int skulls_team_is_enemy(short team_index)
{
	/* (not an object of no team, an empty vehicle's: game_team_is_enemy
	counts the default team the player's enemy) */
	return team_index != NONE && team_index != _game_team_default &&
		game_team_is_enemy(_game_team_player, team_index);
}

int skulls_unit_is_enemy(long unit_index)
{
	struct unit_datum *unit = unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);

	return unit && unit->unit.player_index == NONE && skulls_team_is_enemy(unit->object.owner_team_index);
}

int skulls_unit_is_grunt(long unit_index)
{
	struct unit_datum *unit = unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);

	return unit && randomizer_unit_is_grunt(tag_get_name(unit->definition_index));
}

int skulls_weapon_is_covenant(long weapon_definition_index)
{
	return weapon_definition_index != NONE && randomizer_weapon_is_covenant(tag_get_name(weapon_definition_index));
}

int skulls_hud_broken(short local_player_index, long unit_index, int element)
{
	if (local_player_index < 0 || local_player_index >= SKULLS_LOCAL_PLAYERS || unit_index == NONE ||
		!skull_active(_skull_malfunction))
		return 0;
	/* (a new body, or the game gone back in time: another element breaks.
	Only what this machine shows) */
	if (malfunction[local_player_index].unit_index != unit_index ||
		game_time_get() < malfunction[local_player_index].game_time)
	{
		malfunction[local_player_index].unit_index = unit_index;
		malfunction[local_player_index].element = malfunction_new_element(malfunction[local_player_index].element);
	}
	malfunction[local_player_index].game_time = game_time_get();

	return malfunction[local_player_index].element == element;
}

void skulls_famine_weapon(long weapon_index)
{
	struct weapon_datum *weapon = weapon_index == NONE ? NULL :
		(struct weapon_datum *)object_try_and_get_and_verify_type(weapon_index, _object_mask_weapon);
	struct weapon_definition *definition;
	boolean famine = skull_active(_skull_famine);
	boolean efficient = skull_active(_skull_efficient);
	short magazine;

	/* (both: as it was) */
	if (!weapon || famine == efficient)
		return;
	definition = weapon_definition_get(weapon->definition_index);
	/* (a plasma weapon's charge: half of what it has left, or what it spent
	halved) */
	weapon->weapon.age = famine ? 1.0f - (1.0f - weapon->weapon.age) * 0.5f : weapon->weapon.age * 0.5f;
	for (magazine = 0; magazine < (short)NUMBEROF(weapon->weapon.magazines) &&
		magazine < definition->weapon.magazines.count; magazine++)
	{
		struct weapon_magazine *rounds = &weapon->weapon.magazines[magazine];
		struct weapon_magazine_definition *magazine_definition =
			TAG_BLOCK_GET_ELEMENT(&definition->weapon.magazines, magazine, struct weapon_magazine_definition);

		if (famine)
		{
			rounds->rounds_total = (short)(rounds->rounds_total / 2);
			rounds->rounds_loaded = (short)((rounds->rounds_loaded + 1) / 2);
		}
		/* (Efficient: twice the rounds spare, as many as the weapon holds) */
		else
		{
			long doubled = (long)rounds->rounds_total * 2;

			if (doubled < (long)magazine_definition->rounds_loaded_maximum)
				doubled = magazine_definition->rounds_loaded_maximum;
			if (magazine_definition->rounds_total_maximum > 0 && doubled > magazine_definition->rounds_total_maximum)
				doubled = magazine_definition->rounds_total_maximum;
			rounds->rounds_total = (short)doubled;
		}
	}
}

int skulls_unit_is_player(long object_index)
{
	struct unit_datum *unit = object_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(object_index, _object_mask_unit);

	return unit && unit->unit.player_index != NONE;
}

/* Black Eye's share of a player's shield that a punch gives back */
#define BLACK_EYE_SHIELD 0.5f

void skulls_melee_damage(long victim_index, long attacker_object_index, long attacker_player_index)
{
	struct unit_datum *attacker;

	if (attacker_player_index == NONE || !skulls_unit_is_enemy(victim_index))
		return;
	if (skull_active(_skull_pinata))
	{
		/* (a grenade of the two kinds in turn: the game's own seed, as the
		host alone runs this) */
		unit_port_drop_grenade(victim_index, (short)(random_range(0, 2)));
	}
	attacker = attacker_object_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(attacker_object_index, _object_mask_unit);
	if (attacker && skull_active(_skull_black_eye) && attacker->unit.player_index != NONE &&
		attacker->object.shield_vitality < 1.0f)
	{
		attacker->object.shield_vitality += BLACK_EYE_SHIELD;
		if (attacker->object.shield_vitality > 1.0f)
			attacker->object.shield_vitality = 1.0f;
		attacker->object.shield_stun_ticks = 0;
	}
}

float skulls_boom_scale(void)
{
	return skull_active(_skull_boom) ? 2.0f : 1.0f;
}

/* an effect of the maps' (each campaign map has it), where the unit stands */
static void effect_at_unit(char const *name, long unit_index)
{
	long definition_index = tag_loaded(EFFECT_DEFINITION_TAG, name);
	char const *marker_names[1];
	real_point3d marker_points[1];
	real_vector3d marker_forwards[1];

	if (definition_index == NONE || !object_try_and_get_and_verify_type(unit_index, _object_mask_unit))
		return;
	marker_names[0] = "";
	object_get_origin(unit_index, &marker_points[0]);
	marker_forwards[0] = *global_up3d;
	effect_new_unattached_from_markers(definition_index, unit_index, NULL, 1, marker_names, marker_points,
		marker_forwards, 0.0f, 0.0f, NULL, NULL, TRUE);
}

/* ---------- the game's own sounds

WAVs of 16-bit PCM (port/assets/sounds, tools/embed_assets.py), made sound
tags of the level's while it wants them: a sound of the map's copied (a
grunt's dialog, else its first), with one permutation of these samples,
which the sound cache reads from memory (skulls_sound_read,
xbox_sound_cache.c). Not of the dialog's class: a unit speaks one sound at
a time, and the grunt's death cry, right after, would cut the cheer off; an
explosion's class, as loud and as far heard, plays beside it */
extern const unsigned int skulls_party_sound[];
extern const unsigned long skulls_party_sound_size;
extern const unsigned int skulls_pickup_sound[];
extern const unsigned long skulls_pickup_sound_size;
extern const unsigned int skulls_carrier_sound[];
extern const unsigned long skulls_carrier_sound_size;
extern const unsigned int skulls_floor_sound[];
extern const unsigned long skulls_floor_sound_size;
extern void *cache_files_tag_instances(long *count);
extern void cache_files_set_tag_instances(void *instances, long count);

#define SOUND_COMPRESSION_NONE 0
#define SOUND_COMPRESSION_XBOX_ADPCM 1
#define OWN_SOUND_RATE 22050
/* sound_classes.c's projectile_detonation */
#define OWN_SOUND_CLASS 1

/* cache_files.c's */
struct skulls_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

typedef char skulls_tag_instance_size[sizeof(struct skulls_tag_instance) == 0x20 ? 1 : -1];

/* the sounds the own sounds are made from, the first the map has (else
its first sound) */
static char const *const own_sound_templates[] =
{
	"sound\\dialog\\grunt\\conditional\\combat2\\postcombatactions\\celebration",
	"sound\\dialog\\grunt\\conditional\\combat2\\killingpeople\\killed_enemy",
	"sound\\dialog\\grunt\\conditional\\combat2\\friendsdying\\friend_killed_by_enemy_player",
};

static struct own_sound
{
	unsigned int const *file;
	unsigned long const *file_size;
	char *name;
	/* full loudness this near, in world units, heard this far */
	real minimum_distance;
	real maximum_distance;
	/* the samples, made Xbox ADPCM once (the game's allocator: kept) */
	boolean encoded;
	byte *samples;
	unsigned long sample_bytes;
	long milliseconds;
	/* the level's tag, while it has one */
	struct sound_definition definition;
	struct sound_pitch_range range;
	struct sound_permutation permutation;
	long tag_index;
} own_sounds[NUMBER_OF_OWN_SOUNDS] =
{
	{ skulls_party_sound, &skulls_party_sound_size, "sound\\port\\skulls\\grunt birthday party", 2.0f, 40.0f },
	{ skulls_pickup_sound, &skulls_pickup_sound_size, "sound\\port\\skulls\\skull picked up", 2.0f, 40.0f },
	{ skulls_carrier_sound, &skulls_carrier_sound_size, "sound\\port\\skulls\\skull dropped", 2.0f, 40.0f },
	/* (the skull lying about: heard near it only) */
	{ skulls_floor_sound, &skulls_floor_sound_size, "sound\\port\\skulls\\skull", 0.6f, 6.0f },
};

/* the level's tags with the own sounds added, and those before */
static struct skulls_tag_instance *own_grown_instances;
static struct skulls_tag_instance *own_saved_instances;
static long own_saved_count;

static unsigned long read_le32(byte const *bytes)
{
	return (unsigned long)bytes[0] | ((unsigned long)bytes[1] << 8) | ((unsigned long)bytes[2] << 16) |
		((unsigned long)bytes[3] << 24);
}

/* the WAV's samples, encoded; FALSE when it has none this build plays */
static boolean own_sound_encode(struct own_sound *sound)
{
	byte const *file = (byte const *)sound->file;
	unsigned long size = *sound->file_size;
	unsigned long offset = 12;
	byte const *data = NULL;
	unsigned long data_bytes = 0;
	long channels = 0;
	long rate = 0;
	long bits = 0;

	if (sound->encoded)
		return sound->samples != NULL;
	sound->encoded = TRUE;
	if (size < 12 || memcmp(file, "RIFF", 4) || memcmp(file + 8, "WAVE", 4))
	{
		error(_error_silent, "skulls: %s is not a WAV", sound->name);
		return FALSE;
	}
	/* (its chunks: the format's and the samples') */
	while (offset + 8 <= size)
	{
		unsigned long chunk = read_le32(file + offset + 4);

		if (chunk > size - offset - 8)
			break;
		if (!memcmp(file + offset, "fmt ", 4) && chunk >= 16)
		{
			if (file[offset + 8] != 1 || file[offset + 9] != 0)
				break;
			channels = (long)(file[offset + 10] | (file[offset + 11] << 8));
			rate = (long)read_le32(file + offset + 12);
			bits = (long)(file[offset + 22] | (file[offset + 23] << 8));
		}
		else if (!memcmp(file + offset, "data", 4))
		{
			data = file + offset + 8;
			data_bytes = chunk;
		}
		offset += 8 + chunk + (chunk & 1);
	}
	if (!data || bits != 16 || (channels != 1 && channels != 2) || (rate != 22050 && rate != 44100) ||
		data_bytes < (unsigned long)(2 * channels))
	{
		error(_error_silent, "skulls: %s is not 16-bit PCM, mono or stereo, at 22 or 44 kHz", sound->name);
		return FALSE;
	}
	sound->samples = custom_edition_sounds_encode(data, (long)data_bytes, SOUND_COMPRESSION_NONE, FALSE,
		channels, rate, OWN_SOUND_RATE, &sound->sample_bytes);
	if (!sound->samples || sound->sample_bytes > MAXIMUM_SOUND_DATA_SIZE)
	{
		error(_error_silent, "skulls: %s could not be encoded", sound->name);
		sound->samples = NULL;
		return FALSE;
	}
	sound->milliseconds = (long)((data_bytes / (unsigned long)(2 * channels)) * 1000UL / (unsigned long)rate);
	return TRUE;
}

/* the sound's definition: the template's, its samples this one permutation */
static void own_sound_define(struct own_sound *sound, long template_index)
{
	struct sound_definition *definition = &sound->definition;

	*definition = *sound_definition_get(template_index);
	definition->sound_class = OWN_SOUND_CLASS;
	definition->minimum_distance = sound->minimum_distance;
	definition->maximum_distance = sound->maximum_distance;
	definition->gain_modifier = 1.0f;
	definition->zero_gain_modifier = 1.0f;
	definition->one_gain_modifier = 1.0f;
	definition->zero_pitch_modifier = 1.0f;
	definition->one_pitch_modifier = 1.0f;
	definition->zero_skip_fraction_modifier = 0.0f;
	definition->one_skip_fraction_modifier = 0.0f;
	/* (heard the same all round) */
	definition->inner_cone_angle = 2.0f * _pi;
	definition->outer_cone_angle = 2.0f * _pi;
	definition->outer_cone_gain = 1.0f;
	definition->skip_fraction = 0.0f;
	definition->random_pitch_bounds.lower = 1.0f;
	definition->random_pitch_bounds.upper = 1.0f;
	definition->encoding = 0;
	definition->compression = SOUND_COMPRESSION_XBOX_ADPCM;
	definition->sample_rate = OWN_SOUND_RATE == 44100 ? 1 : 0;
	definition->promotion_sound.index = NONE;
	definition->promotion_count = 0;
	definition->promotion_counter = 0;
	definition->promotion_time = 0;
	definition->scripting_time = 0;
	definition->scripting_sound_index = NONE;
	definition->longest_permutation_length = sound->milliseconds;
	definition->pitch_ranges.count = 1;
	definition->pitch_ranges.address = &sound->range;
	definition->pitch_ranges.definition = NULL;
	csmemset(&sound->range, 0, sizeof(sound->range));
	sound->range.natural_pitch = 1.0f;
	sound->range.bend_bounds.lower = 1.0f;
	sound->range.bend_bounds.upper = 1.0f;
	sound->range.actual_permutation_count = 1;
	sound->range.playback_rate = 1.0f;
	sound->range.played_permutation_mask = 0xFFFFFFFF;
	sound->range.previous_permutation_index = NONE;
	sound->range.forced_permutation_index = NONE;
	sound->range.permutations.count = 1;
	sound->range.permutations.address = &sound->permutation;
	sound->range.permutations.definition = NULL;
	csmemset(&sound->permutation, 0, sizeof(sound->permutation));
	sound->permutation.gain = 1.0f;
	sound->permutation.compression = SOUND_COMPRESSION_XBOX_ADPCM;
	sound->permutation.next_permutation_index = NONE;
	sound->permutation.samples.size = (long)sound->sample_bytes;
	sound->permutation.samples.address = sound->samples;
	/* (the sound cache's: block, address, tag: xbox_sound_cache.c) */
	sound->permutation.unknown0 = NONE;
	sound->permutation.unknown2 = (unsigned long)sound->tag_index;
	sound->permutation.unknown3 = (unsigned long)sound->tag_index;
}

/* the level's tags of the own sounds of the mask, appended to its tags
(after the randomizer's, which the level's unload restores after these) */
static void own_sounds_add(unsigned long wanted)
{
	long existing = 0;
	struct skulls_tag_instance *instances = (struct skulls_tag_instance *)cache_files_tag_instances(&existing);
	long template_index = NONE;
	long salt = 0;
	long added = 0;
	long index;
	int sound;

	for (sound = 0; sound < NUMBER_OF_OWN_SOUNDS; sound++)
		own_sounds[sound].tag_index = NONE;
	if (!wanted || !instances || existing <= 0 || existing + NUMBER_OF_OWN_SOUNDS >= UNSIGNED_SHORT_MAX)
		return;
	for (index = 0; index < (long)NUMBEROF(own_sound_templates) && template_index == NONE; index++)
		template_index = tag_loaded(SOUND_DEFINITION_TAG, own_sound_templates[index]);
	for (index = 0; index < existing && template_index == NONE; index++)
	{
		if (instances[index].group_tag == SOUND_DEFINITION_TAG)
			template_index = instances[index].tag_index;
	}
	if (template_index == NONE)
	{
		error(_error_silent, "skulls: the level has no sound to make the skulls' sounds of");
		return;
	}
	own_grown_instances = (struct skulls_tag_instance *)malloc((unsigned long)(existing + NUMBER_OF_OWN_SOUNDS) *
		sizeof(*own_grown_instances));
	if (!own_grown_instances)
		return;
	memcpy(own_grown_instances, instances, (unsigned long)existing * sizeof(*instances));
	for (index = 0; index < existing; index++)
	{
		long instance_salt = (long)((unsigned long)instances[index].tag_index >> 16);

		if (instance_salt > salt)
			salt = instance_salt;
	}
	for (sound = 0; sound < NUMBER_OF_OWN_SOUNDS; sound++)
	{
		struct own_sound *own = own_sounds + sound;
		struct skulls_tag_instance *instance = own_grown_instances + existing + added;

		if (!(wanted & (1UL << sound)) || !own_sound_encode(own))
			continue;
		own->tag_index = ((salt + 1) << 16) | (existing + added);
		own_sound_define(own, template_index);
		*instance = instances[DATUM_INDEX_TO_ABSOLUTE_INDEX(template_index)];
		instance->tag_index = own->tag_index;
		instance->name = own->name;
		instance->base_address = &own->definition;
		added++;
		error(_error_silent, "skulls: %s is tag %08lx (%lu bytes, %ld ms), made of %s", own->name,
			(unsigned long)own->tag_index, own->sample_bytes, own->milliseconds, tag_get_name(template_index));
	}
	if (!added)
	{
		free(own_grown_instances);
		own_grown_instances = NULL;
		return;
	}
	own_saved_instances = instances;
	own_saved_count = existing;
	cache_files_set_tag_instances(own_grown_instances, existing + added);
}

static void own_sounds_remove(void)
{
	int sound;

	if (own_saved_instances)
		cache_files_set_tag_instances(own_saved_instances, own_saved_count);
	if (own_grown_instances)
		free(own_grown_instances);
	own_grown_instances = NULL;
	own_saved_instances = NULL;
	own_saved_count = 0;
	for (sound = 0; sound < NUMBER_OF_OWN_SOUNDS; sound++)
		own_sounds[sound].tag_index = NONE;
}

/* the sound played from the object; the sound, or NONE */
static long own_sound_play(int sound, long object_index)
{
	if (own_sounds[sound].tag_index == NONE || object_index == NONE)
	{
		error(_error_silent, "skulls: %s not played (tag %08lx, object %08lx)", own_sounds[sound].name,
			(unsigned long)own_sounds[sound].tag_index, (unsigned long)object_index);
		return NONE;
	}
	return object_impulse_sound_new(object_index, own_sounds[sound].tag_index, NONE, global_origin3d,
		global_forward3d, 1.0f);
}

unsigned char skulls_sound_read(struct sound_permutation const *permutation, void *buffer)
{
	int sound;

	for (sound = 0; sound < NUMBER_OF_OWN_SOUNDS; sound++)
	{
		if (permutation == &own_sounds[sound].permutation && own_sounds[sound].samples)
		{
			memcpy(buffer, own_sounds[sound].samples, (size_t)own_sounds[sound].sample_bytes);
			return TRUE;
		}
	}
	return FALSE;
}

/* ---------- confetti

the shirt scraps of the maps' cloth debris, and the sparks of their impacts,
each a colour of a palette's, thrown from a unit's head (the local seed:
only what this machine shows) */
#define CONFETTI_SCRAPS 40
#define CONFETTI_SPARKS 16
#define CONFETTI_SCRAP_TAG "effects\\particles\\solid\\debris shirt"
#define CONFETTI_SPARK_TAG "effects\\particles\\flash\\sparks impact"

/* Grunt Birthday Party's */
static real_rgb_color const party_colors[] =
{
	{ 1.0f, 0.15f, 0.2f }, { 1.0f, 0.8f, 0.1f }, { 0.2f, 0.9f, 0.3f },
	{ 0.15f, 0.55f, 1.0f }, { 0.85f, 0.25f, 1.0f }, { 1.0f, 0.5f, 0.1f },
};

/* a skull dropped's: black and white, floating up */
static real_rgb_color const skull_colors[] =
{
	{ 0.04f, 0.04f, 0.04f }, { 1.0f, 1.0f, 1.0f },
};

#define SKULL_CONFETTI_RISE 0.6f

static void confetti_throw(long definition_index, short count, real_point3d const *center, real speed,
	real radius_lower, real radius_upper, real_rgb_color const *colors, short color_count)
{
	short piece;

	if (definition_index == NONE)
		return;
	for (piece = 0; piece < count; piece++)
	{
		struct new_particle_data data;
		real_vector3d direction;

		csmemset(&data, 0, sizeof(data));
		data.definition_index = definition_index;
		data.object_index = NONE;
		data.node_index = NONE;
		data.local_player_index = NONE;
		data.position = *center;
		direction.i = real_local_random_range(-0.6f, 0.6f);
		direction.j = real_local_random_range(-0.6f, 0.6f);
		direction.k = real_local_random_range(0.7f, 1.2f);
		normalize3d(&direction);
		data.direction = direction;
		scale_vector3d(&direction, speed * real_local_random_range(0.6f, 1.2f), &data.velocity);
		data.rotation = real_local_random_range(0.0f, 2.0f * _pi);
		data.angular_velocity = real_local_random_range(-8.0f, 8.0f);
		data.radius = real_local_random_range(radius_lower, radius_upper);
		data.color.alpha = 1.0f;
		data.color.rgb = colors[local_random() % color_count];
		particle_new(&data);
	}
}

/* (each a share of its particle's own size: a whole scrap of shirt is
bigger than the grunt) */
static void confetti(long unit_index, real_rgb_color const *colors, short color_count, real rise)
{
	struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	real_point3d center;

	if (!unit)
		return;
	center = unit->object.bounding_sphere_center;
	center.z += unit->object.bounding_sphere_radius * 0.5f;
	particles_port_rise(rise);
	confetti_throw(tag_loaded(PARTICLE_TAG, CONFETTI_SCRAP_TAG), CONFETTI_SCRAPS, &center, 1.6f, 0.05f, 0.09f,
		colors, color_count);
	confetti_throw(tag_loaded(PARTICLE_TAG, CONFETTI_SPARK_TAG), CONFETTI_SPARKS, &center, 2.2f, 0.125f, 0.2f,
		colors, color_count);
	particles_port_rise(0.0f);
}

/* ---------- the skulls to be found

The enemies that hold things (randomizer.c's randomizer_unit_can_carry) drop
the oddball the level copied in: the first the players kill, then each
they kill one chance in as many as are left of those the level places
(randomizer_level_carriers, less those dead), until the difficulty's
number (one on easy to four on legendary). Each is a skull not had, at
random, named by the pick-up prompt; it hums on the floor. The action
button takes it (never walking over it): it chimes, is on for the level,
and is kept once the level is finished without dying */
#define SKULLS_DROPPED_MAX 8
#define SKULL_NAME_LENGTH 40

#define SKULLS_POOL_MAX 8

/* the skulls a level drops: game.skulls_drop_count, or with 0 the
difficulty's, one on easy to four on legendary */
static short skulls_drop_quota(void)
{
	long count = config_integer("game.skulls_drop_count");

	if (count <= 0)
		count = game_difficulty_level_get() + 1;
	return (short)PIN(count, 1, SKULLS_POOL_MAX);
}

/* a kill's odds of a skull, multiplied by game.skulls_drop_chance (a
percentage) */
static long skulls_drop_chance(void)
{
	return PIN(config_integer("game.skulls_drop_chance"), 1, 1000);
}

/* the run of the level, from its start (or start again) on: what a death or
a revert does not take back, so that no skull is had twice. The skulls it
drops, chosen at its first drop (the first of the level's order not had
then, as many as its difficulty drops), in that order; those picked up,
and of them those unlocked at once (INSTANT UNLOCK) or lost. Only a skull
picked up is spent: one dropped and gone with a revert drops again, the
first of them at the first kill. This machine's (a revert puts the game
state's back), with a copy in the game state that a saved game keeps
(Continue) */
struct skulls_run
{
	short pool_count;
	short pad;
	signed char pool[SKULLS_POOL_MAX];
	struct skull_set taken;
	struct skull_set unlocked;
	struct skull_set lost;
};

/* in the game state: a checkpoint's revert, and the level started again,
bring back the skulls lying about and found as they were then */
struct skulls_finding
{
	/* the level's carriers (NONE: not counted yet), and those dead */
	long total;
	long deaths;
	/* the skulls taken this level: on, not yet kept */
	struct skull_set found;
	/* the run, as of the last change (skulls_run) */
	struct skulls_run run;
	/* rolled when the level starts (and starts again): the order the skulls
	drop in, and the odds of each kill's, so a revert drops the same skull at
	the same kill; another start of the level, other skulls */
	unsigned long seed;
	signed char order[(NUMBER_OF_SKULLS + 3) & ~3];
	struct
	{
		long object_index;
		long next_hum_time;
		int skull;
		wchar_t name[SKULL_NAME_LENGTH];
	} skulls[SKULLS_DROPPED_MAX];
};

static struct skulls_finding *finding;
static struct skulls_run run;
/* the hum each dropped skull plays (a sound of this machine's, not the
game state's: forgotten when a revert takes the game back), and when */
static long skull_hums[SKULLS_DROPPED_MAX] = { NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE };
static long skull_hums_time;
/* the enemy that dropped each, for a co-op client's cheer and confetti
(the host's, this machine's: not the game state's) */
static long skull_droppers[SKULLS_DROPPED_MAX] = { NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE };

/* the skulls found in the level, not yet kept */
static struct skull_set found_set(void)
{
	return finding ? finding->found : skull_set_none();
}

/* a number of the seed's, for the step (a kill's odds, a place in the
order): the same for the same seed and step */
static unsigned long finding_random(unsigned long step)
{
	unsigned long value = finding->seed ^ (step * 0x9E3779B9UL);

	value ^= value >> 16;
	value *= 0x7FEB352DUL;
	value ^= value >> 15;
	value *= 0x846CA68BUL;
	value ^= value >> 16;
	return value & 0xFFFFFFFFUL;
}

/* the run's change, into the game state's copy */
static void run_saved(void)
{
	if (finding)
		finding->run = run;
}

/* a new run: the level started, or started again */
static void run_reset(void)
{
	csmemset(&run, 0, sizeof(run));
	run.pool_count = NONE;
	run_saved();
}

static void finding_reset(void)
{
	short index;

	finding->total = NONE;
	finding->deaths = 0;
	finding->found = skull_set_none();
	/* (the order, shuffled from the clock: each start of the level its own) */
	finding->seed = (unsigned long)system_milliseconds() * 2654435761UL ^ 0x5EED5EEDUL;
	for (index = 0; index < NUMBER_OF_SKULLS; index++)
		finding->order[index] = (signed char)index;
	for (index = NUMBER_OF_SKULLS - 1; index > 0; index--)
	{
		short other = (short)(finding_random(0x1000UL + (unsigned long)index) % (unsigned long)(index + 1));
		signed char swap = finding->order[index];

		finding->order[index] = finding->order[other];
		finding->order[other] = swap;
	}
	for (index = 0; index < SKULLS_DROPPED_MAX; index++)
	{
		finding->skulls[index].object_index = NONE;
		skull_hums[index] = NONE;
		skull_droppers[index] = NONE;
		finding->skulls[index].next_hum_time = 0;
		finding->skulls[index].skull = NONE;
		finding->skulls[index].name[0] = 0;
	}
}

static int skulls_unit_can_carry(long unit_index)
{
	struct unit_datum *unit = unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);

	return unit && randomizer_unit_can_carry(tag_get_name(unit->definition_index));
}

/* the enemies alive that could carry one, the one dying among them */
static long carriers_left(long dying_index)
{
	struct object_iterator iterator;
	struct unit_datum *unit;
	long count = 1;

	object_iterator_new(&iterator, _object_mask_unit, 0);
	while ((unit = object_iterator_next(&iterator)) != NULL)
	{
		if (iterator.index == dying_index || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		if (skulls_unit_is_enemy(iterator.index) && skulls_unit_can_carry(iterator.index) &&
			!skulls_outbreak_raised(iterator.index))
			count++;
	}
	return count;
}

/* the run's skulls: the first of the level's order not had, as many as
the level drops (skulls_drop_quota) */
static void run_pool(void)
{
	struct skull_set had = skulls_unlocked_set();
	short quota = skulls_drop_quota();
	short index;

	run.pool_count = 0;
	for (index = 0; index < NUMBER_OF_SKULLS && run.pool_count < quota && run.pool_count < SKULLS_POOL_MAX; index++)
	{
		int skull = finding->order[index];

		if (skull >= 0 && skull < NUMBER_OF_SKULLS && !skull_set_has(&had, skull))
			run.pool[run.pool_count++] = (signed char)skull;
	}
	run_saved();
}

/* the skulls lying about, as the game state has them */
static struct skull_set skulls_lying(void)
{
	struct skull_set lying = skull_set_none();
	short index;

	for (index = 0; index < SKULLS_DROPPED_MAX; index++)
	{
		long object_index = finding->skulls[index].object_index;
		struct object_datum *object = object_index == NONE ? NULL : object_try_and_get(object_index);
		int skull = finding->skulls[index].skull;

		if (object && skull != NONE && object->definition_index == carried_definition)
			skull_set_add(&lying, skull);
	}
	return lying;
}

/* the run's next skull to drop: its first not picked up nor lying about;
NONE when it has dropped them all */
static int run_next(void)
{
	struct skull_set lying = skulls_lying();
	short index;

	for (index = 0; index < run.pool_count; index++)
	{
		int skull = run.pool[index];

		if (!skull_set_has(&run.taken, skull) && !skull_set_has(&lying, skull))
			return skull;
	}
	return NONE;
}

/* the skulls still to be found in the run: those not picked up */
static short skulls_left_to_find(void)
{
	short quota = skulls_drop_quota();
	struct skull_set locked = skull_set_difference(skull_set_all(), skulls_unlocked_set());
	short count = 0;
	int skull;

	/* (a co-op client's: the host's count) */
	if (network_game_distributed_client())
		return remote_left;
	if (run.pool_count != NONE)
	{
		short left = 0;
		short index;

		for (index = 0; index < run.pool_count; index++)
			left += (short)!skull_set_has(&run.taken, run.pool[index]);
		return left;
	}
	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
		count += (short)skull_set_has(&locked, skull);
	return count < quota ? count : quota;
}

/* ---------- a kill's odds of a skull

one in as many as are left (the base), and one more for each of these: a
headshot, a punch from behind, an instant kill (the enemy hurt by nothing
before), a group's (one blow killing several), a vehicle's (run over). The
kills of a tick are decided at its end, when the blow's group is known */

/* the enemies hurt, by any blow (an instant kill's is not): a set of their
indices, this machine's, forgotten with the level and a revert */
#define HURT_SLOTS 512

static long hurt_units[HURT_SLOTS];

static void hurt_reset(void)
{
	short index;

	for (index = 0; index < HURT_SLOTS; index++)
		hurt_units[index] = NONE;
}

/* the unit's slot: its own, or the free one it would take (NONE: full) */
static short hurt_slot(long unit_index)
{
	short index = (short)((((unsigned long)unit_index * 2654435761UL) >> 16) % HURT_SLOTS);
	short tries;

	for (tries = 0; tries < HURT_SLOTS; tries++, index = (short)((index + 1) % HURT_SLOTS))
	{
		if (hurt_units[index] == unit_index || hurt_units[index] == NONE)
			return index;
	}
	return NONE;
}

/* the last killing blow, which skulls_unit_killed (its aftermath, at once)
takes */
static struct
{
	long unit_index;
	long definition_index;
	long owner_object_index;
	long time;
	boolean behind;
	boolean vehicle;
	boolean instant;
} killing_blow = { NONE };

void skulls_unit_blow(long unit_index, long definition_index, long owner_object_index,
	void const *direction, int melee, int vehicle, int lethal)
{
	short slot;

	if (carried_definition == NONE)
		return;
	slot = hurt_slot(unit_index);
	if (!lethal)
	{
		if (slot != NONE)
			hurt_units[slot] = unit_index;
		return;
	}
	killing_blow.unit_index = unit_index;
	killing_blow.definition_index = definition_index;
	killing_blow.owner_object_index = owner_object_index;
	killing_blow.time = game_time_get();
	killing_blow.vehicle = (boolean)vehicle;
	killing_blow.instant = slot == NONE || hurt_units[slot] != unit_index;
	killing_blow.behind = FALSE;
	/* (from behind: the blow going the way the body faces) */
	if (melee && direction)
	{
		struct object_datum *object = object_try_and_get(unit_index);
		real_vector3d blow = *(real_vector3d const *)direction;

		if (object && normalize3d(&blow) > 0.0f)
			killing_blow.behind = dot_product3d(&blow, &object->object.forward) > 0.5f;
	}
}

/* the kills of the tick, to be decided at its end */
#define KILLS_PER_TICK 32

static struct
{
	long unit_index;
	long left;
	long deaths;
	long definition_index;
	long owner_object_index;
	short bonus;
} tick_kills[KILLS_PER_TICK];
static short tick_kill_count;
static long tick_kills_time;

static void skull_drop(long unit_index, int skull, long left, short odds);
/* a dropped skull's push by a blast, while it falls (skulls_item_push_scale) */
#define SKULL_FALLING_PUSH_SCALE 0.2f

/* an enemy killed, by a player's blow or another's: counted among the dead
(the odds' base); a player's kill, to be decided at the tick's end */
static void skull_drop_maybe(long unit_index, long killer_player_index, int headshot)
{
	long left;

	/* (a co-op client's: the host decides the drops, and says them) */
	if (network_game_distributed_client())
		return;
	/* (Outbreak's raised Flood: none the level placed, none to count) */
	if (!skulls_unit_is_enemy(unit_index) || !skulls_unit_can_carry(unit_index) || skulls_outbreak_raised(unit_index))
		return;
	/* (the level's carriers, counted at the first death: those it places
	over the whole level, not only those about now) */
	if (finding->total == NONE)
	{
		finding->total = randomizer_level_carriers();
		error(_error_silent, "skulls: the level places %ld enemies that could carry a skull", finding->total);
	}
	/* (as many left as the level places less the dead, and no fewer than
	are alive now: a squad brought back more than its count) */
	left = finding->total - finding->deaths;
	if (left < carriers_left(unit_index))
		left = carriers_left(unit_index);
	finding->deaths++;
	if (killer_player_index == NONE)
		return;
	if (tick_kills_time != game_time_get())
	{
		tick_kill_count = 0;
		tick_kills_time = game_time_get();
	}
	if (tick_kill_count == KILLS_PER_TICK)
		return;
	tick_kills[tick_kill_count].unit_index = unit_index;
	tick_kills[tick_kill_count].left = left;
	tick_kills[tick_kill_count].deaths = finding->deaths;
	tick_kills[tick_kill_count].bonus = (short)(headshot ? 1 : 0);
	tick_kills[tick_kill_count].definition_index = NONE;
	tick_kills[tick_kill_count].owner_object_index = NONE;
	if (killing_blow.unit_index == unit_index && killing_blow.time == game_time_get())
	{
		tick_kills[tick_kill_count].bonus = (short)(tick_kills[tick_kill_count].bonus +
			(killing_blow.behind ? 1 : 0) + (killing_blow.instant ? 1 : 0) + (killing_blow.vehicle ? 1 : 0));
		tick_kills[tick_kill_count].definition_index = killing_blow.definition_index;
		tick_kills[tick_kill_count].owner_object_index = killing_blow.owner_object_index;
	}
	tick_kill_count++;
}

/* the tick's kills decided: a group's (another of the tick's kills by the
same blow: its kind and its owner) one more each */
static void tick_kills_decide(void)
{
	short index;

	if (tick_kills_time != game_time_get())
		tick_kill_count = 0;
	for (index = 0; index < tick_kill_count; index++)
	{
		short odds = (short)(1 + tick_kills[index].bonus);
		short other;
		int skull;

		for (other = 0; other < tick_kill_count; other++)
		{
			if (other != index && tick_kills[index].definition_index != NONE &&
				tick_kills[other].definition_index == tick_kills[index].definition_index &&
				tick_kills[other].owner_object_index == tick_kills[index].owner_object_index)
			{
				odds++;
				break;
			}
		}
		/* (the run's skulls, chosen at its first drop; the next of them, if
		any is left to drop) */
		if (run.pool_count == NONE)
			run_pool();
		skull = run_next();
		if (skull == NONE)
			break;
		/* (the run's first skull, until it is picked up, at any kill: the
		first kill's, and again after a revert took it away before it was
		taken; then the kill's odds, the level's own, by game.skulls_drop_chance) */
		if (skull == run.pool[0] ||
			(long)(finding_random((unsigned long)tick_kills[index].deaths) %
				((unsigned long)tick_kills[index].left * 100UL)) < (long)odds * skulls_drop_chance())
		{
			skull_drop(tick_kills[index].unit_index, skull, tick_kills[index].left, odds);
		}
	}
	tick_kill_count = 0;
}

/* the skull dropped by the dead enemy, with its cheer and confetti */
static void skull_slot_name(short slot, int skull);

/* a skull lying about is never the game's garbage: a dropped weapon come to
rest is (items.c), which goes when too many are about, or none of a co-op
host's players is near it (objects.c), and the host's deletion is its
clients' too. Taken out of the garbage, and kept out (equipment's flag) */
static void skull_keep_from_garbage(long object_index)
{
	struct object_datum *object = object_index == NONE ? NULL : object_try_and_get(object_index);

	if (!object || TEST_FLAG(object->object.flags, _object_cannot_be_garbage_bit))
		return;
	if (TEST_FLAG(object->object.flags, _object_garbage_bit))
		object_set_garbage(object_index, FALSE);
	SET_FLAG(object->object.flags, _object_cannot_be_garbage_bit, TRUE);
}

static void skull_drop(long unit_index, int skull, long left, short odds)
{
	short slot;
	long skull_index;

	if (!object_try_and_get_and_verify_type(unit_index, _object_mask_unit))
		return;
	for (slot = 0; slot < SKULLS_DROPPED_MAX && finding->skulls[slot].object_index != NONE; slot++)
		;
	if (slot == SKULLS_DROPPED_MAX)
		return;
	skull_index = unit_port_drop_object(unit_index, carried_definition);
	if (skull_index == NONE)
		return;
	skull_keep_from_garbage(skull_index);
	finding->skulls[slot].object_index = skull_index;
	skull_hums[slot] = NONE;
	skull_droppers[slot] = unit_index;
	finding->skulls[slot].next_hum_time = game_time_get();
	finding->skulls[slot].skull = skull;
	skull_slot_name(slot, skull);
	error(_error_silent, "skulls: dropped %s SKULL, %d left of %d (%d chances in %ld, at %ld%%)",
		skull_descriptions[skull].label, skulls_left_to_find(), run.pool_count, odds, left, skulls_drop_chance());
	own_sound_play(_own_sound_skull_carrier, unit_index);
	confetti(unit_index, skull_colors, (short)NUMBEROF(skull_colors), SKULL_CONFETTI_RISE);
}

/* ---------- the skulls that change things as the game runs */

/* the players' camo powerup (players.c's) */
extern boolean player_handle_powerup(long player_index, short powerup_type, short duration);

/* Envy: the flashlight's button is active camo, as the flashlight is: a
press turns it on, another off. Its charge is the flashlight's battery, on
the flashlight's meter: a full one lasts ENVY_CAMO_TICKS on, and fills from
empty in ENVY_RECHARGE_TICKS off, a part used or filled as much */
#define ENVY_CAMO_TICKS (7 * TICKS_PER_SECOND)
#define ENVY_RECHARGE_TICKS (20 * TICKS_PER_SECOND)
/* (the least charge that turns it on) */
#define ENVY_MINIMUM_CHARGE 0.05f
#define ENVY_PLAYERS 16

static struct
{
	boolean on;
	/* the last tick the button was held, so a press counts once */
	long pressed_time;
} envy[ENVY_PLAYERS];

static short envy_slot(struct unit_datum const *unit)
{
	return (short)(DATUM_INDEX_TO_ABSOLUTE_INDEX(unit->unit.player_index) % ENVY_PLAYERS);
}

/* the camo the charge left gives, or none now */
static void envy_set(struct unit_datum *unit, short slot, boolean on)
{
	struct player_datum *player = player_get(unit->unit.player_index);

	if (on)
	{
		short ticks = (short)MAX(unit->unit.integrated_light_battery * ENVY_CAMO_TICKS, 1.0f);

		envy[slot].on = player_handle_powerup(unit->unit.player_index, _player_powerup_active_camouflage, ticks);
		return;
	}
	envy[slot].on = FALSE;
	/* (its camo out at the next tick, as the powerup's end takes it) */
	if (player->powerup_durations[_player_powerup_active_camouflage] > 1)
		player->powerup_durations[_player_powerup_active_camouflage] = 1;
}

int skulls_envy(long unit_index)
{
	struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	long now = game_time_get();
	short slot;
	boolean pressed;

	if (!unit || unit->unit.player_index == NONE || !skull_active(_skull_envy))
		return FALSE;
	slot = envy_slot(unit);
	/* (a press: not held from the tick before; a revert to before it, a new
	one; asked again in the same tick, the same) */
	if (envy[slot].pressed_time == now)
		return TRUE;
	pressed = envy[slot].pressed_time != now - 1;
	envy[slot].pressed_time = now;
	if (!pressed)
		return TRUE;
	if (envy[slot].on)
		envy_set(unit, slot, FALSE);
	else if (unit->unit.integrated_light_battery >= ENVY_MINIMUM_CHARGE &&
		!TEST_FLAG(unit->unit.flags, _unit_active_camouflaged_bit))
	{
		envy_set(unit, slot, TRUE);
	}
	return TRUE;
}

int skulls_envy_update(long unit_index)
{
	struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	short slot;

	if (!unit || unit->unit.player_index == NONE || !skull_active(_skull_envy))
		return FALSE;
	slot = envy_slot(unit);
	/* (the light itself never on) */
	SET_FLAG(unit->unit.flags, _unit_integrated_light_on_bit, FALSE);
	unit->unit.integrated_light_power = 0.0f;
	/* (its camo gone another way, a death, the powerup's end: off) */
	if (envy[slot].on && (!TEST_FLAG(unit->unit.flags, _unit_active_camouflaged_bit) ||
		TEST_FLAG(unit->object.damage_flags, _object_dead_bit)))
	{
		envy[slot].on = FALSE;
	}
	if (envy[slot].on)
	{
		unit->unit.integrated_light_battery -= 1.0f / ENVY_CAMO_TICKS;
		if (unit->unit.integrated_light_battery <= 0.0f)
		{
			unit->unit.integrated_light_battery = 0.0f;
			envy_set(unit, slot, FALSE);
		}
	}
	else
		unit->unit.integrated_light_battery = MIN(unit->unit.integrated_light_battery + 1.0f / ENVY_RECHARGE_TICKS, 1.0f);
	return TRUE;
}

/* Swarm: whether a definition is a hunter's, by its tag's name, remembered
for the level (asked of every object whose vitality is read) */
#define HUNTER_KINDS 32

static struct
{
	long definition_index;
	boolean hunter;
} hunter_kinds[HUNTER_KINDS];
static short hunter_kind_count;

int skulls_object_is_hunter(long definition_index)
{
	short index;
	boolean hunter;

	for (index = 0; index < hunter_kind_count; index++)
	{
		if (hunter_kinds[index].definition_index == definition_index)
			return hunter_kinds[index].hunter;
	}
	hunter = definition_index != NONE && randomizer_unit_is_hunter(tag_get_name(definition_index));
	if (hunter_kind_count < HUNTER_KINDS)
	{
		hunter_kinds[hunter_kind_count].definition_index = definition_index;
		hunter_kinds[hunter_kind_count].hunter = hunter;
		hunter_kind_count++;
	}
	return hunter;
}

/* a level's: no hunters known, Envy ready */
static void running_skulls_reset(void)
{
	short slot;

	hurt_reset();
	tick_kill_count = 0;
	hunter_kind_count = 0;
	for (slot = 0; slot < ENVY_PLAYERS; slot++)
	{
		envy[slot].on = FALSE;
		envy[slot].pressed_time = NONE;
	}
	give_and_take_reset();
}

/* Streaking: the players' shields drain from full to none in this long,
and a kill gives back this share of them */
#define STREAKING_DRAIN_TICKS (40 * TICKS_PER_SECOND)
#define STREAKING_KILL_SHIELD 0.25f

/* Protected: an enemy without a shield (a grunt's, a jackal's) is given
one of this share of its health, which does not come back */
#define PROTECTED_SHIELD 0.75f

void skulls_protect(long unit_index)
{
	struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);

	if (!unit || !skull_active(_skull_protected) || unit->object.type != _object_type_biped ||
		unit->object.maximum_shield_vitality > 0.0f || unit->object.maximum_body_vitality <= 0.0f ||
		TEST_FLAG(unit->object.damage_flags, _object_dead_bit) || !skulls_unit_is_enemy(unit_index))
	{
		return;
	}
	unit->object.maximum_shield_vitality = unit->object.maximum_body_vitality * PROTECTED_SHIELD;
	unit->object.shield_vitality = 1.0f;
}

/* the flare of a hit on Protected's shield: the covenant shield's sparks,
blue, at the body; and when it breaks, an elite's shield breaking */
#define GIVEN_SHIELD_SPARKS 6
#define GIVEN_SHIELD_SPARK_TAG "effects\\particles\\energy\\shield c generic"
#define GIVEN_SHIELD_BREAK_TAG "characters\\elite\\effects\\elite shield depletion"

static real_rgb_color const given_shield_colors[] = { { 0.35f, 0.65f, 1.0f }, { 0.6f, 0.85f, 1.0f } };

void skulls_given_shield_hit(long object_index, int broken)
{
	struct object_datum *object = object_try_and_get(object_index);
	real_point3d center;

	if (!object)
		return;
	center = object->object.bounding_sphere_center;
	confetti_throw(tag_loaded(PARTICLE_TAG, GIVEN_SHIELD_SPARK_TAG), GIVEN_SHIELD_SPARKS, &center, 0.4f, 0.1f, 0.16f,
		given_shield_colors, (short)NUMBEROF(given_shield_colors));
	if (broken)
	{
		effect_at_unit(GIVEN_SHIELD_BREAK_TAG, object_index);
		error(_error_silent, "skulls: protected's shield on %s broke", tag_get_name(object->definition_index));
	}
}

/* Give & Take: each player's shots not yet known to have hit (the game times
they were fired, oldest first): a hit on an enemy within this long takes
the oldest away and heals; one older is a miss, and hurts. Health comes
back by nothing else (health packs are left: players.c) */
#define GIVE_AND_TAKE_PLAYERS 4
#define GIVE_AND_TAKE_SHOTS 32
#define GIVE_AND_TAKE_HIT_TICKS (TICKS_PER_SECOND * 3 / 2)
#define GIVE_AND_TAKE_HIT_HEALTH 0.03f
#define GIVE_AND_TAKE_MISS_SHIELD 0.04f
#define GIVE_AND_TAKE_MISS_HEALTH 0.02f
/* (a miss never kills: it leaves this much health) */
#define GIVE_AND_TAKE_MISS_FLOOR 0.05f

static struct
{
	long unit_index;
	long fired[GIVE_AND_TAKE_SHOTS];
	short count;
} give_and_take_shots[GIVE_AND_TAKE_PLAYERS];

static short give_and_take_slot(long unit_index, boolean add)
{
	short slot;
	short free_slot = NONE;

	for (slot = 0; slot < GIVE_AND_TAKE_PLAYERS; slot++)
	{
		if (give_and_take_shots[slot].unit_index == unit_index && give_and_take_shots[slot].count >= 0)
			return slot;
		if (free_slot == NONE && (give_and_take_shots[slot].count == 0 ||
			!object_try_and_get_and_verify_type(give_and_take_shots[slot].unit_index, _object_mask_unit)))
		{
			free_slot = slot;
		}
	}
	if (!add || free_slot == NONE)
		return NONE;
	give_and_take_shots[free_slot].unit_index = unit_index;
	give_and_take_shots[free_slot].count = 0;
	return free_slot;
}

static void give_and_take_reset(void)
{
	short slot;

	for (slot = 0; slot < GIVE_AND_TAKE_PLAYERS; slot++)
	{
		give_and_take_shots[slot].unit_index = NONE;
		give_and_take_shots[slot].count = 0;
	}
}

void skulls_shot_fired(long unit_index)
{
	short slot;

	if (!skull_active(_skull_give_and_take) || network_game_distributed_client() || !skulls_unit_is_player(unit_index))
		return;
	slot = give_and_take_slot(unit_index, TRUE);
	if (slot == NONE)
		return;
	/* (full: the oldest is let go, neither hit nor missed) */
	if (give_and_take_shots[slot].count == GIVE_AND_TAKE_SHOTS)
	{
		memmove(give_and_take_shots[slot].fired, give_and_take_shots[slot].fired + 1,
			(GIVE_AND_TAKE_SHOTS - 1) * sizeof(give_and_take_shots[slot].fired[0]));
		give_and_take_shots[slot].count--;
	}
	give_and_take_shots[slot].fired[give_and_take_shots[slot].count++] = game_time_get();
}

void skulls_damage_dealt(long object_index, long owner_player_index)
{
	struct player_datum *player;
	struct unit_datum *unit;
	short slot;

	if (!skull_active(_skull_give_and_take) || owner_player_index == NONE || !skulls_unit_is_enemy(object_index))
		return;
	player = player_get(owner_player_index);
	unit = player->unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
	if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		return;
	/* (a hit: its shot no miss; and some health back) */
	slot = give_and_take_slot(player->unit_index, FALSE);
	if (slot != NONE && give_and_take_shots[slot].count > 0)
	{
		memmove(give_and_take_shots[slot].fired, give_and_take_shots[slot].fired + 1,
			(GIVE_AND_TAKE_SHOTS - 1) * sizeof(give_and_take_shots[slot].fired[0]));
		give_and_take_shots[slot].count--;
	}
	unit->object.body_vitality += GIVE_AND_TAKE_HIT_HEALTH;
	if (unit->object.body_vitality > 1.0f)
		unit->object.body_vitality = 1.0f;
}

/* the shots that hit nothing in time: each takes some shield, then health */
static void give_and_take_tick(void)
{
	long now = game_time_get();
	short slot;

	if (!skull_active(_skull_give_and_take) || network_game_distributed_client())
		return;
	for (slot = 0; slot < GIVE_AND_TAKE_PLAYERS; slot++)
	{
		struct unit_datum *unit;

		if (give_and_take_shots[slot].count <= 0)
			continue;
		unit = (struct unit_datum *)object_try_and_get_and_verify_type(give_and_take_shots[slot].unit_index,
			_object_mask_unit);
		/* (gone, dead, or a revert to before the shots) */
		if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ||
			now < give_and_take_shots[slot].fired[0])
		{
			give_and_take_shots[slot].count = 0;
			continue;
		}
		while (give_and_take_shots[slot].count > 0 && now - give_and_take_shots[slot].fired[0] > GIVE_AND_TAKE_HIT_TICKS)
		{
			memmove(give_and_take_shots[slot].fired, give_and_take_shots[slot].fired + 1,
				(GIVE_AND_TAKE_SHOTS - 1) * sizeof(give_and_take_shots[slot].fired[0]));
			give_and_take_shots[slot].count--;
			if (unit->object.shield_vitality > 0.0f)
			{
				unit->object.shield_vitality -= GIVE_AND_TAKE_MISS_SHIELD;
				if (unit->object.shield_vitality < 0.0f)
					unit->object.shield_vitality = 0.0f;
			}
			else if (unit->object.body_vitality > GIVE_AND_TAKE_MISS_FLOOR)
			{
				unit->object.body_vitality -= GIVE_AND_TAKE_MISS_HEALTH;
				if (unit->object.body_vitality < GIVE_AND_TAKE_MISS_FLOOR)
					unit->object.body_vitality = GIVE_AND_TAKE_MISS_FLOOR;
			}
		}
	}
}

/* the units' skulls, each tick: Assassins cloaks the enemies (those placed
before it was on too), Protected shields them, Streaking drains the
players' shields; where the shields are run (not a distributed client's:
the host's) */
static void skulls_units_tick(void)
{
	struct object_iterator iterator;
	struct unit_datum *unit;
	boolean assassins = (skull_active(_skull_assassins) || skull_active(_skull_need_glasses)) &&
		game_time_get() % TICKS_PER_SECOND == 0;
	boolean protect = skull_active(_skull_protected) && !network_game_distributed_client() &&
		game_time_get() % TICKS_PER_SECOND == 0;
	/* (Streaking stops for a cutscene) */
	boolean streaking = skull_active(_skull_streaking) && !network_game_distributed_client() &&
		!cinematic_in_progress();

	give_and_take_tick();
	if (!assassins && !streaking && !protect)
		return;
	object_iterator_new(&iterator, _object_mask_unit, 0);
	while ((unit = object_iterator_next(&iterator)) != NULL)
	{
		if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		if (streaking && unit->unit.player_index != NONE && unit->object.shield_vitality > 0.0f)
		{
			unit->object.shield_vitality -= 1.0f / STREAKING_DRAIN_TICKS;
			if (unit->object.shield_vitality < 0.0f)
				unit->object.shield_vitality = 0.0f;
		}
		if (protect && unit->object.maximum_shield_vitality <= 0.0f)
			skulls_protect(iterator.index);
		/* (Assassins' enemies, Need Glasses?'s every unit: skulls_custom.c) */
		if (assassins && !TEST_FLAG(unit->unit.flags, _unit_active_camouflaged_bit) &&
			skulls_unit_cloaked(iterator.index))
		{
			SET_FLAG(unit->unit.flags, _unit_active_camouflaged_bit, TRUE);
			unit->unit.active_camouflage = 1.0f;
		}
	}
}

/* Streaking: a kill of the player's gives back some shield */
static void streaking_kill(long unit_index, long killer_player_index)
{
	struct player_datum *player;
	struct unit_datum *killer;

	if (!skull_active(_skull_streaking) || killer_player_index == NONE || network_game_distributed_client() ||
		!skulls_unit_is_enemy(unit_index))
	{
		return;
	}
	player = player_get(killer_player_index);
	killer = player->unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
	if (!killer)
		return;
	killer->object.shield_vitality += STREAKING_KILL_SHIELD;
	if (killer->object.shield_vitality > 1.0f)
		killer->object.shield_vitality = 1.0f;
}

void skulls_tick(void)
{
	long now;
	short slot;

	skulls_units_tick();
	skulls_outbreak_tick();
	skulls_custom_tick();
	if (carried_definition == NONE)
		return;
	tick_kills_decide();
	now = game_time_get();
	if (now < skull_hums_time)
	{
		for (slot = 0; slot < SKULLS_DROPPED_MAX; slot++)
			skull_hums[slot] = NONE;
	}
	skull_hums_time = now;
	for (slot = 0; slot < SKULLS_DROPPED_MAX; slot++)
	{
		long skull_index = finding->skulls[slot].object_index;
		struct weapon_datum *skull = skull_index == NONE ? NULL :
			(struct weapon_datum *)object_try_and_get_and_verify_type(skull_index, _object_mask_weapon);

		if (skull_index == NONE)
			continue;
		/* (gone, or another object in its place after a revert; a co-op
		client's are the host's word, which may come before the object) */
		if (!skull || skull->definition_index != carried_definition)
		{
			if (!network_game_distributed_client())
				finding->skulls[slot].object_index = NONE;
			continue;
		}
		/* (never garbage: a co-op client's copy too, which comes after the
		host's word of it) */
		skull_keep_from_garbage(skull_index);
		/* (in a unit's hands, which a revert may bring back: not heard) */
		if (skull->object.parent_object_index != NONE)
			continue;
		/* (come to rest once, on the level or on an object: no blast moves it
		again, as a weapon the map places that does not accelerate (items.c);
		taken up and dropped again, the flag is cleared, and it falls anew) */
		if (TEST_FLAG(skull->item.flags, _item_on_structure_bit) || TEST_FLAG(skull->item.flags, _item_on_object_bit))
			SET_FLAG(skull->item.flags, _item_does_not_accelerate_bit, TRUE);
		if (now >= finding->skulls[slot].next_hum_time)
		{
			skull_hums[slot] = own_sound_play(_own_sound_skull_floor, skull_index);
			/* (again as it ends: it loops) */
			finding->skulls[slot].next_hum_time = now +
				MAX(1, own_sounds[_own_sound_skull_floor].milliseconds * TICKS_PER_SECOND / 1000);
		}
	}
	/* (a co-op host's state, to its clients when it changed) */
	skulls_send_state(NONE);
}

/* whether a death (and a revert) loses the skulls found and not kept
(game.skulls_lose_on_death): else they stay on, to be kept when the level
is won */
static boolean skulls_lost_on_death(void)
{
	/* (a co-op client's: the host's rule) */
	if (network_game_distributed_client())
		return (boolean)remote_lose_on_death;
	return config_boolean("game.skulls_lose_on_death");
}

/* whether a skull picked up is unlocked at once (game.skulls_instant_unlock;
a co-op client's, the host's) */
static boolean skulls_instant(void)
{
	if (network_game_distributed_client())
		return (boolean)remote_instant;
	return config_boolean("game.skulls_instant_unlock");
}

static short skull_slot(long object_index)
{
	short slot;

	if (object_index == NONE || carried_definition == NONE)
		return NONE;
	for (slot = 0; slot < SKULLS_DROPPED_MAX; slot++)
	{
		if (finding->skulls[slot].object_index == object_index && finding->skulls[slot].skull != NONE)
			return slot;
	}
	return NONE;
}

float skulls_item_push_scale(long object_index)
{
	/* (a skull still falling from its enemy: heavy, a blast throws it a fifth
	as far as another item; landed, none at all: skulls_tick) */
	return skull_slot(object_index) != NONE ? SKULL_FALLING_PUSH_SCALE : 1.0f;
}

unsigned short const *skulls_item_name(long object_index)
{
	short slot = skull_slot(object_index);

	return slot == NONE ? NULL : (unsigned short const *)finding->skulls[slot].name;
}

/* a line of text on the local player's screen */
static void skulls_print(short local_player_index, char const *text)
{
	wchar_t line[64];
	short character;

	for (character = 0; text[character] && character < (short)NUMBEROF(line) - 1; character++)
		line[character] = (wchar_t)(unsigned char)text[character];
	line[character] = 0;
	hud_print_message(local_player_index, line);
}

/* what the local player is told of a skull taken (by a player of theirs,
or another's in co-op): unlocked at once, or on for the level; and how
many are left. A skull the player had already is only said (the newest
message is shown on top: printed last line first) */
static void skull_taken_messages(short local_player_index, int skull, boolean instant, boolean had)
{
	char text[64];
	short left = skulls_left_to_find();

	if (left == 0)
		snprintf(text, sizeof(text), "No more skulls to be found in here.");
	else
		snprintf(text, sizeof(text), "%d skull%s left to be found in here.", left, left == 1 ? "" : "s");
	skulls_print(local_player_index, text);
	if (had)
	{
		snprintf(text, sizeof(text), "%s SKULL found. You have it already.", skull_descriptions[skull].label);
		skulls_print(local_player_index, text);
		return;
	}
	if (!instant && skulls_lost_on_death())
	{
		skulls_print(local_player_index, "Death will take your prize away.");
		skulls_print(local_player_index, "Finish the level without dying to unlock it.");
	}
	else if (!instant)
		skulls_print(local_player_index, "Finish the level to unlock it.");
	snprintf(text, sizeof(text), instant ? "%s SKULL unlocked." : "%s SKULL picked up.",
		skull_descriptions[skull].label);
	skulls_print(local_player_index, text);
}

int skulls_item_take(short local_player_index, long unit_index, long object_index)
{
	short slot = skull_slot(object_index);
	int skull;
	boolean instant;

	if (slot == NONE)
		return FALSE;
	skull = finding->skulls[slot].skull;
	instant = skulls_instant();
	/* (on for the level from now on, kept once it is finished; or with
	INSTANT UNLOCK, kept now, and not on) */
	skull_set_add(&run.taken, skull);
	if (instant)
	{
		skulls_keep(skull_set_of(skull));
		skull_set_add(&run.unlocked, skull);
	}
	else
		skull_set_add(&finding->found, skull);
	run_saved();
	if (skull_hums[slot] != NONE)
		sound_stop_impulse(skull_hums[slot]);
	finding->skulls[slot].object_index = NONE;
	finding->skulls[slot].skull = NONE;
	skull_droppers[slot] = NONE;
	object_delete(object_index);
	own_sound_play(_own_sound_skull_pickup, unit_index);
	error(_error_silent, "skulls: %s taken%s", skull_descriptions[skull].key, instant ? ", and unlocked" : "");
	/* (the player who took it; in co-op, every player of the host's: the
	clients are told by the host's state, skulls_send_state) */
	if (game_connection() == _game_connection_local)
	{
		if (local_player_index != NONE)
			skull_taken_messages(local_player_index, skull, instant, FALSE);
	}
	else
	{
		for (local_player_index = 0; local_player_index < local_player_count(); local_player_index++)
			skull_taken_messages(local_player_index, skull, instant, FALSE);
	}
	return TRUE;
}

/* the skulls of the set lost to the run, each said once on the screen;
to be found again in another level */
static void run_lose(struct skull_set set, char const *why)
{
	short local_player_index;
	int skull;

	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		char text[64];

		if (!skull_set_has(&set, skull) || skull_set_has(&run.lost, skull))
			continue;
		skull_set_add(&run.lost, skull);
		error(_error_silent, "skulls: %s lost (%s)", skull_descriptions[skull].key, why);
		snprintf(text, sizeof(text), "%s SKULL lost.", skull_descriptions[skull].label);
		for (local_player_index = 0; local_player_index < local_player_count(); local_player_index++)
			skulls_print(local_player_index, text);
	}
	run_saved();
}

/* a player died: the skulls found and not kept are lost */
static void skulls_lose(char const *why)
{
	struct skull_set set = found_set();

	/* (a co-op client's: the host loses them for every player, and says) */
	if (skull_set_is_empty(&set) || !skulls_lost_on_death() || network_game_distributed_client())
		return;
	finding->found = skull_set_none();
	run_lose(set, why);
}

/* the game state put back (a revert, or a saved game loaded): the skulls
picked up and not unlocked are lost (any death loses them), whatever the
game state found then; one dropped and not taken, gone with the revert,
is to drop again */
static void run_restored(char const *why)
{
	/* (a co-op client's: the host's state, which it sends again) */
	if (network_game_distributed_client())
		return;
	/* (or, kept through a death, those picked up and neither unlocked nor
	lost found again, whatever the game state found then) */
	if (!skulls_lost_on_death())
	{
		finding->found = skull_set_difference(skull_set_difference(run.taken, run.unlocked), run.lost);
		return;
	}
	finding->found = skull_set_none();
	run_lose(skull_set_difference(run.taken, run.unlocked), why);
}

/* the level won: the skulls found in it, and not lost to a death or a
revert since, are kept (game.skulls_unlocked), as the HUD said they would
be when they were picked up */
void skulls_level_won(void)
{
	struct skull_set set = found_set();
	char text[SKULLS_TEXT_LENGTH];

	/* (a co-op client's: those the host unlocks now, which it says) */
	if (skull_set_is_empty(&set) || network_game_distributed_client())
		return;
	skulls_keep(set);
	run.unlocked = skull_set_union(run.unlocked, set);
	run_saved();
	skulls_text_from_set(set, text, sizeof(text));
	error(_error_silent, "skulls: the level won, %s kept", text);
	/* (and its clients' players keep them too, before the round ends) */
	skulls_send_state(NONE);
}

void skulls_game_state_reverted(void)
{
	hurt_reset();
	/* (the run is this machine's: the game state's copy, of then, is not) */
	run_restored("the game went back to a checkpoint");
}

void skulls_game_state_loaded(void)
{
	/* (the run is the saved game's: this machine had none of it) */
	run = finding->run;
	run_restored("a saved game was loaded");
}

/* ---------- the skulls on, on the screen

each skull on in the level a small icon at the bottom right, in a row from
the right (on top of another past ten): those turned on, and those found
and not yet kept, pulsing, those unlocked in the level, checked green,
those dropped and lying about, marked yellow, and those lost, crossed out.
The icons are Halopedia's of the Master Chief
Collection's (MCC:Skulls), the menus' bitmap skulls/icons, a frame each in
the skulls' order (port/assets/skulls, tools/ce_menus.py); none without the
menus' bitmaps (display.menus "xbox") */
#define SKULL_ICONS_BITMAP "pc\\skulls\\icons"
#define SKULL_ICON_SIZE 22
#define SKULL_ICON_SPACING 24
#define SKULL_ICON_MARGIN 12
#define SKULL_ICONS_PER_ROW 10

void skulls_hud_draw(short local_player_index)
{
	long bitmap_index;
	struct skull_set found = found_set();
	struct skull_set lying = carried_definition != NONE ? skulls_lying() : skull_set_none();
	struct skull_set unlocked = run.unlocked;
	struct skull_set shown_set = skull_set_union(skull_set_union(level_set, found),
		skull_set_union(skull_set_union(run.lost, lying), unlocked));
	real scale;
	short width, height;
	short shown = 0;
	short lift;
	int skull;

	(void)local_player_index;
	if (skull_set_is_empty(&shown_set) || !global_scenario || global_scenario->type != _scenario_type_solo)
		return;
	bitmap_index = tag_loaded(BITMAP_GROUP_TAG, SKULL_ICONS_BITMAP);
	if (bitmap_index == NONE)
		return;
	width = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	height = (short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	scale = (real)height / 480.0f;
	/* (above the campaign's score and time, where they are drawn, in the
	same corner: campaign_score.c) */
	{
		extern short campaign_score_hud_height(void);

		lift = campaign_score_hud_height();
	}
	for (skull = NUMBER_OF_SKULLS - 1; skull >= 0; skull--)
	{
		struct bitmap_data *bitmap;
		rectangle2d bounds;
		real alpha = 1.0f;
		short overlay;
		short column, row;

		if (!skull_set_has(&shown_set, skull))
			continue;
		bitmap = bitmap_group_try_and_get_bitmap(bitmap_index, (short)skull);
		if (!bitmap)
			continue;
		column = (short)(shown % SKULL_ICONS_PER_ROW);
		row = (short)(shown / SKULL_ICONS_PER_ROW);
		shown++;
		bounds.x1 = (short)(width - (SKULL_ICON_MARGIN + column * SKULL_ICON_SPACING) * scale);
		bounds.x0 = (short)(bounds.x1 - SKULL_ICON_SIZE * scale);
		bounds.y1 = (short)(height - (SKULL_ICON_MARGIN + lift + row * SKULL_ICON_SPACING) * scale);
		bounds.y0 = (short)(bounds.y1 - SKULL_ICON_SIZE * scale);
		/* (found, not yet kept: pulsing, once a second; unlocked in the
		level: faint, a green check on it; dropped and lying about: faint, a
		yellow mark on it; lost: faint, crossed out in red. The marks are the
		frames after the skulls': the cross, the mark, the check) */
		if (skull_set_has(&level_set, skull))
			overlay = NONE;
		else if (skull_set_has(&unlocked, skull))
		{
			alpha = 0.6f;
			overlay = NUMBER_OF_SKULLS + 2;
		}
		else if (skull_set_has(&found, skull))
		{
			alpha = 0.6f + 0.4f * cosine((real)(game_time_get() % TICKS_PER_SECOND) * (2.0f * _pi / TICKS_PER_SECOND));
			overlay = NONE;
		}
		else if (skull_set_has(&lying, skull))
		{
			alpha = 0.45f;
			overlay = NUMBER_OF_SKULLS + 1;
		}
		else
		{
			alpha = 0.45f;
			overlay = NUMBER_OF_SKULLS;
		}
		draw_bitmap_in_rect(bitmap, &bounds, NULL, NULL,
			((pixel32)(alpha * 255.0f + 0.5f) << 24) | 0x00FFFFFF, NULL, TRUE);
		if (overlay != NONE)
		{
			struct bitmap_data *mark = bitmap_group_try_and_get_bitmap(bitmap_index, overlay);

			if (mark)
				draw_bitmap_in_rect(mark, &bounds, NULL, NULL, 0xFFFFFFFF, NULL, TRUE);
		}
	}
}

/* ---------- co-op: the host's skulls to be found, on its clients

The host's state of the skulls to be found, whole: those taken, found (on
for the level), unlocked and lost, how many are left, its rules, and the
skulls lying about (each its object, at the same index on every machine,
and the enemy that dropped it). Sent to every client, reliably, when it
changes and every few seconds (a client's revert brings back its own game
state's), and to a client when it is ready. A client takes it whole, and
plays what changed: a skull dropped (its cheer and confetti), taken (the
messages, and kept at once with INSTANT UNLOCK), lost, unlocked (kept). */

struct skulls_state_message
{
	struct distributed_message_header header;
	byte taken[HALO_PORT_ADVERTISED_SKULLS_SIZE];
	byte found[HALO_PORT_ADVERTISED_SKULLS_SIZE];
	byte unlocked[HALO_PORT_ADVERTISED_SKULLS_SIZE];
	byte lost[HALO_PORT_ADVERTISED_SKULLS_SIZE];
	byte left;
	/* bit 0 INSTANT UNLOCK, bit 1 LOSE ON DEATH */
	byte rules;
	byte reserved[2];
	struct
	{
		long object_index;
		long dropper_index;
		long skull;
	} lying[SKULLS_DROPPED_MAX];
};

#define SKULLS_STATE_RESEND_TICKS (3 * TICKS_PER_SECOND)

/* the host's state as last sent to every client, and when */
static struct skulls_state_message skulls_sent_state;
static long skulls_sent_state_time;

static void skulls_state_build(struct skulls_state_message *message)
{
	short slot;

	csmemset(message, 0, sizeof(*message));
	skull_set_to_bytes(run.taken, message->taken);
	skull_set_to_bytes(found_set(), message->found);
	skull_set_to_bytes(run.unlocked, message->unlocked);
	skull_set_to_bytes(run.lost, message->lost);
	message->left = (byte)PIN(skulls_left_to_find(), 0, 255);
	message->rules = (byte)((skulls_instant() ? 1 : 0) | (skulls_lost_on_death() ? 2 : 0));
	for (slot = 0; slot < SKULLS_DROPPED_MAX; slot++)
	{
		long object_index = finding->skulls[slot].object_index;
		int skull = finding->skulls[slot].skull;

		message->lying[slot].object_index = object_index != NONE && skull != NONE ? object_index : NONE;
		message->lying[slot].skull = object_index != NONE && skull != NONE ? skull : NONE;
		message->lying[slot].dropper_index = object_index != NONE && skull != NONE ? skull_droppers[slot] : NONE;
	}
}

void skulls_send_state(long machine_index)
{
	struct skulls_state_message message;

	/* (the host of a co-op game whose enemies drop skulls) */
	if (game_connection() != _game_connection_network_server || !finding || carried_definition == NONE)
		return;
	skulls_state_build(&message);
	if (machine_index != NONE)
	{
		distributed_send_to_machine_reliably(machine_index, &message, _distributed_message_skulls_state, 1,
			sizeof(message));
		return;
	}
	/* (to every client: when it changed, and every few seconds) */
	if (skulls_sent_state_valid &&
		!memcmp((byte const *)&message + sizeof(message.header), (byte const *)&skulls_sent_state + sizeof(message.header),
			sizeof(message) - sizeof(message.header)) &&
		game_time_get() >= skulls_sent_state_time && game_time_get() - skulls_sent_state_time < SKULLS_STATE_RESEND_TICKS)
	{
		return;
	}
	distributed_send(&message, _distributed_message_skulls_state, 1, sizeof(message), _distributed_to_clients_reliably);
	skulls_sent_state = message;
	skulls_sent_state_valid = TRUE;
	skulls_sent_state_time = game_time_get();
}

int skulls_handles_message(unsigned short const *message, unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return FALSE;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_skulls_state;
}

/* the skulls a player of this machine's has (Grunt Birthday Party from the
start), to be kept: those kept, ALL SKULLS or not */
static struct skull_set skulls_kept_set(void)
{
	return skull_set_union(skulls_set_from_text(config_string("game.skulls_unlocked")),
		skull_set_of(_skull_grunt_birthday_party));
}

/* a slot's name, as the pick-up prompt shows it */
static void skull_slot_name(short slot, int skull)
{
	char name[SKULL_NAME_LENGTH];
	short character;

	snprintf(name, sizeof(name), "%s SKULL", skull_descriptions[skull].label);
	for (character = 0; name[character] && character < SKULL_NAME_LENGTH - 1; character++)
		finding->skulls[slot].name[character] = (wchar_t)(unsigned char)name[character];
	finding->skulls[slot].name[character] = 0;
}

void skulls_handle_message(long machine_index, unsigned short const *data, unsigned short size)
{
	struct skulls_state_message message;
	struct skull_set taken, unlocked, lost, newly;
	struct skull_set had = skulls_unlocked_set();
	struct skull_set kept = skulls_kept_set();
	short local_player_index;
	short slot;
	int skull;
	boolean first = FALSE;

	if (machine_index != NONE || !network_game_distributed_client() || size != sizeof(message))
		return;
	memcpy(&message, data, sizeof(message));
	if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.type != _distributed_message_skulls_state ||
		message.header.count != 1 || message.rules > 3)
	{
		return;
	}
	taken = skull_set_from_bytes(message.taken);
	unlocked = skull_set_from_bytes(message.unlocked);
	lost = skull_set_from_bytes(message.lost);
	remote_instant = (message.rules & 1) != 0;
	remote_lose_on_death = (message.rules & 2) != 0;
	remote_left = message.left;
	/* (the first since the level loaded: taken as it is, nothing played or
	kept, a player joining the level late gets none taken before) */
	if (!skulls_state_heard && finding && game_in_progress())
	{
		skulls_state_heard = TRUE;
		first = TRUE;
		run.taken = taken;
		run.unlocked = unlocked;
		run.lost = lost;
	}
	/* (kept: those the host unlocked, at once or with the level won, that
	this machine has not kept; a game ending may leave no level to show it) */
	newly = skull_set_difference(skull_set_difference(unlocked, run.unlocked), kept);
	if (!skull_set_is_empty(&newly))
	{
		char text[SKULLS_TEXT_LENGTH];

		skulls_keep(newly);
		skulls_text_from_set(newly, text, sizeof(text));
		error(_error_silent, "skulls: the host unlocked %s, kept", text);
	}
	run.unlocked = unlocked;
	if (!finding || !game_in_progress())
	{
		run.taken = taken;
		run.lost = lost;
		return;
	}
	/* (lost: said) */
	run_lose(skull_set_difference(lost, run.lost), "the host lost it");
	run.lost = lost;
	/* (taken: said to every player of this machine's, a skull had already
	only as found) */
	newly = skull_set_difference(taken, run.taken);
	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		if (!skull_set_has(&newly, skull))
			continue;
		error(_error_silent, "skulls: %s taken (the host's)", skull_descriptions[skull].key);
		for (local_player_index = 0; local_player_index < local_player_count(); local_player_index++)
		{
			long player_index = local_player_get_player_index(local_player_index);
			struct player_datum *player = player_index == NONE ? NULL : player_get(player_index);

			skull_taken_messages(local_player_index, skull, (boolean)remote_instant,
				(boolean)(skull_set_has(&had, skull) || skull_set_has(&kept, skull)));
			if (local_player_index == 0 && player && player->unit_index != NONE)
				own_sound_play(_own_sound_skull_pickup, player->unit_index);
		}
	}
	run.taken = taken;
	finding->found = skull_set_from_bytes(message.found);
	/* (lying about: a new one's cheer and confetti, from the enemy that
	dropped it) */
	for (slot = 0; slot < SKULLS_DROPPED_MAX; slot++)
	{
		long object_index = message.lying[slot].object_index;
		long dropper_index = message.lying[slot].dropper_index;

		skull = (int)message.lying[slot].skull;
		if (object_index == NONE || skull < 0 || skull >= NUMBER_OF_SKULLS)
		{
			object_index = NONE;
			skull = NONE;
		}
		if (object_index != finding->skulls[slot].object_index)
		{
			if (skull_hums[slot] != NONE)
				sound_stop_impulse(skull_hums[slot]);
			skull_hums[slot] = NONE;
			finding->skulls[slot].next_hum_time = game_time_get();
			if (!first && object_index != NONE && dropper_index != NONE &&
				object_try_and_get_and_verify_type(dropper_index, _object_mask_unit))
			{
				own_sound_play(_own_sound_skull_carrier, dropper_index);
				confetti(dropper_index, skull_colors, (short)NUMBEROF(skull_colors), SKULL_CONFETTI_RISE);
			}
		}
		finding->skulls[slot].object_index = object_index;
		finding->skulls[slot].skull = skull;
		skull_droppers[slot] = dropper_index;
		if (skull != NONE)
			skull_slot_name(slot, skull);
		else
			finding->skulls[slot].name[0] = 0;
	}
	run_saved();
}

/* ---------- co-op: the level's tags, alike on every machine

A modded co-op game's machines copy tags into the level (the randomizer's,
the skulls'), each by the host's advertisement, before their own menus' and
mods' (cache_files.c); the host's objects name them by index, so a client
whose shared tags differ has the host's enemies and skulls as other things,
or none. The host sends its table's count and
fingerprint to a client when it is ready, and the client says so, loudly,
when its own differ. */

struct mod_tags_message
{
	struct distributed_message_header header;
	long count;
	unsigned long hash;
};

void skulls_send_tags(long machine_index)
{
	struct mod_tags_message message;
	extern int randomizer_show_prefix(void);
	extern boolean tag_multiplayer_hosted(void);

	if (game_connection() != _game_connection_network_server || machine_index == NONE ||
		!(randomizer_show_prefix() || skulls_hosted() || tag_multiplayer_hosted()))
	{
		return;
	}
	csmemset(&message, 0, sizeof(message));
	message.count = shared_tag_count;
	message.hash = shared_tag_hash;
	distributed_send_to_machine_reliably(machine_index, &message, _distributed_message_mod_tags, 1, sizeof(message));
}

int skulls_tags_handles_message(unsigned short const *message, unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return FALSE;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_mod_tags;
}

void skulls_tags_handle_message(long machine_index, unsigned short const *data, unsigned short size)
{
	struct mod_tags_message message;
	long count = shared_tag_count;
	unsigned long hash = shared_tag_hash;
	short local_player_index;

	if (machine_index != NONE || !network_game_distributed_client() || size != sizeof(message))
		return;
	memcpy(&message, data, sizeof(message));
	if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.count != 1)
		return;
	if (count == message.count && hash == message.hash)
	{
		error(_error_silent, "skulls: the level's tags are the host's (%ld, %08lx)", count, hash);
		return;
	}
	error(_error_silent, "skulls: the level's tags differ from the host's: %ld tags, %08lx here; %ld, %08lx on the "
		"host. Its enemies, weapons and skulls will not match", count, hash, message.count, message.hash);
	platform_log("skulls: this level's tags differ from the host's (%ld here, %ld there): the randomizer's and the "
		"skulls' copies are not the same on both machines", count, message.count);
	for (local_player_index = 0; local_player_index < local_player_count(); local_player_index++)
	{
		skulls_print(local_player_index, "Expect enemies and items out of sync.");
		skulls_print(local_player_index, "This level loaded differently from the host's.");
	}
}

/* ---------- a unit's death */

void skulls_unit_killed(long unit_index, int headshot, long killer_player_index)
{
	streaking_kill(unit_index, killer_player_index);
	/* (a player's death: the skulls found and not kept are lost) */
	if (skulls_unit_is_player(unit_index))
		skulls_lose("a player died");
	/* (the Master Chief Collection's confetti and cheer) */
	if (headshot && ((skull_active(_skull_grunt_birthday_party) && skulls_unit_is_grunt(unit_index)) ||
		(skull_active(_skull_confetti_party) && skulls_unit_is_enemy(unit_index))))
	{
		confetti(unit_index, party_colors, (short)NUMBEROF(party_colors), 0.0f);
		own_sound_play(_own_sound_party, unit_index);
	}
	if (carried_definition != NONE)
		skull_drop_maybe(unit_index, killer_player_index, headshot);
	skulls_outbreak_kill(unit_index, killer_player_index);
}

void skulls_unit_died(long unit_index)
{
	/* (a plasma grenade's explosion: its damage the host's alone, as an
	effect's damage is) */
	if (skull_active(_skull_grunt_funeral) && skulls_unit_is_grunt(unit_index))
		effect_at_unit("weapons\\plasma grenade\\effects\\explosion", unit_index);
}

void skulls_initialize(void)
{
	/* (a multiple of four bytes, as the game state allocates) */
	finding = (struct skulls_finding *)game_state_malloc("skulls finding", NULL,
		(long)((sizeof(*finding) + 3) & ~(unsigned long)3));
	finding_reset();
	skulls_outbreak_initialize();
}

void skulls_initialize_for_new_map(void)
{
	finding_reset();
	run_reset();
	skulls_outbreak_initialize_for_new_map();
}
