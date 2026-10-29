/*
TAG_MULTIPLAYER.C

The Tag Injector in multiplayer (tag_multiplayer.h; docs/tag_injector.md,
"In multiplayer"). A game hosted on one of the game's own multiplayer maps
can play a multiplayer profile (the menus' TAG PROFILE screen, before Server
Setup; kept for the game: tag_multiplayer_game_profile), which names a
profile of the Tag Injector's and how the game plays its weapons
(port_tag_mp_profiles.c). The game is a modded one exactly when it plays
one, as chosen, so a game listed for every build stays so.

- The share. The host writes the profile's weapons on as one file (its
  archive, tag_injector/.shared/<id>.tagshare, named for its hash as map
  sharing hashes a map: port_tag_profiles_shared_build), their tag files
  and the profile's pack as they are (nothing compressed again), and
  unpacks it into its own shared profile's folder (.shared/host), which it
  plays, as its clients play theirs. It tells each
  client the hash (_distributed_message_tag_share, reliably, in the lobby
  too), and a client without the share downloads the archive as it would a
  map (map_share.c's _map_share_file_tags: from the host and the other
  players) and makes it the share's folder. The game does not start on a
  machine still downloading it (map_share_downloading).
- The level. As one of the game's own multiplayer maps loads, every machine
  adds the share's tags to it, as the randomizer adds a profile's to a
  campaign level (randomizer.c: the stock HUDs its weapons are drawn with
  copied too), after the level's own tags and before this machine's menus'
  and mods': so they are at the same indices on every machine, whose
  host's objects name them by index.
- The weapons. The host alone spawns the map's weapons and gives its
  players theirs (a client has the host's objects): each weapon of the map
  (the game's own multiplayer weapons; not the flag, the ball or
  equipment) is swapped as the profile says: ROTATION ADD (the map's
  weapons and the profile's) or ONLY (the profile's), and REPLACE RANDOM
  (each spawn), POSITION (rolled once for each place in a game), WEAPON
  (rolled once for each of the map's weapons) or MANUAL (the weapons chosen
  for each of the map's, ORIGINAL among them, one at random each spawn;
  ADD puts the original among them; one with none chosen stays as it is).
  LOADOUT gives each player spawning two of the profile's weapons at
  random (RANDOM) or those chosen (SET), else leaves it to the gametype.
- Co-op. A co-op game hosted on one of the campaign's ten levels can play a
  campaign profile (the same TAG PROFILE screen, between the difficulty and
  Server Setup; kept for the game, every level of it). Its share is all the
  profile's tags on (its AI, its weapons, their equipment) and the campaign
  profile, shared and downloaded as a multiplayer game's is; every machine
  adds them to each level as tag_campaign.c's level played alone would, and
  the host alone swaps the level's weapons and actors (tag_campaign.c, in the
  randomizer's place: its clients have the host's objects and actors). With
  the randomizer on, the screen lists the Tag Injector's profiles too, as
  the solo screen does: one of them is shared the same way (all its tags on,
  no profile of settings), and every machine adds it as the randomizer adds
  its profile played alone, its TAG INJECTOR pools and custom types
  advertised with the randomizer's setup (randomizer.c), so that each
  machine copies the same tags; the host alone swaps with it.
*/

/* ---------- headers */

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "interface/ui_widget.h"
#include "items/item_definitions.h"
#include "items/weapon_definitions.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "objects/object_definitions.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"
#include "units/units.h"
#include "network_distributed.h"
#include "custom_edition_cache.h"
#include "map_share.h"
#include "tag_injector.h"
#include "tag_campaign.h"
#include "tag_multiplayer.h"
#include "network_replay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (the C library's allocator, as map_share.c's) */
#undef malloc
#undef free
#undef realloc

/* port_config.c's, port_tag_profiles.c's, port_tag_mp_profiles.c's,
network_server_manager.c's, network_server_message_handler.c's, p2p_crypto.c's */
int config_boolean(char const *name);
int halo_server_mode(void);
char const *config_string(char const *name);
int config_write(char const *name, char const *value);
unsigned long config_changes(void);
long port_tag_profiles_scan(void);
long port_tag_profiles_find(char const *folder);
char const *port_tag_profiles_name(long profile);
long port_tag_profiles_tag_count(long profile);
unsigned long port_tag_profiles_tag_group(long profile, long tag);
char const *port_tag_profiles_tag_name(long profile, long tag);
int port_tag_profiles_tag_enabled(long profile, long tag);
unsigned long port_tag_profiles_tag_size(long profile, long tag);
unsigned long port_tag_profiles_tag_signature(long profile, long tag);
long port_tag_profiles_shared_open(char const *id);
int port_tag_profiles_shared_present(char const *id);
int port_tag_profiles_shared_remove(char const *name);
int port_tag_profiles_shared_folder_make(void);
void port_tag_profiles_shared_path(char const *name, char *path, unsigned long size);
long port_tag_profiles_shared_build(long profile, unsigned long group, char const *multiplayer, char const *campaign,
	char const *archive);
long port_tag_profiles_shared_import(char const *id);
int port_tag_profiles_shared_unpack(char const *archive, char const *id);
int port_tag_profiles_file_replace(char const *from, char const *to);
long port_tag_mp_profiles_scan(void);
long port_tag_mp_profiles_count(void);
long port_tag_mp_profiles_find(char const *file);
char const *port_tag_mp_profiles_file(long profile);
char const *port_tag_mp_profiles_name(long profile);
char const *port_tag_mp_profiles_option(long profile, char const *key);
long port_tag_mp_profiles_manual_count(long profile, char const *key);
char const *port_tag_mp_profiles_manual(long profile, char const *key, long entry);
void port_tag_mp_profiles_path(long profile, char *path, unsigned long size);
char const *port_tag_profiles_folder(long profile);
int randomizer_show_prefix(void);
long port_tag_campaign_profiles_scan(void);
long port_tag_campaign_profiles_find(char const *file);
char const *port_tag_campaign_profiles_file(long profile);
char const *port_tag_campaign_profiles_option(long profile, char const *key);
void port_tag_campaign_profiles_path(long profile, char *path, unsigned long size);
unsigned long network_game_server_machine_address(long machine_index);
boolean network_distributed_server_send_to_machine_reliably(long machine_index, void *message, word size);
boolean network_connection_last_read_was_unreliable(void);
void p2p_blake2b_256(const void *data, unsigned long size, unsigned char *digest);
void *cache_files_tag_instances(long *count);
FILE *platform_fopen_host(const char *path, const char *mode);
unsigned long system_milliseconds(void);

/* ---------- constants */

enum
{
	/* the machines the host tells, and how often it tells each again */
	TAG_SHARE_MACHINES = HALO_PORT_MAXIMUM_NETWORK_MACHINES,
	TAG_SHARE_RETELL_MILLISECONDS = 10000,
	/* a share is made this long after what it is made of last changed (not
	for each profile Server Setup's TAG PROFILE passes over) */
	TAG_SHARE_SETTLE_MILLISECONDS = 1500,
	/* the profile's weapons a share has at most, a level's swaps of them,
	and the weapons a MANUAL swap chooses among */
	MAXIMUM_SHARED_WEAPONS = 64,
	MAXIMUM_POOL = MAXIMUM_SHARED_WEAPONS + TAG_MULTIPLAYER_STOCK_WEAPONS,
	MAXIMUM_MANUAL_CHOICES = 32,
	/* the place a share is built in before it is named for its hash */
	TAG_SHARE_BUILDING_LENGTH = 16,
};

#define TAG_SHARE_BUILDING "building"
/* the host's own share's folder (a client's is named for its hash) */
#define TAG_SHARE_HOST "host"
#define TAG_SHARE_DIRECTORY "d:\\tag_injector\\.shared\\"
#define TAG_SHARE_EXTENSION ".tagshare"

enum
{
	_rotation_add,
	_rotation_only,
};

enum
{
	_replace_random,
	_replace_position,
	_replace_weapon,
	_replace_manual,
};

enum
{
	_loadout_map,
	_loadout_random,
	_loadout_set,
};

