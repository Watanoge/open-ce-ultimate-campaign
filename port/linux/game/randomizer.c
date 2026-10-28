/*
RANDOMIZER.C

A campaign level can swap its actors and the weapons they carry, and the
weapons lying in it, without changing the map file. The actors are taken
from a pool (any of them, one faction, or one type). A tag the level does
not have is copied from another campaign map, dependencies included, and
appended to the tag table in map order, then by name, the same on every
machine. A bitmap's pixels and a sound's samples are read from
maps/randomizer.pack, which the game writes from the campaign maps at the
main menu (randomizer_pack.c; tools/randomizer_pack.py writes the same
file); otherwise from the other map.

The choice is a hash of the map, the setup, the placement and a roll made
when the level loads, so starting it again is a different layout. A
checkpoint reloads the saved actors, not the tags. A starting location with
a command list, and a squad named by a conversation, keep the actor the
level gave them, and so do the player's allies unless game.randomizer_allies
is on. A weapon a script names is left where it was.

A swapped actor a script seats in a vehicle (vehicle_load_magic: a
dropship's Covenant, a Pelican's marines) whose unit has the animations for
none of its seats is placed back as the actor the level gave it, to ride;
once out of the seat, landed and standing, walking or fighting, it is the
swapped one again, where it stands, in the co-op teleport's flash (a Flood
form in a carrier's harmless burst) (randomizer_tick). Of a squad's infection
forms to be swapped for other actors, only a share is (one in three, or so
many tenths with Go Crazy: the squad's first, then evenly); the rest are not
placed. Go Crazy's parts are each a setting of their own
(game.randomizer_crazy_weapons, _enemies, _flamethrower, _pods). A level
played alone (not a network game, nor a challenge) is given the tags of the
Tag Injector's profile chosen (tag_injector.c), and its weapons and actor
variants on join the pools their settings name (a faction, a type, a
class: by their names when not set), the TAG INJECTOR pools, which are
theirs alone, and the one type of their custom type.

Off unless game.randomizer is on, and only for the ten campaign levels. A
multiplayer game that is not co-op is not randomized. A client uses the
setup the host advertised, not its own.

The same copying brings in the skull the enemies carry when skulls are to
be found (skulls.c): a multiplayer map's oddball, which no campaign map
has, read from maps/skulls.pack (else that map), with the randomizer on or
off. Each map read is a donor of one pack or the other.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/errors.h"
#include "cseries/cseries_windows.h"
#include "ai/actor_definitions.h"
#include "ai/actor_placement.h"
#include "ai/actors.h"
#include "ai/ai_scenario_definitions.h"
#include "bitmaps/bitmap_group.h"
#include "cache/cache_files.h"
#include "effects/effect_definitions.h"
#include "effects/effects.h"
#include "game/game.h"
#include "interface/progress_bar.h"
#include "items/weapon_definitions.h"
#include "main/main.h"
#include "models/model_animation_definitions.h"
#include "models/model_definitions.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer_geometry.h"
#include "saved games/game_state.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "sound/sound_definitions.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "units/bipeds.h"
#include "units/unit_definitions.h"
#include "units/units.h"

#include "halo_port_limits.h"
void platform_log(char const *format, ...);
#include "randomizer.h"
#include "community.h"
#include "tag_schema.h"
#include "tag_injector.h"
#include "tag_multiplayer.h"
#include "tag_campaign.h"
#include "firing_range.h"
#include "../src/port_tag_profiles.h"

#include <xtl.h>
/* the port's zlib, the same one that unpacks a map while precaching it
(source/cache/cache_files_decompress_windows.c) */
#include "../../third_party/zlib/zlib_prefixed.h"

/* the addresses a map file gives its tag pointers, wherever this process
keeps the tag cache (halo_port_window.h) */
#define DONOR_WINDOW 0x803A6000UL

/* the other nine campaign maps, the skull's (wizard) and Outbreak's Flood's
(c40, again: skulls.pack's) */
#define DONOR_COUNT 11
#define WANTED_MAX 8192
#define IMPORT_MAX 2048
#define POOL_MAX 256
#define WEAPON_MAX 128
#define PROTECTED_MAX 128
#define COPY_BUDGET (32UL * 1024UL * 1024UL)
#define WALK_DEPTH 32
/* the most weapons and actor variants of the Tag Injector's profile */
#define CUSTOM_ROOT_MAX 128

enum
{
	POOL_ANY = 0,
	POOL_COVENANT,
	POOL_FLOOD,
	POOL_HUMAN,
	POOL_ONE,
	/* the Tag Injector profile's actor variants alone */
	POOL_TAG_INJECTOR,
	TYPE_GRUNT = 0,
	TYPE_ELITE,
	TYPE_JACKAL,
	TYPE_HUNTER,
	TYPE_FLOOD_COMBAT,
	TYPE_FLOOD_CARRIER,
	TYPE_MARINE,
	TYPE_FLOOD_INFECTION,
	TYPE_ENGINEER,
	TYPE_SENTINEL,
	TYPE_COUNT,
	TYPE_NONE = -1
};

/* a variant's rank, within its type (the tags' names: characters\elite\elite
major\..., stealth elite major, elite commander is the zealot, a flood
combat form a human's or an elite's) */
enum
{
	RANK_ANY = 0,
	RANK_MINOR,
	RANK_MAJOR,
	RANK_SPECOPS,
	RANK_ZEALOT,
	RANK_STEALTH,
	RANK_HUMAN,
	RANK_ELITE,
	RANK_COUNT
};

/* the weapons a pool is made of: the campaign's hand weapons. The fuel rod
and the energy sword are only the actors' */
enum
{
	WEAPON_PISTOL = 0,
	WEAPON_ASSAULT_RIFLE,
	WEAPON_SHOTGUN,
	WEAPON_SNIPER_RIFLE,
	WEAPON_ROCKET_LAUNCHER,
	WEAPON_FLAMETHROWER,
	WEAPON_PLASMA_PISTOL,
	WEAPON_PLASMA_RIFLE,
	WEAPON_NEEDLER,
	WEAPON_FUEL_ROD,
	WEAPON_ENERGY_SWORD,
	WEAPON_KIND_COUNT,
	WEAPON_NONE = -1,
	WEAPON_POOL_ANY = 0,
	WEAPON_POOL_COVENANT,
	WEAPON_POOL_HUMAN,
	WEAPON_POOL_PRECISION,
	WEAPON_POOL_AUTOMATIC,
	WEAPON_POOL_POWER,
	WEAPON_POOL_ONE,
	/* the Tag Injector profile's weapons alone */
	WEAPON_POOL_TAG_INJECTOR,
	WEAPON_POOL_COUNT
};

/* a Tag Injector weapon's classes (its setting "class", or by its name) */
enum
{
	WEAPON_CLASS_PRECISION_BIT = 1,
	WEAPON_CLASS_AUTOMATIC_BIT = 2,
	WEAPON_CLASS_POWER_BIT = 4,
};

static struct
{
	char const *tag;
	char const *setting;
	int covenant;
	int precision;
	int automatic;
	int power;
	int actors_only;
} const weapon_kinds[WEAPON_KIND_COUNT] =
{
	{ "weapons\\pistol\\pistol", "pistol", 0, 1, 0, 0, 0 },
	{ "weapons\\assault rifle\\assault rifle", "assault_rifle", 0, 0, 1, 0, 0 },
	{ "weapons\\shotgun\\shotgun", "shotgun", 0, 0, 0, 1, 0 },
	{ "weapons\\sniper rifle\\sniper rifle", "sniper_rifle", 0, 1, 0, 1, 0 },
	{ "weapons\\rocket launcher\\rocket launcher", "rocket_launcher", 0, 0, 0, 1, 0 },
	{ "weapons\\flamethrower\\flamethrower", "flamethrower", 0, 0, 0, 1, 0 },
	{ "weapons\\plasma pistol\\plasma pistol", "plasma_pistol", 1, 0, 0, 0, 0 },
	{ "weapons\\plasma rifle\\plasma rifle", "plasma_rifle", 1, 0, 1, 0, 0 },
	{ "weapons\\needler\\needler", "needler", 1, 0, 1, 0, 0 },
	{ "weapons\\fuel rod gun\\fuel rod", "fuel_rod", 1, 0, 0, 1, 1 },
	{ "weapons\\energy sword\\energy sword", "energy_sword", 1, 0, 0, 1, 1 },
};

/* a conversation's participant, as tag_schema_scenario.c has it: the name
at 0x24 is the squad the conversation keeps */
struct randomizer_participant
{
	word pad00;
	word flags;
	short selection_type;
	short actor_type;
	short preexisting_object_name_index;
	short new_attach_object_name_index;
	byte unknown0C[0x0C];
	short dialogue_variants[6];
	char ai_index_name[32];
	long runtime_ai_index;
	byte unknown48[0x0C];
};

typedef char randomizer_participant_size[sizeof(struct randomizer_participant) == 0x54 ? 1 : -1];
typedef char randomizer_pointer_size[sizeof(void *) == 4 ? 1 : -1];

struct randomizer_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

struct randomizer_tag_header
{
	struct randomizer_tag_instance *tag_instances;
	long scenario_tag_index;
	unsigned long checksum;
	long tag_count;
	long vertex_buffer_count;
	void *vertex_buffers;
	long index_buffer_count;
	void *index_buffers;
	unsigned long signature;
};

typedef char randomizer_tag_instance_size[sizeof(struct randomizer_tag_instance) == 0x20 ? 1 : -1];
typedef char randomizer_tag_count_offset[offsetof(struct randomizer_tag_header, tag_count) == 0xC ? 1 : -1];

/* one bitmap or sound range in randomizer.pack. file_offset is the
uncompressed map offset the game asks for; data_at is where those bytes
sit in the pack. Twelve bytes, little endian, matching tools/randomizer_pack.py */
struct pack_span
{
	long file_offset;
	long size;
	long data_at;
};

typedef char pack_span_size[sizeof(struct pack_span) == 12 ? 1 : -1];

struct pack_entry
{
	char name[8];
	long file_length;
	long tag_offset;
	long tag_size;
	long tags_at;
	long resource_count;
	long resources_at;
};

/* a range of a map's pixels the pack has not got, read from the map */
struct donor_extra
{
	long offset;
	long size;
	unsigned char *bytes;
};

struct donor
{
	/* CreateFileA, not fopen: the Switch keeps map files in memory, and a
	d:\maps path is only translated by the file calls the game uses */
	HANDLE file;
	/* the tag section only. The disc file is a header plus a zlib stream;
	this is the inflated tag bytes, not the whole map */
	unsigned char *bytes;
	unsigned long size;
	long file_length;
	long tag_offset;
	int compressed;
	/* the pack this map is read from, when it has it, and whether its
	pixels and samples come from there, not this map file */
	struct pack *pack;
	int from_pack;
	struct pack_span *spans;
	long span_count;
	/* the pixels of the bitmaps copied from it that the pack has not got
	(it keeps no bitmap of the ui folder, a HUD's: a stock HUD copied for
	the Tag Injector's weapons, or the sniper rifle's), read from the map as
	the level loads (donor_extras_fill) */
	struct donor_extra *extras;
	long extra_count;
	long extra_capacity;
	long count;
	struct randomizer_tag_instance *instances;
	unsigned char *seen;
	long *datum_of;
	int keep;
	char leaf[16];
};

struct wanted
{
	short donor;
	long absolute;
	long group;
	char name[256];
};

/* a tag another map's copy of which is wanted already: it is that copy */
struct wanted_alias
{
	short donor;
	long absolute;
	short target_donor;
	long target_absolute;
};

/* the wanted tags by group and name, so each is copied from one map. Twice
WANTED_MAX, a power of two; a slot holds an index into wanted, plus one */
#define WANTED_SLOTS 16384

struct copied_tag
{
	short donor;
	void *root;
	struct tag_schema_definition const *schema;
};

struct held_block
{
	struct held_block *next;
};

