/*
TAG_MULTIPLAYER.H

The Tag Injector in multiplayer (tag_multiplayer.c, docs/tag_injector.md
"In multiplayer"): a game on one of the game's own multiplayer maps plays
the weapons of a profile, as a multiplayer profile says (port_tag_mp_profiles.c),
on every machine: the host shares them as one file (map_share.c), and
swaps the map's weapons for them and gives them as its players' loadouts. A
co-op game on one of the campaign's levels plays a campaign profile's tags
(tag_campaign.c), or a profile's in the randomizer's pools (randomizer.c),
on every machine alike, shared the same way.
*/

#ifndef __TAG_MULTIPLAYER_H
#define __TAG_MULTIPLAYER_H

/* ---------- constants */

enum
{
	/* the map's weapons a multiplayer profile swaps: the game's own
	multiplayer maps' (the stock HUDs' of tag_injector.c, in their order) */
	TAG_MULTIPLAYER_STOCK_WEAPONS = 9,
	/* a share's hash (map_share.h's MAP_SHARE_HASH_BYTES), and the digits
	of it its folder and archive are named by */
	TAG_MULTIPLAYER_HASH_BYTES = 32,
	TAG_MULTIPLAYER_ID_DIGITS = 16,
};

/* ---------- prototypes/TAG_MULTIPLAYER.C */

/* the map's weapons a multiplayer profile swaps: of each its key (the
setting's word: "shotgun"), its name as the menus show it ("SHOTGUN") and
its path */
char const *tag_multiplayer_stock_key(
	short weapon);
char const *tag_multiplayer_stock_name(
	short weapon);
char const *tag_multiplayer_stock_tag(
	short weapon);

/* the multiplayer profile chosen last (game.multiplayer_tag_profile:
port_tag_mp_profiles' index, else NONE), which the menus' TAG PROFILE
screen starts on, and it chosen (NONE: none), written to config.toml */
long tag_multiplayer_profile_chosen(
	void);
void tag_multiplayer_profile_choose(
	long profile);
/* the multiplayer profile the game hosted here plays (NONE: none), kept
from when it was chosen for the game (the TAG PROFILE screen, before Server
Setup) until the game ends: what config.toml says meanwhile changes nothing,
so a game listed for every build stays so, and a modded one modded */
long tag_multiplayer_game_profile(
	void);
void tag_multiplayer_game_profile_set(
	long profile);
/* the same for a co-op game: the campaign profile it plays (port_tag_mp_profiles'
campaign profiles' index, else NONE), kept for every level of the game; and
of either kind, the profile (port_tag_profiles' index) the game's profile
plays, NONE for none */
long tag_multiplayer_game_campaign_profile(
	void);
void tag_multiplayer_game_campaign_profile_set(
	long profile);
/* ... or the profile (port_tag_profiles' index) a co-op game plays in the
randomizer's pools (NONE: none, or not this machine's game) */
long tag_multiplayer_game_randomizer_profile(
	void);
void tag_multiplayer_game_randomizer_profile_set(
	long profile);
long tag_multiplayer_game_base_profile(
	void);
/* whether a game on this level plays a multiplayer profile: one of the
game's own multiplayer maps (not a campaign level, nor a Custom Edition
map) */
boolean tag_multiplayer_map_takes(
	char const *level_name);
/* whether a co-op game on this level plays a campaign profile: one of the
campaign's ten levels (not a Custom Edition map) */
boolean tag_multiplayer_campaign_takes(
	char const *level_name);

/* every frame (main.c): the host's share made (once its profile is chosen)
and told to each client in the lobby and in game; a client's download of
it asked for */
void tag_multiplayer_update(
	void);
/* (the host) whether its game plays a multiplayer profile (one was chosen
for it, on one of the game's own multiplayer maps, whether or not its share
could be made): a modded game, which builds without the mod cannot play
(halo_port_limits.h), and its advertisement's flag set */
boolean tag_multiplayer_hosted(
	void);
