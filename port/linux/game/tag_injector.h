/*
TAG_INJECTOR.H

The tag injector (tag_injector.c): tags taken out of Custom Edition maps,
as tag bundles, into profiles (port/linux/src/port_tag_profiles.c), from
Settings > Gameplay > Tag Injector, and the tags of a profile added to a
campaign level the randomizer plays, the profile chosen before its screen.
*/

#ifndef __TAG_INJECTOR_H
#define __TAG_INJECTOR_H
#pragma once

/* ---------- constants */

/* the kinds a map's tags are listed by */
enum tag_injector_category
{
	_tag_injector_weapons,
	_tag_injector_ai,
	_tag_injector_equipment,
	_tag_injector_vehicles,
	_tag_injector_bipeds,
	_tag_injector_scenery,

	NUMBER_OF_TAG_INJECTOR_CATEGORIES
};

/* the longest custom type's name a tag is chosen by (its terminator too) */
#define TAG_INJECTOR_CUSTOM_TYPE_LENGTH 32

/* ---------- prototypes/TAG_INJECTOR.C */

/* ---------- the menu's (menu_functions.c) */

/* Whether tags can be taken out of maps: Custom Edition maps can run
(game.custom_edition) and none is being played. When not, `reason` says
why, for the menu. */
boolean tag_injector_maps_available(
	char *reason,
	long reason_size);

/* The Custom Edition maps, as the map lists have them (the campaign maps,
then the multiplayer maps), found again: how many there are. */
short tag_injector_maps_find(
	void);
short tag_injector_map_count(
	void);
/* a map's name in the menus, and its subfolder of custom_maps ("" for
none) */
wchar_t const *tag_injector_map_name(
	short map);
char const *tag_injector_map_folder(
	short map);
/* the map of the list that is Custom Edition's campaign map (campaign TRUE)
or multiplayer map at `index` of custom_edition_maps_count's, or NONE */
short tag_injector_map_of(
	boolean campaign,
	short index);
/* whether a map is a campaign map */
boolean tag_injector_map_campaign(
	short map);

/* Loads a map's tags, as the game would play them, to list them and take
them out; FALSE after logging why. Opening another, or
tag_injector_map_close, lets it go. The map open, or NONE. */
boolean tag_injector_map_open(
	short map);
void tag_injector_map_close(
	void);
short tag_injector_map_opened(
	void);

/* the name of a category ("WEAPONS"), and whether its tags can be added to
a profile (those the randomizer plays: weapons and AI) */
char const *tag_injector_category_name(
	short category);
boolean tag_injector_category_addable(
	short category);
/* the group of a category's tags ('weap'), or 0 */
unsigned long tag_injector_category_group(
	short category);
/* the open map's tags of a category: how many, and the path of each (by
path, alphabetically) */
long tag_injector_category_count(
	short category);
char const *tag_injector_category_tag(
	short category,
	long index);
/* whether a tag's path is one made for it, its map having none for it (a
protected map's: tag_names.c) */
boolean tag_injector_tag_name_made(
	short category,
	long index);
/* How many tags the bundle of a tag would hold (itself and every tag it
names), and the bytes of its tags; NONE when it cannot be taken out,
`reason` saying why. */
long tag_injector_tag_size(
	short category,
	long index,
	char *reason,
	long reason_size);
/* whether a profile's weapon, actor variant or equipment is of a path a
campaign map has (REPLACE ORIGINAL can replace that one). The campaign maps
are read the first time it is asked */
boolean tag_injector_tag_stock(
	long profile,
	long tag);
/* the tag of a profile that is this one of the open map (by its group and
path), or NONE */
long tag_injector_tag_in_profile(
	long profile,
	short category,
	long index);
/* Takes the open map's tag out, with every tag it names, into the profile;
FALSE after logging why, and `reason` saying it for the menu. An actor
variant's weapons and equipment are added too, each a tag of its own (unless
the profile has one of its path): how many, in `carried`. */
boolean tag_injector_tag_add(
	long profile,
	short category,
	long index,
	long *carried,
	char *reason,
	long reason_size);

/* the most textures and materials a preview's mesh has, and the most needs
a tag has */
#define TAG_INJECTOR_MESH_MAXIMUM_TEXTURES 32
#define TAG_INJECTOR_MESH_MAXIMUM_MATERIALS 32
#define TAG_INJECTOR_MAXIMUM_NEEDS 8
#define TAG_INJECTOR_MAXIMUM_ANIMATIONS 16
/* a weapon's label (tag_files.h's TAG_STRING_LENGTH, and its terminator) */
#define TAG_INJECTOR_LABEL_LENGTH 32