/* ---------- structures */

struct stock_weapon
{
	char const *key;
	char const *name;
	char const *tag;
};

/* what a game's profile is: a multiplayer profile (the game's own
multiplayer maps), a campaign profile or a profile the randomizer plays (a
co-op game's) */
enum
{
	_played_multiplayer,
	_played_campaign,
	_played_randomizer,
};

/* what a share is played as: the game's profile (a multiplayer or campaign
profile's tags), or the randomizer's (the tags join its pools) */
enum
{
	_share_profile,
	_share_randomizer,
	NUMBER_OF_SHARE_KINDS
};

/* the host's share, told each client: its hash, its archive's size and its
kind (an active one: the game plays it) */
struct distributed_tag_share
{
	byte hash[TAG_MULTIPLAYER_HASH_BYTES];
	unsigned long size;
	byte active;
	byte kind;
	byte pad[2];
};

typedef char distributed_tag_share_size_assert[sizeof(struct distributed_tag_share) == 40 ? 1 : -1];

/* a tag of the level's table (cache_files.c's, as randomizer.c has it) */
struct level_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

typedef char level_tag_instance_size_assert[sizeof(struct level_tag_instance) == 0x20 ? 1 : -1];

struct share
{
	boolean active;
	short kind;
	byte hash[TAG_MULTIPLAYER_HASH_BYTES];
	unsigned long size;
	char id[TAG_MULTIPLAYER_ID_DIGITS + 1];
};

/* ---------- globals */

static struct stock_weapon const stock_weapons[TAG_MULTIPLAYER_STOCK_WEAPONS] =
{
	{ "assault_rifle", "ASSAULT RIFLE", "weapons\\assault rifle\\assault rifle" },
	{ "pistol", "PISTOL", "weapons\\pistol\\pistol" },
	{ "sniper_rifle", "SNIPER RIFLE", "weapons\\sniper rifle\\sniper rifle" },
	{ "rocket_launcher", "ROCKET LAUNCHER", "weapons\\rocket launcher\\rocket launcher" },
	{ "shotgun", "SHOTGUN", "weapons\\shotgun\\shotgun" },
	{ "flamethrower", "FLAMETHROWER", "weapons\\flamethrower\\flamethrower" },
	{ "plasma_rifle", "PLASMA RIFLE", "weapons\\plasma rifle\\plasma rifle" },
	{ "plasma_pistol", "PLASMA PISTOL", "weapons\\plasma pistol\\plasma pistol" },
	{ "needler", "NEEDLER", "weapons\\needler\\needler" },
};

static struct
{
	/* the host's share, what it was made from (its profile's name, and its
	weapons on: their paths and sizes, hashed: 0 for none), whether it was
	made (or failed to be) for that, and whether the settings are to be
	looked at again (they changed, or a game is hosted anew) */
	struct share hosted;
	unsigned long built_signature;
	boolean built;
	boolean look_again;
	unsigned long checked_changes;
	/* what it is to be made of, and since when */
	boolean checked;
	unsigned long wanted_signature;
	unsigned long wanted_at;
	/* (the host) each client machine told the share (its address, and the
	serial of the share told, and when) */
	unsigned long hosted_serial;
	unsigned long told_address[TAG_SHARE_MACHINES];
	unsigned long told_serial[TAG_SHARE_MACHINES];
	unsigned long told_at[TAG_SHARE_MACHINES];
	/* (a client) the host's share, and the one this machine asked to
	download */
	struct share remote;
	boolean remote_heard;
	char asked_id[TAG_MULTIPLAYER_ID_DIGITS + 1];
	/* (the host) the multiplayer profile its game plays, or a co-op game's
	campaign profile or profile the randomizer plays (game_kind; its file's
	name, the profile's folder: "" for none), and whether it was chosen for
	this game (a dedicated server's is config.toml's, as its game is hosted) */
	char game_profile[256];
	short game_kind;
	boolean game_profile_set;
	/* (a replay's playback) the share its level played, which it plays in
	the host's place (network_replay_playback.c) */
	struct share replayed;
} tag_multiplayer;

/* the level played with a share: its settings, as the host's multiplayer
profile had them as the level loaded, and the weapons the swaps choose
among (the level's tags) */
static struct
{
	boolean active;
	short rotation;
	short replace;
	short loadout;
	/* the map's own weapons (NONE: the map has not got it), and of those the
	netgame equipment spawns */
	long stock[TAG_MULTIPLAYER_STOCK_WEAPONS];
	boolean stock_spawned[TAG_MULTIPLAYER_STOCK_WEAPONS];
	/* the share's weapons added, and their paths */
	long shared[MAXIMUM_SHARED_WEAPONS];
	char shared_paths[MAXIMUM_SHARED_WEAPONS][256];
	short shared_count;
	/* what RANDOM, POSITION and WEAPON choose among */
	long pool[MAXIMUM_POOL];
	short pool_count;
	/* MANUAL's choices for each of the map's weapons */
	long manual[TAG_MULTIPLAYER_STOCK_WEAPONS][MAXIMUM_MANUAL_CHOICES];
	short manual_count[TAG_MULTIPLAYER_STOCK_WEAPONS];
	/* POSITION's rolls (NONE: not yet), a netgame equipment each, and
	WEAPON's, a map's weapon each */
	long *position_rolls;
	long position_count;
	long weapon_rolls[TAG_MULTIPLAYER_STOCK_WEAPONS];
	/* SET's weapons (NONE: none) */
	long loadout_weapons[2];
	unsigned long random;
} tag_multiplayer_level;

/* ---------- private code */

static boolean elapsed(
	unsigned long since,
	unsigned long now,
	unsigned long milliseconds)
{
	return now - since >= milliseconds;
}

static unsigned long level_random(
	void)
{
	unsigned long value = tag_multiplayer_level.random;

	value ^= value << 13;
	value ^= value >> 17;
	value ^= value << 5;
	tag_multiplayer_level.random = value ? value : 0x2545F491UL;
	return tag_multiplayer_level.random;
}

static short choice_of(
	char const *value,
	char const *const *choices,
	short count)
{
	short index;

	for (index = 0; value && index < count; index++)
	{
		if (!strcmp(value, choices[index]))
			return index;
	}
	return 0;
}

/* a share's id: its hash's first digits */
static void share_id(
	byte const *hash,
	char *id)
{
	static char const digits[] = "0123456789abcdef";
	short index;

	for (index = 0; index < TAG_MULTIPLAYER_ID_DIGITS / 2; index++)
	{
		id[index * 2] = digits[hash[index] >> 4];
		id[index * 2 + 1] = digits[hash[index] & 15];
	}
	id[TAG_MULTIPLAYER_ID_DIGITS] = 0;
}

/* the archive's path, as the platform has it (port_tag_profiles.c's) */
static void share_host_path(
	char const *id,
	char *path,
	unsigned long size)
{
	char name[TAG_MULTIPLAYER_ID_DIGITS + sizeof(TAG_SHARE_EXTENSION) + 8];

	snprintf(name, sizeof(name), "%s%s", id, TAG_SHARE_EXTENSION);
	port_tag_profiles_shared_path(name, path, size);
}

/* (a client) the host's share, or a replay's played back: the one its
recording's level played */
static struct share const *client_share(
	void)
{
	return network_replay_playing() ? &tag_multiplayer.replayed : &tag_multiplayer.remote;
}