/* ... a co-op game, playing a campaign profile (the randomizer is not
played, nor advertised) */
boolean tag_multiplayer_campaign_hosted(
	void);
/* whether the co-op game plays a profile in the randomizer's pools: the
host's chosen for it, a client's told by the host (its share's kind) */
boolean tag_multiplayer_randomizer_shared(
	void);
void tag_multiplayer_advertisement(
	unsigned char *reserved);
/* the host's share, told each client (_distributed_message_tag_share),
over its connection alone */
boolean tag_multiplayer_handles_message(
	unsigned short const *message,
	unsigned short size);
void tag_multiplayer_handle_message(
	long machine_index,
	unsigned short const *message,
	unsigned short size,
	boolean from_stream);

/* map_share.c's: the share this machine sends (the host's, or a client's
download of it), its archive's path, size and hash; FALSE for none. The
archive's path of a share's hash (d:\tag_injector\.shared\<id>.tagshare);
and a download of one, made the share's folder: FALSE if it cannot be */
boolean tag_multiplayer_share_file(
	char *path,
	long path_size,
	unsigned long *size,
	unsigned char *hash);
void tag_multiplayer_share_archive_path(
	unsigned char const *hash,
	char *path,
	long path_size);
boolean tag_multiplayer_share_downloaded(
	char const *path,
	unsigned char const *hash);

/* ---------- replays (network_replay.c, network_replay_playback.c) */

/* the share the game this machine plays plays (the host's, or the host's
this client has): its hash, its archive's size and its kind; FALSE for
none. A recording keeps it, and its playback plays it */
boolean tag_multiplayer_game_share(
	unsigned char *hash,
	unsigned long *size,
	short *kind);
/* the archive's path, as the platform has it, of a share's hash (an export
of a replay carries it), and its id (TAG_MULTIPLAYER_ID_DIGITS + 1 bytes) */
void tag_multiplayer_share_local_path(
	unsigned char const *hash,
	char *path,
	unsigned long path_size);
void tag_multiplayer_share_name(
	unsigned char const *hash,
	char *id);
/* (a playback) whether this machine has the share a replay played, to play
it: its folder, or its archive here (unpacked), or the archive the replay's
export carries at bundle_offset of the file at bundle_path (NULL, or -1:
none), taken out and unpacked once it is the share of that hash */
boolean tag_multiplayer_replay_share_ready(
	unsigned char const *hash,
	unsigned long size,
	char const *bundle_path,
	long bundle_offset);
/* (a playback) the share its level plays, in the host's place (NULL:
none), of the kind the recording kept */
void tag_multiplayer_replay_share_set(
	unsigned char const *hash,
	unsigned long size,
	short kind);

/* randomizer.c's, as a level loads: the shared profile a multiplayer
level adds the tags of (opened: port_tag_profiles' index, else NONE); then
the weapons it added (the tags they were taken out for, and their indices in
the profile) for the host to swap and give; and the level let go */
long tag_multiplayer_level_profile(
	char const *scenario_name);
/* ... and a co-op game's campaign level's (tag_campaign.c plays it: the
host's campaign profile on the host, nothing to swap on a client) */
long tag_multiplayer_campaign_level_profile(
	char const *scenario_name);
/* ... and the share a co-op game's randomizer adds to its pools (opened) */
long tag_multiplayer_randomizer_level_profile(
	char const *scenario_name);
void tag_multiplayer_level_loaded(
	long profile,
	long const *roots,
	long const *root_tags,
	long count);
void tag_multiplayer_level_unloaded(
	void);

/* game_engine.c's (the host's): the item a netgame equipment spawns, as
the multiplayer profile swaps the map's weapons; and a player's loadout
given (TRUE), or left to the gametype (FALSE) */
long tag_multiplayer_netgame_item(
	long equipment_index,
	long definition_index);
boolean tag_multiplayer_give_loadout(
	long unit_index);

#endif