struct setup
{
	int enemies;
	/* the player's allies are swapped with the enemies */
	int allies;
	/* game.randomizer_crazy, and which of its parts (each off without it):
	actors holding weapons with another's animations, the engineers and
	sentinels in the pools, the flamethrower in the mixed pools */
	int crazy;
	int any_weapon;
	int odd_enemies;
	int flamethrower;
	/* the Tag Injector's profile's tags added, and their weapons and actor
	variants in the pools (a level played alone only: never a network
	game's, nor a challenge's) */
	int custom_tags;
	/* the infection forms swapped, in tenths (the rest are not placed); 0,
	one in three, without game.randomizer_crazy */
	int pods;
	int pickups;
	int weapons;
	int pool;
	int type;
	/* the one type's rank (RANK_ANY for a type that has none) */
	int rank;
	int pickup_pool;
	int pickup_type;
	int weapon_pool;
	int weapon_type;
	/* the one type a custom type of the profile's (ONE TYPE's TAG INJECTOR),
	and its name, in capitals (tag_injector_tag_custom_type) */
	int custom_type;
	int pickup_custom_type;
	int weapon_custom_type;
	char custom_type_name[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
	char pickup_custom_type_name[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
	char weapon_custom_type_name[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
};

extern boolean tag_index_is_group(long tag_index, long group_tag);
extern void *cache_files_tag_instances(long *count);
extern void cache_files_set_tag_instances(void *instances, long count);
extern int config_boolean(char const *name);
extern long config_integer(char const *name);
extern char const *config_string(char const *name);

/* the loaded map's tag, as cache_files.c has it. The semantics header only
names it (menu_tags.c defines it the same way) */
struct cache_file_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

typedef char randomizer_tag_table_size[
	sizeof(struct cache_file_tag_instance) == 0x20 ? 1 : -1];

static struct donor donors[DONOR_COUNT];
static int donor_count;
/* a pack of maps' tags and pixels: randomizer.pack's of the campaign maps,
skulls.pack's of the map the skulls are copied from (randomizer_pack.c) */
struct pack
{
	char const *file_name;
	HANDLE file;
	struct pack_entry entries[DONOR_COUNT];
	int entry_count;
	/* 1 open, -1 not there (the maps are read instead), 0 not tried */
	int state;
};

static struct pack randomizer_pack = { "randomizer.pack" };
static struct pack skulls_pack = { "skulls.pack" };
static struct wanted *wanted;
static long wanted_count;
static struct copied_tag copied[IMPORT_MAX];
static long copied_count;
static struct held_block *held;
static unsigned long held_bytes;
static int import_stopped;
static long walk_steps;
static long *wanted_slots;
static struct wanted_alias *aliases;
static long alias_count;
/* a root (a palette's actor, a pool's weapon) and what it needs did not fit:
none of it was taken, and no root after it is */
static int wanted_full;
static int import_progress_open;
static struct cache_file_tag_instance *saved_instances;
static long saved_count;
static struct cache_file_tag_instance *grown_instances;
static long grown_count;
static long import_absolute[IMPORT_MAX];
static short import_donor[IMPORT_MAX];
static long import_count;
/* how many copied bitmap and sound ranges the pack does not hold, and what
the reads during play actually returned. The detail lines stop after a few
dozen so a level does not fill the log */
/* the most bytes of pixels a level reads from the maps for the pack
(donor_extras_fill), all its donors', and how many it has */
#define DONOR_EXTRA_BYTES_MAX (16L * 1024L * 1024L)
static long donor_extra_bytes;
static long resource_checked;
static long resource_uncovered;
static long resource_empty;
static long resource_logged;
static long donor_reads;
static long donor_misses;
static long donor_logged;
static int donor_saw_bitmap;
static int donor_saw_sound;
static unsigned char donor_noted[IMPORT_MAX];
static long variant_pool[POOL_MAX];
static long variant_count;
static char protected_names[PROTECTED_MAX][32];
static int protected_count;
static struct setup current;
static int loaded;
static char map_name_leaf[32];
static int remote_on;
/* the setup as the host advertises it (halo_port_limits.h): which swaps,
the pool, the type, the placement roll (little endian), the rank, the
pickups' pool and type, the actors' weapons' pool and type */
static unsigned char remote_setup[HALO_PORT_ADVERTISED_RANDOMIZER_SETUP_SIZE];
/* ... and Go Crazy's parts (HALO_PORT_ADVERTISED_RANDOMIZER_CRAZY_OFFSET) */
static unsigned char remote_crazy;
static struct weapon_choice
{
	long definitions[WEAPON_MAX];
	long count;
} pickup_choice, actor_weapon_choice;
static unsigned long placement_roll;
static int placement_roll_ready;
/* the weapons and actor variants of the Tag Injector's profile the level was
given (tag_injector_tags_load), which join the pools, and the profile's tag
each is (its settings) */
static long custom_roots[CUSTOM_ROOT_MAX];
static long custom_root_tags[CUSTOM_ROOT_MAX];
static long custom_root_count;
static long custom_profile = NONE;
/* the stock HUDs the profile's weapons want that the level has not got
(tag_injector_tags_read), which gather_stock_huds copies */
#define STOCK_HUD_MAX 16
static short stock_huds[STOCK_HUD_MAX];
static long stock_hud_count;
static void *map_widget;
static wchar_t prefixed_title[96];

/* in the game state, so a checkpoint's revert brings them back as they
were: the actors swapped lately, by their units, and the actor each was
to be; the riders placed back as the actors the level gave them, and what
each is to become once out and on its feet; and how many of each squad's
infection forms were to be swapped so far */
#define PLACED_MAX 256
#define RIDERS_MAX 64
#define POD_SQUADS_MAX 64
/* a rider placed back as its own that is not seated by then stays so */
#define RIDER_SEAT_TICKS (2 * TICKS_PER_SECOND)

static struct randomizer_state
{
	struct
	{
		long unit_index;
		long original;
	} placed[PLACED_MAX];
	long placed_next;
	struct
	{
		long unit_index;
		long variant;
		long since;
		long seated;
	} riders[RIDERS_MAX];
	struct
	{
		long encounter;
		long squad;
		long count;
	} pod_squads[POD_SQUADS_MAX];
	long pod_squads_next;
} *state;

/* the actor actor_place is to place as it is asked (a rider placed back,
or swapped once out), or NONE */
static long forced_variant = NONE;
/* the last actor_place left out an infection form (randomizer_actor_variant) */
static int placement_left_out;
/* the level's infection forms so far, for the log: swapped for other
actors, left out, and placed as infection forms (the pool's choice) */
static long pods_swapped;
static long pods_left_out;
static long pods_kept;

static unsigned long mix(unsigned long value)
{
	value ^= value >> 16;
	value *= 0x7FEB352DUL;
	value ^= value >> 15;
	value *= 0x846CA68BUL;
	value ^= value >> 16;

	return value;
}

static unsigned long mix_text(unsigned long seed, char const *text)
{
	for (; text && *text; text++)
		seed = mix(seed ^ (unsigned char)*text);

	return seed;
}

static unsigned long mix_bits(unsigned long seed, float value)
{
	unsigned long bits = 0;

	memcpy(&bits, &value, sizeof(value) < sizeof(bits) ? sizeof(value) : sizeof(bits));

	return mix(seed ^ bits);
}

static int campaign_leaf(char const *name, char *leaf, unsigned int leaf_size)
{
	char const *stripped;
	int index;

	if (!name || !name[0])
		return 0;
	stripped = tag_name_strip_path(name);
	for (index = 0; index < 10; index++)
	{
		char const *path = main_get_solo_level_name(index);

		if (!_stricmp(name, path) || !_stricmp(stripped, tag_name_strip_path(path)))
		{
			snprintf(leaf, leaf_size, "%s", tag_name_strip_path(path));
			return 1;
		}
	}

	return 0;
}

static int pool_id(char const *text)
{
	static char const *const names[] = { "any", "covenant", "flood", "human", "one", "tag_injector" };
	int index;

	if (!text)
		return POOL_ANY;
	for (index = 0; index < (int)NUMBEROF(names); index++)
	{
		if (!_stricmp(text, names[index]))
			return index;
	}

	return POOL_ANY;
}

/* game.randomizer_type's values (the flood's infection forms are not one) */
static struct
{
	char const *name;
	int type;
} const type_names[] =
{
	{ "grunt", TYPE_GRUNT },
	{ "elite", TYPE_ELITE },
	{ "jackal", TYPE_JACKAL },
	{ "hunter", TYPE_HUNTER },
	{ "flood_combat", TYPE_FLOOD_COMBAT },
	{ "flood_carrier", TYPE_FLOOD_CARRIER },
	{ "marine", TYPE_MARINE },
	{ "engineer", TYPE_ENGINEER },
	{ "sentinel", TYPE_SENTINEL },
};

static int type_id(char const *text)
{
	int index;

	for (index = 0; text && index < (int)NUMBEROF(type_names); index++)
	{
		if (!_stricmp(text, type_names[index].name))
			return type_names[index].type;
	}

	return TYPE_GRUNT;
}

/* a type an advertisement may name */
static int type_known(int type)
{
	int index;

	for (index = 0; index < (int)NUMBEROF(type_names); index++)
	{
		if (type_names[index].type == type)
			return 1;
	}

	return 0;
}

static int name_index(char const *text, char const *const *names, int count, int otherwise)
{
	int index;

	for (index = 0; text && index < count; index++)
	{
		if (!_stricmp(text, names[index]))
			return index;
	}

	return otherwise;
}

static int rank_id(char const *text)
{
	static char const *const names[RANK_COUNT] = {
		"any", "minor", "major", "specops", "zealot", "stealth", "human", "elite"
	};

	return name_index(text, names, RANK_COUNT, RANK_ANY);
}

static int weapon_pool_id(char const *text)
{
	static char const *const names[WEAPON_POOL_COUNT] = {
		"any", "covenant", "human", "precision", "automatic", "power", "one", "tag_injector"
	};

	return name_index(text, names, WEAPON_POOL_COUNT, WEAPON_POOL_ANY);
}

static int weapon_kind_id(char const *text, int otherwise)
{
	int index;

	for (index = 0; text && index < WEAPON_KIND_COUNT; index++)
	{
		if (!_stricmp(text, weapon_kinds[index].setting))
			return index;
	}

	return otherwise;
}

/* the setting of the one type's rank, for a type that has ranks */
static char const *rank_setting(int type)
{
	switch (type)
	{
	case TYPE_GRUNT: return "game.randomizer_grunt_rank";
	case TYPE_ELITE: return "game.randomizer_elite_rank";
	case TYPE_JACKAL: return "game.randomizer_jackal_rank";
	case TYPE_HUNTER: return "game.randomizer_hunter_rank";
	case TYPE_FLOOD_COMBAT: return "game.randomizer_flood_combat_rank";
	}

	return NULL;
}

/* a custom type's name as the setup keeps it: in capitals, as
tag_injector_tag_custom_type gives a tag's */
static void custom_type_name_copy(char *name, char const *text)
{
	int index;

	for (index = 0; text && text[index] && index < TAG_INJECTOR_CUSTOM_TYPE_LENGTH - 1; index++)
		name[index] = text[index] >= 'a' && text[index] <= 'z' ? (char)(text[index] - 'a' + 'A') : text[index];
	name[index] = 0;
}

static void setup_from_config(struct setup *setup)
{
	setup->enemies = config_boolean("game.randomizer_enemies");
	setup->allies = config_boolean("game.randomizer_allies");
	setup->crazy = config_boolean("game.randomizer_crazy");
	setup->any_weapon = setup->crazy && config_boolean("game.randomizer_crazy_weapons");
	setup->odd_enemies = setup->crazy && config_boolean("game.randomizer_crazy_enemies");
	setup->flamethrower = setup->crazy && config_boolean("game.randomizer_crazy_flamethrower");
	/* (the profile's tags, played alone: a network game's machines must all
	have the same, and a challenge is the same for everyone) */
	setup->custom_tags = !network_game_get_game() && !network_game_distributed_client() &&
		!community_challenge_running() && tag_injector_profile_playable() != NONE;
	/* (a percentage, to the nearest tenth from one to all of them) */
	setup->pods = setup->crazy ? (int)PIN((config_integer("game.randomizer_crazy_pods") + 5) / 10, 1, 10) : 0;
	setup->pickups = config_boolean("game.randomizer_pickups");
	setup->weapons = config_boolean("game.randomizer_enemy_weapons");
	setup->pool = pool_id(config_string("game.randomizer_pool"));
	setup->type = type_id(config_string("game.randomizer_type"));
	setup->rank = rank_setting(setup->type) ? rank_id(config_string(rank_setting(setup->type))) : RANK_ANY;
	setup->pickup_pool = weapon_pool_id(config_string("game.randomizer_pickup_pool"));
	setup->pickup_type = weapon_kind_id(config_string("game.randomizer_pickup_type"), WEAPON_PISTOL);
	/* (the actors' weapons are not the player's to pick up) */
	if (weapon_kinds[setup->pickup_type].actors_only)
		setup->pickup_type = WEAPON_PISTOL;
	setup->weapon_pool = weapon_pool_id(config_string("game.randomizer_weapon_pool"));
	setup->weapon_type = weapon_kind_id(config_string("game.randomizer_weapon_type"), WEAPON_PLASMA_RIFLE);
	/* (the flamethrower, chosen alone too, only with Go Crazy's: the
	randomizer screen passes over it without, and a setting or a profile
	saved before is taken as the default. What is advertised is this, so a
	client takes the same) */
	if (setup->pickup_type == WEAPON_FLAMETHROWER && !setup->flamethrower)
		setup->pickup_type = WEAPON_PISTOL;
	if (setup->weapon_type == WEAPON_FLAMETHROWER && !setup->flamethrower)
		setup->weapon_type = WEAPON_PLASMA_RIFLE;
	/* (ONE TYPE's TAG INJECTOR: a custom type of the profile's) */
	setup->custom_type = !_stricmp(config_string("game.randomizer_type"), "tag_injector");
	setup->pickup_custom_type = !_stricmp(config_string("game.randomizer_pickup_type"), "tag_injector");
	setup->weapon_custom_type = !_stricmp(config_string("game.randomizer_weapon_type"), "tag_injector");
	custom_type_name_copy(setup->custom_type_name, config_string("game.randomizer_custom_type"));
	custom_type_name_copy(setup->pickup_custom_type_name, config_string("game.randomizer_pickup_custom"));
	custom_type_name_copy(setup->weapon_custom_type_name, config_string("game.randomizer_weapon_custom"));
	/* (the TAG INJECTOR pools and types without the profile's tags: any) */
	if (!setup->custom_tags)
	{
		if (setup->pool == POOL_TAG_INJECTOR || (setup->pool == POOL_ONE && setup->custom_type))
			setup->pool = POOL_ANY;
		if (setup->pickup_pool == WEAPON_POOL_TAG_INJECTOR ||
			(setup->pickup_pool == WEAPON_POOL_ONE && setup->pickup_custom_type))
			setup->pickup_pool = WEAPON_POOL_ANY;
		if (setup->weapon_pool == WEAPON_POOL_TAG_INJECTOR ||
			(setup->weapon_pool == WEAPON_POOL_ONE && setup->weapon_custom_type))
			setup->weapon_pool = WEAPON_POOL_ANY;
		setup->custom_type = 0;
		setup->pickup_custom_type = 0;
		setup->weapon_custom_type = 0;
	}
}

static void setup_from_remote(struct setup *setup)
{
	setup->enemies = (remote_setup[0] & 1) != 0;
	setup->pickups = (remote_setup[0] & 2) != 0;
	setup->weapons = (remote_setup[0] & 4) != 0;
	setup->allies = (remote_setup[0] & 8) != 0;
	setup->crazy = (remote_setup[0] & 16) != 0;
	setup->pool = remote_setup[1] <= POOL_ONE ? remote_setup[1] : POOL_ANY;
	setup->type = type_known(remote_setup[2]) ? remote_setup[2] : TYPE_GRUNT;
	setup->rank = remote_setup[7] < RANK_COUNT ? remote_setup[7] : RANK_ANY;
	setup->pickup_pool = remote_setup[8] < WEAPON_POOL_TAG_INJECTOR ? remote_setup[8] : WEAPON_POOL_ANY;
	setup->pickup_type = remote_setup[9] < WEAPON_KIND_COUNT && !weapon_kinds[remote_setup[9]].actors_only ?
		remote_setup[9] : WEAPON_PISTOL;
	setup->weapon_pool = remote_setup[10] < WEAPON_POOL_TAG_INJECTOR ? remote_setup[10] : WEAPON_POOL_ANY;
	setup->weapon_type = remote_setup[11] < WEAPON_KIND_COUNT ? remote_setup[11] : WEAPON_PLASMA_RIFLE;
	/* (Go Crazy's parts; a host built before there were any has all of
	them, and swaps every infection form) */
	if (remote_crazy & HALO_PORT_ADVERTISED_RANDOMIZER_CRAZY_SET)
	{
		setup->any_weapon = setup->crazy && (remote_crazy & 1) != 0;
		setup->odd_enemies = setup->crazy && (remote_crazy & 2) != 0;
		setup->flamethrower = setup->crazy && (remote_crazy & 4) != 0;
		setup->pods = setup->crazy ? (int)PIN(remote_crazy >> 4, 1, 10) : 0;
	}
	else
	{
		setup->any_weapon = setup->crazy;
		setup->odd_enemies = setup->crazy;
		setup->flamethrower = setup->crazy;
		setup->pods = 10;
	}
	/* (the host's profile's tags are its own) */
	setup->custom_tags = 0;
	setup->custom_type = 0;
	setup->pickup_custom_type = 0;
	setup->weapon_custom_type = 0;
}

static int setup_swaps(struct setup const *setup)
{
	return setup->enemies || setup->pickups || setup->weapons;
}

static int host_randomized(void)
{
	struct network_game *game = network_game_get_game();
	struct setup setup;
	char leaf[32];

	if (network_game_distributed_client())
		return remote_on;
	if (!config_boolean("game.randomizer"))
		return 0;
	if (game && game->variant.game_engine_index != game_engine_none)
		return 0;
	if (!game || !campaign_leaf(game->map.name, leaf, sizeof(leaf)))
		return 0;
	setup_from_config(&setup);

	return setup_swaps(&setup);
}

static int classify(char const *name)
{
	char lower[256];
	unsigned int index;

	if (!name || !name[0])
		return TYPE_NONE;
	for (index = 0; name[index] && index + 1 < sizeof(lower); index++)
	{
		char character = name[index];

		if (character >= 'A' && character <= 'Z')
			character = (char)(character - 'A' + 'a');
		lower[index] = character;
	}
	lower[index] = 0;
	if (strstr(lower, "floodcombat") || strstr(lower, "flood_combat"))
		return TYPE_FLOOD_COMBAT;
	if (strstr(lower, "floodcarrier") || strstr(lower, "flood_carrier"))
		return TYPE_FLOOD_CARRIER;
	if (strstr(lower, "flood"))
		return TYPE_FLOOD_INFECTION;
	if (strstr(lower, "grunt"))
		return TYPE_GRUNT;
	if (strstr(lower, "jackal"))
		return TYPE_JACKAL;
	if (strstr(lower, "hunter"))
		return TYPE_HUNTER;
	if (strstr(lower, "elite"))
		return TYPE_ELITE;
	if (strstr(lower, "sentinel"))
		return TYPE_SENTINEL;
	if (strstr(lower, "marine") || strstr(lower, "crew"))
		return TYPE_MARINE;
	if (strstr(lower, "bugger") || strstr(lower, "engineer"))
		return TYPE_ENGINEER;

	return TYPE_NONE;
}

static void lower_copy(char *lower, unsigned int size, char const *name)
{
	unsigned int index;

	for (index = 0; name && name[index] && index + 1 < size; index++)
	{
		char character = name[index];

		if (character >= 'A' && character <= 'Z')
			character = (char)(character - 'A' + 'a');
		lower[index] = character;
	}
	lower[index] = 0;
}

/* a variant's rank, by its tag's name, among its type's */
static int variant_rank(int kind, char const *variant_name)
{
	char lower[256];

	lower_copy(lower, sizeof(lower), variant_name);
	if (kind == TYPE_FLOOD_COMBAT)
		return strstr(lower, "floodcombat_human") ? RANK_HUMAN : RANK_ELITE;
	if (strstr(lower, "stealth"))
		return RANK_STEALTH;
	if (strstr(lower, "commander"))
		return RANK_ZEALOT;
	if (strstr(lower, "specops"))
		return RANK_SPECOPS;
	if (strstr(lower, "major"))
		return RANK_MAJOR;

	return RANK_MINOR;
}

/* which of the campaign's hand weapons a weapon tag is, by its name */
static int weapon_kind(char const *name)
{
	int index;

	for (index = 0; name && index < WEAPON_KIND_COUNT; index++)
	{
		if (!_stricmp(name, weapon_kinds[index].tag))
			return index;
	}

	return WEAPON_NONE;
}

static int weapon_pool_allows(int pool, int type, int kind, int for_pickups)
{
	if (kind == WEAPON_NONE || (for_pickups && weapon_kinds[kind].actors_only))
		return 0;
	/* (the flamethrower, the campaign's for its last levels, in a pool of
	several with game.randomizer_crazy_flamethrower only; chosen alone it is
	the one, which setup_from_config allows with it only, a host built
	before with or without) */
	if (kind == WEAPON_FLAMETHROWER && pool != WEAPON_POOL_ONE && !current.flamethrower)
		return 0;
	switch (pool)
	{
	case WEAPON_POOL_COVENANT: return weapon_kinds[kind].covenant;
	case WEAPON_POOL_HUMAN: return !weapon_kinds[kind].covenant;
	case WEAPON_POOL_PRECISION: return weapon_kinds[kind].precision;
	case WEAPON_POOL_AUTOMATIC: return weapon_kinds[kind].automatic;
	case WEAPON_POOL_POWER: return weapon_kinds[kind].power;
	case WEAPON_POOL_ONE: return kind == type;
	/* (the profile's weapons alone: add_custom_root) */
	case WEAPON_POOL_TAG_INJECTOR: return 0;
	}

	return 1;
}

/* the one weapon type of the pickups' pool, and of the actors' weapons':
none of the campaign's when it is a custom type of the profile's */
static int pickup_one_type(void)
{
	return current.pickup_custom_type ? WEAPON_NONE : current.pickup_type;
}

static int weapon_one_type(void)
{
	return current.weapon_custom_type ? WEAPON_NONE : current.weapon_type;
}

/* a weapon to copy from the other maps: one a pool the setup chose names.
"any" is the level's own weapons and those its actors carry, as it was */
static int weapon_wanted(char const *name)
{
	int kind = weapon_kind(name);

	return (current.pickups && current.pickup_pool != WEAPON_POOL_ANY &&
			weapon_pool_allows(current.pickup_pool, pickup_one_type(), kind, 1)) ||
		(current.weapons && current.weapon_pool != WEAPON_POOL_ANY &&
			weapon_pool_allows(current.weapon_pool, weapon_one_type(), kind, 0));
}

static int pool_allows(int kind);

/* the one type's rank, when the pool is one type with ranks */
static int rank_allows(int kind, char const *variant_name)
{
	if (current.pool != POOL_ONE || current.rank == RANK_ANY || kind != current.type)
		return 1;

	return variant_rank(kind, variant_name) == current.rank;
}

static int pool_allows(int kind)
{
	if (kind == TYPE_NONE)
		return 0;
	/* (the profile's actor variants alone: add_custom_root) */
	if (current.pool == POOL_TAG_INJECTOR || (current.pool == POOL_ONE && current.custom_type))
		return 0;
	if (current.pool == POOL_ONE)
		return kind == current.type;
	/* (the engineers, which the campaign hardly uses, and the sentinels,
	which are not the covenant's: with game.randomizer_crazy_enemies only) */
	if ((kind == TYPE_ENGINEER || kind == TYPE_SENTINEL) && !current.odd_enemies)
		return 0;
	if (current.pool == POOL_ANY)
		return 1;
	if (current.pool == POOL_COVENANT)
		return kind == TYPE_GRUNT || kind == TYPE_ELITE || kind == TYPE_JACKAL || kind == TYPE_HUNTER ||
			kind == TYPE_ENGINEER;
	if (current.pool == POOL_FLOOD)
		return kind == TYPE_FLOOD_COMBAT || kind == TYPE_FLOOD_CARRIER || kind == TYPE_FLOOD_INFECTION;
	if (current.pool == POOL_HUMAN)
		return kind == TYPE_MARINE;

	return 0;
}

static void *donor_address(struct donor *donor, void *pointer)
{
	unsigned long address = (unsigned long)pointer;

	if (!pointer || address < DONOR_WINDOW || address - DONOR_WINDOW >= donor->size)
		return NULL;

	return donor->bytes + (address - DONOR_WINDOW);
}

static int range_in(struct donor *donor, void const *pointer, unsigned long bytes)
{
	unsigned char const *start = pointer;

	if (!pointer)
		return bytes == 0;
	if (start < donor->bytes || start > donor->bytes + donor->size)
		return 0;
	if ((unsigned long)(donor->bytes + donor->size - start) < bytes)
		return 0;

	return 1;
}

static char const *instance_name(struct donor *donor, struct randomizer_tag_instance *instance)
{
	char const *name = donor_address(donor, instance->name);
	unsigned long room;
	void const *end;

	if (!name || !range_in(donor, name, 1))
		return NULL;
	room = (unsigned long)(donor->bytes + donor->size - (unsigned char const *)name);
	end = memchr(name, 0, room > 256 ? 256 : room);
	if (!end)
		return NULL;

	return name;
}

static struct tag_schema_definition const *schema_of(unsigned long group)
{
	long list;

	for (list = 0; tag_schema_group_lists[list]; list++)
	{
		struct tag_schema_group const *groups = tag_schema_group_lists[list];
		long index;

		for (index = 0; groups[index].group_tag; index++)
		{
			if (groups[index].group_tag == group)
				return groups[index].definition;
		}
	}

	return NULL;
}

static void *grab(unsigned long size)
{
	struct held_block *block;

	if (!size || import_stopped || held_bytes > COPY_BUDGET || size > 0x7fffffffUL - sizeof(*block))
	{
		import_stopped = 1;
		return NULL;
	}
	block = malloc(sizeof(*block) + size);
	if (!block)
	{
		import_stopped = 1;
		return NULL;
	}
	block->next = held;
	held = block;
	held_bytes += size;

	return block + 1;
}

static void held_free(void)
{
	while (held)
	{
		struct held_block *next = held->next;

		free(held);
		held = next;
	}
	held_bytes = 0;
}

static int leave_group(long group)
{
	return group == 'vehi' || group == 'scnr' || group == 'sbsp' || group == (long)'matg';
}

static void want(struct donor *donor, long datum);

/* a field whose declared bytes run past the element is skipped: the schema
can name an offset the donor tag does not have */
/* whether a walk of this schema can reach a tag reference. Geometry,
collision nodes and animation frames cannot, and walking every one of them
is what made a donor's tags fault or never return */
static int schema_has_tags(struct tag_schema_definition const *schema, int depth)
{
	struct tag_schema_field const *field;
	static struct tag_schema_definition const *visiting[16];
	int visit;
	int has;

	if (!schema || !schema->fields || depth > 8)
		return 0;
	for (visit = 0; visit < depth && visit < 16; visit++)
	{
		if (visiting[visit] == schema)
			return 0;
	}
	if (depth < 16)
		visiting[depth] = schema;
	has = 0;
	for (field = schema->fields; field->type != _tag_schema_terminator; field++)
	{
		if (field->type == _tag_schema_reference || field->type == _tag_schema_tag_index)
		{
			has = 1;
			break;
		}
		if ((field->type == _tag_schema_struct || field->type == _tag_schema_block) &&
			schema_has_tags(field->definition, depth + 1))
		{
			has = 1;
			break;
		}
	}

	return has;
}

static int field_inside(struct tag_schema_definition const *schema, struct tag_schema_field const *field)
{
	unsigned long end;

	if (!schema || !field || schema->size <= 0 || field->count <= 0 || field->size <= 0 || field->offset < 0)
		return 0;
	if ((unsigned long)field->offset >= (unsigned long)schema->size)
		return 0;
	if ((unsigned long)field->size >
		((unsigned long)schema->size - (unsigned long)field->offset) / (unsigned long)field->count)
		return 0;
	end = (unsigned long)field->offset + (unsigned long)field->count * (unsigned long)field->size;

	return end <= (unsigned long)schema->size;
}

static void walk_want(struct donor *donor, void *element, struct tag_schema_definition const *schema, int depth)
{
	struct tag_schema_field const *field;

	if (walk_steps > 400000)
		wanted_full = 1;
	if (wanted_full || import_stopped)
		return;
	if (depth >= WALK_DEPTH || !element || !schema || schema->size <= 0 ||
		!range_in(donor, element, (unsigned long)schema->size))
		return;
	walk_steps++;
	for (field = schema->fields; field && field->type != _tag_schema_terminator; field++)
	{
		long item;

		if (!field_inside(schema, field))
			continue;
		for (item = 0; item < field->count; item++)
		{
			void *place = (char *)element + field->offset + item * field->size;

			if (field->type == _tag_schema_reference)
				want(donor, ((struct tag_reference *)place)->index);
			else if (field->type == _tag_schema_tag_index)
				want(donor, *(long *)place);
			else if (field->type == _tag_schema_struct)
			{
				if (schema_has_tags(field->definition, 0))
					walk_want(donor, place, field->definition, depth + 1);
			}
			else if (field->type == _tag_schema_block)
			{
				struct tag_block *block = place;
				struct tag_schema_definition const *child = field->definition;
				void *elements = donor_address(donor, block->address);
				long index;
				long count = block->count;

				if (!child || count <= 0 || !elements || child->size <= 0 || count > 4096 ||
					!schema_has_tags(child, 0) ||
					(unsigned long)count > 0x1000000UL / (unsigned long)child->size ||
					!range_in(donor, elements, (unsigned long)count * (unsigned long)child->size))
					continue;
				for (index = 0; index < count; index++)
					walk_want(donor, (char *)elements + index * (unsigned long)child->size, child, depth + 1);
			}
		}
	}
}

static unsigned long wanted_hash(long group, char const *name)
{
	unsigned long hash = (unsigned long)group * 0x9E3779B1UL;

	for (; *name; name++)
	{
		char character = *name;

		if (character >= 'A' && character <= 'Z')
			character = (char)(character - 'A' + 'a');
		hash = (hash ^ (unsigned char)character) * 0x01000193UL;
	}

	return hash & (WANTED_SLOTS - 1);
}

/* the wanted tag of this group and name, or NONE; with insert, the slot it
goes in is taken by `index` when there is none */
static long wanted_find(long group, char const *name, long insert)
{
	unsigned long slot = wanted_hash(group, name);

	for (;;)
	{
		long held = wanted_slots[slot];

		if (!held)
		{
			if (insert != NONE)
				wanted_slots[slot] = insert + 1;
			return NONE;
		}
		if (wanted[held - 1].group == group && !_stricmp(wanted[held - 1].name, name))
			return held - 1;
		slot = (slot + 1) & (WANTED_SLOTS - 1);
	}
}

static long wanted_find(long group, char const *name, long insert);

/* the table again, of the wanted_count entries: the one time a root does
not fit, and what it added goes */
static void wanted_slots_rebuild(void)
{
	long index;

	memset(wanted_slots, 0, WANTED_SLOTS * sizeof(*wanted_slots));
	for (index = 0; index < wanted_count; index++)
		wanted_find(wanted[index].group, wanted[index].name, index);
}

static void want(struct donor *donor, long datum)
{
	long absolute = datum & 0xFFFF;
	long other;
	struct randomizer_tag_instance *instance;
	char const *name;
	long existing;

	if (datum == NONE || absolute < 0 || absolute >= donor->count || donor->seen[absolute])
		return;
	donor->seen[absolute] = 1;
	instance = donor->instances + absolute;
	name = instance_name(donor, instance);
	if (leave_group(instance->group_tag))
	{
		donor->datum_of[absolute] = name ? tag_loaded(instance->group_tag, name) : NONE;
		return;
	}
	existing = name ? tag_loaded(instance->group_tag, name) : NONE;
	if (existing != NONE)
	{
		donor->datum_of[absolute] = existing;
		return;
	}
	if (!name || wanted_count >= WANTED_MAX || alias_count >= WANTED_MAX)
	{
		wanted_full = 1;
		return;
	}
	/* (the same tag from another map: the copy of it already wanted, which
	brings what it needs from its own map) */
	other = wanted_find(instance->group_tag, name, wanted_count);
	if (other != NONE)
	{
		aliases[alias_count].donor = (short)(donor - donors);
		aliases[alias_count].absolute = absolute;
		aliases[alias_count].target_donor = wanted[other].donor;
		aliases[alias_count].target_absolute = wanted[other].absolute;
		alias_count++;
		return;
	}
	snprintf(wanted[wanted_count].name, sizeof(wanted[wanted_count].name), "%s", name);
	wanted[wanted_count].donor = (short)(donor - donors);
	wanted[wanted_count].absolute = absolute;
	wanted[wanted_count].group = instance->group_tag;
	wanted_count++;
}

/* a root and every tag it needs, or none of them: a model taken without its
shaders is drawn with a shader that is not there, and the game stops */
static int want_root(struct donor *donor, long absolute)
{
	long start = wanted_count;
	long start_aliases = alias_count;
	long queued = wanted_count;

	if (wanted_full)
		return 0;
	want(donor, absolute);
	while (queued < wanted_count && !wanted_full)
	{
		struct wanted *entry = wanted + queued;
		struct donor *owner = donors + entry->donor;
		struct randomizer_tag_instance *instance = owner->instances + entry->absolute;
		struct tag_schema_definition const *schema = schema_of((unsigned long)instance->group_tag);

		queued++;
		if (schema)
			walk_want(owner, donor_address(owner, instance->base_address), schema, 0);
	}
	if (!wanted_full && wanted_count <= IMPORT_MAX)
		return 1;
	/* (what this root added goes; seen stays set, which no later root asks
	about, there being none) */
	wanted_count = start;
	alias_count = start_aliases;
	wanted_slots_rebuild();
	wanted_full = 1;
	error(_error_silent, "randomizer: %s.map's next tags do not fit; the level takes the %ld before them",
		donor->leaf, wanted_count);

	return 0;
}

static int wanted_less(void const *left, void const *right)
{
	struct wanted const *a = left;
	struct wanted const *b = right;
	int order;

	/* map first, so the copy reads each map's tags once. The name then
	keeps two machines appending the same tag at the same index */
	if (a->donor != b->donor)
		return a->donor - b->donor;
	order = _stricmp(a->name, b->name);
	if (order)
		return order;

	return a->absolute < b->absolute ? -1 : a->absolute > b->absolute;
}

static void copy_fields(struct donor *donor, void *element, struct tag_schema_definition const *schema, int depth);

static void copy_fields(struct donor *donor, void *element, struct tag_schema_definition const *schema, int depth)
{
	struct tag_schema_field const *field;

	if (depth >= WALK_DEPTH || !element || !schema || schema->size <= 0)
		return;
	for (field = schema->fields; field->type != _tag_schema_terminator; field++)
	{
		long item;

		if (!field_inside(schema, field))
			continue;
		for (item = 0; item < field->count; item++)
		{
			void *place = (char *)element + field->offset + item * field->size;

			if (field->type == _tag_schema_reference)
			{
				struct tag_reference *reference = place;
				char const *source = donor_address(donor, reference->name);
				unsigned long room;
				void const *end;
				char *copy;

				reference->name = "";
				reference->name_length = 0;
				if (!source || !range_in(donor, source, 1))
				{
					reference->index = NONE;
					continue;
				}
				room = (unsigned long)(donor->bytes + donor->size - (unsigned char const *)source);
				end = memchr(source, 0, room > 256 ? 256 : room);
				if (!end)
					continue;
				copy = grab((unsigned long)strlen(source) + 1);
				if (!copy)
					continue;
				strcpy(copy, source);
				reference->name = copy;
				reference->name_length = (long)strlen(copy);
			}
			else if (field->type == _tag_schema_data || field->type == _tag_schema_file_data)
			{
				struct tag_data *data = place;

				data->definition = NULL;
				if (field->type == _tag_schema_file_data || data->size <= 0)
				{
					data->address = NULL;
					continue;
				}
				{
					void *source = donor_address(donor, data->address);
					void *bytes;

					if (!source || data->size > 0x1000000 ||
						!range_in(donor, source, (unsigned long)data->size))
					{
						data->size = 0;
						data->address = NULL;
						continue;
					}
					bytes = grab((unsigned long)data->size);
					if (!bytes)
					{
						data->size = 0;
						data->address = NULL;
						continue;
					}
					memcpy(bytes, source, (unsigned long)data->size);
					data->address = bytes;
				}
			}
			else if (field->type == _tag_schema_reset)
			{
				short size = field->size;
				long value = field->target_offset;

				if (size > 4)
					memset(place, 0, (unsigned long)size);
				else if (size > 0)
				{
					memset(place, 0, (unsigned long)size);
					memcpy(place, &value, (unsigned long)size);
				}
			}
			else if (field->type == _tag_schema_struct)
				copy_fields(donor, place, field->definition, depth + 1);
			else if (field->type == _tag_schema_block)
			{
				struct tag_block *block = place;
				struct tag_schema_definition const *child = field->definition;
				void *source = donor_address(donor, block->address);
				void *elements;
				long index;
				long count = block->count;

				block->definition = NULL;
				block->address = NULL;
				if (!child || count <= 0 || !source || child->size <= 0 || count > 100000 ||
					(unsigned long)count > 0x1000000UL / (unsigned long)child->size ||
					!range_in(donor, source, (unsigned long)count * (unsigned long)child->size))
				{
					block->count = 0;
					continue;
				}
				elements = grab((unsigned long)count * (unsigned long)child->size);
				if (!elements)
				{
					block->count = 0;
					continue;
				}
				memcpy(elements, source, (unsigned long)count * (unsigned long)child->size);
				block->address = elements;
				for (index = 0; index < count; index++)
					copy_fields(donor, (char *)elements + index * (unsigned long)child->size, child, depth + 1);
			}
		}
	}
}

static long retarget_datum(struct donor *donor, long datum)
{
	long absolute = datum & 0xFFFF;

	if (datum == NONE || absolute < 0 || absolute >= donor->count)
		return NONE;

	return donor->datum_of[absolute];
}

static void retarget_fields(struct donor *donor, void *element, struct tag_schema_definition const *schema, int depth)
{
	struct tag_schema_field const *field;

	if (depth >= WALK_DEPTH || !element || !schema || schema->size <= 0)
		return;
	for (field = schema->fields; field->type != _tag_schema_terminator; field++)
	{
		long item;

		if (!field_inside(schema, field))
			continue;
		for (item = 0; item < field->count; item++)
		{
			void *place = (char *)element + field->offset + item * field->size;

			if (field->type == _tag_schema_reference)
			{
				struct tag_reference *reference = place;
				long old = reference->index;
				long index = reference->name && reference->name[0] ?
					tag_loaded(reference->group_tag, reference->name) : NONE;

				if (index == NONE)
					index = retarget_datum(donor, old);
				reference->index = index;
				if (index != NONE)
				{
					long count = 0;
					struct cache_file_tag_instance *instances = cache_files_tag_instances(&count);
					long absolute = index & 0xFFFF;

					reference->name = tag_get_name(index);
					reference->name_length = reference->name ? (long)strlen(reference->name) : 0;
					if (instances && absolute >= 0 && absolute < count &&
						tag_loaded(reference->group_tag, reference->name) != index)
						reference->group_tag = (unsigned long)instances[absolute].group_tag;
				}
			}
			else if (field->type == _tag_schema_tag_index)
				*(long *)place = retarget_datum(donor, *(long *)place);
			else if (field->type == _tag_schema_struct)
				retarget_fields(donor, place, field->definition, depth + 1);
			else if (field->type == _tag_schema_block)
			{
				struct tag_block *block = place;
				struct tag_schema_definition const *child = field->definition;
				long index;

				if (!child || !block->address || block->count <= 0)
					continue;
				for (index = 0; index < block->count; index++)
					retarget_fields(donor, (char *)block->address + index * (unsigned long)child->size, child, depth + 1);
			}
		}
	}
}

static int donor_file_at(HANDLE file, long offset)
{
	DWORD result = SetFilePointer(file, offset, NULL, FILE_BEGIN);

	return result != INVALID_SET_FILE_POINTER || GetLastError() == 0;
}

static int donor_file_read(HANDLE file, void *buffer, unsigned long size)
{
	unsigned long done = 0;

	while (done < size)
	{
		DWORD chunk = (DWORD)(size - done);
		DWORD got = 0;

		if (chunk > 0x40000)
			chunk = 0x40000;
		if (!ReadFile(file, (char *)buffer + done, chunk, &got, NULL) || got == 0)
			return 0;
		done += got;
	}

	return 1;
}

static void donor_bytes_free(struct donor *donor)
{
	if (donor->bytes)
		free(donor->bytes);
	donor->bytes = NULL;
}

/* `start` and `size` are offsets in the inflated file. The disc file's
header is plain; everything from 0x800 on is the zlib stream precaching
inflates. Output before `start` is discarded, so the whole map is never
held */
static int inflate_span(HANDLE file, long start, long size, unsigned char *destination)
{
	z_stream stream;
	unsigned char input[0x2000];
	unsigned char discard[0x2000];
	long produced;
	long end;
	int result;
	unsigned int asked;
	long filled;
	DWORD got;

	if (!destination || start < 0x800 || size <= 0)
		return 0;
	if (!donor_file_at(file, 0x800))
		return 0;
	memset(&stream, 0, sizeof(stream));
	if (inflateInit(&stream) != Z_OK)
		return 0;
	produced = 0x800;
	end = start + size;
	result = Z_OK;
	while (produced < end && result != Z_STREAM_END)
	{
		if (stream.avail_in == 0)
		{
			got = 0;
			if (!ReadFile(file, input, (DWORD)sizeof(input), &got, NULL) || got == 0)
				break;
			stream.next_in = (Bytef *)input;
			stream.avail_in = got;
		}
		if (produced < start)
		{
			long skip = start - produced;

			if (skip > (long)sizeof(discard))
				skip = (long)sizeof(discard);
			stream.next_out = (Bytef *)discard;
			asked = (unsigned int)skip;
		}
		else
		{
			long want = end - produced;

			if (want > 0x4000)
				want = 0x4000;
			stream.next_out = (Bytef *)(destination + (produced - start));
			asked = (unsigned int)want;
		}
		stream.avail_out = asked;
		result = inflate(&stream, Z_NO_FLUSH);
		if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR)
			break;
		filled = (long)(asked - stream.avail_out);
		if (filled <= 0)
			break;
		produced += filled;
		if (result == Z_BUF_ERROR)
			result = Z_OK;
	}
	inflateEnd(&stream);

	return produced >= end;
}

static int pack_read_u32(struct pack *pack, long at, unsigned long *value)
{
	unsigned char bytes[4];
	DWORD got = 0;

	if (!donor_file_at(pack->file, at) || !ReadFile(pack->file, bytes, 4, &got, NULL) || got != 4)
		return 0;
	memcpy(value, bytes, 4);

	return 1;
}

static void pack_close(struct pack *pack)
{
	if (pack->file && pack->file != INVALID_HANDLE_VALUE)
		CloseHandle(pack->file);
	pack->file = NULL;
	pack->entry_count = 0;
	pack->state = 0;
}

/* each map the pack has is still the map in maps/. One that was replaced
(another disc's, or a fresh copy) would have its pixels read at the old
offsets */
static int pack_matches_maps(struct pack *pack)
{
	int index;

	for (index = 0; index < pack->entry_count; index++)
	{
		struct pack_entry *entry = pack->entries + index;
		long file_length, tag_offset, tag_size;

		if (!randomizer_pack_map_header(entry->name, &file_length, &tag_offset, &tag_size, NULL))
			continue;
		if (file_length != entry->file_length || tag_offset != entry->tag_offset || tag_size != entry->tag_size)
		{
			error(_error_silent, "randomizer: %s.map is not the one in %s", entry->name, pack->file_name);
			return 0;
		}
	}

	return 1;
}

/* a pack: 'rpak', version 1, then one 32-byte record per map (8-byte name,
file length, tag offset, tag size, where the tag bytes are, how many
resource spans, where that table is) */
static int pack_load(struct pack *pack, char const *path)
{
	unsigned long magic = 0;
	unsigned long version = 0;
	unsigned long count = 0;
	unsigned long index;
	DWORD open_error;

	pack->file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (pack->file == INVALID_HANDLE_VALUE)
	{
		open_error = GetLastError();
		error(_error_silent, "randomizer: no %s (error %lu)", path, (unsigned long)open_error);
		pack->file = NULL;
		return 0;
	}
	if (!pack_read_u32(pack, 0, &magic) || !pack_read_u32(pack, 4, &version) || !pack_read_u32(pack, 8, &count) ||
		magic != 'rpak' || version != 1 || count > (unsigned long)DONOR_COUNT)
	{
		error(_error_silent, "randomizer: %s is not a randomizer pack", path);
		pack_close(pack);
		return 0;
	}
	for (index = 0; index < count; index++)
	{
		unsigned char record[32];
		struct pack_entry *entry = pack->entries + index;
		long at = 12 + (long)index * 32;

		if (!donor_file_at(pack->file, at) || !donor_file_read(pack->file, record, sizeof(record)))
		{
			error(_error_silent, "randomizer: %s directory is short", path);
			pack_close(pack);
			return 0;
		}
		memset(entry, 0, sizeof(*entry));
		memcpy(entry->name, record, 7);
		memcpy(&entry->file_length, record + 8, 4);
		memcpy(&entry->tag_offset, record + 12, 4);
		memcpy(&entry->tag_size, record + 16, 4);
		memcpy(&entry->tags_at, record + 20, 4);
		memcpy(&entry->resource_count, record + 24, 4);
		memcpy(&entry->resources_at, record + 28, 4);
	}
	pack->entry_count = (int)count;
	if (!pack_matches_maps(pack))
	{
		pack_close(pack);
		return 0;
	}

	return 1;
}

/* the pack (written at the main menu: randomizer_pack.c). Not there, or no
longer the maps', and the maps are read instead */
static int pack_open(struct pack *pack)
{
	char path[512];

	if (pack->state == 1)
		return 1;
	if (pack->state < 0)
		return 0;
	snprintf(path, sizeof(path), "%s%s", cache_files_map_directory(), pack->file_name);
	if (!pack_load(pack, path))
	{
		error(_error_silent, "randomizer: reading the maps instead of %s", path);
		pack->state = -1;
		return 0;
	}
	pack->state = 1;
	error(_error_silent, "randomizer: using %s (%d maps)", path, pack->entry_count);

	return 1;
}

static struct pack_entry *pack_find(struct pack *pack, char const *leaf)
{
	int index;

	if (!pack || !pack_open(pack))
		return NULL;
	for (index = 0; index < pack->entry_count; index++)
	{
		if (!_stricmp(pack->entries[index].name, leaf))
			return pack->entries + index;
	}

	return NULL;
}

static int pack_read_tags(struct donor *donor, struct pack_entry *entry)
{
	unsigned long span_bytes;

	donor->file_length = entry->file_length;
	donor->tag_offset = entry->tag_offset;
	donor->compressed = 0;
	if (!donor->from_pack)
	{
		if (entry->resource_count < 0 || entry->resource_count > 100000 ||
			entry->tag_size <= 0 || entry->tag_size > (long)HALO_PORT_TAG_CACHE_SIZE ||
			entry->tags_at < 0 || entry->resources_at < 0)
		{
			error(_error_silent, "randomizer: %s.map in the pack is not usable", donor->leaf);
			return 0;
		}
		if (entry->resource_count > 0)
		{
			span_bytes = (unsigned long)entry->resource_count * sizeof(*donor->spans);
			donor->spans = malloc(span_bytes);
			if (!donor->spans)
			{
				error(_error_silent, "randomizer: no room for %s.map's pack ranges", donor->leaf);
				return 0;
			}
			if (!donor_file_at(donor->pack->file, entry->resources_at) ||
				!donor_file_read(donor->pack->file, donor->spans, span_bytes))
			{
				error(_error_silent, "randomizer: could not read %s.map's pack ranges", donor->leaf);
				free(donor->spans);
				donor->spans = NULL;
				return 0;
			}
			donor->span_count = entry->resource_count;
		}
		donor->from_pack = 1;
	}
	donor->bytes = malloc((unsigned long)entry->tag_size);
	donor->size = (unsigned long)entry->tag_size;
	if (!donor->bytes)
	{
		error(_error_silent, "randomizer: no room for %s.map's packed tags (%ld bytes)", donor->leaf, entry->tag_size);
		return 0;
	}
	if (!donor_file_at(donor->pack->file, entry->tags_at) ||
		!donor_file_read(donor->pack->file, donor->bytes, (unsigned long)entry->tag_size))
	{
		error(_error_silent, "randomizer: could not read %s.map's packed tags", donor->leaf);
		donor_bytes_free(donor);
		return 0;
	}
	error(_error_silent, "randomizer: %s.map from %s (%ld byte tags)", donor->leaf, donor->pack->file_name,
		entry->tag_size);

	return 1;
}

/* 1 when one pack range holds the whole read, -1 when it holds the start
only, 0 when the offset is not in any range */
static int span_covers(struct donor *donor, long offset, long size)
{
	long low;
	long high;
	struct pack_span *span;

	if (!donor || !donor->from_pack || !donor->spans || donor->span_count <= 0 || offset < 0 || size <= 0)
		return 0;
	low = 0;
	high = donor->span_count;
	while (low < high)
	{
		long mid = low + (high - low) / 2;

		if (donor->spans[mid].file_offset <= offset)
			low = mid + 1;
		else
			high = mid;
	}
	if (low == 0)
		return 0;
	span = donor->spans + (low - 1);
	if (span->size <= 0 || offset < span->file_offset || offset >= span->file_offset + span->size)
		return 0;
	if (size > span->file_offset + span->size - offset)
		return -1;

	return 1;
}

static long pack_copy(struct donor *donor, long offset, long size, void *buffer)
{
	unsigned char *out = buffer;
	long filled = 0;
	HANDLE file = donor->pack ? donor->pack->file : NULL;

	if (!donor->spans || donor->span_count <= 0 || !file || file == INVALID_HANDLE_VALUE)
		return 0;
	while (size > 0)
	{
		long low = 0;
		long high = donor->span_count;
		struct pack_span *span;
		long into;
		long have;

		while (low < high)
		{
			long mid = low + (high - low) / 2;

			if (donor->spans[mid].file_offset <= offset)
				low = mid + 1;
			else
				high = mid;
		}
		if (low == 0)
			return filled;
		span = donor->spans + (low - 1);
		if (span->size <= 0 || offset < span->file_offset || offset >= span->file_offset + span->size)
			return filled;
		into = offset - span->file_offset;
		have = span->size - into;
		if (have > size)
			have = size;
		if (span->data_at < 0 || into < 0)
			return filled;
		if (!donor_file_at(file, span->data_at + into) ||
			!donor_file_read(file, out, (unsigned long)have))
			return filled;
		offset += have;
		size -= have;
		out += have;
		filled += have;
	}

	return filled;
}

/* the tag bytes only. datum_of stays, so a tag found while the file was open
still retargets after the bytes are gone */
static int load_donor_bytes(struct donor *donor)
{
	char path[512];
	unsigned char header[0x800];
	long version;
	long file_length;
	long tag_offset;
	long tag_size;
	unsigned long signature;
	DWORD open_error;
	DWORD physical;
	struct randomizer_tag_header *tags;
	struct pack_entry *packed;

	if (donor->bytes)
		return 1;
	packed = pack_find(donor->pack, donor->leaf);
	if (packed)
	{
		snprintf(path, sizeof(path), "%s%s", cache_files_map_directory(), donor->pack->file_name);
		if (!pack_read_tags(donor, packed))
			return 0;
	}
	else
	{
	snprintf(path, sizeof(path), "%s%s.map", cache_files_map_directory(), donor->leaf);
	if (!donor->file || donor->file == INVALID_HANDLE_VALUE)
	{
		donor->file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (donor->file == INVALID_HANDLE_VALUE)
		{
			open_error = GetLastError();
			error(_error_silent, "randomizer: could not open %s (error %lu)", path, (unsigned long)open_error);
			donor->file = NULL;
			return 0;
		}
	}
	if (!donor_file_at(donor->file, 0) || !donor_file_read(donor->file, header, sizeof(header)))
	{
		error(_error_silent, "randomizer: could not read the header of %s", path);
		return 0;
	}
	memcpy(&signature, header, 4);
	memcpy(&version, header + 4, 4);
	memcpy(&file_length, header + 8, 4);
	memcpy(&tag_offset, header + 0x10, 4);
	memcpy(&tag_size, header + 0x14, 4);
	if (signature != 'head' || version != 5 || file_length < (long)sizeof(header) ||
		file_length > 0x11600000 || tag_offset < 0 || tag_size < (long)sizeof(struct randomizer_tag_header) ||
		tag_size > (long)HALO_PORT_TAG_CACHE_SIZE || tag_size > file_length || tag_offset > file_length - tag_size)
	{
		error(_error_silent,
			"randomizer: %s is not an Xbox map (signature %08lx, version %ld, %ld bytes, tags %ld at %ld)",
			path, signature, version, file_length, tag_size, tag_offset);
		return 0;
	}
	donor->file_length = file_length;
	donor->tag_offset = tag_offset;
	physical = GetFileSize(donor->file, NULL);
	donor->compressed = physical == INVALID_FILE_SIZE || (long)physical < file_length;
	error(_error_silent, "randomizer: unpacking %s (%ld byte map, file is %lu, tags %ld at %ld)",
		path, file_length, (unsigned long)physical, tag_size, tag_offset);
	donor->bytes = malloc((unsigned long)tag_size);
	donor->size = (unsigned long)tag_size;
	if (!donor->bytes)
	{
		error(_error_silent, "randomizer: no room for %s tags (%ld bytes)", path, tag_size);
		return 0;
	}
	if (donor->compressed)
	{
		if (tag_offset < 0x800 || !inflate_span(donor->file, tag_offset, tag_size, donor->bytes))
		{
			error(_error_silent, "randomizer: could not decompress %s", path);
			donor_bytes_free(donor);
			return 0;
		}
	}
	else if (!donor_file_at(donor->file, tag_offset) ||
		!donor_file_read(donor->file, donor->bytes, (unsigned long)tag_size))
	{
		error(_error_silent, "randomizer: could not read %ld bytes of tags from %s", tag_size, path);
		donor_bytes_free(donor);
		return 0;
	}
	}
	tags = (struct randomizer_tag_header *)donor->bytes;
	if (tags->signature != 'tags' || tags->tag_count <= 0 || tags->tag_count > 100000)
	{
		error(_error_silent, "randomizer: %s tags are not a tag section (signature %08lx, count %ld)",
			path, tags->signature, tags->tag_count);
		donor_bytes_free(donor);
		return 0;
	}
	donor->count = tags->tag_count;
	donor->instances = donor_address(donor, tags->tag_instances);
	if (!donor->instances ||
		!range_in(donor, donor->instances, (unsigned long)donor->count * sizeof(*donor->instances)))
	{
		error(_error_silent, "randomizer: %s tag table is not in its tag section (%ld tags at %08lx)",
			path, tags->tag_count, (unsigned long)tags->tag_instances);
		donor_bytes_free(donor);
		donor->instances = NULL;
		return 0;
	}

	return 1;
}

static void release_donor_blob(struct donor *donor)
{
	donor_bytes_free(donor);
	donor->instances = NULL;
	if (donor->seen)
		free(donor->seen);
	donor->seen = NULL;
	if (!donor->keep && donor->file && donor->file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(donor->file);
		donor->file = NULL;
	}
}

static int open_donor(struct donor *donor, char const *leaf, struct pack *pack)
{
	long index;

	memset(donor, 0, sizeof(*donor));
	snprintf(donor->leaf, sizeof(donor->leaf), "%s", leaf);
	donor->pack = pack;
	if (!load_donor_bytes(donor))
		return 0;
	/* malloc, not calloc: free() is the debug allocator, and a block it did
	not hand out faults when released */
	donor->seen = malloc((unsigned long)donor->count);
	donor->datum_of = malloc((unsigned long)donor->count * sizeof(*donor->datum_of));
	if (!donor->seen || !donor->datum_of)
	{
		error(_error_silent, "randomizer: no room for %s.map's tag list (%ld tags)", donor->leaf, donor->count);
		return 0;
	}
	memset(donor->seen, 0, (unsigned long)donor->count);
	for (index = 0; index < donor->count; index++)
		donor->datum_of[index] = NONE;

	return 1;
}

static void close_donor_bytes(struct donor *donor)
{
	long index;

	release_donor_blob(donor);
	if (donor->spans)
		free(donor->spans);
	donor->spans = NULL;
	donor->span_count = 0;
	for (index = 0; index < donor->extra_count; index++)
	{
		if (donor->extras[index].bytes)
			free(donor->extras[index].bytes);
	}
	if (donor->extras)
		free(donor->extras);
	donor->extras = NULL;
	donor->extra_count = 0;
	donor->extra_capacity = 0;
	donor->from_pack = 0;
	if (donor->datum_of)
		free(donor->datum_of);
	donor->datum_of = NULL;
}

static int donor_bytes(struct donor *donor)
{
	int index;

	if (donor->bytes && donor->instances)
		return 1;
	for (index = 0; index < donor_count; index++)
	{
		if (donors + index != donor)
			release_donor_blob(donors + index);
	}

	return load_donor_bytes(donor);
}

static void donors_close(void)
{
	int index;

	for (index = 0; index < donor_count; index++)
	{
		close_donor_bytes(donors + index);
		if (donors[index].file && donors[index].file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(donors[index].file);
			donors[index].file = NULL;
		}
	}
	donor_count = 0;
	donor_extra_bytes = 0;
	pack_close(&randomizer_pack);
	pack_close(&skulls_pack);
}

static void remember_import(long absolute, short donor_index)
{
	if (import_count >= IMPORT_MAX)
		return;
	import_absolute[import_count] = absolute;
	import_donor[import_count] = donor_index;
	import_count++;
}

static int variant_in_pool(struct donor *donor, long datum)
{
	long absolute = datum & 0xFFFF;
	struct randomizer_tag_instance *instance;
	struct actor_variant_definition *variant;
	char const *unit_name;

	if (datum == NONE || absolute < 0 || absolute >= donor->count)
		return 0;
	instance = donor->instances + absolute;
	if (instance->group_tag != ACTOR_VARIANT_DEFINITION_TAG)
		return 0;
	variant = donor_address(donor, instance->base_address);
	if (!variant || !range_in(donor, variant, sizeof(*variant)))
		return 0;
	unit_name = donor_address(donor, variant->unit_reference.name);
	if (!unit_name || !range_in(donor, unit_name, 1))
		return 0;
	{
		unsigned long room = (unsigned long)(donor->bytes + donor->size - (unsigned char const *)unit_name);
		if (room > 256)
			room = 256;
		if (!memchr(unit_name, 0, room))
			return 0;
	}

	return pool_allows(classify(unit_name)) &&
		rank_allows(classify(unit_name), instance_name(donor, instance));
}

static void seed_donor(struct donor *donor)
{
	long scenario_absolute;
	struct randomizer_tag_header *tags;
	struct scenario *scenario;
	struct tag_reference *palette;
	long index;
	long queued;

	error(_error_silent, "randomizer: seeding %s", donor->leaf);
	tags = (struct randomizer_tag_header *)donor->bytes;
	scenario_absolute = tags->scenario_tag_index & 0xFFFF;
	if (scenario_absolute < 0 || scenario_absolute >= donor->count)
		return;
	scenario = donor_address(donor, donor->instances[scenario_absolute].base_address);
	if (!scenario || !range_in(donor, scenario, sizeof(*scenario)))
		return;
	queued = wanted_count;
	/* the weapons a pool has that this level has not, once each */
	for (index = 0; index < donor->count; index++)
	{
		struct randomizer_tag_instance *instance = donor->instances + index;
		char const *name;

		if (instance->group_tag != WEAPON_DEFINITION_TAG)
			continue;
		name = instance_name(donor, instance);
		if (!name || !weapon_wanted(name) || tag_loaded(WEAPON_DEFINITION_TAG, name) != NONE)
			continue;
		want_root(donor, index);
	}
	if (wanted_count > queued)
		error(_error_silent, "randomizer: %s queued %ld weapons", donor->leaf, wanted_count - queued);
	palette = donor_address(donor, scenario->ai_actor_palette.address);
	if (current.enemies && palette && scenario->ai_actor_palette.count > 0 && scenario->ai_actor_palette.count <= 256 &&
		range_in(donor, palette, (unsigned long)scenario->ai_actor_palette.count * sizeof(*palette)))
	{
		long actors = wanted_count;

		for (index = 0; index < scenario->ai_actor_palette.count; index++)
		{
			if (variant_in_pool(donor, palette[index].index))
				want_root(donor, palette[index].index & 0xFFFF);
		}
		error(_error_silent, "randomizer: %s queued %ld of %ld palette tags (pool %d)",
			donor->leaf, wanted_count - actors, scenario->ai_actor_palette.count, current.pool);
	}
	error(_error_silent, "randomizer: seeded %s (%ld wanted, %ld the same as another map's)",
		donor->leaf, wanted_count, alias_count);
}

/* a model's draw buffers are addresses in the donor map. The part is drawn
from a vertex buffer made here; one that cannot be made is not drawn */
struct randomizer_model_geometry
{
	byte reserved[0x24];
	struct tag_block parts;
};

struct randomizer_model_part
{
	unsigned long flags;
	short shader_index;
	char previous_part_index;
	char next_part_index;
	short centroid_primary_node_index;
	short centroid_secondary_node_index;
	real centroid_primary_node_weight;
	real centroid_secondary_node_weight;
	real_point3d centroid;
	struct tag_block uncompressed_vertices;
	struct tag_block compressed_vertices;
	struct tag_block triangles;
	struct triangle_buffer triangle_buffer;
	struct vertex_buffer vertex_buffer;
};

typedef char randomizer_model_part_size[sizeof(struct randomizer_model_part) == 0x68 ? 1 : -1];

/* the parts given buffers here, whose buffers go when the level does. The
map's own are released with its tags (tags_header_deregister_vertex_and_index_buffers);
these were not, and each level load kept the last one's: the vertices in
contiguous memory, which the textures need too, and the indices in the heap */
#define BOUND_BLOCK_PARTS 256

struct bound_block
{
	struct bound_block *next;
	long count;
	struct randomizer_model_part *parts[BOUND_BLOCK_PARTS];
};

static struct bound_block *bound_blocks;
static long bound_part_count;
static unsigned long bound_vertex_bytes;

static void bound_remember(struct randomizer_model_part *part, unsigned long vertex_bytes)
{
	struct bound_block *block = bound_blocks;

	if (!block || block->count >= BOUND_BLOCK_PARTS)
	{
		block = malloc(sizeof(*block));
		if (!block)
			return;
		block->next = bound_blocks;
		block->count = 0;
		bound_blocks = block;
	}
	block->parts[block->count++] = part;
	bound_part_count++;
	bound_vertex_bytes += vertex_bytes;
}

static void bound_release(void)
{
	while (bound_blocks)
	{
		struct bound_block *next = bound_blocks->next;
		long index;

		for (index = 0; index < bound_blocks->count; index++)
		{
			struct randomizer_model_part *part = bound_blocks->parts[index];

			rasterizer_triangle_buffer_delete(&part->triangle_buffer);
			rasterizer_vertex_buffer_delete(&part->vertex_buffer);
			csmemset(&part->triangle_buffer, 0, sizeof(part->triangle_buffer));
			csmemset(&part->vertex_buffer, 0, sizeof(part->vertex_buffer));
		}
		free(bound_blocks);
		bound_blocks = next;
	}
	bound_part_count = 0;
	bound_vertex_bytes = 0;
}

static void *buffer_record_data(struct donor *donor, void *hardware_format, unsigned long bytes)
{
	unsigned char *record = donor_address(donor, hardware_format);
	void *data;

	if (!record || !range_in(donor, record, 12) || !bytes)
		return NULL;
	data = donor_address(donor, *(void **)(record + 4));
	if (!data || !range_in(donor, data, bytes))
		return NULL;

	return data;
}

static void bind_part(struct donor *donor, struct randomizer_model_part *part)
{
	struct vertex_buffer *vertex = &part->vertex_buffer;
	struct triangle_buffer *triangle = &part->triangle_buffer;
	void *format;
	void *vertices;
	void *indices;
	long vertex_size;
	long index_bytes;
	long count;
	short type;
	short triangle_type;
	long triangle_count;

	format = vertex->hardware_format;
	count = vertex->count;
	type = vertex->type;
	triangle_type = triangle->type;
	triangle_count = triangle->count;
	vertex_size = rasterizer_geometry_get_vertex_size(type);
	if (!format || count <= 0 || vertex_size <= 0 || count > 65535 ||
		(unsigned long)count > 0x1000000UL / (unsigned long)vertex_size)
	{
		csmemset(vertex, 0, sizeof(*vertex));
		csmemset(triangle, 0, sizeof(*triangle));
		return;
	}
	vertices = buffer_record_data(donor, format, (unsigned long)count * (unsigned long)vertex_size);
	if (triangle_type == 0)
		index_bytes = triangle_count > 0 ? triangle_count * 6 : 0;
	else if (triangle_type == 1)
		index_bytes = triangle_count > 0 ? triangle_count * 2 + 4 : 0;
	else
		index_bytes = 0;
	indices = index_bytes > 0 ? buffer_record_data(donor, triangle->hardware_format, (unsigned long)index_bytes) : NULL;
	if (!vertices || !indices)
	{
		csmemset(vertex, 0, sizeof(*vertex));
		csmemset(triangle, 0, sizeof(*triangle));
		return;
	}
	if (!rasterizer_vertex_buffer_new(vertex, type, count, vertices, count * vertex_size))
	{
		csmemset(vertex, 0, sizeof(*vertex));
		csmemset(triangle, 0, sizeof(*triangle));
		return;
	}
	if (!rasterizer_triangle_buffer_new(triangle, triangle_type, triangle_count, indices))
	{
		rasterizer_vertex_buffer_delete(vertex);
		csmemset(vertex, 0, sizeof(*vertex));
		csmemset(triangle, 0, sizeof(*triangle));
		return;
	}
	bound_remember(part, (unsigned long)count * (unsigned long)vertex_size);
}

static void bind_model(struct donor *donor, struct model *model)
{
	long geometry_index;

	if (!model)
		return;
	for (geometry_index = 0; geometry_index < model->geometries.count; geometry_index++)
	{
		struct randomizer_model_geometry *geometry = tag_block_get_element_with_size(
			&model->geometries, geometry_index, sizeof(*geometry));
		long part_index;

		if (!geometry)
			continue;
		for (part_index = 0; part_index < geometry->parts.count; part_index++)
		{
			struct randomizer_model_part *part = tag_block_get_element_with_size(
				&geometry->parts, part_index, sizeof(*part));

			if (part)
				bind_part(donor, part);
		}
	}
}

/* the precache screen has already closed. Open it again and fill it as
the tags are copied, so a stop is a bar that stops moving */
static void import_progress(long done, long total)
{
	real fraction;

	if (total <= 0)
		return;
	fraction = (real)done / (real)total;
	if (fraction < 0.001f)
		fraction = 0.001f;
	if (fraction > 1.0f)
		fraction = 1.0f;
	error(_error_silent, "randomizer: copying tag %ld of %ld", done, total);
	if (!import_progress_open)
	{
		progress_bar_begin(TRUE);
		import_progress_open = 1;
	}
	game_loading_progress_show(TRUE, fraction);
	main_pregame_render();
	main_present_frame();
}

static void import_progress_end(void)
{
	if (!import_progress_open)
		return;
	progress_bar_end();
	game_loading_progress_show(FALSE, 1.0f);
	import_progress_open = 0;
}

/* a range of pixels the pack has not got, to be read from the map */
static void donor_extra_add(struct donor *donor, long offset, long size)
{
	long index;

	if (offset < 0x800 || size > DONOR_EXTRA_BYTES_MAX - donor_extra_bytes)
		return;
	for (index = 0; index < donor->extra_count; index++)
	{
		if (donor->extras[index].offset == offset && donor->extras[index].size >= size)
			return;
	}
	if (donor->extra_count == donor->extra_capacity)
	{
		long capacity = donor->extra_capacity ? donor->extra_capacity * 2 : 32;
		struct donor_extra *grown = realloc(donor->extras, (unsigned long)capacity * sizeof(*grown));

		if (!grown)
			return;
		donor->extras = grown;
		donor->extra_capacity = capacity;
	}
	donor->extras[donor->extra_count].offset = offset;
	donor->extras[donor->extra_count].size = size;
	donor->extras[donor->extra_count].bytes = NULL;
	donor->extra_count++;
	donor_extra_bytes += size;
}

static int donor_extra_less(void const *left, void const *right)
{
	struct donor_extra const *a = left;
	struct donor_extra const *b = right;

	return a->offset < b->offset ? -1 : a->offset > b->offset;
}

/* the ranges inflated from a disc map's zlib stream in one pass, each
copied out as the stream passes it (sorted by offset); whether it reached
the last */
static int extras_inflate(HANDLE file, struct donor_extra *extras, long count)
{
	z_stream stream;
	unsigned char input[0x2000];
	unsigned char output[0x4000];
	long produced = 0x800;
	long end = 0;
	long first = 0;
	long index;
	int result = Z_OK;
	DWORD got;

	for (index = 0; index < count; index++)
	{
		if (extras[index].offset + extras[index].size > end)
			end = extras[index].offset + extras[index].size;
	}
	if (!donor_file_at(file, 0x800))
		return 0;
	memset(&stream, 0, sizeof(stream));
	if (inflateInit(&stream) != Z_OK)
		return 0;
	while (produced < end && result != Z_STREAM_END)
	{
		long filled;

		if (stream.avail_in == 0)
		{
			got = 0;
			if (!ReadFile(file, input, (DWORD)sizeof(input), &got, NULL) || got == 0)
				break;
			stream.next_in = (Bytef *)input;
			stream.avail_in = got;
		}
		stream.next_out = (Bytef *)output;
		stream.avail_out = (unsigned int)sizeof(output);
		result = inflate(&stream, Z_NO_FLUSH);
		if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR)
			break;
		filled = (long)(sizeof(output) - stream.avail_out);
		if (filled <= 0 && stream.avail_in)
			break;
		for (index = first; index < count && extras[index].offset < produced + filled; index++)
		{
			struct donor_extra *extra = extras + index;
			long from = extra->offset > produced ? extra->offset : produced;
			long to = extra->offset + extra->size < produced + filled ? extra->offset + extra->size : produced + filled;

			if (from < to)
				memcpy(extra->bytes + (from - extra->offset), output + (from - produced), (unsigned long)(to - from));
		}
		produced += filled;
		while (first < count && extras[first].offset + extras[first].size <= produced)
			first++;
		if (result == Z_BUF_ERROR)
			result = Z_OK;
	}
	inflateEnd(&stream);

	return produced >= end;
}

/* the pixels the pack has not got of the bitmaps copied from a donor,
read from its map (a few HUD bitmaps: the stream is inflated once, to the
last of them); none when the map cannot be read, which draws them black */
static void donor_extras_fill(struct donor *donor)
{
	char path[512];
	HANDLE file;
	DWORD physical;
	unsigned long started;
	long bytes = 0;
	long index;
	int read;

	if (!donor->from_pack || donor->extra_count <= 0)
		return;
	qsort(donor->extras, (unsigned long)donor->extra_count, sizeof(*donor->extras), donor_extra_less);
	for (index = 0; index < donor->extra_count; index++)
	{
		donor->extras[index].bytes = malloc((unsigned long)donor->extras[index].size);
		if (!donor->extras[index].bytes)
			break;
		bytes += donor->extras[index].size;
	}
	snprintf(path, sizeof(path), "%s%s.map", cache_files_map_directory(), donor->leaf);
	file = index == donor->extra_count ? CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL) :
		INVALID_HANDLE_VALUE;
	read = 0;
	started = system_milliseconds();
	if (file != INVALID_HANDLE_VALUE)
	{
		physical = GetFileSize(file, NULL);
		if (physical != INVALID_FILE_SIZE && (long)physical >= donor->file_length)
		{
			for (read = 1, index = 0; read && index < donor->extra_count; index++)
			{
				read = donor_file_at(file, donor->extras[index].offset) &&
					donor_file_read(file, donor->extras[index].bytes, (unsigned long)donor->extras[index].size);
			}
		}
		else
			read = extras_inflate(file, donor->extras, donor->extra_count);
		CloseHandle(file);
	}
	if (!read)
	{
		error(_error_silent, "randomizer: the %ld ranges the pack has not got of %s cannot be read from %s",
			donor->extra_count, donor->leaf, path);
		for (index = 0; index < donor->extra_count; index++)
		{
			if (donor->extras[index].bytes)
				free(donor->extras[index].bytes);
			donor->extras[index].bytes = NULL;
		}
		return;
	}
	error(_error_silent, "randomizer: %ld ranges the pack has not got (%ld bytes) read from %s in %lu ms",
		donor->extra_count, bytes, path, system_milliseconds() - started);
}