/* an archive hashed as map sharing hashes a file (its pieces', then
theirs): FALSE if it cannot be read */
static boolean share_hash(
	char const *host_path,
	byte *hash,
	unsigned long *size,
	byte **piece_hashes,
	long *piece_count)
{
	FILE *stream = platform_fopen_host(host_path, "rb");
	byte *piece = NULL;
	long count;
	long index;
	boolean hashed;

	*piece_hashes = NULL;
	*piece_count = 0;
	if (!stream)
		return FALSE;
	hashed = fseek(stream, 0, SEEK_END) == 0;
	*size = hashed ? (unsigned long)ftell(stream) : 0;
	hashed = hashed && *size > 0 && fseek(stream, 0, SEEK_SET) == 0;
	count = hashed ? map_share_piece_count(*size) : 0;
	piece = hashed ? malloc(MAP_SHARE_PIECE_BYTES) : NULL;
	*piece_hashes = hashed ? malloc((unsigned long)count * MAP_SHARE_HASH_BYTES) : NULL;
	hashed = piece && *piece_hashes;
	for (index = 0; hashed && index < count; index++)
	{
		unsigned long bytes = MIN((unsigned long)MAP_SHARE_PIECE_BYTES,
			*size - (unsigned long)index * MAP_SHARE_PIECE_BYTES);

		hashed = fread(piece, 1, bytes, stream) == bytes;
		if (hashed)
			p2p_blake2b_256(piece, bytes, *piece_hashes + index * MAP_SHARE_HASH_BYTES);
	}
	fclose(stream);
	free(piece);
	if (!hashed)
	{
		free(*piece_hashes);
		*piece_hashes = NULL;
		return FALSE;
	}
	*piece_count = count;
	map_share_file_hash_of_pieces(*size, *piece_hashes, count, hash);
	return TRUE;
}

/* of the profile the game plays (a multiplayer or campaign profile, or a
profile the randomizer plays: its own index), the profile it plays, and its
file's path ("": a profile the randomizer plays has none) */
static long played_profile(
	short kind,
	long played)
{
	if (played == NONE)
		return NONE;
	port_tag_profiles_scan();
	if (kind == _played_randomizer)
		return played;
	return port_tag_profiles_find(kind == _played_campaign ? port_tag_campaign_profiles_option(played, "profile") :
		port_tag_mp_profiles_option(played, "profile"));
}

static void played_path(
	short kind,
	long played,
	char *path,
	unsigned long size)
{
	if (kind == _played_campaign)
		port_tag_campaign_profiles_path(played, path, size);
	else if (kind == _played_multiplayer)
		port_tag_mp_profiles_path(played, path, size);
	else if (size)
		path[0] = 0;
}

/* the tags a share has of its profile: a multiplayer game's weapons, a
co-op game's all (its AI, its weapons, their equipment) */
static unsigned long played_group(
	short kind)
{
	return kind == _played_multiplayer ? WEAPON_DEFINITION_TAG : 0;
}

/* the profile the game plays's profile (its folder), and the signature of
what its share would be: its tags on (a multiplayer profile's weapons), their
paths and sizes (0: none, no share) */
static unsigned long share_signature(
	short kind,
	long mp_profile,
	long *profile)
{
	unsigned long value = 2166136261UL;
	long count;
	long tag;
	long weapons = 0;
	char const *text;

	*profile = played_profile(kind, mp_profile);
	if (*profile == NONE)
		return 0;
	/* (and what it is played as) */
	value = (value ^ (unsigned long)kind) * 16777619UL & 0xFFFFFFFFUL;
	count = port_tag_profiles_tag_count(*profile);
	for (text = port_tag_profiles_name(*profile); *text; text++)
		value = (value ^ (unsigned char)*text) * 16777619UL & 0xFFFFFFFFUL;
	/* (and the multiplayer or campaign profile's settings, which the share
	carries: its file's bytes) */
	{
		char path[1024];
		FILE *stream;
		int character;

		played_path(kind, mp_profile, path, sizeof(path));
		stream = path[0] ? platform_fopen_host(path, "rb") : NULL;
		while (stream && (character = fgetc(stream)) != EOF)
			value = (value ^ (unsigned long)character) * 16777619UL & 0xFFFFFFFFUL;
		if (stream)
			fclose(stream);
	}
	for (tag = 0; tag < count; tag++)
	{
		unsigned long size = port_tag_profiles_tag_size(*profile, tag);
		short byte_index;

		if (!port_tag_profiles_tag_enabled(*profile, tag) || !size ||
			(played_group(kind) && port_tag_profiles_tag_group(*profile, tag) != played_group(kind)))
			continue;
		weapons++;
		for (text = port_tag_profiles_tag_name(*profile, tag); *text; text++)
			value = (value ^ (unsigned char)*text) * 16777619UL & 0xFFFFFFFFUL;
		for (byte_index = 0; byte_index < 4; byte_index++)
			value = (value ^ (size >> (byte_index * 8) & 0xFF)) * 16777619UL & 0xFFFFFFFFUL;
		/* (and its settings and edits, which its share carries: its HUD
		moved, its values changed in the Tag Editor) */
		size = port_tag_profiles_tag_signature(*profile, tag);
		for (byte_index = 0; byte_index < 4; byte_index++)
			value = (value ^ (size >> (byte_index * 8) & 0xFF)) * 16777619UL & 0xFFFFFFFFUL;
	}
	return weapons ? value | 1 : 0;
}

/* (the host) its share made of the profile chosen's tags (a multiplayer
profile's weapons; their archive, named for its hash, and its folder,
.shared/host), and registered with map sharing; FALSE if it cannot be
(logged) */
static boolean share_build(
	short kind,
	long mp_profile,
	long profile)
{
	struct share *hosted = &tag_multiplayer.hosted;
	char building[1024];
	char final_path[1024];
	char xbox_path[MAP_SHARE_PATH_SIZE];
	char name[TAG_MULTIPLAYER_ID_DIGITS + sizeof(TAG_SHARE_EXTENSION)];
	char multiplayer[1024];
	long shared;
	byte *piece_hashes = NULL;
	long piece_count = 0;
	unsigned long started = system_milliseconds();

	csmemset(hosted, 0, sizeof(*hosted));
	if (!port_tag_profiles_shared_folder_make())
	{
		error(_error_silent, "tag multiplayer: the Tag Injector's shared folder cannot be made");
		return FALSE;
	}
	share_host_path(TAG_SHARE_BUILDING, building, sizeof(building));
	/* (its tags, and the multiplayer or campaign profile, which a client
	takes a copy of with them) */
	played_path(kind, mp_profile, multiplayer, sizeof(multiplayer));
	shared = port_tag_profiles_shared_build(profile, played_group(kind),
		kind == _played_multiplayer ? multiplayer : NULL, kind == _played_campaign ? multiplayer : NULL, building);
	if (!shared || !share_hash(building, hosted->hash, &hosted->size, &piece_hashes, &piece_count))
	{
		error(_error_silent, "tag multiplayer: the tags of %s cannot be shared", port_tag_profiles_name(profile));
		port_tag_profiles_shared_remove(TAG_SHARE_BUILDING TAG_SHARE_EXTENSION);
		free(piece_hashes);
		csmemset(hosted, 0, sizeof(*hosted));
		return FALSE;
	}
	share_id(hosted->hash, hosted->id);
	share_host_path(hosted->id, final_path, sizeof(final_path));
	snprintf(name, sizeof(name), "%s%s", hosted->id, TAG_SHARE_EXTENSION);
	port_tag_profiles_shared_remove(name);
	/* (the archive in its place, and the folder played made of it, as a
	client's is) */
	if (!port_tag_profiles_file_replace(building, final_path) ||
		!port_tag_profiles_shared_unpack(final_path, TAG_SHARE_HOST))
	{
		error(_error_silent, "tag multiplayer: the share %s cannot be put in its place", hosted->id);
		port_tag_profiles_shared_remove(TAG_SHARE_BUILDING TAG_SHARE_EXTENSION);
		free(piece_hashes);
		csmemset(hosted, 0, sizeof(*hosted));
		return FALSE;
	}
	/* (map sharing sends it without hashing it again) */
	tag_multiplayer_share_archive_path(hosted->hash, xbox_path, sizeof(xbox_path));
	map_share_file_hashed(xbox_path, _map_share_file_tags, hosted->size, piece_hashes, piece_count, hosted->hash);
	free(piece_hashes);
	hosted->active = TRUE;
	hosted->kind = kind == _played_randomizer ? _share_randomizer : _share_profile;
	tag_multiplayer.hosted_serial++;
	error(_error_silent, "tag multiplayer: shared %ld %s of %s as %s, %lu bytes, in %lu ms", shared,
		kind == _played_multiplayer ? "weapons" : "tags", port_tag_profiles_name(profile), hosted->id, hosted->size,
		system_milliseconds() - started);
	return TRUE;
}