/* A texture of a preview's mesh: a map of a shader of its model (a base
map, or a multipurpose map), one mipmap level of its first bitmap (the
largest no larger than the preview draws), its pixels as Halo PC lays them
out, of a format of bitmaps.c's. */
struct tag_injector_mesh_texture
{
	short format;
	short width;
	short height;
	unsigned long size;
	unsigned char *pixels;
};

/* A tag's model, for the menu's preview of it (tag_preview.c): the triangles
of the most detailed geometry of the first permutation of each of its
regions, in the model's space (its default pose), opaque parts alone (all
of them when it has none), each vertex's texture coordinates (its model's
and its shader's map scales applied) and each triangle's material (NONE for
none). A weapon's, equipment's, a vehicle's, a biped's or scenery's own
model, an actor variant's its unit's, with the change colors an object of
it is drawn with (an actor variant's, else its unit's). */
/* A material of a preview's mesh, a shader of its model's (a model's or an
environment's): its base map's texture, and for a model's shader that names
a change color the object has, the color (0 to 1) its multipurpose map's
texture tints the base map by, where its color change mask (Halo PC's
alpha) says (textures NONE for none). */
struct tag_injector_mesh_material
{
	short base_texture;
	short mask_texture;
	boolean tinted;
	float tint[3];
};

struct tag_injector_mesh
{
	long vertex_count;
	long triangle_count;
	float (*positions)[3];
	float (*normals)[3];
	float (*texcoords)[2];
	unsigned short (*triangles)[3];
	short *triangle_materials;
	short texture_count;
	struct tag_injector_mesh_texture textures[TAG_INJECTOR_MESH_MAXIMUM_TEXTURES];
	short material_count;
	struct tag_injector_mesh_material materials[TAG_INJECTOR_MESH_MAXIMUM_MATERIALS];
};

/* whether a tag has a tag it needs to work: it has it, it has not (it
cannot work without it), or it has none and needs none (a melee weapon's
projectile, an unarmed actor variant's weapon) */
enum tag_injector_need_state
{
	_tag_injector_need_present,
	_tag_injector_need_missing,
	_tag_injector_need_none,
};

/* the tags a tag needs to work, by its group: a weapon its model, its
collision (lying on the ground), its first-person model and animations, its
projectile and its HUD; an actor variant its actor (its behaviour), its
unit, the unit's model, animations and collision, and its weapon;
equipment its model and collision; a vehicle or a biped its model,
animations and collision; scenery its model and collision */
struct tag_injector_need
{
	char const *name;
	short state;
};

/* What a tag is seen doing in a game (its animations: the usability
report), each animation it has or has not: a weapon's first-person ones
that every weapon of the game's has (idle, ready, reload, melee, grenade:
the last three where it does them), and whether the campaign's player is
animated holding it (third_person: present when the player has animations
for its label, missing when it is held as another, none when that cannot
be read); an actor variant's (actor), whether its unit is animated holding
its weapon (third_person), and its moves holding it (idle, walk, fire,
reload, melee, grenade, evade, dive, berserk). */
struct tag_injector_usability
{
	boolean actor;
	short third_person;
	char label[TAG_INJECTOR_LABEL_LENGTH];
	short count;
	struct tag_injector_need animations[TAG_INJECTOR_MAXIMUM_ANIMATIONS];
};

struct tag_injector_needs
{
	short count;
	struct tag_injector_need needs[TAG_INJECTOR_MAXIMUM_NEEDS];
	/* (weapons' and actor variants' alone: else third_person none, and no
	animations) */
	struct tag_injector_usability usability;
};

/* The mesh and the needs of the open map's tag of a category, or of a
profile's tag (read from its bundle); FALSE when the tag cannot be read
(both are then empty). The mesh is empty when its model cannot be read.
tag_injector_mesh_free lets it go. */
boolean tag_injector_map_tag_preview(
	short category,
	long index,
	struct tag_injector_mesh *mesh,
	struct tag_injector_needs *needs);
boolean tag_injector_profile_tag_preview(
	long profile,
	long tag,
	struct tag_injector_mesh *mesh,
	struct tag_injector_needs *needs);
void tag_injector_mesh_free(
	struct tag_injector_mesh *mesh);

/* What a profile's tag's "failed" setting says (its bundle could not be
played in the level last loaded, and it was turned off): a sentence for the
menu, or "" when it is not set. */
char const *tag_injector_failure_describe(
	char const *failed);

/* How many of the profiles' tags are bundles as they were taken out (an
earlier build kept them so), which the menu stores as port_tag_store.c keeps
tags (a tag file each, and what they share once in the profile's pack); and
stores the next of them: FALSE when none is left, else TRUE, with the
bytes of its profile before and after (both 0 when it could not be stored:
it is kept as it was, and not tried again until the game starts again). */
long tag_injector_profiles_unstored(
	void);