/* bytes at `offset` of a donor's map from the ranges read for the pack
(donor_extras_fill): how many */
static long extras_copy(struct donor *donor, long offset, long size, unsigned char *buffer)
{
	long index;

	for (index = 0; index < donor->extra_count; index++)
	{
		struct donor_extra const *extra = donor->extras + index;

		if (extra->bytes && offset >= extra->offset && offset < extra->offset + extra->size)
		{
			long have = extra->offset + extra->size - offset;

			if (have > size)
				have = size;
			memcpy(buffer, extra->bytes + (offset - extra->offset), (unsigned long)have);
			return have;
		}
	}

	return 0;
}

/* a copied bitmap's or sound's file range, checked against the pack before
the tag bytes are dropped. A miss here is a black texture or an empty sample */
static void note_range(struct donor *donor, char const *kind, char const *name, long index, long offset, long size)
{
	int cover;
	long probe;

	if (size < 0)
		size = 0;
	probe = size > 0 ? size : 1;
	cover = span_covers(donor, offset, probe);
	resource_checked++;
	if (size <= 0)
		resource_empty++;
	if (cover == 1)
		return;
	resource_uncovered++;
	if (donor->from_pack && size > 0 && !strcmp(kind, "bitm"))
		donor_extra_add(donor, offset, size);
	if (resource_logged >= 48)
		return;
	resource_logged++;
	error(_error_silent, "randomizer: %s '%s' #%ld offset %ld size %ld %s (%s.map, %ld ranges)",
		kind, name ? name : "?", index, offset, size,
		cover < 0 ? "only partly in the pack" : "not in the pack",
		donor->leaf, donor->span_count);
}