/* whether this machine plays a level (the main menu's is a game too) */
static boolean playing(
	void)
{
	return game_in_progress() && !main_menu_is_active();
}

/* the scenario of the level loading (its tags are there; the game's
global scenario is not set yet: randomizer.c's loaded_scenario) */
static struct scenario *loading_scenario(
	void)
{
	long count = 0;
	struct level_tag_instance *instances = cache_files_tag_instances(&count);
	long index;

	for (index = 0; instances && index < count; index++)
	{
		if (instances[index].group_tag == SCENARIO_TAG && instances[index].base_address)
			return scenario_definition_get(instances[index].tag_index);
	}
	return NULL;
}

/* a message's headers (map_share.c's map_share_fill_header) */
static void share_fill_header(
	struct distributed_message_header *header,
	word size)
{
	header->header = 0;
	build_message_header(&header->header, size, 2, 0);
	header->type = _distributed_message_tag_share;
	header->count = 1;
	header->game_time = game_in_progress() ? game_time_get() : 0;
}

/* whether this machine hosts the network game, and the game */
static boolean hosting(
	void)
{
	return global_network_game_server_get() != NULL;
}

/* whether a game can play a share: a multiplayer game on one of the game's
own multiplayer maps, a co-op game (no game engine) on one of the campaign's
levels */
static boolean game_takes(
	struct network_game const *game)
{
	if (!game || !game->map.name[0])
		return FALSE;
	return game->variant.game_engine_index == game_engine_none ? tag_multiplayer_campaign_takes(game->map.name) :
		tag_multiplayer_map_takes(game->map.name);
}

/* (the host) ... and plays the kind of profile chosen for it: a campaign
profile, or a profile the randomizer plays while it swaps something
(randomizer.c), a co-op game; a multiplayer profile a multiplayer game */
static boolean host_takes(
	struct network_game const *game)
{
	if (!game_takes(game))
		return FALSE;
	if (game->variant.game_engine_index != game_engine_none)
		return tag_multiplayer.game_kind == _played_multiplayer;
	return tag_multiplayer.game_kind == _played_campaign ||
		(tag_multiplayer.game_kind == _played_randomizer && randomizer_show_prefix());
}

/* (the host) the profile its game plays, of its kind, NONE for none */
static long host_played(
	short *kind)
{
	*kind = tag_multiplayer.game_kind;
	if (*kind == _played_campaign)
		return tag_multiplayer_game_campaign_profile();
	return *kind == _played_randomizer ? tag_multiplayer_game_randomizer_profile() : tag_multiplayer_game_profile();
}

/* (the host) whether its share is to be made (again): not made yet, what it
is made of changed, or its folder went; with Custom Edition maps on (the
Tag Injector's bundles are theirs) */
static boolean host_build_pending(
	void)
{
	return halo_custom_edition_tag_cache() && tag_multiplayer.checked &&
		(!tag_multiplayer.built || tag_multiplayer.wanted_signature != tag_multiplayer.built_signature ||
		(tag_multiplayer.hosted.active && !port_tag_profiles_shared_present(TAG_SHARE_HOST)));
}

static void host_build(
	void)
{
	short kind;
	long mp_profile = host_played(&kind);
	long profile;

	tag_multiplayer.built = TRUE;
	tag_multiplayer.built_signature = share_signature(kind, mp_profile, &profile);
	tag_multiplayer.wanted_signature = tag_multiplayer.built_signature;
	csmemset(&tag_multiplayer.hosted, 0, sizeof(tag_multiplayer.hosted));
	tag_multiplayer.hosted_serial++;
	if (tag_multiplayer.built_signature)
		share_build(kind, mp_profile, profile);
}

/* (the host) the share made again when what it is made of changed (a
setting written, or the first time), and told each client machine */
static void host_update(
	void)
{
	struct network_game *game = network_game_get_game();
	struct share *hosted = &tag_multiplayer.hosted;
	boolean active;
	long index;

	boolean takes = host_takes(game);

	/* (what it is made of looked at again when a setting changed, or a game
	is hosted anew) */
	if (tag_multiplayer.checked_changes != config_changes() || tag_multiplayer.look_again || !tag_multiplayer.checked)
	{
		long profile;
		short kind;
		long played;
		unsigned long signature;

		tag_multiplayer.checked_changes = config_changes();
		tag_multiplayer.look_again = FALSE;
		tag_multiplayer.checked = TRUE;
		port_tag_mp_profiles_scan();
		port_tag_campaign_profiles_scan();
		played = host_played(&kind);
		signature = share_signature(kind, played, &profile);
		/* (the first time at once; a change a moment after it settles) */
		if (signature != tag_multiplayer.wanted_signature)
		{
			tag_multiplayer.wanted_signature = signature;
			tag_multiplayer.wanted_at = system_milliseconds() -
				(tag_multiplayer.built ? 0 : TAG_SHARE_SETTLE_MILLISECONDS);
		}
	}
	/* (made again a moment after that changed, or when its folder went; on
	the game's own multiplayer maps or campaign levels, with Custom Edition
	maps on (the Tag
	Injector's bundles are theirs), and never while a game is played, its
	clients downloading it then) */
	if (host_build_pending() && elapsed(tag_multiplayer.wanted_at, system_milliseconds(), TAG_SHARE_SETTLE_MILLISECONDS) &&
		takes && !playing())
	{
		host_build();
	}
	/* (the one made stays the game's until another is made: every machine
	plays it alike meanwhile) */
	active = hosted->active && takes;
	for (index = 0; index < TAG_SHARE_MACHINES; index++)
	{
		unsigned long address = network_game_server_machine_address(index);
		unsigned long serial = tag_multiplayer.hosted_serial << 1 | (active ? 1 : 0);
		unsigned long now = system_milliseconds();
		struct
		{
			struct distributed_message_header header;
			struct distributed_tag_share share;
		} message;

		if (!address || index == network_game_client_get_local_machine_index())
		{
			tag_multiplayer.told_address[index] = 0;
			continue;
		}
		if (tag_multiplayer.told_address[index] == address && tag_multiplayer.told_serial[index] == serial &&
			!elapsed(tag_multiplayer.told_at[index], now, TAG_SHARE_RETELL_MILLISECONDS))
			continue;
		csmemset(&message, 0, sizeof(message));
		share_fill_header(&message.header, (word)sizeof(message));
		if (active)
		{
			csmemcpy(message.share.hash, hosted->hash, sizeof(message.share.hash));
			message.share.size = hosted->size;
			message.share.active = 1;
			message.share.kind = (byte)hosted->kind;
		}
		if (!network_distributed_server_send_to_machine_reliably(index, &message, (word)sizeof(message)))
			continue;
		tag_multiplayer.told_address[index] = address;
		tag_multiplayer.told_serial[index] = serial;
		tag_multiplayer.told_at[index] = now;
	}
}

/* (a client) the game left, the player told why when the main menu comes:
it cannot have the host's tags, without which its level would not be the
host's */
static void client_leave(
	char const *reason)
{
	wchar_t text[256];
	short index;

	error(_error_silent, "tag multiplayer: leaving the game: %s", reason);
	for (index = 0; reason[index] && index < NUMBEROF(text) - 1; index++)
		text[index] = (wchar_t)(unsigned char)reason[index];
	text[index] = 0;
	display_error_text_when_main_menu_loaded(text);
	network_game_abort();
}

