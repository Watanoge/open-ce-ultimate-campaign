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

#endif