static void note_bitmap(struct donor *donor, char const *name, void *root)
{
	struct bitmap_group *group = root;
	long index;
	long count;

	if (!group)
		return;
	count = group->bitmaps.count;
	/* the pixels are at pixels_offset. pixel_data.file_offset is not a base
	on these caches (adding it lands off the pixels, and the blocks come out
	as noise). A loaded map never adds it; clear it so nothing does later */
	if (count > 256)
		count = 256;
	for (index = 0; index < count; index++)
	{
		struct bitmap_data *bitmap;

		bitmap = tag_block_get_element_with_size(&group->bitmaps, index, sizeof(*bitmap));
		if (!bitmap)
			continue;
		note_range(donor, "bitm", name, index, bitmap->pixels_offset, bitmap->pixels_size);
	}
	group->pixel_data.file_offset = 0;
}

static void note_sound(struct donor *donor, char const *name, void *root, long datum)
{
	struct sound_definition *sound = root;
	long range_index;
	long range_count;

	if (!sound)
		return;
	range_count = sound->pitch_ranges.count;
	if (range_count > 64)
		range_count = 64;
	for (range_index = 0; range_index < range_count; range_index++)
	{
		struct sound_pitch_range *range;
		long permutation_index;
		long permutation_count;

		range = tag_block_get_element_with_size(&sound->pitch_ranges, range_index, sizeof(*range));
		if (!range)
			continue;
		permutation_count = range->permutations.count;
		if (permutation_count > 256)
			permutation_count = 256;
		for (permutation_index = 0; permutation_index < permutation_count; permutation_index++)
		{
			struct sound_permutation *permutation;

			permutation = tag_block_get_element_with_size(
				&range->permutations, permutation_index, sizeof(*permutation));
			if (!permutation)
				continue;
			/* the sample read goes through cache_tag_index (unknown2). A
			loaded map's check writes this sound's tag there; a copy still
			names the donor's, so the read never reaches the pack */
			permutation->unknown2 = (unsigned long)datum;
			permutation->unknown3 = (unsigned long)datum;
			note_range(donor, "snd!", name, permutation_index,
				permutation->samples.file_offset, permutation->samples.size);
		}
	}
}