/* (a client) the host's share asked for, once, when this machine has not
got it */
static void client_update(
	void)
{
	struct network_game *game = network_game_get_game();
	struct share *remote = &tag_multiplayer.remote;

	/* (not while this machine plays a game: a share the host made since is
	for its next; nor a replay's, which has no host to download from) */
	if (!remote->active || !game_takes(game) || network_replay_playing() ||
		playing() || port_tag_profiles_shared_present(remote->id) ||
		!strcmp(tag_multiplayer.asked_id, remote->id))
		return;
	snprintf(tag_multiplayer.asked_id, sizeof(tag_multiplayer.asked_id), "%s", remote->id);
	/* (the folder the download is written into, which this machine may never
	have made) */
	if (!port_tag_profiles_shared_folder_make())
	{
		client_leave("The host's game plays tags of its Tag Injector profile, and the tag_injector folder "
			"cannot be made to download them into.");
		return;
	}
	if (!map_share_tags_download_begin(game->map.name, remote->hash, remote->size))
	{
		client_leave("The host's game plays tags of its Tag Injector profile. Downloading them needs map "
			"downloads (network.map_download) and Custom Edition maps (game.custom_edition) on.");
	}
}

/* ---------- public code */

char const *tag_multiplayer_stock_key(
	short weapon)
{
	return weapon >= 0 && weapon < TAG_MULTIPLAYER_STOCK_WEAPONS ? stock_weapons[weapon].key : "";
}

char const *tag_multiplayer_stock_name(
	short weapon)
{
	return weapon >= 0 && weapon < TAG_MULTIPLAYER_STOCK_WEAPONS ? stock_weapons[weapon].name : "";
}

char const *tag_multiplayer_stock_tag(
	short weapon)
{
	return weapon >= 0 && weapon < TAG_MULTIPLAYER_STOCK_WEAPONS ? stock_weapons[weapon].tag : "";
}

long tag_multiplayer_profile_chosen(
	void)
{
	char const *file = config_string("game.multiplayer_tag_profile");
	long profile = file && file[0] ? port_tag_mp_profiles_find(file) : NONE;

	/* (found again by a scan of before, and else scanned) */
	if (profile == NONE && file && file[0])
	{
		port_tag_mp_profiles_scan();
		profile = port_tag_mp_profiles_find(file);
	}
	return profile;
}

void tag_multiplayer_profile_choose(
	long profile)
{
	if (!config_write("game.multiplayer_tag_profile", profile == NONE ? "" : port_tag_mp_profiles_file(profile)))
		error(_error_silent, "tag multiplayer: could not write game.multiplayer_tag_profile to config.toml");
}

long tag_multiplayer_game_profile(
	void)
{
	char const *file = tag_multiplayer.game_kind == _played_multiplayer ? tag_multiplayer.game_profile : "";
	long profile = file[0] ? port_tag_mp_profiles_find(file) : NONE;

	/* (found again by a scan of before, and else scanned) */
	if (profile == NONE && file[0])
	{
		port_tag_mp_profiles_scan();
		profile = port_tag_mp_profiles_find(file);
	}
	return profile;
}

void tag_multiplayer_game_profile_set(
	long profile)
{
	snprintf(tag_multiplayer.game_profile, sizeof(tag_multiplayer.game_profile), "%s",
		profile == NONE ? "" : port_tag_mp_profiles_file(profile));
	tag_multiplayer.game_kind = _played_multiplayer;
	tag_multiplayer.game_profile_set = TRUE;
	tag_multiplayer.look_again = TRUE;
	error(_error_silent, "tag multiplayer: the game hosted plays %s%s%s", profile == NONE ? "no multiplayer profile" :
		"the multiplayer profile ", profile == NONE ? "" : tag_multiplayer.game_profile,
		profile == NONE ? " (listed for every build)" : " (listed for builds with the mod alone)");
}

long tag_multiplayer_game_campaign_profile(
	void)
{
	char const *file = tag_multiplayer.game_kind == _played_campaign ? tag_multiplayer.game_profile : "";
	long profile = file[0] ? port_tag_campaign_profiles_find(file) : NONE;

	/* (found again by a scan of before, and else scanned) */
	if (profile == NONE && file[0])
	{
		port_tag_campaign_profiles_scan();
		profile = port_tag_campaign_profiles_find(file);
	}
	return profile;
}

void tag_multiplayer_game_campaign_profile_set(
	long profile)
{
	snprintf(tag_multiplayer.game_profile, sizeof(tag_multiplayer.game_profile), "%s",
		profile == NONE ? "" : port_tag_campaign_profiles_file(profile));
	tag_multiplayer.game_kind = profile != NONE ? _played_campaign : _played_multiplayer;
	tag_multiplayer.game_profile_set = TRUE;
	tag_multiplayer.look_again = TRUE;
	error(_error_silent, "tag multiplayer: the co-op game hosted plays %s%s%s", profile == NONE ? "no campaign profile" :
		"the campaign profile ", profile == NONE ? "" : tag_multiplayer.game_profile,
		profile == NONE ? " (listed for every build)" : " (listed for builds with the mod alone)");
}

long tag_multiplayer_game_randomizer_profile(
	void)
{
	char const *folder = tag_multiplayer.game_kind == _played_randomizer ? tag_multiplayer.game_profile : "";

	if (!hosting() || !folder[0])
		return NONE;
	port_tag_profiles_scan();
	return port_tag_profiles_find(folder);
}

void tag_multiplayer_game_randomizer_profile_set(
	long profile)
{
	snprintf(tag_multiplayer.game_profile, sizeof(tag_multiplayer.game_profile), "%s",
		profile == NONE ? "" : port_tag_profiles_folder(profile));
	tag_multiplayer.game_kind = profile != NONE ? _played_randomizer : _played_multiplayer;
	tag_multiplayer.game_profile_set = TRUE;
	tag_multiplayer.look_again = TRUE;
	error(_error_silent, "tag multiplayer: the co-op game hosted plays %s%s%s", profile == NONE ? "no profile" :
		"the randomizer with the profile ", profile == NONE ? "" : tag_multiplayer.game_profile,
		profile == NONE ? " (listed for every build)" : " (listed for builds with the mod alone)");
}

long tag_multiplayer_game_base_profile(
	void)
{
	short kind;
	long played = host_played(&kind);

	return played_profile(kind, played);
}

boolean tag_multiplayer_map_takes(
	char const *level_name)
{
	char const *leaf;
	short index;

	if (!level_name || !level_name[0] || custom_edition_level_name(level_name))
		return FALSE;
	leaf = tag_name_strip_path(level_name);
	for (index = 0; index < 10; index++)
	{
		if (!csstrcasecmp(leaf, tag_name_strip_path(main_get_solo_level_name(index))))
			return FALSE;
	}
	return csstrcasecmp(leaf, "ui") != 0;
}

boolean tag_multiplayer_campaign_takes(
	char const *level_name)
{
	char const *leaf;
	short index;

	if (!level_name || !level_name[0] || custom_edition_level_name(level_name))
		return FALSE;
	leaf = tag_name_strip_path(level_name);
	for (index = 0; index < 10; index++)
	{
		if (!csstrcasecmp(leaf, tag_name_strip_path(main_get_solo_level_name(index))))
			return TRUE;
	}
	return FALSE;
}

void tag_multiplayer_update(
	void)
{
	struct network_game *game = network_game_get_game();

	if (!game)
	{
		csmemset(&tag_multiplayer.remote, 0, sizeof(tag_multiplayer.remote));
		tag_multiplayer.remote_heard = FALSE;
		tag_multiplayer.asked_id[0] = 0;
		csmemset(tag_multiplayer.told_address, 0, sizeof(tag_multiplayer.told_address));
		/* (the next game hosted reads its settings again: a share of the
		same weapons is kept, not made again; and plays no profile until one
		is chosen for it) */
		tag_multiplayer.look_again = TRUE;
		tag_multiplayer.game_profile[0] = 0;
		tag_multiplayer.game_kind = _played_multiplayer;
		tag_multiplayer.game_profile_set = FALSE;
		return;
	}
	/* (a dedicated server, which no menu chooses for: config.toml's, as
	its game is hosted, with the Tag Injector on; a co-op game's, on a
	campaign level, game.campaign_tag_profile's, else with the randomizer on
	game.randomizer_tag_profile's) */
	if (hosting() && !tag_multiplayer.game_profile_set && halo_server_mode())
	{
		if (tag_multiplayer_campaign_takes(game->map.name) && tag_campaign_profile_chosen() == NONE &&
			config_boolean("game.randomizer"))
			tag_multiplayer_game_randomizer_profile_set(tag_injector_profile_playable());
		else if (tag_multiplayer_campaign_takes(game->map.name))
			tag_multiplayer_game_campaign_profile_set(tag_campaign_profile_chosen());
		else
			tag_multiplayer_game_profile_set(config_boolean("game.tag_injection") ? tag_multiplayer_profile_chosen() :
				NONE);
	}
	if (hosting())
		host_update();
	else if (network_game_distributed_client())
		client_update();
}