boolean tag_injector_profile_store_next(
	unsigned long *before,
	unsigned long *after);

/* ---------- the randomizer's (randomizer.c) */

/* The profile the randomizer plays: game.tag_injection's on, and the one
game.randomizer_tag_profile names (the randomizer's profile screen chooses
it) has a tag on. Its index, else NONE. */
long tag_injector_profile_playable(
	void);
/* how many tags of a profile are on: of a group, or of all (0) */
long tag_injector_profile_enabled_count(
	long profile,
	unsigned long group_tag);
/* a tag's setting the randomizer reads ("faction", "type", "class", "hud",
"replace", "name": port_tag_profiles.h's), "" when it is not set */
char const *tag_injector_tag_setting(
	long profile,
	long tag,
	char const *key);
/* the name a profile's tag is shown by: the one it was given (its "name"
setting), else the last part of its path */
void tag_injector_tag_display_name(
	long profile,
	long tag,
	char *name,
	long name_size);
/* the name of a tag's own type, which the randomizer's one type chooses it
by: its custom_type, else the name it is shown by; in capitals */
void tag_injector_tag_custom_type(
	long profile,
	long tag,
	char *name,
	long name_size);
/* the custom types of a profile's tags on of a group, each once, by name:
how many (`capacity` at most) */
long tag_injector_custom_types(
	long profile,
	unsigned long group_tag,
	char (*names)[TAG_INJECTOR_CUSTOM_TYPE_LENGTH],
	long capacity);

/* The campaign's weapon HUDs a weapon's "hud" setting can draw it with: how many, and of each, its setting's value ("sniper_rifle"), its
weapon HUD interface's path, and the campaign map that has it (the first,
of the levels' order), which the randomizer copies it from into a level
without it. */
short tag_injector_stock_hud_count(
	void);
char const *tag_injector_stock_hud_value(
	short hud);
char const *tag_injector_stock_hud_tag(
	short hud);
char const *tag_injector_stock_hud_map(
	short hud);

/* Reads the bundles of a profile's tags on for the level loading, before
the randomizer copies the tags it wants (tag_injector_tags_load adds the
bundles' after those, without reading them again): the stock HUDs their
weapons are drawn with that the level has not got go into `huds`
(tag_injector_stock_hud_* indices, `capacity` at most), each once: how
many. */
long tag_injector_tags_read(
	long profile,
	short *huds,
	long capacity);
/* Adds the tags on of a profile to the level loaded, after its own and
those the randomizer copied: each tag a bundle holds that the level has not
got (a tag of its group and path), the bundle's models made this build's and
its bitmaps' channels found, and the weapons a stock HUD is chosen for
given it, when the level has it (else they keep their own). A bundle that
cannot be played is left out. The
weapons and actor variants of the bundles (the tags they were taken out
for), the level's own when it has them, go into `roots`, and the profile's
tag each is into `root_tags` (`root_capacity` at most): how many. */
long tag_injector_tags_load(
	long profile,
	long *roots,
	long *root_tags,
	long root_capacity);
/* the tags added let go, and the level's table as it was */
void tag_injector_tags_unloaded(
	void);
/* Each tick of a level (game.c): once it has played a moment, the word of
the profile's tags that could not be played in it, on each local player's
HUD, without stopping it (each was turned off, its "failed" setting saying
why: tag_injector_tags_read). */
void tag_injector_tick(
	void);
/* The profile whose tags the level loaded has (tag_injector_tags_load: a
network game's shared profile too), else NONE. */
long tag_injector_level_profile(
	void);
/* The Tag Editor's (tag_editor.c), as a level adds a profile's tags: a copy
of a tag of the level (its root, and with `blocks` its blocks and theirs,
its data and the tags it names shared), named `name`, added to the level's
tags past those added, and let go with them; the one of that group and name
when the level has it. Its index, or NONE when it cannot be made. */
long tag_injector_tag_copy(
	long datum,
	char const *name,
	boolean blocks);
/* whether a tag is one a profile added, and whether the level has any:
Halo PC's tags, which the engine takes as a Custom Edition map's
(tag_groups.c, hud_draw.c and the texture cache) */
boolean tag_injector_tag_injected(
	long tag_index);
boolean tag_injector_tags_loaded(
	void);

/* A cache file read of a tag a profile added: its bitmap pixels or sound
samples, read from its bundle (a bitmap's laid out as the texture cache
wants them). 0 for any other tag, which is read as it would be. */
int tag_injector_read(
	long tag_index,
	long offset,
	long size,
	void *buffer);

#endif