static void import_wanted(void)
{
	long existing = 0;
	struct cache_file_tag_instance *instances = cache_files_tag_instances(&existing);
	long salt = 0;
	long index;
	short copying;
	short failed;

	copying = -1;
	failed = -1;
	resource_checked = 0;
	resource_uncovered = 0;
	resource_empty = 0;
	resource_logged = 0;
	if (!instances || wanted_count <= 0)
		return;
	/* (want_root keeps it to IMPORT_MAX: a list cut short here would copy
	tags without some of those they name) */
	if (wanted_count > IMPORT_MAX)
	{
		error(_error_silent, "randomizer: %ld tags wanted, past %d; none copied", wanted_count, IMPORT_MAX);
		return;
	}
	qsort(wanted, (unsigned long)wanted_count, sizeof(*wanted), wanted_less);
	grown_instances = malloc((unsigned long)(existing + wanted_count) * sizeof(*grown_instances));
	if (!grown_instances)
	{
		platform_log("randomizer: no room for the imported tags");
		return;
	}
	memcpy(grown_instances, instances, (unsigned long)existing * sizeof(*grown_instances));
	for (index = 0; index < existing; index++)
	{
		long instance_salt = (unsigned long)instances[index].tag_index >> 16;

		if (instance_salt > salt)
			salt = instance_salt;
	}
	saved_instances = instances;
	saved_count = existing;
	grown_count = existing;
	cache_files_set_tag_instances(grown_instances, grown_count);
	salt++;
	import_progress(0, wanted_count);
	for (index = 0; index < wanted_count && !import_stopped; index++)
	{
		struct wanted *entry = wanted + index;
		struct donor *donor;
		struct randomizer_tag_instance *source;
		struct tag_schema_definition const *schema;
		char const *name;
		void *root_source;
		void *root;
		char *name_copy;
		long datum;
		struct cache_file_tag_instance *dest;

		if (entry->donor < 0 || entry->donor >= donor_count || !entry->name[0])
			continue;
		donor = donors + entry->donor;
		if ((index & 15) == 0)
			import_progress(index, wanted_count);
		if (entry->donor != copying)
		{
			copying = entry->donor;
			error(_error_silent, "randomizer: copying %s.map", donor->leaf);
		}
		if (!donor_bytes(donor) || !donor->datum_of || !donor->instances ||
			entry->absolute < 0 || entry->absolute >= donor->count)
		{
			if (failed != entry->donor)
			{
				failed = entry->donor;
				error(_error_silent, "randomizer: could not read %s.map again", donor->leaf);
			}
			continue;
		}
		source = donor->instances + entry->absolute;
		schema = schema_of((unsigned long)source->group_tag);
		name = entry->name;
		if (!schema)
		{
			platform_log("randomizer: left out %s, which has no schema", name);
			continue;
		}
		if (tag_loaded(source->group_tag, name) != NONE)
		{
			donor->datum_of[entry->absolute] = tag_loaded(source->group_tag, name);
			continue;
		}
		if (grown_count > 0xFFFE || copied_count >= IMPORT_MAX)
		{
			import_stopped = 1;
			break;
		}
		root_source = donor_address(donor, source->base_address);
		if (!root_source || schema->size <= 0 || !range_in(donor, root_source, (unsigned long)schema->size))
			continue;
		root = grab((unsigned long)schema->size);
		name_copy = grab((unsigned long)strlen(name) + 1);
		if (!root || !name_copy)
			break;
		memcpy(root, root_source, (unsigned long)schema->size);
		strcpy(name_copy, name);
		copy_fields(donor, root, schema, 0);
		if (source->group_tag == BITMAP_GROUP_TAG)
			note_bitmap(donor, name, root);
		if (source->group_tag == 'mode')
			bind_model(donor, root);
		datum = (salt << 16) | grown_count;
		if (source->group_tag == SOUND_DEFINITION_TAG)
			note_sound(donor, name, root, datum);
		dest = grown_instances + grown_count;
		memset(dest, 0, sizeof(*dest));
		dest->group_tag = source->group_tag;
		dest->parent_group_tags[0] = source->parent_group_tags[0];
		dest->parent_group_tags[1] = source->parent_group_tags[1];
		dest->tag_index = datum;
		dest->name = name_copy;
		dest->base_address = root;
		donor->datum_of[entry->absolute] = datum;
		donor->keep = 1;
		remember_import(grown_count, entry->donor);
		copied[copied_count].donor = entry->donor;
		copied[copied_count].root = root;
		copied[copied_count].schema = schema;
		copied_count++;
		grown_count++;
		cache_files_set_tag_instances(grown_instances, grown_count);
	}
	import_progress(wanted_count, wanted_count);
	import_progress_end();
	error(_error_silent, "randomizer: %ld pixel and sample ranges, %ld not in the pack, %ld empty",
		resource_checked, resource_uncovered, resource_empty);
	for (index = 0; index < donor_count; index++)
	{
		release_donor_blob(donors + index);
		donor_extras_fill(donors + index);
	}
	if (import_stopped)
	{
		/* (out of room part way: tags copied without some they name would
		be drawn with what is not there. The level keeps only its own) */
		error(_error_silent, "randomizer: ran out of room after %ld tags; none are kept", copied_count);
		cache_files_set_tag_instances(saved_instances, saved_count);
		free(grown_instances);
		grown_instances = NULL;
		saved_instances = NULL;
		saved_count = 0;
		grown_count = 0;
		bound_release();
		held_free();
		copied_count = 0;
		import_count = 0;
		return;
	}
	/* a tag wanted from one map and found in another is that map's copy */
	for (index = 0; index < alias_count; index++)
	{
		struct wanted_alias *alias = aliases + index;

		if (alias->donor >= 0 && alias->donor < donor_count && donors[alias->donor].datum_of &&
			alias->target_donor >= 0 && alias->target_donor < donor_count && donors[alias->target_donor].datum_of)
			donors[alias->donor].datum_of[alias->absolute] =
				donors[alias->target_donor].datum_of[alias->target_absolute];
	}
	for (index = 0; index < copied_count; index++)
		retarget_fields(donors + copied[index].donor, copied[index].root, copied[index].schema, 0);
	if (copied_count)
		error(_error_silent, "randomizer: imported %ld tags", copied_count);
}

static void add_variant(long datum)
{
	struct actor_variant_definition *variant;
	char const *unit_name;

	/* (a profile's join by their settings: add_custom_root) */
	if (variant_count >= POOL_MAX || datum == NONE || !tag_index_is_group(datum, ACTOR_VARIANT_DEFINITION_TAG) ||
		tag_injector_tag_injected(datum))
		return;
	variant = actor_variant_definition_get(datum);
	if (!variant)
		return;
	unit_name = variant->unit_reference.index == NONE ? NULL : tag_get_name(variant->unit_reference.index);
	if (!pool_allows(classify(unit_name)) || !rank_allows(classify(unit_name), tag_get_name(datum)))
		return;
	variant_pool[variant_count++] = datum;
}

/* a weapon a choice may give, when its pool has it */
static void add_weapon(struct weapon_choice *choice, long datum, int pool, int type, int for_pickups)
{
	long index;

	if (choice->count >= WEAPON_MAX || datum == NONE || !tag_index_is_group(datum, WEAPON_DEFINITION_TAG) ||
		!weapon_pool_allows(pool, type, weapon_kind(tag_get_name(datum)), for_pickups))
		return;
	for (index = 0; index < choice->count; index++)
	{
		if (choice->definitions[index] == datum)
			return;
	}
	choice->definitions[choice->count++] = datum;
}

/* whether a name has any of the words */
static int name_has(char const *lower, char const *const *words)
{
	for (; *words; words++)
	{
		if (strstr(lower, *words))
			return 1;
	}

	return 0;
}