boolean tag_multiplayer_hosted(
	void)
{
	struct network_game *game = network_game_get_game();

	/* (as chosen, not as its share was made: a game listed for every build
	is never made a modded one under its players, nor the other way) */
	return hosting() && tag_multiplayer.game_profile[0] && host_takes(game);
}

/* (not by host_takes, which asks the randomizer, which asks this) */
boolean tag_multiplayer_campaign_hosted(
	void)
{
	return hosting() && tag_multiplayer.game_profile[0] && tag_multiplayer.game_kind == _played_campaign &&
		game_takes(network_game_get_game());
}

boolean tag_multiplayer_randomizer_shared(
	void)
{
	if (hosting())
		return tag_multiplayer_game_randomizer_profile() != NONE;
	return client_share()->active && client_share()->kind == _share_randomizer;
}

void tag_multiplayer_advertisement(
	unsigned char *reserved)
{
	if (reserved && tag_multiplayer_hosted())
	{
		reserved[HALO_PORT_ADVERTISED_FLAGS_OFFSET] =
			(unsigned char)(reserved[HALO_PORT_ADVERTISED_FLAGS_OFFSET] | HALO_PORT_ADVERTISED_TAG_SHARE_FLAG);
	}
}

boolean tag_multiplayer_handles_message(
	unsigned short const *message,
	unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return FALSE;
	csmemcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_tag_share;
}

/* (the host's alone, over its stream, whole) */
void tag_multiplayer_handle_message(
	long machine_index,
	unsigned short const *data,
	unsigned short size,
	boolean from_stream)
{
	struct
	{
		struct distributed_message_header header;
		struct distributed_tag_share share;
	} message;
	struct share *remote = &tag_multiplayer.remote;

	if (machine_index != NONE || !from_stream || hosting() || size != sizeof(message))
		return;
	csmemcpy(&message, data, sizeof(message));
	if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.count != 1 || message.share.active > 1 ||
		message.share.kind >= NUMBER_OF_SHARE_KINDS)
		return;
	if (!message.share.active)
	{
		if (remote->active)
			error(_error_silent, "tag multiplayer: the host plays no multiplayer profile now");
		csmemset(remote, 0, sizeof(*remote));
		tag_multiplayer.remote_heard = TRUE;
		return;
	}
	if (remote->active && remote->kind == message.share.kind &&
		!csmemcmp(remote->hash, message.share.hash, sizeof(remote->hash)))
		return;
	csmemset(remote, 0, sizeof(*remote));
	remote->active = TRUE;
	remote->kind = message.share.kind;
	csmemcpy(remote->hash, message.share.hash, sizeof(remote->hash));
	remote->size = message.share.size;
	share_id(remote->hash, remote->id);
	tag_multiplayer.remote_heard = TRUE;
	error(_error_silent, "tag multiplayer: the host plays the tags of its share %s (%lu bytes)%s%s", remote->id,
		remote->size, remote->kind == _share_randomizer ? " in the randomizer's pools" : "",
		port_tag_profiles_shared_present(remote->id) ? ", which this machine has" : "");
}

boolean tag_multiplayer_share_file(
	char *path,
	long path_size,
	unsigned long *size,
	unsigned char *hash)
{
	struct share const *share = hosting() ? &tag_multiplayer.hosted : &tag_multiplayer.remote;

	if (!share->active || !port_tag_profiles_shared_present(hosting() ? TAG_SHARE_HOST : share->id))
		return FALSE;
	if (path)
		tag_multiplayer_share_archive_path(share->hash, path, path_size);
	if (size)
		*size = share->size;
	if (hash)
		csmemcpy(hash, share->hash, TAG_MULTIPLAYER_HASH_BYTES);
	return TRUE;
}

void tag_multiplayer_share_archive_path(
	unsigned char const *hash,
	char *path,
	long path_size)
{
	char id[TAG_MULTIPLAYER_ID_DIGITS + 1];

	share_id(hash, id);
	snprintf(path, (size_t)path_size, "%s%s%s", TAG_SHARE_DIRECTORY, id, TAG_SHARE_EXTENSION);
}

boolean tag_multiplayer_share_downloaded(
	char const *path,
	unsigned char const *hash)
{
	char id[TAG_MULTIPLAYER_ID_DIGITS + 1];
	char host_path[1024];

	(void)path;
	share_id(hash, id);
	share_host_path(id, host_path, sizeof(host_path));
	if (!port_tag_profiles_shared_unpack(host_path, id))
	{
		error(_error_silent, "tag multiplayer: the host's tags (%s) were downloaded, but are not a share", id);
		return FALSE;
	}
	error(_error_silent, "tag multiplayer: downloaded the host's tags (%s)", id);
	/* (and a copy of them, and of its multiplayer profile, the player's own
	to play and change: once) */
	if (port_tag_profiles_shared_import(id) != NONE)
		error(_error_silent, "tag multiplayer: the host's profile is in TAG PROFILES now, as yours");
	return TRUE;
}

boolean tag_multiplayer_game_share(
	unsigned char *hash,
	unsigned long *size,
	short *kind)
{
	struct network_game *game = network_game_get_game();
	struct share const *share;

	if (hosting())
	{
		share = &tag_multiplayer.hosted;
		if (!share->active || !host_takes(game) || !port_tag_profiles_shared_present(TAG_SHARE_HOST))
			return FALSE;
	}
	else
	{
		share = &tag_multiplayer.remote;
		if (!network_game_distributed_client() || !share->active || !game_takes(game) ||
			!port_tag_profiles_shared_present(share->id))
			return FALSE;
	}
	csmemcpy(hash, share->hash, TAG_MULTIPLAYER_HASH_BYTES);
	*size = share->size;
	*kind = share->kind;
	return TRUE;
}

void tag_multiplayer_share_local_path(
	unsigned char const *hash,
	char *path,
	unsigned long path_size)
{
	char id[TAG_MULTIPLAYER_ID_DIGITS + 1];

	share_id(hash, id);
	share_host_path(id, path, path_size);
}

void tag_multiplayer_share_name(
	unsigned char const *hash,
	char *id)
{
	share_id(hash, id);
}

/* whether this machine has the share of that hash as a folder to play,
unpacked from its archive here if it has only that */
static boolean share_ready(
	unsigned char const *hash)
{
	char id[TAG_MULTIPLAYER_ID_DIGITS + 1];
	char archive[1024];
	FILE *file;

	share_id(hash, id);
	if (port_tag_profiles_shared_present(id))
		return TRUE;
	share_host_path(id, archive, sizeof(archive));
	if (!(file = platform_fopen_host(archive, "rb")))
		return FALSE;
	fclose(file);
	if (!port_tag_profiles_shared_unpack(archive, id))
	{
		error(_error_silent, "tag multiplayer: the share %s's archive here is not a share", id);
		return FALSE;
	}
	error(_error_silent, "tag multiplayer: unpacked the share %s from its archive", id);
	return TRUE;
}

