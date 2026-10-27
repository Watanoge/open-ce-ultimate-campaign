/*
FIRING_RANGE.H

The firing range (firing_range.c): a level played alone to try a Tag
Injector profile's weapons and AI, on a plane this build makes in place of
a stock multiplayer map's structure bsp.
*/

#ifndef __FIRING_RANGE_H
#define __FIRING_RANGE_H
#pragma once

/* ---------- constants */

enum
{
	FIRING_RANGE_NAME_LENGTH = 64,
};

/* ---------- prototypes/FIRING_RANGE.C */

/* the map the range is played on (its scenario's name, for
campaign_start), and the range asked for, with the profile it plays (its
folder's name): the next level of that map loaded is the range, and so is
each loaded again (a restart) until another map is */
char const *firing_range_map_name(
	void);
void firing_range_request(
	char const *profile);
/* whether the level loaded is the range, and the profile it plays ("" for
none) */
boolean firing_range_active(
	void);
char const *firing_range_profile(
	void);

/* randomizer.c's, as the range's level loads: the profile it plays (its
folder's name; NULL when the level is not the range), and the weapons and
actor variants its tags added (tag_injector_tags_load's roots), of which the
weapons are laid out, each once, on the benches the bsp is written with */
char const *firing_range_level_profile(
	char const *scenario_name);
void firing_range_tags_injected(
	long const *roots,
	long count);

/* cache_files.c's tags_load and tags_unload: the range's scenario has its
map's objects taken out and its players start on the plane (put back as the
tags go) */
void firing_range_tags_loaded(
	char const *scenario_name,
	long scenario_tag_index);
void firing_range_tags_unloaded(
	void);

/* cache_files.c's scenario_structure_bsp_load: in the range, the bsp is
written where the map's would be read (`base_address`, `size` bytes: a
header, struct cache_file_structure_bsp_header's, then the bsp), its
materials' vertices uncompressed, as a Custom Edition map's are
(custom_edition_structure_bsp_load compresses them and makes their buffers).
FALSE outside the range, or when it does not fit (the map's is read). */
boolean firing_range_structure_bsp_write(
	void *base_address,
	long size);
/* whether the bsp loaded is the one written (its buffers are let go of as
a Custom Edition bsp's are) */
boolean firing_range_structure_bsp_written(
	void);
void firing_range_structure_bsp_unloaded(
	void);

/* game.c's, each tick of a level: the weapons put back on the benches,
and the mirror's copy made, given the player's weapons, or let go */
void firing_range_tick(
	void);
/* players.c's, as a player's controls are given its unit: the mirror's
copy given them, turned about the mirror */
struct unit_control_data;
void firing_range_player_control(
	long unit_index,
	struct unit_control_data const *control);
/* damage.c's: whether damage is not dealt, the mirror's copy's (it hurts
no one) or to it (nothing hurts it) */
boolean firing_range_damage_ignored(
	long owner_object_index,
	long object_index);

#endif