/* a Tag Injector root's words, which AUTO finds its pools by: its path and
the name it is shown by (one a player gave a protected map's "weapon 2"),
in lower case */
static void custom_words(long root, char const *name, char *words, unsigned int size)
{
	char shown[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
	char both[256 + TAG_INJECTOR_CUSTOM_TYPE_LENGTH];

	tag_injector_tag_display_name(custom_profile, custom_root_tags[root], shown, sizeof(shown));
	snprintf(both, sizeof(both), "%s %s", name ? name : "", shown);
	lower_copy(words, size, both);
}

/* a Tag Injector actor variant's faction (its setting "faction", else its
unit's type's): the enemy pool it joins, else POOL_ANY for ANY's alone */
static int custom_variant_faction(long root, int kind)
{
	char const *faction = tag_injector_tag_setting(custom_profile, custom_root_tags[root], "faction");

	if (faction[0] && _stricmp(faction, "auto"))
	{
		int pool = pool_id(faction);

		return pool == POOL_COVENANT || pool == POOL_FLOOD || pool == POOL_HUMAN ? pool : POOL_ANY;
	}
	switch (kind)
	{
	case TYPE_GRUNT: case TYPE_ELITE: case TYPE_JACKAL: case TYPE_HUNTER: case TYPE_ENGINEER:
		return POOL_COVENANT;
	case TYPE_FLOOD_COMBAT: case TYPE_FLOOD_CARRIER: case TYPE_FLOOD_INFECTION:
		return POOL_FLOOD;
	case TYPE_MARINE:
		return POOL_HUMAN;
	}

	return POOL_ANY;
}

/* ... and the campaign's type it counts as for ONE TYPE (its setting
"type", else its unit's), or TYPE_NONE */
static int custom_variant_type(long root, int kind)
{
	char const *type = tag_injector_tag_setting(custom_profile, custom_root_tags[root], "type");
	int index;

	if (!type[0] || !_stricmp(type, "auto"))
		return kind;
	for (index = 0; index < (int)NUMBEROF(type_names); index++)
	{
		if (!_stricmp(type, type_names[index].name))
			return type_names[index].type;
	}

	return TYPE_NONE;
}

/* a Tag Injector weapon's faction (its setting "faction", else the
campaign's kind's of its name, else its name's words): Covenant's or not */
static int custom_weapon_covenant(long root, char const *name)
{
	static char const *const covenant_words[] = {
		"covenant", "plasma", "needle", "carbine", "fuel rod", "sword", "brute", "spike", "elite", "beam", NULL
	};
	char const *faction = tag_injector_tag_setting(custom_profile, custom_root_tags[root], "faction");
	char lower[256 + TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
	int kind = weapon_kind(name);

	if (faction[0] && _stricmp(faction, "auto"))
		return !_stricmp(faction, "covenant");
	if (kind != WEAPON_NONE)
		return weapon_kinds[kind].covenant;
	custom_words(root, name, lower, sizeof(lower));

	return name_has(lower, covenant_words);
}

/* ... and its classes (WEAPON_CLASS_*_BIT: its setting "class", else the
campaign's kind's, else its name's words) */
static int custom_weapon_classes(long root, char const *name)
{
	static char const *const precision_words[] = {
		"sniper", "pistol", "carbine", "battle rifle", "battlerifle", "dmr", "magnum", "beam rifle", NULL
	};
	static char const *const power_words[] = {
		"rocket", "fuel", "sword", "shotgun", "launcher", "cannon", "hammer", "flame", "laser", "grenade", "mortar", NULL
	};
	static char const *const automatic_words[] = {
		"assault", "smg", "rifle", "needler", "spiker", "machine", "repeater", "mauler", NULL
	};
	char const *setting = tag_injector_tag_setting(custom_profile, custom_root_tags[root], "class");
	char lower[256 + TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
	int kind = weapon_kind(name);

	if (setting[0] && _stricmp(setting, "auto"))
	{
		return !_stricmp(setting, "precision") ? WEAPON_CLASS_PRECISION_BIT :
			!_stricmp(setting, "automatic") ? WEAPON_CLASS_AUTOMATIC_BIT :
			!_stricmp(setting, "power") ? WEAPON_CLASS_POWER_BIT : 0;
	}
	if (kind != WEAPON_NONE)
	{
		return (weapon_kinds[kind].precision ? WEAPON_CLASS_PRECISION_BIT : 0) |
			(weapon_kinds[kind].automatic ? WEAPON_CLASS_AUTOMATIC_BIT : 0) |
			(weapon_kinds[kind].power ? WEAPON_CLASS_POWER_BIT : 0);
	}
	custom_words(root, name, lower, sizeof(lower));
	if (name_has(lower, precision_words))
		return WEAPON_CLASS_PRECISION_BIT;
	if (name_has(lower, power_words))
		return WEAPON_CLASS_POWER_BIT;
	if (name_has(lower, automatic_words))
		return WEAPON_CLASS_AUTOMATIC_BIT;

	return 0;
}

/* whether a Tag Injector root is of the custom type `name` */
static int custom_type_is(long root, char const *name)
{
	char type[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];

	tag_injector_tag_custom_type(custom_profile, custom_root_tags[root], type, sizeof(type));

	return name[0] && !strcmp(type, name);
}

/* whether a pool of weapons has a Tag Injector weapon */
static int custom_weapon_pool_allows(long root, char const *name, int pool, int custom_type, char const *custom_name)
{
	int classes;

	switch (pool)
	{
	case WEAPON_POOL_ANY: case WEAPON_POOL_TAG_INJECTOR: return 1;
	case WEAPON_POOL_COVENANT: return custom_weapon_covenant(root, name);
	case WEAPON_POOL_HUMAN: return !custom_weapon_covenant(root, name);
	case WEAPON_POOL_ONE: return custom_type && custom_type_is(root, custom_name);
	}
	classes = custom_weapon_classes(root, name);
	switch (pool)
	{
	case WEAPON_POOL_PRECISION: return (classes & WEAPON_CLASS_PRECISION_BIT) != 0;
	case WEAPON_POOL_AUTOMATIC: return (classes & WEAPON_CLASS_AUTOMATIC_BIT) != 0;
	case WEAPON_POOL_POWER: return (classes & WEAPON_CLASS_POWER_BIT) != 0;
	}

	return 0;
}

/* a weapon or an actor variant of the Tag Injector's profile (the root
`root` of custom_roots), in each pool of the setup its settings name: ANY's
and TAG INJECTOR's, its faction's, its class's, and the one type of its
type or its custom type */
static void add_custom_root(long root)
{
	long datum = custom_roots[root];
	struct weapon_choice *choices[2];
	int pools[2];
	int custom_types[2];
	char const *custom_names[2];
	short choice_count = 0;
	short choice;
	char const *name;

	if (datum == NONE)
		return;
	name = tag_get_name(datum);
	if (tag_index_is_group(datum, ACTOR_VARIANT_DEFINITION_TAG))
	{
		struct actor_variant_definition *variant = actor_variant_definition_get(datum);
		char const *unit_name = variant && variant->unit_reference.index != NONE ?
			tag_get_name(variant->unit_reference.index) : NULL;
		int kind = classify(unit_name);
		int allowed;
		char words[256 + TAG_INJECTOR_CUSTOM_TYPE_LENGTH];

		if (!current.enemies || variant_count >= POOL_MAX)
			return;
		/* (a unit of no type the campaign has: the variant's words', its
		name's too) */
		custom_words(root, name, words, sizeof(words));
		if (kind == TYPE_NONE)
			kind = classify(words);
		switch (current.pool)
		{
		case POOL_ANY: case POOL_TAG_INJECTOR:
			allowed = 1;
			break;
		case POOL_ONE:
			allowed = current.custom_type ? custom_type_is(root, current.custom_type_name) :
				custom_variant_type(root, kind) == current.type && rank_allows(current.type, words);
			break;
		default:
			allowed = custom_variant_faction(root, kind) == current.pool;
			break;
		}
		if (!allowed)
			return;
		for (choice = 0; choice < variant_count && variant_pool[choice] != datum; choice++)
		{
		}
		if (choice == variant_count)
			variant_pool[variant_count++] = datum;
		return;
	}
	if (!tag_index_is_group(datum, WEAPON_DEFINITION_TAG))
		return;
	if (current.pickups)
	{
		choices[choice_count] = &pickup_choice;
		pools[choice_count] = current.pickup_pool;
		custom_types[choice_count] = current.pickup_custom_type;
		custom_names[choice_count++] = current.pickup_custom_type_name;
	}
	if (current.weapons)
	{
		choices[choice_count] = &actor_weapon_choice;
		pools[choice_count] = current.weapon_pool;
		custom_types[choice_count] = current.weapon_custom_type;
		custom_names[choice_count++] = current.weapon_custom_type_name;
	}
	for (choice = 0; choice < choice_count; choice++)
	{
		struct weapon_choice *weapons = choices[choice];
		long index;

		if (!custom_weapon_pool_allows(root, name, pools[choice], custom_types[choice], custom_names[choice]))
			continue;
		for (index = 0; index < weapons->count && weapons->definitions[index] != datum; index++)
		{
		}
		if (index == weapons->count && weapons->count < WEAPON_MAX)
			weapons->definitions[weapons->count++] = datum;
	}
}

/* a block definition still at the address the map was linked with is not a
pointer on this machine. The tag walk clears the ones it visits */
static void clear_linked_definition(struct tag_block *block)
{
	unsigned long address;

	if (!block || !block->definition)
		return;
	address = (unsigned long)block->definition;
	if (address >= 0x80000000UL && address < 0x88000000UL)
		block->definition = NULL;
}

/* scenario_tags_load calls us before scenario_load stores global_scenario.
The scenario tag is already in the table. */
static struct scenario *loaded_scenario(void)
{
	long count = 0;
	struct cache_file_tag_instance *instances = cache_files_tag_instances(&count);
	long index;

	if (!instances)
		return NULL;
	for (index = 0; index < count; index++)
	{
		if (instances[index].group_tag == SCENARIO_TAG && instances[index].base_address)
			return scenario_definition_get(instances[index].tag_index);
	}

	return NULL;
}

static void build_pools(void)
{
	struct scenario *scenario = loaded_scenario();
	long count = 0;
	struct cache_file_tag_instance *instances = cache_files_tag_instances(&count);
	long index;

	variant_count = 0;
	pickup_choice.count = 0;
	actor_weapon_choice.count = 0;
	if (!current.enemies && !current.pickups && !current.weapons)
		return;
	for (index = 0; instances && index < count; index++)
	{
		if (instances[index].group_tag == ACTOR_VARIANT_DEFINITION_TAG)
			add_variant(instances[index].tag_index);
	}
	/* a pool of its own: each of its weapons the level has, or was given */
	for (index = 0; instances && index < count; index++)
	{
		if (instances[index].group_tag != WEAPON_DEFINITION_TAG)
			continue;
		if (current.pickup_pool != WEAPON_POOL_ANY)
			add_weapon(&pickup_choice, instances[index].tag_index, current.pickup_pool, pickup_one_type(), 1);
		add_weapon(&actor_weapon_choice, instances[index].tag_index, current.weapon_pool, weapon_one_type(), 0);
	}
	/* the Tag Injector's profile's: the pools their settings name */
	for (index = 0; index < custom_root_count; index++)
		add_custom_root(index);
	/* "any": the level's own lying about, and those its actors carry */
	if (current.pickup_pool == WEAPON_POOL_ANY)
	{
		if (scenario && scenario->weapon_palette.count > 0 && scenario->weapon_palette.count <= 256)
		{
			clear_linked_definition(&scenario->weapon_palette);
			for (index = 0; index < scenario->weapon_palette.count; index++)
			{
				struct scenario_object_palette_entry *entry = tag_block_get_element_with_size(
					&scenario->weapon_palette, index, sizeof(*entry));

				if (entry)
					add_weapon(&pickup_choice, entry->reference.index, WEAPON_POOL_ANY, 0, 1);
			}
		}
		for (index = 0; index < variant_count; index++)
		{
			struct actor_variant_definition *variant = actor_variant_definition_get(variant_pool[index]);

			if (variant)
				add_weapon(&pickup_choice, variant->ranged_combat.reference.index, WEAPON_POOL_ANY, 0, 1);
		}
	}
}

static void remember_protected(char const *name)
{
	if (!name || !name[0] || protected_count >= PROTECTED_MAX)
		return;
	snprintf(protected_names[protected_count], sizeof(protected_names[0]), "%s", name);
	protected_count++;
}

static void build_protected(void)
{
	struct scenario *scenario = loaded_scenario();
	long conversation_index;

	protected_count = 0;
	if (!scenario || scenario->ai_conversations.count <= 0 || scenario->ai_conversations.count > 256)
		return;
	clear_linked_definition(&scenario->ai_conversations);
	for (conversation_index = 0; conversation_index < scenario->ai_conversations.count; conversation_index++)
	{
		struct ai_conversation *conversation = tag_block_get_element_with_size(
			&scenario->ai_conversations, conversation_index, sizeof(*conversation));
		long participant_index;

		if (!conversation || conversation->participants.count <= 0 || conversation->participants.count > 64)
			continue;
		clear_linked_definition(&conversation->participants);
		for (participant_index = 0; participant_index < conversation->participants.count; participant_index++)
		{
			struct randomizer_participant *participant = tag_block_get_element_with_size(
				&conversation->participants, participant_index, sizeof(*participant));

			if (participant)
				remember_protected(participant->ai_index_name);
		}
	}
}

static int squad_is_protected(char const *name)
{
	int index;

	if (!name || !name[0])
		return 0;
	for (index = 0; index < protected_count; index++)
	{
		if (!_stricmp(name, protected_names[index]))
			return 1;
	}

	return 0;
}

/* a fresh load of a mission (scenario_name's, NULL for none known yet). A
checkpoint revert does not load the tags again, so the actors already placed
keep this roll. A daily challenge's level rolls the challenge's seed, the
same for every player (community.c) */
static void placement_roll_new(char const *scenario_name)
{
	static unsigned long salt;
	unsigned long challenge_seed = scenario_name ? community_randomizer_seed(scenario_name) : 0;

	salt++;
	placement_roll = challenge_seed ? challenge_seed : mix(system_milliseconds() ^ salt);
	if (!placement_roll)
		placement_roll = 1;
	placement_roll_ready = 1;
	error(_error_silent, "randomizer: roll %08lx", placement_roll);
}

unsigned long randomizer_roll(void)
{
	return placement_roll;
}

static void placement_roll_from_remote(void)
{
	placement_roll = (unsigned long)remote_setup[3] |
		((unsigned long)remote_setup[4] << 8) |
		((unsigned long)remote_setup[5] << 16) |
		((unsigned long)remote_setup[6] << 24);
	placement_roll_ready = 1;
	error(_error_silent, "randomizer: roll %08lx", placement_roll);
}

static unsigned long placement_seed(long encounter_index, long squad_index, long location_index, float const *position)
{
	unsigned long seed = mix_text(0x524E444DUL, map_name_leaf);

	seed = mix(seed ^ placement_roll);
	seed = mix(seed ^ (unsigned long)current.enemies);
	seed = mix(seed ^ ((unsigned long)current.pickups << 1));
	seed = mix(seed ^ ((unsigned long)current.weapons << 2));
	seed = mix(seed ^ (unsigned long)current.pool);
	seed = mix(seed ^ (unsigned long)current.type);
	seed = mix(seed ^ (unsigned long)encounter_index);
	seed = mix(seed ^ (unsigned long)squad_index);
	seed = mix(seed ^ (unsigned long)location_index);
	if (position)
	{
		seed = mix_bits(seed, position[0]);
		seed = mix_bits(seed, position[1]);
		seed = mix_bits(seed, position[2]);
	}

	return seed;
}

static long pool_pick(long const *pool, long count, unsigned long seed)
{
	if (count <= 0)
		return NONE;

	return pool[seed % (unsigned long)count];
}

static void wanted_free(void)
{
	if (wanted)
		free(wanted);
	if (wanted_slots)
		free(wanted_slots);
	if (aliases)
		free(aliases);
	wanted = NULL;
	wanted_slots = NULL;
	aliases = NULL;
	alias_count = 0;
}

/* the lists a level's copying gathers into; 0 when there is no room */
static int gather_begin(void)
{
	walk_steps = 0;
	wanted_full = 0;
	alias_count = 0;
	wanted = malloc(WANTED_MAX * sizeof(*wanted));
	wanted_slots = malloc(WANTED_SLOTS * sizeof(*wanted_slots));
	aliases = malloc(WANTED_MAX * sizeof(*aliases));
	if (!wanted || !wanted_slots || !aliases)
	{
		error(_error_silent, "randomizer: no room to gather tags");
		return 0;
	}
	memset(wanted, 0, WANTED_MAX * sizeof(*wanted));
	memset(wanted_slots, 0, WANTED_SLOTS * sizeof(*wanted_slots));

	return 1;
}

/* the randomizer's: each other campaign map's actors and weapons the
setup's pools take */
static void gather_campaign(char const *leaf)
{
	int level;

	/* (a pool of weapons other than the level's needs them copied too; the
	TAG INJECTOR pools and custom types are the profile's alone) */
	int enemies = current.enemies && current.pool != POOL_TAG_INJECTOR &&
		!(current.pool == POOL_ONE && current.custom_type);
	int pickups = current.pickups && current.pickup_pool != WEAPON_POOL_ANY &&
		current.pickup_pool != WEAPON_POOL_TAG_INJECTOR &&
		!(current.pickup_pool == WEAPON_POOL_ONE && current.pickup_custom_type);
	int weapons = current.weapons && current.weapon_pool != WEAPON_POOL_ANY &&
		current.weapon_pool != WEAPON_POOL_TAG_INJECTOR &&
		!(current.weapon_pool == WEAPON_POOL_ONE && current.weapon_custom_type);

	if (!enemies && !pickups && !weapons)
		return;
	error(_error_silent, "randomizer: reading actors and weapons from the other campaign maps");
	for (level = 0; level < 10 && donor_count < DONOR_COUNT; level++)
	{
		char const *path = main_get_solo_level_name(level);
		char const *other = tag_name_strip_path(path);
		struct donor *donor = donors + donor_count;

		if (!_stricmp(other, leaf))
			continue;
		error(_error_silent, "randomizer: %s.map", other);
		if (!open_donor(donor, other, &randomizer_pack))
		{
			close_donor_bytes(donor);
			continue;
		}
		donor_count++;
		error(_error_silent, "randomizer: read %s.map (%ld tags)", other, donor->count);
		seed_donor(donor);
		release_donor_blob(donor);
	}
}

/* the skulls': tags no campaign map has, or not every one, copied from a
map of skulls.pack's (randomizer_pack.c), else from the map itself. The
skull the enemies carry to be found, a multiplayer map's oddball
(skulls.c); Scarab's rocket, Wizard's too; Outbreak's Flood (its spore,
carrier and combat forms, and the carrier's burst: skulls_outbreak.c) and
Scarab's fuel rod, Two Betrayals' (c40). A level that has a root keeps its
own; the randomizer's copy of one from c40 too (want's aliases) */
#define SKULL_DONOR "wizard"

struct skull_root
{
	long group;
	char const *name;
};

static void gather_skull_roots(char const *leaf, struct skull_root const *roots, short count)
{
	struct donor *donor = donors + donor_count;
	short root;
	short missing = 0;
	long index;

	for (root = 0; root < count; root++)
		missing += (short)(tag_loaded(roots[root].group, roots[root].name) == NONE);
	if (!missing || donor_count >= DONOR_COUNT)
		return;
	error(_error_silent, "randomizer: reading %d skull tags from %s.map", missing, leaf);
	if (!open_donor(donor, leaf, &skulls_pack))
	{
		close_donor_bytes(donor);
		return;
	}
	donor_count++;
	for (root = 0; root < count; root++)
	{
		if (tag_loaded(roots[root].group, roots[root].name) != NONE)
			continue;
		for (index = 0; index < donor->count; index++)
		{
			struct randomizer_tag_instance *instance = donor->instances + index;
			char const *name = instance_name(donor, instance);

			if (instance->group_tag == roots[root].group && name && !_stricmp(name, roots[root].name))
			{
				want_root(donor, index);
				break;
			}
		}
		if (index == donor->count)
			error(_error_silent, "randomizer: %s.map has no %s", leaf, roots[root].name);
	}
	release_donor_blob(donor);
}

static void gather_skulls(int carried, int outbreak, int scarab)
{
	static struct skull_root const flood_roots[] =
	{
		{ 'bipd', SKULLS_OUTBREAK_SPORE_TAG },
		{ 'actv', SKULLS_OUTBREAK_CARRIER_TAG },
		{ 'effe', SKULLS_OUTBREAK_BURST_TAG },
		{ 'actv', "characters\\floodcombat elite\\floodcombat elite unarmed" },
		{ 'actv', "characters\\floodcombat elite\\floodcombat elite plasma rifle" },
		{ 'actv', "characters\\floodcombat elite\\floodcombat elite needler" },
		{ 'actv', "characters\\floodcombat_human\\floodcombat_human unarmed" },
		{ 'actv', "characters\\floodcombat_human\\floodcombat_human assault rifle" },
		{ 'actv', "characters\\floodcombat_human\\floodcombat_human pistol" },
	};
	struct skull_root roots[NUMBEROF(flood_roots) + 2];
	short count = 0;
	short index;

	if (carried)
	{
		roots[count].group = WEAPON_DEFINITION_TAG;
		roots[count++].name = SKULLS_CARRIED_TAG;
	}
	if (scarab)
	{
		roots[count].group = 'proj';
		roots[count++].name = SKULLS_SCARAB_ROCKET_TAG;
	}
	if (count)
		gather_skull_roots(SKULL_DONOR, roots, count);
	count = 0;
	if (outbreak)
	{
		for (index = 0; index < (short)NUMBEROF(flood_roots); index++)
			roots[count++] = flood_roots[index];
	}
	if (scarab)
	{
		roots[count].group = 'proj';
		roots[count++].name = SKULLS_SCARAB_FUEL_ROD_TAG;
	}
	if (count)
		gather_skull_roots(SKULLS_FLOOD_DONOR, roots, count);
}

/* the Tag Injector's: the stock weapon HUDs its profile's weapons are drawn
with that the level has not got (tag_injector_tags_read), each from the
first campaign map that has it, read again when the randomizer read it
already for its pools */
static void gather_stock_huds(char const *leaf)
{
	long index;

	for (index = 0; index < stock_hud_count; index++)
	{
		char const *map = tag_injector_stock_hud_map(stock_huds[index]);
		char const *name = tag_injector_stock_hud_tag(stock_huds[index]);
		struct donor *donor;
		long donor_index;
		long tag;

		if (tag_loaded('wphi', name) != NONE || !_stricmp(map, leaf))
			continue;
		for (donor_index = 0; donor_index < donor_count && _stricmp(donors[donor_index].leaf, map); donor_index++)
		{
		}
		donor = donors + donor_index;
		if (donor_index < donor_count)
		{
			if (!donor_bytes(donor))
				continue;
			/* (let go with its bytes: what was wanted of it before is known
			by name, wanted_find) */
			if (!donor->seen)
			{
				donor->seen = malloc((unsigned long)donor->count);
				if (!donor->seen)
					continue;
				memset(donor->seen, 0, (unsigned long)donor->count);
			}
		}
		else
		{
			if (donor_count >= DONOR_COUNT)
			{
				error(_error_silent, "randomizer: no room to read %s.map for the HUD %s", map, name);
				continue;
			}
			if (!open_donor(donor, map, &randomizer_pack))
			{
				close_donor_bytes(donor);
				continue;
			}
			donor_count++;
		}
		for (tag = 0; tag < donor->count; tag++)
		{
			struct randomizer_tag_instance *instance = donor->instances + tag;
			char const *instance_name_text = instance_name(donor, instance);

			if (instance->group_tag == 'wphi' && instance_name_text && !_stricmp(instance_name_text, name))
			{
				error(_error_silent, "randomizer: copying the HUD %s from %s.map for the Tag Injector's weapons",
					name, map);
				want_root(donor, tag);
				break;
			}
		}
		if (tag == donor->count)
			error(_error_silent, "randomizer: %s.map has no %s", map, name);
		release_donor_blob(donor);
	}
}

/* the campaign maps' weapons, actor variants and equipment, by group and
path (randomizer_stock_tag_exists): read once, the first time one is asked
for */
#define STOCK_TAG_MAX 1024
static struct
{
	long group;
	char *name;
} stock_tags[STOCK_TAG_MAX];
static long stock_tag_count;
static int stock_tags_read;

static void stock_tags_gather(void)
{
	/* (the pack left as it was: a level in play reads its pixels from it) */
	int pack_was_open = randomizer_pack.state == 1;
	int level;

	stock_tags_read = 1;
	for (level = 0; level < 10; level++)
	{
		char const *leaf = tag_name_strip_path(main_get_solo_level_name(level));
		struct donor donor;
		long tag;

		if (!open_donor(&donor, leaf, &randomizer_pack))
		{
			close_donor_bytes(&donor);
			continue;
		}
		for (tag = 0; tag < donor.count; tag++)
		{
			struct randomizer_tag_instance *instance = donor.instances + tag;
			char const *name;
			long index;

			if (instance->group_tag != WEAPON_DEFINITION_TAG && instance->group_tag != ACTOR_VARIANT_DEFINITION_TAG &&
				instance->group_tag != 'eqip')
				continue;
			name = instance_name(&donor, instance);
			if (!name)
				continue;
			for (index = 0; index < stock_tag_count &&
				(stock_tags[index].group != instance->group_tag || _stricmp(stock_tags[index].name, name)); index++)
			{
			}
			if (index < stock_tag_count || stock_tag_count >= STOCK_TAG_MAX)
				continue;
			stock_tags[stock_tag_count].name = malloc(strlen(name) + 1);
			if (!stock_tags[stock_tag_count].name)
				break;
			strcpy(stock_tags[stock_tag_count].name, name);
			stock_tags[stock_tag_count++].group = instance->group_tag;
		}
		/* (its file closed with its bytes: it is not kept) */
		close_donor_bytes(&donor);
	}
	if (!pack_was_open)
		pack_close(&randomizer_pack);
	error(_error_silent, "randomizer: the campaign maps have %ld weapons, actor variants and equipment", stock_tag_count);
}

int randomizer_stock_tag_exists(long group_tag, char const *name)
{
	long index;

	if (!name || !name[0])
		return 0;
	if (!stock_tags_read)
		stock_tags_gather();
	for (index = 0; index < stock_tag_count; index++)
	{
		if (stock_tags[index].group == group_tag && !_stricmp(stock_tags[index].name, name))
			return 1;
	}

	return 0;
}

/* the weapons the campaign's player is animated holding, by their weapon
types' labels (a weapon's label: "ar"; "unarmed" for a type without one),
with their weapon class's animations and the type's
(randomizer_stock_player_class): read once, the first time one is asked
for, from a10's characters\cyborg\cyborg (its standing seat's) */
#define STOCK_PLAYER_CLASS_MAX 64
#define STOCK_PLAYER_ANIMATIONS "characters\\cyborg\\cyborg"
static struct
{
	char label[TAG_STRING_LENGTH + 1];
	unsigned long long animations;
	unsigned long long overlays;
} stock_player_classes[STOCK_PLAYER_CLASS_MAX];
static long stock_player_class_count;
/* 0 not read, 1 read, -1 not readable */
static int stock_player_classes_read;

/* which of the first 64 of a donor's block of animation indices are some
animation (not NONE) */
static unsigned long long stock_animation_bits(struct donor *donor, struct tag_block const *block)
{
	short const *indices = donor_address(donor, block->address);
	unsigned long long bits = 0;
	long index;

	if (!indices || block->count <= 0 || !range_in(donor, indices, (unsigned long)block->count * sizeof(short)))
		return 0;
	for (index = 0; index < block->count && index < 64; index++)
	{
		if (indices[index] != NONE)
			bits |= 1ULL << index;
	}
	return bits;
}

static void stock_player_classes_gather(void)
{
	int pack_was_open = randomizer_pack.state == 1;
	struct donor donor;
	long tag;

	stock_player_classes_read = -1;
	if (!open_donor(&donor, "a10", &randomizer_pack))
	{
		close_donor_bytes(&donor);
		if (!pack_was_open)
			pack_close(&randomizer_pack);
		error(_error_silent, "randomizer: a10 cannot be read for the player's animations");
		return;
	}
	for (tag = 0; tag < donor.count; tag++)
	{
		struct randomizer_tag_instance *instance = donor.instances + tag;
		struct animation_graph const *graph;
		struct animation_graph_unit_seat const *seats;
		char const *name;
		long seat_index;
		long chosen = 0;

		if (instance->group_tag != ANIMATION_GRAPH_TAG || !(name = instance_name(&donor, instance)) ||
			_stricmp(name, STOCK_PLAYER_ANIMATIONS))
			continue;
		graph = donor_address(&donor, instance->base_address);
		if (!graph || !range_in(&donor, graph, sizeof(*graph)))
			break;
		seats = donor_address(&donor, graph->unit_seats.address);
		if (!seats || graph->unit_seats.count <= 0 ||
			!range_in(&donor, seats, (unsigned long)graph->unit_seats.count * sizeof(*seats)))
			break;
		/* (its standing seat, else its first) */
		for (seat_index = 0; seat_index < graph->unit_seats.count; seat_index++)
		{
			if (!_strnicmp(seats[seat_index].label, "stand", 5))
			{
				chosen = seat_index;
				break;
			}
		}
		{
			struct animation_graph_unit_seat const *seat = &seats[chosen];
			struct animation_graph_weapon_class const *classes = donor_address(&donor, seat->weapon_classes.address);
			long class_index;

			if (!classes || !range_in(&donor, classes, (unsigned long)seat->weapon_classes.count * sizeof(*classes)))
				break;
			for (class_index = 0; class_index < seat->weapon_classes.count; class_index++)
			{
				struct animation_graph_weapon_class const *weapon_class = &classes[class_index];
				struct animation_graph_weapon_type const *types = donor_address(&donor,
					weapon_class->weapon_types.address);
				long type_index;

				if (!types || !range_in(&donor, types, (unsigned long)weapon_class->weapon_types.count * sizeof(*types)))
					continue;
				for (type_index = 0; type_index < weapon_class->weapon_types.count &&
					stock_player_class_count < STOCK_PLAYER_CLASS_MAX; type_index++)
				{
					snprintf(stock_player_classes[stock_player_class_count].label, TAG_STRING_LENGTH + 1, "%.*s",
						TAG_STRING_LENGTH, types[type_index].label[0] ? types[type_index].label : "unarmed");
					stock_player_classes[stock_player_class_count].animations = stock_animation_bits(&donor,
						&weapon_class->animations);
					stock_player_classes[stock_player_class_count].overlays = stock_animation_bits(&donor,
						&types[type_index].animations);
					stock_player_class_count++;
				}
			}
			stock_player_classes_read = 1;
		}
		break;
	}
	close_donor_bytes(&donor);
	if (!pack_was_open)
		pack_close(&randomizer_pack);
	error(_error_silent, "randomizer: the player is animated holding %ld weapon types", stock_player_class_count);
}

int randomizer_stock_player_class(char const *label, unsigned long long *animations, unsigned long long *overlays)
{
	long index;

	if (!stock_player_classes_read)
		stock_player_classes_gather();
	if (stock_player_classes_read < 0)
		return -1;
	for (index = 0; label && index < stock_player_class_count; index++)
	{
		if (!_stricmp(stock_player_classes[index].label, label))
		{
			*animations = stock_player_classes[index].animations;
			*overlays = stock_player_classes[index].overlays;
			return 1;
		}
	}
	return 0;
}

void randomizer_tag_table_fingerprint(long *count, unsigned long *hash)
{
	long index;
	struct cache_file_tag_instance *instances = cache_files_tag_instances(count);
	unsigned long value = 0x811C9DC5UL;

	for (index = 0; instances && index < *count; index++)
	{
		char const *name = instances[index].name;
		short byte_index;

		for (byte_index = 0; byte_index < 4; byte_index++)
			value = ((value ^ ((unsigned long)instances[index].group_tag >> (byte_index * 8) & 0xFF)) * 16777619UL) &
				0xFFFFFFFFUL;
		for (; name && *name; name++)
			value = ((value ^ (unsigned char)*name) * 16777619UL) & 0xFFFFFFFFUL;
		value = ((value ^ 0xFF) * 16777619UL) & 0xFFFFFFFFUL;
	}
	if (!instances)
		*count = 0;
	*hash = value;
}

/* a multiplayer level playing the host's shared weapons (tag_multiplayer.c):
the share's tags added as a profile's are to a campaign level, the stock
HUDs its weapons want copied first, and its weapons told
tag_multiplayer.c, which the host swaps the map's for */
/* a profile's tags added to a level not of the campaign: a multiplayer
game's shared ones (multiplayer: given the game's swaps and loadouts), or
the firing range's */
static void multiplayer_tags_loaded(char const *scenario_name, long profile, int multiplayer)
{
	int index;

	custom_profile = profile;
	snprintf(map_name_leaf, sizeof(map_name_leaf), "%s", tag_name_strip_path(scenario_name));
	stock_hud_count = tag_injector_tags_read(custom_profile, stock_huds, STOCK_HUD_MAX);
	if (stock_hud_count && gather_begin())
	{
		gather_stock_huds(map_name_leaf);
		import_wanted();
	}
	custom_root_count = tag_injector_tags_load(custom_profile, custom_roots, custom_root_tags, CUSTOM_ROOT_MAX);
	if (multiplayer)
		tag_multiplayer_level_loaded(custom_profile, custom_roots, custom_root_tags, custom_root_count);
	else
		firing_range_tags_injected(custom_profile, custom_roots, custom_root_tags, custom_root_count);
	donor_reads = 0;
	donor_misses = 0;
	donor_logged = 0;
	donor_saw_bitmap = 0;
	donor_saw_sound = 0;
	memset(donor_noted, 0, sizeof(donor_noted));
	for (index = 0; index < donor_count; index++)
		error(_error_silent, "randomizer: %s.map keeps %ld pack ranges",
			donors[index].leaf, donors[index].from_pack ? donors[index].span_count : 0);
	wanted_free();
	{
		long count;
		unsigned long hash;

		randomizer_tag_table_fingerprint(&count, &hash);
		error(_error_silent, "%s: %s's tag table: %ld tags, fingerprint %08lx",
			multiplayer ? "tag multiplayer" : "firing range", map_name_leaf, count, hash);
	}
}

void randomizer_tags_loaded(char const *scenario_name)
{
	struct network_game *game = network_game_get_game();
	char leaf[32];
	int index;

	int randomize = 0;
	int skull;
	int outbreak;
	int scarab;
	long campaign_profile = NONE;

	randomizer_tags_unloaded();
	/* (a multiplayer game's shared weapons, on the game's own multiplayer
	maps) */
	{
		long profile = tag_multiplayer_level_profile(scenario_name);

		if (profile != NONE)
		{
			multiplayer_tags_loaded(scenario_name, profile, 1);
			return;
		}
	}
	/* (the firing range's profile, played alone on its map: firing_range.c) */
	{
		char const *range_profile = firing_range_level_profile(scenario_name);

		if (range_profile)
		{
			long profile;

			port_tag_profiles_scan();
			profile = port_tag_profiles_find(range_profile);
			if (profile != NONE)
				multiplayer_tags_loaded(scenario_name, profile, 0);
			else
				error(_error_silent, "firing range: no profile %s", range_profile);
			return;
		}
	}
	if (!campaign_leaf(scenario_name, leaf, sizeof(leaf)))
		return;
	if (game && game->variant.game_engine_index != game_engine_none)
		return;
	/* (a campaign profile, played alone as the Tag Injector's profiles are:
	tag_campaign.c swaps the level's weapons and actors in the randomizer's
	place) */
	if (!game && !network_game_distributed_client() && !community_challenge_running())
		campaign_profile = tag_campaign_level_profile();
	if (campaign_profile != NONE)
	{
		memset(&current, 0, sizeof(current));
	}
	else if (network_game_distributed_client())
	{
		if (remote_on)
		{
			setup_from_remote(&current);
			placement_roll_from_remote();
			randomize = setup_swaps(&current);
		}
	}
	else if (config_boolean("game.randomizer"))
	{
		setup_from_config(&current);
		randomize = setup_swaps(&current);
	}
	/* (and the skull the enemies carry, which the randomizer may be off for) */
	skull = skulls_carried_wanted();
	outbreak = skulls_outbreak_wanted();
	scarab = skulls_scarab_wanted();
	if (!randomize && campaign_profile == NONE && !skull && !outbreak && !scarab)
		return;
	if ((randomize || campaign_profile != NONE) && !network_game_distributed_client() && !placement_roll_ready)
		placement_roll_new(scenario_name);
	snprintf(map_name_leaf, sizeof(map_name_leaf), "%s", leaf);
	/* (the Tag Injector's profile: a level played alone only, as only this
	machine has it, and never a challenge's, which is the same for everyone.
	Its bundles are read now, for the stock HUDs its weapons want) */
	custom_root_count = 0;
	custom_profile = NONE;
	stock_hud_count = 0;
	if (campaign_profile != NONE)
	{
		custom_profile = campaign_profile;
		stock_hud_count = tag_injector_tags_read(custom_profile, stock_huds, STOCK_HUD_MAX);
	}
	else if (randomize && current.custom_tags && !game && !network_game_distributed_client() &&
		!community_challenge_running())
	{
		custom_profile = tag_injector_profile_playable();
		if (custom_profile != NONE)
			stock_hud_count = tag_injector_tags_read(custom_profile, stock_huds, STOCK_HUD_MAX);
		else
			error(_error_silent, "randomizer: the Tag Injector's profile has no tags on any more");
	}
	if (gather_begin())
	{
		if (randomize)
			gather_campaign(leaf);
		gather_skulls(skull, outbreak, scarab);
		gather_stock_huds(leaf);
		import_wanted();
	}
	/* (its tags, after those) */
	if (custom_profile != NONE)
		custom_root_count = tag_injector_tags_load(custom_profile, custom_roots, custom_root_tags, CUSTOM_ROOT_MAX);
	if (campaign_profile != NONE)
	{
		tag_campaign_level_loaded(custom_profile, custom_roots, custom_root_tags, custom_root_count);
		build_protected();
		loaded = tag_campaign_level_playing();
	}
	if (randomize)
	{
		error(_error_silent, "randomizer: gathering actors already in %s", leaf);
		if (current.pods)
			error(_error_silent, "randomizer: infection forms swapped: %d0%% of each squad's", current.pods);
		else
			error(_error_silent, "randomizer: infection forms swapped: one in three of each squad's");
		build_pools();
		build_protected();
		loaded = 1;
	}
	/* the tag blobs are already gone. The pack ranges stay until the level
	unloads: bitmap pixels and sound samples are read from them in play */
	donor_reads = 0;
	donor_misses = 0;
	donor_logged = 0;
	donor_saw_bitmap = 0;
	donor_saw_sound = 0;
	memset(donor_noted, 0, sizeof(donor_noted));
	for (index = 0; index < donor_count; index++)
		error(_error_silent, "randomizer: %s.map keeps %ld pack ranges",
			donors[index].leaf, donors[index].from_pack ? donors[index].span_count : 0);
	wanted_free();
	error(_error_silent, "randomizer: %s, %ld actors, %ld weapons, %ld model parts in %lu vertex bytes",
		leaf, variant_count, pickup_choice.count + actor_weapon_choice.count, bound_part_count, bound_vertex_bytes);
	/* (the table as it is now, which a co-op game's machines must all have
	alike: skulls.c compares a client's with its host's) */
	{
		long count;
		unsigned long hash;

		randomizer_tag_table_fingerprint(&count, &hash);
		error(_error_silent, "randomizer: %s's tag table: %ld tags, fingerprint %08lx", leaf, count, hash);
	}
}

void randomizer_tags_unloaded(void)
{
	if (pods_swapped || pods_left_out || pods_kept)
	{
		error(_error_silent, "randomizer: infection forms: %ld swapped, %ld left out, %ld placed as they were",
			pods_swapped, pods_left_out, pods_kept);
	}
	pods_swapped = 0;
	pods_left_out = 0;
	pods_kept = 0;
	/* the level is going away. The next start, or the next advertisement
	before it, rolls again. The cleanup at the start of a load finds this
	already clear */
	if (loaded)
		placement_roll_ready = 0;
	tag_injector_tags_unloaded();
	tag_multiplayer_level_unloaded();
	tag_campaign_level_unloaded();
	custom_root_count = 0;
	custom_profile = NONE;
	donors_close();
	bound_release();
	held_free();
	if (saved_instances)
		cache_files_set_tag_instances(saved_instances, saved_count);
	if (grown_instances)
		free(grown_instances);
	grown_instances = NULL;
	saved_instances = NULL;
	saved_count = 0;
	grown_count = 0;
	wanted_free();
	wanted_count = 0;
	copied_count = 0;
	import_count = 0;
	import_stopped = 0;
	variant_count = 0;
	pickup_choice.count = 0;
	actor_weapon_choice.count = 0;
	protected_count = 0;
	loaded = 0;
	map_widget = NULL;
}

static void fourcc_text(long group, char text[5])
{
	int index;

	text[0] = (char)((group >> 24) & 0xFF);
	text[1] = (char)((group >> 16) & 0xFF);
	text[2] = (char)((group >> 8) & 0xFF);
	text[3] = (char)(group & 0xFF);
	text[4] = 0;
	for (index = 0; index < 4; index++)
	{
		if (text[index] < 32 || text[index] > 126)
			text[index] = '?';
	}
}

static int leading_zero(void const *buffer, long size)
{
	unsigned char const *bytes = buffer;
	long index;
	long check;

	check = size;
	if (check > 32)
		check = 32;
	if (check <= 0)
		return 1;
	for (index = 0; index < check; index++)
	{
		if (bytes[index] != 0)
			return 0;
	}

	return 1;
}

static void log_donor_read(struct donor *donor, long slot, long absolute, long offset, long size, long filled,
	void const *buffer)
{
	long tags = 0;
	struct cache_file_tag_instance *instances;
	char const *name;
	long group;
	char kind[5];
	int miss;
	int zero;
	int named;
	int detail;

	donor_reads++;
	miss = filled < size;
	if (miss)
		donor_misses++;
	instances = cache_files_tag_instances(&tags);
	name = "?";
	group = 0;
	if (instances && absolute >= 0 && absolute < tags)
	{
		if (instances[absolute].name)
			name = instances[absolute].name;
		group = instances[absolute].group_tag;
	}
	fourcc_text(group, kind);
	zero = !miss && leading_zero(buffer, filled);
	named = name && (strstr(name, "elite") || strstr(name, "carrier"));
	detail = miss || zero || named ||
		(group == BITMAP_GROUP_TAG && !donor_saw_bitmap) ||
		(group == SOUND_DEFINITION_TAG && !donor_saw_sound);
	if (named && slot >= 0 && slot < IMPORT_MAX && donor_noted[slot] && !miss && !zero)
		detail = 0;
	if (detail && donor_logged < 64)
	{
		donor_logged++;
		if (slot >= 0 && slot < IMPORT_MAX)
			donor_noted[slot] = 1;
		if (!miss && group == BITMAP_GROUP_TAG)
			donor_saw_bitmap = 1;
		if (!miss && group == SOUND_DEFINITION_TAG)
			donor_saw_sound = 1;
		error(_error_silent, "randomizer: %s %s '%s' offset %ld size %ld filled %ld donor %s (%ld ranges)",
			miss ? (filled == 0 ? "miss" : "short") : (zero ? "zero" : "ok"),
			kind, name, offset, size, filled, donor->leaf, donor->from_pack ? donor->span_count : 0);
	}
	if (donor_reads == 1 || donor_misses == 1 || (donor_reads % 200) == 0)
		error(_error_silent, "randomizer: resource reads %ld, misses %ld", donor_reads, donor_misses);
}

int randomizer_donor_read(long tag_index, long offset, long size, void *buffer)
{
	long absolute;
	long index;
	long filled;
	long asked;
	struct donor *donor;

	/* (the randomizer's tags, or the skull's alone) */
	if (import_count <= 0 || tag_index == NONE || !buffer || offset < 0 || size < 0)
		return 0;
	absolute = tag_index & 0xFFFF;
	for (index = 0; index < import_count; index++)
	{
		if (import_absolute[index] == absolute)
			break;
	}
	if (index >= import_count)
		return 0;
	donor = donors + import_donor[index];
	memset(buffer, 0, (unsigned long)size);
	asked = size;
	filled = 0;
	if (offset < donor->file_length)
	{
		if (size > donor->file_length - offset)
			size = donor->file_length - offset;
		if (donor->from_pack)
		{
			filled = pack_copy(donor, offset, size, buffer);
			if (filled < size)
				filled += extras_copy(donor, offset + filled, size - filled, (unsigned char *)buffer + filled);
		}
		else if (donor->file && donor->file != INVALID_HANDLE_VALUE)
		{
			if (!donor->compressed)
			{
				if (donor_file_at(donor->file, offset) &&
					donor_file_read(donor->file, buffer, (unsigned long)size))
					filled = size;
			}
			else if (offset >= 0x800)
			{
				if (inflate_span(donor->file, offset, size, buffer))
					filled = size;
			}
			else if (offset + size <= 0x800)
			{
				if (donor_file_at(donor->file, offset) &&
					donor_file_read(donor->file, buffer, (unsigned long)size))
					filled = size;
			}
		}
	}
	log_donor_read(donor, index, absolute, offset, asked, filled, buffer);

	return 1;
}

/* the team an actor of this variant fights for in this encounter: the
encounter's, or with the default team, its unit's own (encounter_attach_unit
gives the encounter its first unit's) */
static short variant_team(long variant_index, struct encounter_definition const *encounter)
{
	struct actor_variant_definition *variant;
	struct unit_definition *unit;

	if (encounter->team_index != _game_team_default)
		return encounter->team_index;
	variant = actor_variant_definition_get(variant_index);
	if (!variant || variant->unit_reference.index == NONE ||
		!tag_index_is_group(variant->unit_reference.index, UNIT_DEFINITION_TAG))
		return _game_team_default;
	unit = unit_definition_get(variant->unit_reference.index);

	return unit ? unit->unit.default_team : _game_team_default;
}

long randomizer_level_carriers(void)
{
	struct scenario *scenario = global_scenario_get();
	long total = 0;
	long encounter_index;

	if (!scenario)
		return 0;
	for (encounter_index = 0; encounter_index < scenario->ai_encounters.count; encounter_index++)
	{
		struct encounter_definition *encounter = TAG_BLOCK_GET_ELEMENT(&scenario->ai_encounters, encounter_index,
			struct encounter_definition);
		long squad_index;

		for (squad_index = 0; squad_index < encounter->squads.count; squad_index++)
		{
			struct squad_definition *squad = TAG_BLOCK_GET_ELEMENT(&encounter->squads, squad_index,
				struct squad_definition);
			struct tag_reference *palette_entry;
			struct actor_variant_definition *variant;
			short count;

			if (!VALID_INDEX(squad->actor_palette_index, scenario->ai_actor_palette.count))
				continue;
			palette_entry = TAG_BLOCK_GET_ELEMENT(&scenario->ai_actor_palette, squad->actor_palette_index,
				struct tag_reference);
			if (palette_entry->index == NONE)
				continue;
			variant = actor_variant_definition_get(palette_entry->index);
			if (!variant || variant->unit_reference.index == NONE ||
				!randomizer_unit_can_carry(tag_get_name(variant->unit_reference.index)) ||
				!skulls_team_is_enemy(variant_team(palette_entry->index, encounter)))
			{
				continue;
			}
			/* (as encounters.c places a squad on each difficulty, and those it
			brings back) */
			switch (game_difficulty_level_get())
			{
			case _game_difficulty_level_easy:
			case _game_difficulty_level_normal:
				count = squad->min_count;
				break;
			case _game_difficulty_level_hard:
				count = (short)((squad->max_count + squad->min_count) / 2);
				break;
			default:
				count = squad->max_count;
				break;
			}
			if (count > 0)
				total += count;
			if (squad->respawn_total_count > 0)
				total += squad->respawn_total_count;
		}
	}

	return total;
}

/* a variant's type, by its unit's name */
static int variant_type(long variant_index)
{
	struct actor_variant_definition *variant = variant_index == NONE ? NULL :
		actor_variant_definition_get(variant_index);

	if (!variant || variant->unit_reference.index == NONE)
		return TYPE_NONE;

	return classify(tag_get_name(variant->unit_reference.index));
}

/* whether a squad's next infection form to be swapped is, of the setup's
share: its first, then one in three (or so many tenths, with go crazy),
counted as the squad places them, its respawns too */
static int pod_swapped(long encounter_block, long squad_index)
{
	long count;
	long index;

	if (!state)
		return 1;
	for (index = 0; index < POD_SQUADS_MAX; index++)
	{
		if (state->pod_squads[index].encounter == encounter_block && state->pod_squads[index].squad == squad_index)
			break;
	}
	/* (a squad not counted yet takes the oldest's place) */
	if (index == POD_SQUADS_MAX)
	{
		error(_error_silent, "randomizer: infection forms so far: %ld swapped, %ld left out, %ld placed as they were",
			pods_swapped, pods_left_out, pods_kept);
		index = state->pod_squads_next;
		state->pod_squads_next = (state->pod_squads_next + 1) % POD_SQUADS_MAX;
		state->pod_squads[index].encounter = encounter_block;
		state->pod_squads[index].squad = squad_index;
		state->pod_squads[index].count = 0;
	}
	count = state->pod_squads[index].count++;
	if (!current.pods)
		return count % 3 == 0;

	/* (the nth swapped when n + 1 tenths, rounded up, are more than n's) */
	return ((count + 1) * current.pods + 9) / 10 > (count * current.pods + 9) / 10;
}

int randomizer_actor_left_out(void)
{
	return placement_left_out;
}

long randomizer_actor_variant(long original, long encounter_index, long squad_index, void *starting_location)
{
	struct actor_starting_location *location = starting_location;
	struct scenario *scenario;
	struct encounter_definition *encounter;
	struct squad_definition *squad;
	long encounter_block;
	long location_index = 0;
	unsigned long seed;
	/* (a campaign profile's level: tag_campaign.c chooses, the player's
	allies too) */
	int campaign = tag_campaign_level_playing();
	int ally = 0;

	placement_left_out = 0;
	if (forced_variant != NONE)
		return forced_variant;
	if (!loaded || original == NONE || !location)
		return original;
	if (!campaign && (!current.enemies || variant_count <= 0))
		return original;
	if (encounter_index == NONE || squad_index == NONE)
		return original;
	if (location->command_list_index != NONE)
		return original;
	scenario = global_scenario_get();
	encounter_block = DATUM_INDEX_TO_ABSOLUTE_INDEX(encounter_index);
	encounter = scenario ? tag_block_get_element_with_size(&scenario->ai_encounters, encounter_block, sizeof(*encounter)) :
		NULL;
	if (!encounter)
		return original;
	/* marines who fight with the player stay the actors the level gave them,
	unless the setup swaps them too. Most of the levels' marines are in
	encounters of the default team, which is their unit's */
	if (!current.allies)
	{
		short team = variant_team(original, encounter);

		ally = team == _game_team_player || team == _game_team_human;
		if (ally && !campaign)
			return original;
	}
	squad = tag_block_get_element_with_size(&encounter->squads, squad_index, sizeof(*squad));
	if (!squad)
		return original;
	if (squad_is_protected(squad->name))
		return original;
	{
		struct actor_starting_location *first = tag_block_get_element_with_size(
			&squad->starting_locations, 0, sizeof(*first));

		if (first && location >= first && location < first + squad->starting_locations.count)
			location_index = (long)(location - first);
	}
	seed = placement_seed(encounter_block, squad_index, location_index, &location->position.x);
	{
		long picked = campaign ? tag_campaign_actor_variant(original, (short)variant_type(original), ally, seed) :
			pool_pick(variant_pool, variant_count, seed);

		/* (a campaign profile's level leaves this one as it is) */
		if (picked == NONE || (campaign && picked == original))
			return original;
		/* an infection form to be swapped for another actor: only the
		setup's share of a squad's, the rest not placed (actor_place) */
		if (variant_type(original) == TYPE_FLOOD_INFECTION)
		{
			if (variant_type(picked) == TYPE_FLOOD_INFECTION)
				pods_kept++;
			else if (!pod_swapped(encounter_block, squad_index))
			{
				pods_left_out++;
				placement_left_out = 1;
				return NONE;
			}
			else
				pods_swapped++;
		}

		return picked;
	}
}

static void state_reset(void)
{
	long index;

	if (!state)
		return;
	for (index = 0; index < PLACED_MAX; index++)
	{
		state->placed[index].unit_index = NONE;
		state->placed[index].original = NONE;
	}
	state->placed_next = 0;
	for (index = 0; index < RIDERS_MAX; index++)
	{
		state->riders[index].unit_index = NONE;
		state->riders[index].variant = NONE;
		state->riders[index].since = 0;
		state->riders[index].seated = 0;
	}
	for (index = 0; index < POD_SQUADS_MAX; index++)
	{
		state->pod_squads[index].encounter = NONE;
		state->pod_squads[index].squad = NONE;
		state->pod_squads[index].count = 0;
	}
	state->pod_squads_next = 0;
}

void randomizer_initialize(void)
{
	/* (a multiple of four bytes, as the game state allocates) */
	state = (struct randomizer_state *)game_state_malloc("randomizer", NULL,
		(long)((sizeof(*state) + 3) & ~(unsigned long)3));
	state_reset();
}

void randomizer_initialize_for_new_map(void)
{
	state_reset();
}

void randomizer_actor_placed(long actor_index, long requested)
{
	struct actor_datum *actor;

	if (!state || !loaded || actor_index == NONE)
		return;
	actor = actor_get(actor_index);
	if (actor->meta.unit_index == NONE || actor->meta.variant_definition_index == requested)
		return;
	state->placed[state->placed_next].unit_index = actor->meta.unit_index;
	state->placed[state->placed_next].original = requested;
	state->placed_next = (state->placed_next + 1) % PLACED_MAX;
}

/* the unit's actor placed again where it stands, of its encounter and
squad, by actor_place (the team of the unit asked for, as at the level's
placement), as the variant forced; the one it was is let go, not killed.
Its new unit, or NONE (and the unit is as it was) */
static long actor_replace(long unit_index, long requested, long forced)
{
	struct unit_datum *unit = unit_get(unit_index);
	long actor_index = unit->unit.actor_index;
	struct actor_datum *actor = actor_get(actor_index);
	struct actor_starting_location location;
	real body_vitality = unit->object.body_vitality;
	long placed_index;
	long placed_unit_index;

	/* (the squad's states, no command list: coop_enemies.c's riders' too) */
	csmemset(&location, 0, sizeof(location));
	object_get_origin(unit_index, &location.position);
	location.facing = (real)atan2(unit->object.forward.j, unit->object.forward.i);
	location.cluster_index = NONE;
	location.noncombat_sequence_id = NONE;
	location.actor_variant_index = NONE;
	location.command_list_index = NONE;
	forced_variant = forced;
	placed_index = actor_place(requested, actor->meta.encounter_index, actor->meta.squad_index, &location, FALSE, 0);
	forced_variant = NONE;
	if (placed_index == NONE)
		return NONE;
	actor_delete(actor_index, FALSE);
	object_delete(unit_index);
	placed_unit_index = actor_get(placed_index)->meta.unit_index;
	/* (as hurt as it was) */
	if (placed_unit_index != NONE && unit_get(placed_unit_index)->object.body_vitality > body_vitality)
		unit_get(placed_unit_index)->object.body_vitality = body_vitality;

	return placed_unit_index;
}

long randomizer_rider_restore(long unit_index)
{
	struct unit_datum *unit;
	long original = NONE;
	long variant;
	long restored;
	long slot;
	long rider;

	if (!state || !loaded || network_game_distributed_client())
		return NONE;
	unit = unit_try_and_get(unit_index);
	if (!unit || unit->unit.actor_index == NONE || unit->object.parent_object_index != NONE ||
		TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
	{
		return NONE;
	}
	for (slot = 0; slot < PLACED_MAX; slot++)
	{
		if (state->placed[slot].unit_index == unit_index)
		{
			original = state->placed[slot].original;
			break;
		}
	}
	if (original == NONE)
		return NONE;
	variant = actor_get(unit->unit.actor_index)->meta.variant_definition_index;
	if (variant == original)
		return NONE;
	for (rider = 0; rider < RIDERS_MAX && state->riders[rider].unit_index != NONE; rider++)
		;
	if (rider == RIDERS_MAX)
		return NONE;
	restored = actor_replace(unit_index, original, original);
	if (restored == NONE)
		return NONE;
	state->placed[slot].unit_index = NONE;
	state->riders[rider].unit_index = restored;
	state->riders[rider].variant = variant;
	state->riders[rider].since = game_time_get();
	state->riders[rider].seated = 0;
	error(_error_silent, "randomizer: %s rides as %s", tag_name_strip_path(tag_get_name(variant)),
		tag_name_strip_path(tag_get_name(original)));

	return restored;
}

/* a rider out of its seat, landed and standing, walking or fighting */
static int rider_on_its_feet(struct unit_datum *unit, long unit_index)
{
	struct biped_datum *biped = biped_try_and_get(unit_index);

	if (!biped || TEST_FLAG(biped->biped.flags, _biped_airborne_bit))
		return 0;
	switch (unit->unit.animation.state)
	{
	case _unit_state_idle:
	case _unit_state_gesture:
	case _unit_state_turn_left:
	case _unit_state_turn_right:
	case _unit_state_move_front:
	case _unit_state_move_back:
	case _unit_state_move_left:
	case _unit_state_move_right:
	case _unit_state_melee_attack:
	case _unit_state_throw_grenade:
		return 1;
	}

	return 0;
}

/* a rider swapped once out: the co-op teleport's flash where it stands, a
Flood form in a carrier's burst (Outbreak's: skulls_outbreak.c), harmless;
the flash where the level has no burst */
#define SWAP_TELEPORT_EFFECT "effects\\coop teleport"

static void swap_effect(long unit_index, long variant)
{
	int type = variant_type(variant);
	long definition_index = NONE;
	char const *marker_names[1];
	real_point3d marker_points[1];
	real_vector3d marker_forwards[1];

	if (type == TYPE_FLOOD_COMBAT || type == TYPE_FLOOD_CARRIER || type == TYPE_FLOOD_INFECTION)
		definition_index = tag_loaded(EFFECT_DEFINITION_TAG, SKULLS_OUTBREAK_BURST_TAG);
	if (definition_index == NONE)
		definition_index = tag_loaded(EFFECT_DEFINITION_TAG, SWAP_TELEPORT_EFFECT);
	if (definition_index == NONE)
		return;
	marker_names[0] = "";
	object_get_origin(unit_index, &marker_points[0]);
	marker_forwards[0] = *global_up3d;
	skulls_effect_harmless = TRUE;
	effect_new_unattached_from_markers(definition_index, unit_index, NULL, 1, marker_names, marker_points,
		marker_forwards, 0.0f, 0.0f, NULL, NULL, TRUE);
	skulls_effect_harmless = FALSE;
}

void randomizer_tick(void)
{
	long now;
	long rider;

	if (!state || network_game_distributed_client())
		return;
	now = game_time_get();
	for (rider = 0; rider < RIDERS_MAX; rider++)
	{
		long unit_index = state->riders[rider].unit_index;
		struct unit_datum *unit;
		long own;

		if (unit_index == NONE)
			continue;
		unit = unit_try_and_get(unit_index);
		/* (gone, dead, or no longer an actor's) */
		if (!loaded || !unit || unit->unit.actor_index == NONE || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		{
			state->riders[rider].unit_index = NONE;
			continue;
		}
		if (unit->object.parent_object_index != NONE)
		{
			state->riders[rider].seated = 1;
			continue;
		}
		if (!state->riders[rider].seated)
		{
			if (now - state->riders[rider].since > RIDER_SEAT_TICKS || now < state->riders[rider].since)
				state->riders[rider].unit_index = NONE;
			continue;
		}
		if (!rider_on_its_feet(unit, unit_index))
			continue;
		own = actor_get(unit->unit.actor_index)->meta.variant_definition_index;
		state->riders[rider].unit_index = NONE;
		unit_index = actor_replace(unit_index, own, state->riders[rider].variant);
		if (unit_index != NONE)
		{
			swap_effect(unit_index, state->riders[rider].variant);
			error(_error_silent, "randomizer: %s, out, is %s", tag_name_strip_path(tag_get_name(own)),
				tag_name_strip_path(tag_get_name(state->riders[rider].variant)));
		}
	}
}

long randomizer_weapon_definition(long definition_index, void *scenario_object)
{
	struct scenario_object_datum *object = scenario_object;
	unsigned long seed;
	long picked;

	if (!loaded || definition_index == NONE || !object)
		return definition_index;
	if (!tag_campaign_level_playing() && (!current.pickups || pickup_choice.count <= 0))
		return definition_index;
	if (object->name_index != NONE || !tag_index_is_group(definition_index, WEAPON_DEFINITION_TAG))
		return definition_index;
	seed = placement_seed(object->palette_entry_index, object->name_index, 0, &object->position.x);
	picked = tag_campaign_level_playing() ? tag_campaign_pickup(definition_index, seed) :
		pool_pick(pickup_choice.definitions, pickup_choice.count, seed);

	return picked == NONE ? definition_index : picked;
}

extern boolean unit_can_hold_weapon_definition(long unit_index, long weapon_definition_index);

/* the weapon types (their animation labels, weapon_definition.label) whose
animations an actor's unit holds a weapon with when it has none of its own
for it, nearest first: game.randomizer_crazy_weapons's borrowed animations */
static struct
{
	char const *label;
	char const *borrowed[4];
} const borrowed_labels[] =
{
	{ "hp", { "pp", "ne", "pr", NULL } },
	{ "pp", { "hp", "ne", "pr", NULL } },
	{ "ne", { "pr", "pp", "hp", NULL } },
	{ "pr", { "ne", "ar", "pp", NULL } },
	{ "ar", { "pr", "sr", "sg", NULL } },
	{ "sg", { "ar", "pr", "rl", NULL } },
	{ "sr", { "ar", "pr", "sg", NULL } },
	{ "rl", { "fr", "ar", "pr", NULL } },
	{ "ft", { "ar", "pr", "sg", NULL } },
	{ "fr", { "rl", "pr", "ne", NULL } },
	{ "fb", { "pr", "ne", "ar", NULL } },
};

/* units.c's: the nth label to try a weapon's animations under in place of
its own, or NULL. Only actors' units (not a player's), in a level the
randomizer swaps the actors' weapons in with game.randomizer_crazy_weapons */
char const *randomizer_borrowed_weapon_label(long unit_index, char const *label, int nth)
{
	struct unit_datum *unit;
	int index;

	if (!loaded || !current.any_weapon || !current.weapons || !label || nth < 0 || nth >= 3)
		return NULL;
	unit = unit_try_and_get(unit_index);
	if (!unit || unit->unit.player_index != NONE)
		return NULL;
	for (index = 0; index < (int)NUMBEROF(borrowed_labels); index++)
	{
		if (!_stricmp(label, borrowed_labels[index].label))
			return borrowed_labels[index].borrowed[nth];
	}

	return NULL;
}

/* the pool's weapons this unit has the animations to hold */
static long held_weapons(long unit_index, long *weapons, long capacity)
{
	long index;
	long count = 0;

	for (index = 0; index < actor_weapon_choice.count && count < capacity; index++)
	{
		if (unit_can_hold_weapon_definition(unit_index, actor_weapon_choice.definitions[index]))
			weapons[count++] = actor_weapon_choice.definitions[index];
	}

	return count;
}

/* the first attempt the pool's choice (a campaign profile's level: the
loadout of the actor variant's, tag_campaign.c), the next the actor's own
weapon (one the unit refused leaves it unarmed otherwise), then none */
long randomizer_enemy_weapon(long weapon_definition, long unit_index, long attempt, long variant_index)
{
	struct unit_datum *unit;
	long held[WEAPON_MAX];
	long held_count;
	unsigned long seed;

	if (loaded && tag_campaign_level_playing())
	{
		if (attempt > 0)
			return attempt == 1 ? weapon_definition : NONE;
		unit = unit_try_and_get(unit_index);
		if (!unit)
			return weapon_definition;
		seed = placement_seed(weapon_definition, 0, attempt, &unit->object.position.x);
		return tag_campaign_enemy_weapon(variant_index, weapon_definition, unit_index, seed);
	}
	if (!loaded || !current.weapons || actor_weapon_choice.count <= 0 || attempt > 0)
		return attempt == 0 || (attempt == 1 && loaded && current.weapons) ? weapon_definition : NONE;
	unit = unit_try_and_get(unit_index);
	if (!unit)
		return weapon_definition;
	held_count = held_weapons(unit_index, held, WEAPON_MAX);
	if (held_count <= 0)
		return weapon_definition;
	/* the place it spawned, not the unit's index: a checkpoint's unit is a
	new one, and the weapon has to be the one it had */
	seed = placement_seed(weapon_definition, 0, attempt, &unit->object.position.x);

	return pool_pick(held, held_count, seed);
}

int randomizer_weapon_is_covenant(char const *tag_name)
{
	int kind = weapon_kind(tag_name);

	return kind != WEAPON_NONE && weapon_kinds[kind].covenant;
}

int randomizer_unit_is_grunt(char const *tag_name)
{
	return classify(tag_name) == TYPE_GRUNT;
}

int randomizer_unit_is_hunter(char const *tag_name)
{
	return classify(tag_name) == TYPE_HUNTER;
}

int randomizer_unit_can_carry(char const *tag_name)
{
	int type = classify(tag_name);

	return type == TYPE_GRUNT || type == TYPE_ELITE || type == TYPE_JACKAL || type == TYPE_FLOOD_COMBAT;
}

void randomizer_advertisement(unsigned char *reserved)
{
	struct network_game *game = network_game_get_game();
	struct setup setup;
	char leaf[32];

	if (!reserved || !game || game->variant.game_engine_index != game_engine_none)
		return;
	if (!config_boolean("game.randomizer") || !campaign_leaf(game->map.name, leaf, sizeof(leaf)))
		return;
	setup_from_config(&setup);
	if (!setup_swaps(&setup))
		return;
	/* (a daily challenge's lobby rolls its seed: its clients lay the level
	out as every challenger does, community.c) */
	if (!placement_roll_ready)
		placement_roll_new(game->map.name);
	reserved[HALO_PORT_ADVERTISED_FLAGS_OFFSET] =
		(unsigned char)(reserved[HALO_PORT_ADVERTISED_FLAGS_OFFSET] | HALO_PORT_ADVERTISED_RANDOMIZER_FLAG);
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_SETUP_OFFSET] =
		(unsigned char)((setup.enemies ? 1 : 0) | (setup.pickups ? 2 : 0) | (setup.weapons ? 4 : 0) |
			(setup.allies ? 8 : 0) | (setup.crazy ? 16 : 0));
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_SETUP_OFFSET + 1] = (unsigned char)setup.pool;
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_SETUP_OFFSET + 2] = (unsigned char)setup.type;
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_ROLL_OFFSET] = (unsigned char)placement_roll;
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_ROLL_OFFSET + 1] = (unsigned char)(placement_roll >> 8);
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_ROLL_OFFSET + 2] = (unsigned char)(placement_roll >> 16);
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_ROLL_OFFSET + 3] = (unsigned char)(placement_roll >> 24);
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_WEAPONS_OFFSET] = (unsigned char)setup.rank;
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_WEAPONS_OFFSET + 1] = (unsigned char)setup.pickup_pool;
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_WEAPONS_OFFSET + 2] = (unsigned char)setup.pickup_type;
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_WEAPONS_OFFSET + 3] = (unsigned char)setup.weapon_pool;
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_WEAPONS_OFFSET + 4] = (unsigned char)setup.weapon_type;
	reserved[HALO_PORT_ADVERTISED_RANDOMIZER_CRAZY_OFFSET] =
		(unsigned char)(HALO_PORT_ADVERTISED_RANDOMIZER_CRAZY_SET | (setup.any_weapon ? 1 : 0) |
			(setup.odd_enemies ? 2 : 0) | (setup.flamethrower ? 4 : 0) | (setup.pods << 4));
}

void randomizer_set_remote(int on, unsigned char const *setup, unsigned char crazy)
{
	remote_on = on && setup;
	if (remote_on)
	{
		memcpy(remote_setup, setup, sizeof(remote_setup));
		remote_crazy = crazy;
	}
}

int randomizer_remote_on(void)
{
	return remote_on;
}

int randomizer_show_prefix(void)
{
	return host_randomized();
}

void randomizer_note_map_widget(void *widget)
{
	map_widget = widget;
}

wchar_t const *randomizer_level_title_text(void *widget, wchar_t const *string)
{
	static wchar_t const prefix[] = L"[RNDM MOD] ";
	int index;
	int out;

	if (widget != map_widget || !string || !randomizer_show_prefix())
		return NULL;
	for (out = 0; prefix[out] && out < (int)NUMBEROF(prefixed_title) - 1; out++)
		prefixed_title[out] = prefix[out];
	for (index = 0; string[index] && out < (int)NUMBEROF(prefixed_title) - 1; index++, out++)
		prefixed_title[out] = string[index];
	prefixed_title[out] = 0;

	return prefixed_title;
}