boolean tag_multiplayer_replay_share_ready(
	unsigned char const *hash,
	unsigned long size,
	char const *bundle_path,
	long bundle_offset)
{
	char id[TAG_MULTIPLAYER_ID_DIGITS + 1];
	char archive[1024];
	char building[1024];
	byte found[TAG_MULTIPLAYER_HASH_BYTES];
	unsigned long found_size = 0;
	byte *piece_hashes = NULL;
	long piece_count = 0;
	FILE *from;
	FILE *to;
	byte *chunk;
	unsigned long left = size;
	boolean copied;

	if (share_ready(hash))
		return TRUE;
	if (!bundle_path || bundle_offset < 0 || !size || !port_tag_profiles_shared_folder_make())
		return FALSE;
	/* (out of the replay's export, which carries its archive after it: into
	the archive's place, once it is the share it says) */
	share_id(hash, id);
	share_host_path(id, archive, sizeof(archive));
	share_host_path(TAG_SHARE_BUILDING, building, sizeof(building));
	from = platform_fopen_host(bundle_path, "rb");
	to = from ? platform_fopen_host(building, "wb") : NULL;
	chunk = to ? malloc(MAP_SHARE_PIECE_BYTES) : NULL;
	copied = chunk && fseek(from, bundle_offset, SEEK_SET) == 0;
	while (copied && left)
	{
		unsigned long bytes = MIN(left, (unsigned long)MAP_SHARE_PIECE_BYTES);

		copied = fread(chunk, 1, bytes, from) == bytes && fwrite(chunk, 1, bytes, to) == bytes;
		left -= bytes;
	}
	free(chunk);
	if (to && fclose(to) != 0)
		copied = FALSE;
	if (from)
		fclose(from);
	copied = copied && share_hash(building, found, &found_size, &piece_hashes, &piece_count) &&
		found_size == size && !csmemcmp(found, hash, sizeof(found));
	free(piece_hashes);
	if (!copied || !port_tag_profiles_file_replace(building, archive))
	{
		error(_error_silent, "tag multiplayer: the share %s the replay carries cannot be taken out of it", id);
		port_tag_profiles_shared_remove(TAG_SHARE_BUILDING TAG_SHARE_EXTENSION);
		return FALSE;
	}
	error(_error_silent, "tag multiplayer: took the share %s out of the replay", id);
	return share_ready(hash);
}

void tag_multiplayer_replay_share_set(
	unsigned char const *hash,
	unsigned long size,
	short kind)
{
	struct share *replayed = &tag_multiplayer.replayed;

	csmemset(replayed, 0, sizeof(*replayed));
	if (!hash || kind < 0 || kind >= NUMBER_OF_SHARE_KINDS)
		return;
	replayed->active = TRUE;
	replayed->kind = kind;
	csmemcpy(replayed->hash, hash, sizeof(replayed->hash));
	replayed->size = size;
	share_id(hash, replayed->id);
}

/* the share a level loading plays (opened), of a game that takes it, as
the kind given (_share_*) */
static long level_share_open(
	char const *scenario_name,
	short kind)
{
	struct share const *share;
	char const *folder;

	/* (the host's level loading before its share was made, its lobby a
	moment long: made now, though its clients, if any, have not heard of it) */
	if (hosting() && host_build_pending() && host_takes(network_game_get_game()))
	{
		error(_error_silent, "tag multiplayer: the game started before its tags were shared: sharing them now");
		host_build();
	}
	share = hosting() ? &tag_multiplayer.hosted : network_game_distributed_client() ? client_share() : NULL;
	if (!share || !share->active || share->kind != kind)
		return NONE;
	folder = hosting() ? TAG_SHARE_HOST : share->id;
	if (!port_tag_profiles_shared_present(folder))
	{
		error(_error_silent, "tag multiplayer: this machine has not got the host's tags (%s): the level is "
			"played without them, and will not match the host's", share->id);
		return NONE;
	}
	error(_error_silent, "tag multiplayer: %s plays the host's tags (%s)", tag_name_strip_path(scenario_name),
		share->id);
	return port_tag_profiles_shared_open(folder);
}

long tag_multiplayer_level_profile(
	char const *scenario_name)
{
	struct network_game *game = network_game_get_game();

	if (!game || game->variant.game_engine_index == game_engine_none || !tag_multiplayer_map_takes(scenario_name))
		return NONE;
	return level_share_open(scenario_name, _share_profile);
}

long tag_multiplayer_campaign_level_profile(
	char const *scenario_name)
{
	struct network_game *game = network_game_get_game();

	if (!game || game->variant.game_engine_index != game_engine_none || !tag_multiplayer_campaign_takes(scenario_name))
		return NONE;
	return level_share_open(scenario_name, _share_profile);
}

long tag_multiplayer_randomizer_level_profile(
	char const *scenario_name)
{
	struct network_game *game = network_game_get_game();

	if (!game || game->variant.game_engine_index != game_engine_none || !tag_multiplayer_campaign_takes(scenario_name))
		return NONE;
	return level_share_open(scenario_name, _share_randomizer);
}

void tag_multiplayer_level_loaded(
	long profile,
	long const *roots,
	long const *root_tags,
	long count)
{
	static char const *const rotations[] = { "add", "only" };
	static char const *const replaces[] = { "random", "position", "weapon", "manual" };
	static char const *const loadouts[] = { "map", "random", "set" };
	struct scenario *scenario = loading_scenario();
	long mp_profile = hosting() ? tag_multiplayer_game_profile() : NONE;
	long index;
	short weapon;

	tag_multiplayer_level_unloaded();
	if (profile == NONE)
		return;
	tag_multiplayer_level.active = TRUE;
	tag_multiplayer_level.random = system_milliseconds() * 2654435761UL | 1;
	tag_multiplayer_level.rotation = choice_of(port_tag_mp_profiles_option(mp_profile, "rotation"), rotations,
		NUMBEROF(rotations));
	tag_multiplayer_level.replace = choice_of(port_tag_mp_profiles_option(mp_profile, "replace"), replaces,
		NUMBEROF(replaces));
	tag_multiplayer_level.loadout = choice_of(port_tag_mp_profiles_option(mp_profile, "loadout"), loadouts,
		NUMBEROF(loadouts));

	/* the map's own weapons, and those its netgame equipment spawns */
	for (weapon = 0; weapon < TAG_MULTIPLAYER_STOCK_WEAPONS; weapon++)
	{
		tag_multiplayer_level.stock[weapon] = tag_loaded(WEAPON_DEFINITION_TAG, stock_weapons[weapon].tag);
		tag_multiplayer_level.weapon_rolls[weapon] = NONE;
	}
	for (index = 0; scenario && index < scenario->netgame_equipment.count; index++)
	{
		struct scenario_netgame_equipment *equipment = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_equipment, index,
			struct scenario_netgame_equipment);
		struct item_collection_definition *collection;
		long permutation;

		if (equipment->item_collection.index == NONE)
			continue;
		collection = (struct item_collection_definition *)tag_get(ITEM_COLLECTION_DEFINITION_TAG,
			equipment->item_collection.index);
		for (permutation = 0; permutation < collection->permutations.count; permutation++)
		{
			struct item_permutation_definition *item = TAG_BLOCK_GET_ELEMENT(&collection->permutations, permutation,
				struct item_permutation_definition);

			for (weapon = 0; weapon < TAG_MULTIPLAYER_STOCK_WEAPONS; weapon++)
			{
				if (item->item.index != NONE && item->item.index == tag_multiplayer_level.stock[weapon])
					tag_multiplayer_level.stock_spawned[weapon] = TRUE;
			}
		}
	}
	/* the share's weapons */
	for (index = 0; index < count && tag_multiplayer_level.shared_count < MAXIMUM_SHARED_WEAPONS; index++)
	{
		short shared = tag_multiplayer_level.shared_count;

		if (roots[index] == NONE || port_tag_profiles_tag_group(profile, root_tags[index]) != 'weap')
			continue;
		tag_multiplayer_level.shared[shared] = roots[index];
		snprintf(tag_multiplayer_level.shared_paths[shared], sizeof(tag_multiplayer_level.shared_paths[shared]), "%s",
			port_tag_profiles_tag_name(profile, root_tags[index]));
		tag_multiplayer_level.shared_count++;
	}
	/* RANDOM's, POSITION's and WEAPON's: the share's, and with ADD the map's
	that it spawns */
	for (index = 0; index < tag_multiplayer_level.shared_count; index++)
		tag_multiplayer_level.pool[tag_multiplayer_level.pool_count++] = tag_multiplayer_level.shared[index];
	for (weapon = 0; tag_multiplayer_level.rotation == _rotation_add && weapon < TAG_MULTIPLAYER_STOCK_WEAPONS; weapon++)
	{
		if (tag_multiplayer_level.stock_spawned[weapon])
			tag_multiplayer_level.pool[tag_multiplayer_level.pool_count++] = tag_multiplayer_level.stock[weapon];
	}
	/* MANUAL's, by the paths the multiplayer profile has (ORIGINAL the map's
	own), and with ADD the original too */
	for (weapon = 0; weapon < TAG_MULTIPLAYER_STOCK_WEAPONS; weapon++)
	{
		long entries = port_tag_mp_profiles_manual_count(mp_profile, stock_weapons[weapon].key);
		long entry;
		boolean original = tag_multiplayer_level.rotation == _rotation_add;

		for (entry = 0; entry < entries; entry++)
		{
			char const *path = port_tag_mp_profiles_manual(mp_profile, stock_weapons[weapon].key, entry);
			short shared;

			if (!csstrcmp(path, "original"))
			{
				original = TRUE;
				continue;
			}
			for (shared = 0; shared < tag_multiplayer_level.shared_count &&
				csstrcasecmp(tag_multiplayer_level.shared_paths[shared], path); shared++)
			{
			}
			if (shared < tag_multiplayer_level.shared_count &&
				tag_multiplayer_level.manual_count[weapon] < MAXIMUM_MANUAL_CHOICES)
				tag_multiplayer_level.manual[weapon][tag_multiplayer_level.manual_count[weapon]++] =
					tag_multiplayer_level.shared[shared];
		}
		if (original && tag_multiplayer_level.manual_count[weapon] &&
			tag_multiplayer_level.manual_count[weapon] < MAXIMUM_MANUAL_CHOICES)
			tag_multiplayer_level.manual[weapon][tag_multiplayer_level.manual_count[weapon]++] =
				tag_multiplayer_level.stock[weapon];
	}
	/* SET's: a share's weapon by its path, or the map's ("stock:<key>") */
	for (index = 0; index < 2; index++)
	{
		char const *value = port_tag_mp_profiles_option(mp_profile, index ? "secondary" : "primary");
		short shared;

		tag_multiplayer_level.loadout_weapons[index] = NONE;
		if (!strncmp(value, "stock:", 6))
		{
			for (weapon = 0; weapon < TAG_MULTIPLAYER_STOCK_WEAPONS; weapon++)
			{
				if (!strcmp(value + 6, stock_weapons[weapon].key))
					tag_multiplayer_level.loadout_weapons[index] = tag_multiplayer_level.stock[weapon];
			}
			continue;
		}
		for (shared = 0; value[0] && shared < tag_multiplayer_level.shared_count; shared++)
		{
			if (!csstrcasecmp(tag_multiplayer_level.shared_paths[shared], value))
				tag_multiplayer_level.loadout_weapons[index] = tag_multiplayer_level.shared[shared];
		}
	}
	tag_multiplayer_level.position_count = scenario ? scenario->netgame_equipment.count : 0;
	if (tag_multiplayer_level.position_count > 0)
	{
		tag_multiplayer_level.position_rolls = malloc((unsigned long)tag_multiplayer_level.position_count *
			sizeof(*tag_multiplayer_level.position_rolls));
		for (index = 0; tag_multiplayer_level.position_rolls && index < tag_multiplayer_level.position_count; index++)
			tag_multiplayer_level.position_rolls[index] = NONE;
	}
	error(_error_silent, "tag multiplayer: %d weapons shared, %d to choose among (rotation %s, replace %s, loadout %s)",
		(int)tag_multiplayer_level.shared_count, (int)tag_multiplayer_level.pool_count,
		rotations[tag_multiplayer_level.rotation], replaces[tag_multiplayer_level.replace],
		loadouts[tag_multiplayer_level.loadout]);
}

void tag_multiplayer_level_unloaded(
	void)
{
	free(tag_multiplayer_level.position_rolls);
	csmemset(&tag_multiplayer_level, 0, sizeof(tag_multiplayer_level));
}

long tag_multiplayer_netgame_item(
	long equipment_index,
	long definition_index)
{
	short weapon;
	long chosen;

	if (!tag_multiplayer_level.active || network_game_distributed_client() || definition_index == NONE)
		return definition_index;
	for (weapon = 0; weapon < TAG_MULTIPLAYER_STOCK_WEAPONS &&
		tag_multiplayer_level.stock[weapon] != definition_index; weapon++)
	{
	}
	/* (not one of the map's weapons: the flag, the ball, a grenade, a
	powerup) */
	if (weapon == TAG_MULTIPLAYER_STOCK_WEAPONS)
		return definition_index;
	switch (tag_multiplayer_level.replace)
	{
	case _replace_manual:
		if (!tag_multiplayer_level.manual_count[weapon])
			return definition_index;
		return tag_multiplayer_level.manual[weapon][level_random() % (unsigned long)tag_multiplayer_level.manual_count[weapon]];
	case _replace_position:
		if (!tag_multiplayer_level.pool_count || equipment_index < 0 ||
			equipment_index >= tag_multiplayer_level.position_count || !tag_multiplayer_level.position_rolls)
			return definition_index;
		if (tag_multiplayer_level.position_rolls[equipment_index] == NONE)
			tag_multiplayer_level.position_rolls[equipment_index] =
				tag_multiplayer_level.pool[level_random() % (unsigned long)tag_multiplayer_level.pool_count];
		return tag_multiplayer_level.position_rolls[equipment_index];
	case _replace_weapon:
		if (!tag_multiplayer_level.pool_count)
			return definition_index;
		if (tag_multiplayer_level.weapon_rolls[weapon] == NONE)
			tag_multiplayer_level.weapon_rolls[weapon] =
				tag_multiplayer_level.pool[level_random() % (unsigned long)tag_multiplayer_level.pool_count];
		return tag_multiplayer_level.weapon_rolls[weapon];
	default:
		if (!tag_multiplayer_level.pool_count)
			return definition_index;
		chosen = tag_multiplayer_level.pool[level_random() % (unsigned long)tag_multiplayer_level.pool_count];
		return chosen;
	}
}

boolean tag_multiplayer_give_loadout(
	long unit_index)
{
	long weapons[2] = { NONE, NONE };
	boolean first = TRUE;
	short index;

	if (!tag_multiplayer_level.active || network_game_distributed_client() || unit_index == NONE ||
		tag_multiplayer_level.loadout == _loadout_map)
		return FALSE;
	if (tag_multiplayer_level.loadout == _loadout_set)
	{
		weapons[0] = tag_multiplayer_level.loadout_weapons[0];
		weapons[1] = tag_multiplayer_level.loadout_weapons[1];
	}
	else if (tag_multiplayer_level.pool_count)
	{
		weapons[0] = tag_multiplayer_level.pool[level_random() % (unsigned long)tag_multiplayer_level.pool_count];
		/* (another, when there is another) */
		if (tag_multiplayer_level.pool_count > 1)
		{
			unsigned long other = level_random() % (unsigned long)(tag_multiplayer_level.pool_count - 1);
			short at;

			for (at = 0; at < tag_multiplayer_level.pool_count && tag_multiplayer_level.pool[at] != weapons[0]; at++)
			{
			}
			weapons[1] = tag_multiplayer_level.pool[(at + 1 + (short)other) % tag_multiplayer_level.pool_count];
		}
	}
	if (weapons[0] == NONE && weapons[1] == NONE)
		return FALSE;
	unit_delete_all_weapons(unit_index);
	for (index = 0; index < 2; index++)
	{
		struct object_placement_data placement_data;
		long weapon_index;

		if (weapons[index] == NONE)
			continue;
		object_placement_data_new(&placement_data, weapons[index], NONE);
		weapon_index = object_new(&placement_data);
		if (weapon_index == NONE)
			continue;
		if (!first && unit_has_weapon_definition_index(unit_index, weapons[index]))
			object_delete(weapon_index);
		else if (unit_add_weapon_to_inventory(unit_index, weapon_index, first ? 2 : 0))
			first = FALSE;
		else
			object_delete(weapon_index);
	}
	return TRUE;
}
