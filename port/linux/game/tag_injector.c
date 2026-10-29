/*
TAG_INJECTOR.C

The tag injector (tag_injector.h): Settings > Gameplay > Tag Injector lists
the Custom Edition maps, a map's weapons, AI, equipment, vehicles, bipeds
and scenery, and takes a weapon or an actor variant out of it, with every
tag it names, as a tag bundle (tag_bundle.h) into a profile
(port/linux/src/port_tag_profiles.c), from which the randomizer adds it to
a campaign level and its pools (randomizer.c).

A map's tags are taken out as the game would play them: loaded, converted
and checked by its loader (custom_edition_cache_bundle_open), its sounds
those of the tag decoded to Xbox ADPCM, which this build plays; its models
are kept as Halo PC's, and its bitmaps' pixels as Halo PC lays them out. A
tag is listed by its group and path, and the menu loads its map again to
take one out, with the sounds of the tags it names decoded.

A profile's tags are added to a level after its own (and those the
randomizer copied from the other campaign maps) as the randomizer adds its
own: each tag of a bundle the level has not got, by its group and path, is
appended to the tag table, the next salt after the table's, in the
profile's order; one it has is the level's, and so is a tag another bundle
added before. The bundle's bytes stay as they were loaded, and are the
tags': each reference and tag index is given the tag of the level it names
now, and a sound's permutations the tag their samples are read for. A
gbxmodel is given this build's layout and buffers as a Custom Edition map's
are (custom_edition_injected_model_convert), and its group the models'. A
bitmap's pixels and a sound's samples are read from the bundle's file data
(tag_injector_read, from cache_file_read), a bitmap's laid out for the
texture cache as a Custom Edition map's are, with its channels where Halo PC
keeps them (custom_edition_loaded_reordered_bitmaps_find finds them in the
level's tags, the bundles' with them). A bundle whose models cannot be made,
or whose bitmaps cannot be drawn, is left out, and the tags of none are
added when there is no room for them all.

A weapon added whose tag of the profile (its own, whichever bundle adds it:
an actor variant's carries its weapons) has a "hud" setting of a stock HUD
(the campaign's HUD of one of its weapons, which the randomizer copies from
the map that has it into a level without it) has its first-person interface
name that one: its own is added as the rest are, and not drawn. The bundles
are read first (tag_injector_tags_read), before the randomizer copies the
tags it wants, so that it knows the stock HUDs to copy.

A bundle whose tag has "replace" on takes the place of the level's tag of
its path (the campaign's: an energy sword, an elite major): the level's tag
is given the bundle's tag, where it is in the table, so that whatever names
it (the scenario's palettes, its actor variants, the randomizer's copies)
has the bundle's now; and the bundle's other tags are added as its own,
rather than being the level's of their paths, so that the tag is as its map
had it. Its tags of a path another bundle's replaced are that one.

Adding an actor variant to a profile adds the weapons and equipment it
names too, each a tag of the profile of its own (its HUD, its replacing).

The menu's preview of a tag (tag_preview.c) is given its model's triangles
(tag_injector_map_tag_mesh, tag_injector_profile_tag_mesh): the map open's
read from its tags and its files, a profile's from its bundle's tags and
file data, Halo PC's parts either way.

Only a machine playing alone adds them (randomizer.c): the tags are this
machine's, and a network game's machines must have the same tag table.
*/

/* ---------- headers */

#include "cseries.h"
#include "errors.h"
#include "ai/actor_definitions.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "items/weapon_definitions.h"
#include "models/model_animation_definitions.h"
#include "models/model_definitions.h"
#include "objects/object_definitions.h"
#include "rasterizer/rasterizer_swizzle.h"
#include "sound/sound_definitions.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "game/game.h"
#include "game/players.h"
#include "memory/data.h"
#include "cutscene/cinematics.h"
#include "interface/hud_messaging.h"
#include "cache_file_formats.h"
#include "custom_edition_cache.h"
#include "custom_edition_maps.h"
#include "tag_schema.h"
#include "tag_bundle.h"
#include "tag_editor.h"
#include "tag_injector.h"
#include "tag_names.h"
#include "randomizer.h"
#include "halo_port_capacity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- constants */

enum
{
	GBXMODEL_GROUP_TAG = 'mod2',
	MODEL_GROUP_TAG = 'mode',
	WEAPON_HUD_INTERFACE_GROUP_TAG = 'wphi',
	EQUIPMENT_GROUP_TAG = 'eqip',
	ACTOR_VARIANT_GROUP_TAG = 'actv',
	VEHICLE_GROUP_TAG = 'vehi',
	BIPED_GROUP_TAG = 'bipd',
	SCENERY_GROUP_TAG = 'scen',
	SHADER_MODEL_GROUP_TAG = 'soso',
	SHADER_ENVIRONMENT_GROUP_TAG = 'senv',

	/* the most maps listed, and tags of a category */
	MAXIMUM_TAG_INJECTOR_MAPS = 512,
	/* the most bytes of bundles' tags a level holds (their pixels, samples
	and vertices are read from their files), and bundles */
	MAXIMUM_INJECTED_BYTES = 0x4000000,
	MAXIMUM_INJECTED_BUNDLES = 64,
	/* the tags of a level, as datum indices count them */
	MAXIMUM_TAG_COUNT = 0xFFFF,
	MAXIMUM_WALK_DEPTH = 32,
	MAP_PATH_SIZE = 256,
	/* a tag's size not looked for yet (tag_injector_tag_size) */
	TAG_SIZE_UNKNOWN = -2,
	/* the zoom past which AUTO's HUD is the sniper rifle's (the pistol's
	is 2x, the sniper rifle's 2x and 8x) */
	STOCK_HUD_SNIPER_ZOOM = 4,

	/* (bitmaps.c's) */
	BITMAP_TYPE_3D = 1,
	BITMAP_LINEAR_BIT = 4,
	/* the largest a bitmap added is drawn at with game.tag_injection_textures
	"small", or "auto" on a texture cache smaller than AUTO_SMALL_CACHE (the
	Xbox's 22 MB, the web build's: one Custom Edition weapon's bitmaps
	are as much as 16 MB, uncompressed 512s) */
	SMALL_BITMAP_LIMIT = 256,
	AUTO_SMALL_CACHE = 0x4000000,

	/* a Custom Edition gbxmodel's part (tag_bundle.c's, and
	custom_edition_geometry.c's struct custom_edition_model_part) and vertex
	(struct model_vertex_uncompressed) */
	GBXMODEL_PART_BYTES = 0x84,
	GBXMODEL_PART_SHADER_INDEX_OFFSET = 0x04,
	GBXMODEL_PART_STRIP_TRIANGLE_COUNT_OFFSET = 0x48,
	GBXMODEL_PART_STRIP_OFFSET_OFFSET = 0x4C,
	GBXMODEL_PART_VERTEX_COUNT_OFFSET = 0x58,
	GBXMODEL_PART_VERTEX_OFFSET_OFFSET = 0x64,
	GBXMODEL_VERTEX_BYTES = 0x44,
	GBXMODEL_VERTEX_POSITION_OFFSET = 0x00,
	GBXMODEL_VERTEX_NORMAL_OFFSET = 0x0C,
	GBXMODEL_VERTEX_TEXCOORD_OFFSET = 0x30,
	/* a model's shader's map scales and base map, and an environment's
	base map (tag_schema_render.c's struct shader_model_definition and
	struct shader_environment_definition) */
	SHADER_MODEL_MAP_U_SCALE_OFFSET = 0x9C,
	SHADER_MODEL_MAP_V_SCALE_OFFSET = 0xA0,
	SHADER_MODEL_BASE_MAP_OFFSET = 0xA4,
	SHADER_MODEL_CHANGE_COLOR_SOURCE_OFFSET = 0x4C,
	SHADER_MODEL_MULTIPURPOSE_MAP_OFFSET = 0xBC,
	SHADER_ENVIRONMENT_BASE_MAP_OFFSET = 0x88,
	/* a preview's mesh (tag_injector_mesh): the most of it read, as
	object_mesh.c's, a part's strip at most, the regions read, and the
	models' detail levels (models.c's NUMBER_OF_DETAIL_LEVELS_PER_MODEL) */
	PREVIEW_MAXIMUM_VERTICES = 16384,
	PREVIEW_MAXIMUM_TRIANGLES = 24576,
	PREVIEW_MAXIMUM_STRIP_TRIANGLES = 0x10000,
	PREVIEW_MAXIMUM_REGIONS = 32,
	PREVIEW_DETAIL_LEVELS = 5,
	/* a preview's model's shaders whose materials are made at most, and a
	shader's material not made yet; an object's change colors (A to D:
	objects.h's NUMBER_OF_OBJECT_CHANGE_COLORS); the largest texture read (a
	mipmap level no larger), and the largest bitmap */
	PREVIEW_MAXIMUM_SHADERS = 64,
	PREVIEW_MATERIAL_UNMADE = -2,
	PREVIEW_CHANGE_COLORS = 4,
	PREVIEW_TEXTURE_SIZE = 128,
	PREVIEW_MAXIMUM_BITMAP_SIZE = 4096,
};

/* (bitmaps.c's bitmap type and formats a preview's textures may be of) */
enum
{
	_preview_bitmap_type_2d = 0,

	_preview_bitmap_format_a8 = 0,
	_preview_bitmap_format_y8 = 1,
	_preview_bitmap_format_ay8 = 2,
	_preview_bitmap_format_a8y8 = 3,
	_preview_bitmap_format_r5g6b5 = 6,
	_preview_bitmap_format_a1r5g5b5 = 8,
	_preview_bitmap_format_a4r4g4b4 = 9,
	_preview_bitmap_format_x8r8g8b8 = 10,
	_preview_bitmap_format_a8r8g8b8 = 11,
	_preview_bitmap_format_dxt1 = 14,
	_preview_bitmap_format_dxt3 = 15,
	_preview_bitmap_format_dxt5 = 16,
};

/* where each animation the usability report looks for is in its list
(model_animation_definitions.c's): a first-person weapon's, a weapon
class's (the dives four from dive-front, the evades two from evade-left)
and a weapon type's */
enum
{
	_first_person_idle = 0,
	_first_person_reload_empty = 7,
	_first_person_reload_full = 8,
	_first_person_ready = 10,
	_first_person_melee = 13,
	_first_person_throw_grenade = 16,

	_class_idle = 0,
	_class_dive_front = 4,
	_class_move_front = 8,
	_class_throw_grenade = 20,
	_class_berserk = 29,
	_class_evade_left = 30,
	_class_melee = 39,

	_type_reload = 0,
	_type_fire = 4,
	_type_melee = 8,
};

/* the farthest from its origin a preview's vertex is taken to be (one
farther is taken to be at it: a vertex that is not a number) */
#define PREVIEW_MAXIMUM_COORDINATE 1000.0f
/* the largest map scale read (a larger one, or none, is taken to be 1) */
#define PREVIEW_MAXIMUM_MAP_SCALE 1000.0f

/* the word of the tags that could not be played in a level: its lines (as
many as the HUD shows at once), the characters of each (fewer than a HUD
message's), and the ticks of play it waits for (seconds, at 30 ticks each:
past a cutscene, with the players in their bodies) */
#define TAG_INJECTOR_NOTICE_LINES 4
#define TAG_INJECTOR_NOTICE_WIDTH 58
#define TAG_INJECTOR_NOTICE_TICKS (2 * 30)

/* the "failed" settings' values, and what the menu says of each */
static struct
{
	char const *value;
	char const *described;
} const failures[] =
{
	{ "missing", "its bundle is missing from the profile's folder" },
	{ "unreadable", "its bundle could not be read" },
	{ "too large", "its tags, with those before it, were more than a level adds" },
	{ "unplayable", "its bundle is not one this build can play" },
	{ "no memory", "there was no memory for it" },
	{ "not holdable", "a weapon it adds has no first-person model or animations" },
	{ "bitmaps", "a bitmap it adds cannot be drawn by this build" },
	{ "models", "a model it adds cannot be made this build's" },
};

/* the campaign's weapon HUDs (tag_injector_stock_hud_*): the "hud"
setting's value, the HUD's path, and the first campaign map that has it */
static struct
{
	char const *value;
	char const *tag;
	char const *map;
} const stock_huds[] =
{
	{ "assault_rifle", "weapons\\assault rifle\\assault rifle", "a10" },
	{ "pistol", "weapons\\pistol\\pistol", "a10" },
	{ "sniper_rifle", "weapons\\sniper rifle\\sniper rifle", "a30" },
	{ "rocket_launcher", "weapons\\rocket launcher\\rocket_launcher", "b30" },
	{ "shotgun", "weapons\\shotgun\\shotgun", "c10" },
	{ "flamethrower", "weapons\\flamethrower\\flame thrower", "c20" },
	{ "plasma_rifle", "weapons\\plasma rifle\\plasma rifle", "a10" },
	{ "plasma_pistol", "weapons\\plasma pistol\\plasma pistol", "a10" },
	{ "needler", "weapons\\needler\\needler", "a30" },
};

/* AUTO's words, in a weapon's path (any case, _ as a space): the first
found chooses its stock HUD */
static struct
{
	char const *word;
	char const *value;
} const stock_hud_words[] =
{
	{ "sniper", "sniper_rifle" },
	{ "beam", "sniper_rifle" },
	{ "needle", "needler" },
	{ "rocket", "rocket_launcher" },
	{ "launcher", "rocket_launcher" },
	{ "fuel rod", "rocket_launcher" },
	{ "shotgun", "shotgun" },
	{ "mauler", "shotgun" },
	{ "flame", "flamethrower" },
	{ "plasma pistol", "plasma_pistol" },
	{ "plasma rifle", "plasma_rifle" },
	{ "pistol", "pistol" },
	{ "magnum", "pistol" },
};

/* ---------- structures */

/* a tag of the tag table (cache_files.c's, as randomizer.c has it) */
struct injector_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

/* a Custom Edition map's tags' header, as tag_bundle.c has it: the table,
and where its models' vertices and strips are in its files (a part's
vertices at its vertex offset in the model data, its strip at its strip
offset past the model data's index data offset) */
struct injector_map_header
{
	struct injector_tag_instance *instances;
	long scenario_tag_index;
	unsigned long checksum;
	long tag_count;
	long model_part_count;
	unsigned long model_data_offset;
	long model_part_count_again;
	unsigned long model_index_data_offset;
	unsigned long model_data_size;
	unsigned long signature;
};

/* a gbxmodel's shader, its geometry, and its geometry's part (read by its
offsets: GBXMODEL_PART_*) */
struct preview_model_shader
{
	struct tag_reference shader;
	short permutation_index;
	word pad;
	long unused[3];
};

struct preview_model_geometry
{
	byte reserved[0x24];
	struct tag_block parts;
};

struct preview_model_part
{
	byte bytes[GBXMODEL_PART_BYTES];
};

/* an actor variant's change color (actors.c's struct
actor_variant_change_colors) */
struct preview_actor_variant_change_colors
{
	real_rgb_color color_lower_bound;
	real_rgb_color color_upper_bound;
	unsigned long unused[2];
};

/* a first-person animation graph's animations (first_person_weapons.c's
struct animation_graph_first_person_weapon_animations): an index of the
graph's animations of each of model_animation_definitions.c's first-person
list, NONE for none */
struct preview_first_person_animations
{
	long unused[4];
	struct tag_block animations;
};

typedef char verify_preview_first_person_animations_size[
	sizeof(struct preview_first_person_animations) == 0x1C ? 1 : -1];
typedef char verify_preview_model_shader_size[sizeof(struct preview_model_shader) == 0x20 ? 1 : -1];
typedef char verify_preview_model_geometry_size[sizeof(struct preview_model_geometry) == 0x30 ? 1 : -1];
typedef char verify_preview_actor_variant_change_colors_size[
	sizeof(struct preview_actor_variant_change_colors) == 0x20 ? 1 : -1];

typedef char verify_injector_tag_instance_size[sizeof(struct injector_tag_instance) == 0x20 ? 1 : -1];
typedef char verify_injector_map_header_size[sizeof(struct injector_map_header) == 0x28 ? 1 : -1];

/* a map of the menu's list */
struct injector_map
{
	short display_index;
	boolean campaign;
};

/* a bundle of a profile added to the level: its file, open while the level
is (port_tag_profiles_bundle_open), its tags, and the tag of the level each
of them is (NONE: none). Its file data stays in its file */
struct injected_bundle
{
	long handle;
	byte *bytes;
	unsigned long size;
	struct tag_bundle_loaded loaded;
	long *datums;
	/* left out (its models could not be made, or its bitmaps drawn), and
	why: a "failed" setting's value (failures[]) */
	boolean refused;
	char const *failure;
	/* the profile's tag it is, whether it replaces the level's tag of its
	path ("replace"), and the level's tag it has replaced (NONE) */
	long profile_tag;
	boolean replace;
	long replaced;
	/* an AI's (an actor variant's bundle) that carries weapons a player
	could not hold: they are left out, and it is added unarmed of them
	(its loadout, or the randomizer, arms it) */
	boolean drops_weapons;
};

/* ---------- globals */

/* (cache_files.c's) */
void *cache_files_tag_instances(long *count);
void cache_files_set_tag_instances(void *instances, long count);
/* (port_config.c's) */
int config_boolean(char const *name);
char const *config_string(char const *name);
/* (port/linux/src/xbox_textures.c's) */
void halo_custom_edition_texels_channels(void const *texels, unsigned char channel_order);

/* (port/linux/src/port_tag_profiles.c's: port_tag_profiles.h) */
long port_tag_profiles_scan(void);
long port_tag_profiles_count(void);
char const *port_tag_profiles_name(long profile);
long port_tag_profiles_find(char const *folder);
long port_tag_profiles_tag_count(long profile);
char const *port_tag_profiles_tag_map(long profile, long tag);
unsigned long port_tag_profiles_tag_group(long profile, long tag);
char const *port_tag_profiles_tag_name(long profile, long tag);
long port_tag_profiles_tag_find(long profile, unsigned long group, char const *name);
int port_tag_profiles_tag_enabled(long profile, long tag);
int port_tag_profiles_tag_set_enabled(long profile, long tag, int enabled);
int port_tag_profiles_tag_set_option(long profile, long tag, char const *key, char const *value);
char const *port_tag_profiles_tag_option(long profile, long tag, char const *key);
unsigned long port_tag_profiles_tag_size(long profile, long tag);
unsigned long port_tag_profiles_size(long profile);
int port_tag_profiles_tag_stored(long profile, long tag);
char const *port_tag_profiles_folder(long profile);
/* (its regions port_tag_store.h's, laid out as tag_bundle.h's) */
int port_tag_profiles_tag_add(long profile, char const *map, unsigned long group, char const *name,
	void const *bundle, unsigned long size, unsigned long tags_size, struct tag_bundle_file_region const *regions,
	long region_count);
long port_tag_profiles_bundle_open(long profile, long tag, unsigned long *size);
int port_tag_profiles_bundle_read_at(long handle, unsigned long offset, void *buffer, unsigned long size);
void port_tag_profiles_bundle_close(long handle);
int port_tag_profiles_shared(long profile);

static struct
{
	char const *name;
	unsigned long group_tag;
	boolean addable;
} const tag_injector_categories[NUMBER_OF_TAG_INJECTOR_CATEGORIES] =
{
	{ "WEAPONS", 'weap', TRUE },
	{ "AI", 'actv', TRUE },
	{ "EQUIPMENT", 'eqip', TRUE },
	{ "VEHICLES", 'vehi', FALSE },
	{ "BIPEDS", 'bipd', FALSE },
	{ "SCENERY", 'scen', FALSE },
};

/* the menu's: the maps, the map open, and its tags of each category (their
absolute indices), each one's size (TAG_SIZE_UNKNOWN until asked) and the
tag that keeps it from being taken out (NONE); and of each of its tags,
whether its name is one made for it (tag_names.c: NULL for none) */
static struct
{
	struct injector_map maps[MAXIMUM_TAG_INJECTOR_MAPS];
	short map_count;
	short opened;
	char path[MAP_PATH_SIZE];
	char map_name[64];
	struct injector_map_header *header;
	long *tags[NUMBER_OF_TAG_INJECTOR_CATEGORIES];
	long *sizes[NUMBER_OF_TAG_INJECTOR_CATEGORIES];
	long *problems[NUMBER_OF_TAG_INJECTOR_CATEGORIES];
	long tag_counts[NUMBER_OF_TAG_INJECTOR_CATEGORIES];
	unsigned char *names_made;
} tag_injector_menu = { { { 0 } }, 0, NONE };

/* the level's: the bundles read (of which profile, and their tags' bytes),
the table as it was and as it is now, and of the tags added (from
first_injected on), the bundle each is of */
static struct
{
	struct injected_bundle bundles[MAXIMUM_INJECTED_BUNDLES];
	long bundle_count;
	boolean bundles_read;
	long read_profile;
	unsigned long read_bytes;
	struct injector_tag_instance *saved_instances;
	long saved_count;
	struct injector_tag_instance *grown_instances;
	long first_injected;
	long injected_count;
	long replaced_count;
	short *injected_bundle;
	boolean bitmaps_added;
	/* the salt of the tags added, and the copies the Tag Editor's edits
	made (tag_injector_tag_copy: past the tags added) and what they hold
	(their names, roots and blocks), let go with the level */
	long salt;
	long copied_count;
	void **allocations;
	long allocation_count;
	long allocation_capacity;
	/* the word of the profile's tags that could not be played, which the
	HUD shows once the level has played a moment (tag_injector_tick) */
	char notice[TAG_INJECTOR_NOTICE_LINES][TAG_INJECTOR_NOTICE_WIDTH + 4];
	short notice_lines;
	long notice_ticks;
} tag_injector_level;

/* ---------- private code */

static unsigned long read_unsigned_long(
	byte const *address)
{
	unsigned long value;

	memcpy(&value, address, sizeof(value));
	return value;
}

static struct tag_schema_definition const *schema_of(
	unsigned long group_tag)
{
	struct tag_schema_group const *const *list;
	struct tag_schema_group const *group;

	for (list = tag_schema_group_lists; *list; list++)
	{
		for (group = *list; group->group_tag; group++)
		{
			if (group->group_tag == group_tag)
				return group->definition;
		}
	}

	return NULL;
}

/* the group the level knows a tag of a bundle by: a gbxmodel is a model */
static unsigned long played_group(
	unsigned long group_tag)
{
	return group_tag == GBXMODEL_GROUP_TAG ? MODEL_GROUP_TAG : group_tag;
}

static void reason_set(
	char *reason,
	long reason_size,
	char const *text)
{
	if (reason && reason_size > 0)
		snprintf(reason, (size_t)reason_size, "%s", text);
}

/* ---------- the menu's */

static void menu_tags_free(
	void)
{
	short category;

	for (category = 0; category < NUMBER_OF_TAG_INJECTOR_CATEGORIES; category++)
	{
		/* (the game's free does not take NULL) */
		if (tag_injector_menu.tags[category])
			free(tag_injector_menu.tags[category]);
		if (tag_injector_menu.sizes[category])
			free(tag_injector_menu.sizes[category]);
		if (tag_injector_menu.problems[category])
			free(tag_injector_menu.problems[category]);
		tag_injector_menu.tags[category] = NULL;
		tag_injector_menu.sizes[category] = NULL;
		tag_injector_menu.problems[category] = NULL;
		tag_injector_menu.tag_counts[category] = 0;
	}
}

static char const *menu_tag_name(
	long absolute)
{
	struct injector_map_header const *header = tag_injector_menu.header;

	if (!header || absolute < 0 || absolute >= header->tag_count || !header->instances[absolute].name)
		return "";
	return header->instances[absolute].name;
}

static int menu_tag_less(
	void const *left,
	void const *right)
{
	return _stricmp(menu_tag_name(*(long const *)left), menu_tag_name(*(long const *)right));
}

/* the absolute index of a tag of a category of the map open, or NONE */
static long menu_tag(
	short category,
	long index)
{
	if (tag_injector_menu.opened == NONE || category < 0 || category >= NUMBER_OF_TAG_INJECTOR_CATEGORIES ||
		index < 0 || index >= tag_injector_menu.tag_counts[category])
		return NONE;
	return tag_injector_menu.tags[category][index];
}

/* the map open's tags without names of their own (a protected map's) given
names made for them, as each time it is loaded: the same, which its tags
are listed, kept in profiles and added to levels by (tag_names.c) */
static void menu_names_restore(
	struct injector_map_header *header)
{
	unsigned char *made = NULL;
	long count;

	if (tag_injector_menu.names_made)
		free(tag_injector_menu.names_made);
	tag_injector_menu.names_made = NULL;
	count = tag_names_restore(header, tag_injector_menu.map_name, &made);
	tag_injector_menu.names_made = made;
	if (count == NONE)
		error(_error_silent, "tag injector: no memory to name the tags of %s that have no names", tag_injector_menu.path);
	else if (count)
		error(_error_silent, "tag injector: %ld tags of %s have no names of their own (a protected map): they are given "
			"names made from what they are", count, tag_injector_menu.path);
}

/* the map's file in the Custom Edition maps folders, into path */
static boolean menu_map_path(
	short map,
	char *path)
{
	char const *level_name;

	if (map < 0 || map >= tag_injector_menu.map_count)
		return FALSE;
	level_name = custom_edition_maps_level_name(tag_injector_menu.maps[map].display_index);
	return level_name && custom_edition_map_file_path(level_name, path);
}

/* writes a bundle into a growing buffer (tag_bundle_write's) */
struct bundle_buffer
{
	byte *bytes;
	unsigned long size;
	unsigned long capacity;
};

static boolean bundle_buffer_write(
	void *context,
	void const *bytes,
	unsigned long size)
{
	struct bundle_buffer *buffer = context;

	if (size > 0x7FFFFFFFUL - buffer->size)
		return FALSE;
	if (buffer->size + size > buffer->capacity)
	{
		unsigned long capacity = buffer->capacity ? buffer->capacity : 0x100000;
		byte *grown;

		while (capacity < buffer->size + size)
		{
			if (capacity > 0x3FFFFFFFUL)
				return FALSE;
			capacity *= 2;
		}
		grown = buffer->bytes ? realloc(buffer->bytes, capacity) : malloc(capacity);
		if (!grown)
			return FALSE;
		buffer->bytes = grown;
		buffer->capacity = capacity;
	}
	memcpy(buffer->bytes + buffer->size, bytes, size);
	buffer->size += size;

	return TRUE;
}

/* ---------- the level's */

/* the bundle's tag of `bundle_index`: its group and path */
static unsigned long bundle_tag_group(
	struct tag_bundle_loaded const *loaded,
	long bundle_index)
{
	return read_unsigned_long(loaded->tags + bundle_index * TAG_BUNDLE_TAG_BYTES + TAG_BUNDLE_TAG_GROUP_OFFSET);
}

static char const *bundle_tag_name(
	struct tag_bundle_loaded const *loaded,
	long bundle_index)
{
	return loaded->names +
		read_unsigned_long(loaded->tags + bundle_index * TAG_BUNDLE_TAG_BYTES + TAG_BUNDLE_TAG_NAME_OFFSET);
}

static byte *bundle_tag_root(
	struct tag_bundle_loaded const *loaded,
	long bundle_index)
{
	return loaded->data +
		read_unsigned_long(loaded->tags + bundle_index * TAG_BUNDLE_TAG_BYTES + TAG_BUNDLE_TAG_ROOT_OFFSET);
}

/* the level's tag of a group and path, the bundles added before this
one's too, or NONE */
static long level_tag(
	unsigned long group_tag,
	char const *name)
{
	return name && name[0] ? tag_loaded((long)played_group(group_tag), name) : NONE;
}

/* the tags the level does not keep from another map: a scenario, a
structure, the globals (randomizer.c's) */
static boolean group_left_out(
	unsigned long group_tag)
{
	return group_tag == 'scnr' || group_tag == 'sbsp' || group_tag == 'matg';
}

/* whether a bundle's tag will be the level's of its path, as it reads (a
replacing bundle's tags are its own) */
static boolean bundle_tag_kept(
	struct injected_bundle const *bundle,
	unsigned long group_tag,
	char const *name)
{
	return !bundle->replace && level_tag(group_tag, name) != NONE;
}

/* whether every bitmap of a bundle's bitmap tags can be drawn from Halo
PC's pixels */
static boolean bundle_bitmaps_drawable(
	struct injected_bundle *bundle)
{
	long bundle_index;

	for (bundle_index = 0; bundle_index < bundle->loaded.tag_count; bundle_index++)
	{
		struct bitmap_group *group;
		long bitmap_index;

		if (bundle_tag_group(&bundle->loaded, bundle_index) != BITMAP_GROUP_TAG ||
			bundle_tag_kept(bundle, BITMAP_GROUP_TAG, bundle_tag_name(&bundle->loaded, bundle_index)))
			continue;
		group = (struct bitmap_group *)bundle_tag_root(&bundle->loaded, bundle_index);
		for (bitmap_index = 0; bitmap_index < group->bitmaps.count; bitmap_index++)
		{
			struct bitmap_data *bitmap = TAG_BLOCK_GET_ELEMENT(&group->bitmaps, bitmap_index, struct bitmap_data);

			if (!custom_edition_bitmap_drawable(bitmap))
			{
				error(_error_silent, "tag injector: bitmap %ld of '%s' cannot be drawn", bitmap_index,
					bundle_tag_name(&bundle->loaded, bundle_index));
				return FALSE;
			}
		}
	}

	return TRUE;
}

/* whether a player can hold a weapon: its first-person view is its
first-person model moved by its first-person animations, which the game
reads without asking whether it has them (first_person_weapons.c; a debug
build stops on the tag it does not have). A map's weapon without them is
one it never let a player hold */
static boolean weapon_holdable(
	struct weapon_definition const *weapon)
{
	return weapon && weapon->weapon.interface_definition.first_person_model.index != NONE &&
		weapon->weapon.interface_definition.first_person_animations.index != NONE;
}

/* whether a bundle's tag is a weapon it would add that a player could not
hold */
static boolean bundle_weapon_unholdable(
	struct injected_bundle const *bundle,
	long bundle_index)
{
	return bundle_tag_group(&bundle->loaded, bundle_index) == WEAPON_DEFINITION_TAG &&
		!bundle_tag_kept(bundle, WEAPON_DEFINITION_TAG, bundle_tag_name(&bundle->loaded, bundle_index)) &&
		!weapon_holdable((struct weapon_definition const *)bundle_tag_root(&bundle->loaded, bundle_index));
}

/* whether every weapon a bundle would add can be held (a bundle's
references name its own tags, or none); an AI's that cannot are left out
of it instead (drops_weapons), the AI added without them */
static boolean bundle_weapons_holdable(
	struct injected_bundle *bundle)
{
	long bundle_index;

	bundle->drops_weapons = FALSE;
	for (bundle_index = 0; bundle_index < bundle->loaded.tag_count; bundle_index++)
	{
		if (!bundle_weapon_unholdable(bundle, bundle_index))
			continue;
		if (bundle_index > 0 && bundle_tag_group(&bundle->loaded, 0) == 'actv')
		{
			error(_error_silent, "tag injector: the weapon '%s' has no first-person model or animations: %s is added "
				"without it", bundle_tag_name(&bundle->loaded, bundle_index), bundle_tag_name(&bundle->loaded, 0));
			bundle->drops_weapons = TRUE;
			continue;
		}
		error(_error_silent, "tag injector: the weapon '%s' has no first-person model or animations: it cannot be "
			"held", bundle_tag_name(&bundle->loaded, bundle_index));
		return FALSE;
	}

	return TRUE;
}

/* reads `size` bytes at `offset` in a bundle's file data, from its file */
static boolean bundle_file_read(
	void *context,
	unsigned long offset,
	unsigned long size,
	void *buffer)
{
	struct injected_bundle const *bundle = context;

	return offset <= bundle->loaded.file_data_size && size <= bundle->loaded.file_data_size - offset &&
		port_tag_profiles_bundle_read_at(bundle->handle, bundle->loaded.file_data_offset + offset, buffer, size);
}

/* a bundle's gbxmodels the level has not got made this build's models;
FALSE when one cannot be */
static boolean bundle_models_convert(
	struct injected_bundle *bundle)
{
	long bundle_index;

	for (bundle_index = 0; bundle_index < bundle->loaded.tag_count; bundle_index++)
	{
		char const *name = bundle_tag_name(&bundle->loaded, bundle_index);
		byte *entry = (byte *)bundle->loaded.tags + bundle_index * TAG_BUNDLE_TAG_BYTES;

		if (bundle_tag_group(&bundle->loaded, bundle_index) != GBXMODEL_GROUP_TAG ||
			bundle_tag_kept(bundle, MODEL_GROUP_TAG, name))
			continue;
		if (!custom_edition_injected_model_convert((struct model *)bundle_tag_root(&bundle->loaded, bundle_index), name,
			bundle->loaded.file_data_size, bundle_file_read, bundle))
		{
			return FALSE;
		}
		/* (a model now, as the level's are: its schema the Xbox's) */
		memcpy(entry + TAG_BUNDLE_TAG_GROUP_OFFSET, &(unsigned long){ MODEL_GROUP_TAG }, sizeof(unsigned long));
	}

	return TRUE;
}

/* the level's tag for a bundle's tag index, or NONE */
static long bundle_datum(
	struct injected_bundle const *bundle,
	long bundle_index)
{
	return bundle_index >= 0 && bundle_index < bundle->loaded.tag_count ? bundle->datums[bundle_index] : NONE;
}

/* gives each reference and tag index of an element of a tag added the
level's tag it names */
static void retarget_fields(
	struct injected_bundle const *bundle,
	byte *element,
	struct tag_schema_definition const *definition,
	short depth)
{
	struct tag_schema_field const *field;

	if (depth >= MAXIMUM_WALK_DEPTH || !element || !definition)
		return;
	for (field = definition->fields; field->type != _tag_schema_terminator; field++)
	{
		short item;

		for (item = 0; item < field->count; item++)
		{
			byte *place = element + field->offset + item * field->size;

			switch (field->type)
			{
			case _tag_schema_reference:
			{
				struct tag_reference *reference = (struct tag_reference *)place;
				long datum = bundle_datum(bundle, reference->index);

				reference->index = datum;
				reference->group_tag = played_group(reference->group_tag);
				if (datum == NONE)
				{
					/* (the bundle's names end with an empty one) */
					reference->name = (char *)bundle->loaded.names + bundle->loaded.names_size - 1;
					reference->name_length = 0;
				}
				else
				{
					reference->name = tag_get_name(datum);
					reference->name_length = reference->name ? (long)strlen(reference->name) : 0;
				}
				break;
			}

			case _tag_schema_tag_index:
				*(long *)place = bundle_datum(bundle, *(long *)place);
				break;

			case _tag_schema_struct:
				retarget_fields(bundle, place, field->definition, (short)(depth + 1));
				break;

			case _tag_schema_block:
			{
				struct tag_block *block = (struct tag_block *)place;
				struct tag_schema_definition const *element = field->definition;
				long index;

				for (index = 0; element && block->address && index < block->count; index++)
				{
					retarget_fields(bundle, (byte *)block->address + index * element->size, element,
						(short)(depth + 1));
				}
				break;
			}

			default:
				break;
			}
		}
	}
}

/* a sound added: the tag its permutations' samples are read for (the sound
cache's cache_tag_index), which the bundle still has as its map's */
static void sound_permutations_retarget(
	struct sound_definition *sound,
	long datum)
{
	long range_index;

	for (range_index = 0; range_index < sound->pitch_ranges.count; range_index++)
	{
		struct sound_pitch_range *range = TAG_BLOCK_GET_ELEMENT(&sound->pitch_ranges, range_index,
			struct sound_pitch_range);
		long permutation_index;

		for (permutation_index = 0; permutation_index < range->permutations.count; permutation_index++)
		{
			struct sound_permutation *permutation = TAG_BLOCK_GET_ELEMENT(&range->permutations, permutation_index,
				struct sound_permutation);

			permutation->unknown2 = (unsigned long)datum;
			permutation->unknown3 = (unsigned long)datum;
		}
	}
}

/* the stock HUD of a setting's value, or NONE */
static short stock_hud_find(
	char const *value)
{
	short hud;

	for (hud = 0; hud < (short)NUMBEROF(stock_huds); hud++)
	{
		if (!strcmp(stock_huds[hud].value, value))
			return hud;
	}

	return NONE;
}

/* the stock HUD a bundle's weapon is drawn with, as the "hud" setting of
the profile's tag of that weapon says (whichever bundle has it: an actor
variant's carries its weapons), or NONE for its own (a weapon the profile
has no tag of keeps it). AUTO's is by the words of its path, else a weapon
that zooms far the sniper rifle's, one that zooms the pistol's, one without
a magazine (a plasma weapon's battery or heat) the plasma rifle's, and one
with the assault rifle's */
static short weapon_stock_hud(
	struct injected_bundle const *bundle,
	long bundle_index)
{
	long profile = tag_injector_level.read_profile;
	long weapon_tag = port_tag_profiles_tag_find(profile, WEAPON_DEFINITION_TAG,
		bundle_tag_name(&bundle->loaded, bundle_index));
	char const *setting = weapon_tag == NONE ? "" : port_tag_profiles_tag_option(profile, weapon_tag, "hud");
	struct weapon_definition const *weapon;
	char path[256];
	long index;

	if (!setting[0])
		return NONE;
	if (strcmp(setting, "auto"))
		return stock_hud_find(setting);
	snprintf(path, sizeof(path), "%s", bundle_tag_name(&bundle->loaded, bundle_index));
	for (index = 0; path[index]; index++)
	{
		path[index] = path[index] == '_' ? ' ' :
			path[index] >= 'A' && path[index] <= 'Z' ? (char)(path[index] - 'A' + 'a') : path[index];
	}
	for (index = 0; index < (long)NUMBEROF(stock_hud_words); index++)
	{
		if (strstr(path, stock_hud_words[index].word))
			return stock_hud_find(stock_hud_words[index].value);
	}
	weapon = (struct weapon_definition const *)bundle_tag_root(&bundle->loaded, bundle_index);
	if (weapon->weapon.zoom_level_count > 0)
	{
		return stock_hud_find(weapon->weapon.zoom_magnification_maximum >= STOCK_HUD_SNIPER_ZOOM ?
			"sniper_rifle" : "pistol");
	}

	return stock_hud_find(weapon->weapon.magazines.count ? "assault_rifle" : "plasma_rifle");
}

/* the largest an added bitmap with mipmaps is drawn at (game.tag_injection_textures),
or 0 for as it is */
static short injected_bitmap_limit(
	void)
{
	char const *setting = config_string("game.tag_injection_textures");

	if (!_stricmp(setting, "full"))
		return 0;
	if (!_stricmp(setting, "small"))
		return SMALL_BITMAP_LIMIT;
	return (unsigned long)HALO_PORT_TEXTURE_CACHE_SIZE < AUTO_SMALL_CACHE ? SMALL_BITMAP_LIMIT : 0;
}

/* A bitmap added drawn from its mipmaps no larger than `limit`: its first
levels left out (Halo PC lays them out one after another, a cube map's six
faces in each, so the pixels start past them), its size theirs. The bytes
it no longer draws, else 0 (none to leave out, or it is linear and has
none) */
static unsigned long bitmap_downsize(
	struct bitmap_data *bitmap,
	short limit)
{
	unsigned long skipped = 0;
	short drop = 0;
	short level;

	if (!limit || TEST_FLAG(bitmap->flags, BITMAP_LINEAR_BIT) || bitmap->mipmap_count <= 0)
		return 0;
	while (drop < bitmap->mipmap_count && MAX(bitmap->width >> drop, bitmap->height >> drop) > limit)
		drop++;
	for (level = 0; level < drop; level++)
		skipped += (unsigned long)bitmap_mipmap_get_pixel_data_size(bitmap, level);
	if (!drop || skipped >= (unsigned long)bitmap->pixels_size)
		return 0;
	bitmap->pixels_offset += (long)skipped;
	bitmap->pixels_size -= (long)skipped;
	bitmap->width = (short)MAX(bitmap->width >> drop, 1);
	bitmap->height = (short)MAX(bitmap->height >> drop, 1);
	if (bitmap->type == BITMAP_TYPE_3D)
		bitmap->depth = (short)MAX(bitmap->depth >> drop, 1);
	bitmap->registration_point.x = (short)(bitmap->registration_point.x >> drop);
	bitmap->registration_point.y = (short)(bitmap->registration_point.y >> drop);
	bitmap->mipmap_count = (short)(bitmap->mipmap_count - drop);

	return skipped;
}

/* a weapon added, whose stock HUD the level has, given it: its first-person
interface names it (its own stays among the tags added, not drawn) */
static void weapon_hud_swap(
	struct injected_bundle const *bundle,
	long bundle_index,
	struct weapon_definition *weapon)
{
	struct tag_reference *reference = &weapon->weapon.interface_definition.hud_interface;
	short hud = weapon_stock_hud(bundle, bundle_index);
	char const *name = bundle_tag_name(&bundle->loaded, bundle_index);
	long datum;

	if (hud == NONE)
		return;
	datum = tag_loaded(WEAPON_HUD_INTERFACE_GROUP_TAG, stock_huds[hud].tag);
	if (datum == NONE)
	{
		error(_error_silent, "tag injector: %s keeps its own HUD: the level has not got %s", name,
			stock_huds[hud].tag);
		return;
	}
	reference->group_tag = WEAPON_HUD_INTERFACE_GROUP_TAG;
	reference->index = datum;
	reference->name = tag_get_name(datum);
	reference->name_length = reference->name ? (long)strlen(reference->name) : 0;
	error(_error_silent, "tag injector: %s is drawn with the HUD %s", name, stock_huds[hud].tag);
}

/* whether the level's tag has been replaced by a bundle's (tags_load) */
static boolean tag_replaced(
	long datum)
{
	long index;

	for (index = 0; datum != NONE && index < tag_injector_level.bundle_count; index++)
	{
		if (tag_injector_level.bundles[index].replaced == datum)
			return TRUE;
	}

	return FALSE;
}

static void level_bundles_free(
	void)
{
	long index;

	for (index = 0; index < tag_injector_level.bundle_count; index++)
	{
		struct injected_bundle *bundle = &tag_injector_level.bundles[index];

		if (bundle->bytes)
			free(bundle->bytes);
		if (bundle->datums)
			free(bundle->datums);
		port_tag_profiles_bundle_close(bundle->handle);
	}
	memset(tag_injector_level.bundles, 0, sizeof(tag_injector_level.bundles));
	tag_injector_level.bundle_count = 0;
}

/* opens a profile's bundle and reads and checks its tags; FALSE after
logging why (what it holds is the caller's to let go: level_bundles_free) */
static boolean level_bundle_read(
	long profile,
	long tag,
	struct injected_bundle *bundle,
	unsigned long *total)
{
	enum tag_bundle_status status;
	char const *name = port_tag_profiles_tag_name(profile, tag);
	byte header[TAG_BUNDLE_HEADER_BYTES];
	unsigned long tags_size;

	memset(bundle, 0, sizeof(*bundle));
	bundle->handle = port_tag_profiles_bundle_open(profile, tag, &bundle->size);
	if (bundle->handle == NONE)
	{
		error(_error_silent, "tag injector: the bundle of %s is missing", name);
		bundle->failure = "missing";
		return FALSE;
	}
	/* (its tags alone: all but its file data) */
	if (bundle->size < sizeof(header) || !port_tag_profiles_bundle_read_at(bundle->handle, 0, header, sizeof(header)))
	{
		error(_error_silent, "tag injector: the bundle of %s cannot be read", name);
		bundle->failure = "unreadable";
		return FALSE;
	}
	tags_size = tag_bundle_file_data_offset(header);
	if (tags_size < sizeof(header) || tags_size > bundle->size || tags_size > MAXIMUM_INJECTED_BYTES - *total)
	{
		error(_error_silent, "tag injector: %s is not a bundle, or its tags and those before it are past the %d MB "
			"a level adds", name, MAXIMUM_INJECTED_BYTES >> 20);
		bundle->failure = tags_size < sizeof(header) || tags_size > bundle->size ? "unreadable" : "too large";
		return FALSE;
	}
	bundle->bytes = malloc(tags_size);
	if (!bundle->bytes || !port_tag_profiles_bundle_read_at(bundle->handle, 0, bundle->bytes, tags_size))
	{
		error(_error_silent, "tag injector: the bundle of %s cannot be read", name);
		bundle->failure = "unreadable";
		return FALSE;
	}
	status = tag_bundle_load_tags(bundle->bytes, tags_size, bundle->size, &bundle->loaded);
	if (status != _tag_bundle_status_ok)
	{
		error(_error_silent, "tag injector: the bundle of %s is not one this build can play: %s (its tag %ld)", name,
			tag_bundle_status_describe(status), bundle->loaded.problem_tag_index);
		bundle->failure = "unplayable";
		return FALSE;
	}
	bundle->datums = malloc((unsigned long)bundle->loaded.tag_count * sizeof(long));
	if (!bundle->datums)
	{
		bundle->failure = "no memory";
		return FALSE;
	}
	*total += tags_size;

	return TRUE;
}

/* whether a profile's tag could not be stored (tag_injector_profile_store_next),
by its profile's folder and its path, and with `failed` that it could not:
it is not tried again until the game starts again */
static boolean tag_injector_store_failed(
	long profile,
	long tag,
	boolean failed)
{
	static char failures[32][320];
	static long failure_count = 0;
	char key[320];
	long index;

	snprintf(key, sizeof(key), "%s\\%s", port_tag_profiles_folder(profile), port_tag_profiles_tag_name(profile, tag));
	for (index = 0; index < failure_count; index++)
	{
		if (!_stricmp(failures[index], key))
			return TRUE;
	}
	if (failed && failure_count < (long)NUMBEROF(failures))
		snprintf(failures[failure_count++], sizeof(failures[0]), "%s", key);

	return failed;
}

/* adds a bundle to a profile, kept as port_tag_store.c keeps it: its file
data's regions found by loading a copy of its tags; FALSE after logging why */
static boolean profile_bundle_add(
	long profile,
	char const *map,
	unsigned long group_tag,
	char const *name,
	byte const *bytes,
	unsigned long size)
{
	static struct tag_bundle_file_region const no_region = { 0, 0 };
	struct tag_bundle_loaded loaded;
	struct tag_bundle_file_region *regions = NULL;
	enum tag_bundle_status status;
	long region_count = 0;
	unsigned long tags_size;
	byte *copy;
	boolean added;

	tags_size = size >= TAG_BUNDLE_HEADER_BYTES ? tag_bundle_file_data_offset(bytes) : 0;
	if (tags_size < TAG_BUNDLE_HEADER_BYTES || tags_size > size)
	{
		error(_error_silent, "tag injector: the bundle of %s is not one", name);
		return FALSE;
	}
	/* (loading makes its pointers this process's, in place) */
	copy = malloc(tags_size);
	if (!copy)
		return FALSE;
	memcpy(copy, bytes, tags_size);
	status = tag_bundle_load_tags_regions(copy, tags_size, size, &loaded, &regions, &region_count);
	free(copy);
	if (status != _tag_bundle_status_ok)
	{
		error(_error_silent, "tag injector: the bundle of %s is not one this build can play: %s", name,
			tag_bundle_status_describe(status));
		return FALSE;
	}
	added = port_tag_profiles_tag_add(profile, map, group_tag, name, bytes, size, tags_size,
		regions ? regions : &no_region, region_count) != 0;
	if (regions)
		free(regions);

	return added;
}

/* ---------- the Tag Editor's (tag_editor.c) */

/* `size` bytes the level holds until its tags are let go (a copy's), or
NULL */
static void *level_allocation(
	unsigned long size)
{
	void *bytes;

	if (tag_injector_level.allocation_count == tag_injector_level.allocation_capacity)
	{
		long capacity = tag_injector_level.allocation_capacity ? 2 * tag_injector_level.allocation_capacity : 64;
		void **grown = realloc(tag_injector_level.allocations, (unsigned long)capacity * sizeof(*grown));

		if (!grown)
			return NULL;
		tag_injector_level.allocations = grown;
		tag_injector_level.allocation_capacity = capacity;
	}
	bytes = malloc(size ? size : 1);
	if (bytes)
		tag_injector_level.allocations[tag_injector_level.allocation_count++] = bytes;
	return bytes;
}

/* each block of an element of a copy given its own elements (and theirs),
so that the copy's can be changed without its original's; FALSE when there
is no memory for one */
static boolean copy_blocks(
	byte *element,
	struct tag_schema_definition const *definition,
	short depth)
{
	struct tag_schema_field const *field;

	if (depth >= MAXIMUM_WALK_DEPTH || !element || !definition)
		return TRUE;
	for (field = definition->fields; field->type != _tag_schema_terminator; field++)
	{
		short item;

		for (item = 0; item < field->count; item++)
		{
			byte *place = element + field->offset + item * field->size;

			if (field->type == _tag_schema_struct && !copy_blocks(place, field->definition, (short)(depth + 1)))
				return FALSE;
			if (field->type == _tag_schema_block)
			{
				struct tag_block *block = (struct tag_block *)place;
				struct tag_schema_definition const *elements = field->definition;
				byte *copy;
				long index;

				if (!elements || !block->address || block->count <= 0)
					continue;
				copy = level_allocation((unsigned long)block->count * (unsigned long)elements->size);
				if (!copy)
					return FALSE;
				memcpy(copy, block->address, (unsigned long)block->count * (unsigned long)elements->size);
				block->address = copy;
				for (index = 0; index < block->count; index++)
				{
					if (!copy_blocks(copy + index * elements->size, elements, (short)(depth + 1)))
						return FALSE;
				}
			}
		}
	}

	return TRUE;
}

long tag_injector_tag_copy(
	long datum,
	char const *name,
	boolean blocks)
{
	struct injector_tag_instance *instance;
	struct injector_tag_instance *grown;
	struct tag_schema_definition const *definition;
	long count = tag_injector_level.first_injected + tag_injector_level.injected_count +
		tag_injector_level.copied_count;
	long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(datum);
	long existing;
	char *copy_name;
	byte *root;

	if (!tag_injector_level.grown_instances || datum == NONE || absolute < 0 || absolute >= count || !name)
		return NONE;
	instance = &tag_injector_level.grown_instances[absolute];
	/* (one made already, by a weapon of the same edits) */
	existing = tag_loaded(instance->group_tag, name);
	if (existing != NONE)
		return existing;
	definition = schema_of((unsigned long)instance->group_tag);
	if (!definition || !instance->base_address || count >= MAXIMUM_TAG_COUNT)
		return NONE;
	copy_name = level_allocation((unsigned long)strlen(name) + 1);
	root = level_allocation((unsigned long)definition->size);
	grown = realloc(tag_injector_level.grown_instances, (unsigned long)(count + 1) * sizeof(*grown));
	if (!copy_name || !root || !grown)
		return NONE;
	tag_injector_level.grown_instances = grown;
	instance = &grown[absolute];
	strcpy(copy_name, name);
	memcpy(root, instance->base_address, (unsigned long)definition->size);
	if (blocks && !copy_blocks(root, definition, 0))
		return NONE;
	grown[count] = *instance;
	grown[count].tag_index = (tag_injector_level.salt << 16) | count;
	grown[count].name = copy_name;
	grown[count].base_address = root;
	tag_injector_level.copied_count++;
	cache_files_set_tag_instances(grown, count + 1);

	return grown[count].tag_index;
}

/* The profile's weapons a level has from its bundles (added, or replacing
the level's) given their edits (tag_editor.c), each once, in the bundles'
order: the same on every machine of a network game. */
static void tag_injector_weapons_edit(
	long profile,
	long existing)
{
	long bundle_index;
	long *done;
	long done_count = 0;

	done = malloc((unsigned long)(tag_injector_level.injected_count + tag_injector_level.bundle_count + 1) *
		sizeof(long));
	if (!done)
		return;
	for (bundle_index = 0; bundle_index < tag_injector_level.bundle_count; bundle_index++)
	{
		struct injected_bundle const *bundle = &tag_injector_level.bundles[bundle_index];
		long tag;

		for (tag = 0; !bundle->refused && tag < bundle->loaded.tag_count; tag++)
		{
			long datum = bundle->datums[tag];
			long index;

			if (bundle_tag_group(&bundle->loaded, tag) != WEAPON_DEFINITION_TAG || datum == NONE ||
				(DATUM_INDEX_TO_ABSOLUTE_INDEX(datum) < existing && datum != bundle->replaced))
				continue;
			for (index = 0; index < done_count && done[index] != datum; index++)
			{
			}
			if (index < done_count)
				continue;
			done[done_count++] = datum;
			tag_editor_weapon_loaded(profile, datum);
		}
	}
	free(done);
}

/* ---------- public code */

boolean tag_injector_maps_available(
	char *reason,
	long reason_size)
{
	if (!halo_custom_edition_tag_cache())
	{
		reason_set(reason, reason_size, "Custom Edition maps are off (game.custom_edition in config.toml), or cannot run here.");
		return FALSE;
	}
	if (custom_edition_cache_tags_loaded())
	{
		reason_set(reason, reason_size, "A Custom Edition map is being played.");
		return FALSE;
	}
	reason_set(reason, reason_size, "");

	return TRUE;
}

short tag_injector_maps_find(
	void)
{
	short pass;

	custom_edition_maps_look_again();
	tag_injector_menu.map_count = 0;
	for (pass = 0; pass < 2; pass++)
	{
		boolean campaign = pass == 0;
		short count = custom_edition_maps_count(campaign);
		short index;

		for (index = 0; index < count && tag_injector_menu.map_count < MAXIMUM_TAG_INJECTOR_MAPS; index++)
		{
			struct injector_map *map = &tag_injector_menu.maps[tag_injector_menu.map_count];

			map->display_index = custom_edition_maps_display_index_of(campaign, index);
			map->campaign = campaign;
			/* (Halo PC's own multiplayer maps are Custom Edition maps too) */
			if (map->display_index != NONE && custom_edition_maps_level_name(map->display_index))
				tag_injector_menu.map_count++;
		}
	}

	return tag_injector_menu.map_count;
}

short tag_injector_map_count(
	void)
{
	return tag_injector_menu.map_count;
}

wchar_t const *tag_injector_map_name(
	short map)
{
	wchar_t const *name = map >= 0 && map < tag_injector_menu.map_count ?
		custom_edition_maps_name(tag_injector_menu.maps[map].display_index) : NULL;

	return name ? name : L"";
}

char const *tag_injector_map_folder(
	short map)
{
	struct injector_map const *found;
	short count;
	short index;

	if (map < 0 || map >= tag_injector_menu.map_count)
		return "";
	found = &tag_injector_menu.maps[map];
	count = custom_edition_maps_count(found->campaign);
	for (index = 0; index < count; index++)
	{
		if (custom_edition_maps_display_index_of(found->campaign, index) == found->display_index)
		{
			char const *category = custom_edition_maps_category(found->campaign, index);

			return category ? category : "";
		}
	}

	return "";
}

short tag_injector_map_of(
	boolean campaign,
	short index)
{
	short display_index = custom_edition_maps_display_index_of(campaign, index);
	short map;

	for (map = 0; display_index != NONE && map < tag_injector_menu.map_count; map++)
	{
		if (tag_injector_menu.maps[map].display_index == display_index && tag_injector_menu.maps[map].campaign == campaign)
			return map;
	}

	return NONE;
}

boolean tag_injector_map_campaign(
	short map)
{
	return map >= 0 && map < tag_injector_menu.map_count && tag_injector_menu.maps[map].campaign;
}

boolean tag_injector_map_open(
	short map)
{
	struct injector_map_header *header;
	char const *level_name;
	short category;
	long absolute;

	tag_injector_map_close();
	if (!menu_map_path(map, tag_injector_menu.path))
	{
		error(_error_silent, "tag injector: map %d has no file", map);
		return FALSE;
	}
	header = custom_edition_cache_bundle_open(tag_injector_menu.path, NULL, 0);
	if (!header)
		return FALSE;
	tag_injector_menu.header = header;
	tag_injector_menu.opened = map;
	level_name = custom_edition_maps_level_name(tag_injector_menu.maps[map].display_index);
	snprintf(tag_injector_menu.map_name, sizeof(tag_injector_menu.map_name), "%s", tag_name_strip_path(level_name));
	menu_names_restore(header);
	for (category = 0; category < NUMBER_OF_TAG_INJECTOR_CATEGORIES; category++)
	{
		long count = 0;

		for (absolute = 0; absolute < header->tag_count; absolute++)
			count += (unsigned long)header->instances[absolute].group_tag == tag_injector_categories[category].group_tag;
		tag_injector_menu.tags[category] = malloc((unsigned long)(count + 1) * sizeof(long));
		tag_injector_menu.sizes[category] = malloc((unsigned long)(count + 1) * sizeof(long));
		tag_injector_menu.problems[category] = malloc((unsigned long)(count + 1) * sizeof(long));
		if (!tag_injector_menu.tags[category] || !tag_injector_menu.sizes[category] ||
			!tag_injector_menu.problems[category])
		{
			tag_injector_map_close();
			return FALSE;
		}
		for (absolute = 0; absolute < header->tag_count; absolute++)
		{
			if ((unsigned long)header->instances[absolute].group_tag == tag_injector_categories[category].group_tag)
			{
				long index = tag_injector_menu.tag_counts[category]++;

				tag_injector_menu.tags[category][index] = absolute;
				tag_injector_menu.sizes[category][index] = TAG_SIZE_UNKNOWN;
				tag_injector_menu.problems[category][index] = NONE;
			}
		}
		qsort(tag_injector_menu.tags[category], (size_t)tag_injector_menu.tag_counts[category], sizeof(long),
			menu_tag_less);
	}
	error(_error_silent, "tag injector: %s open: %ld weapons, %ld actor variants", tag_injector_menu.path,
		tag_injector_menu.tag_counts[_tag_injector_weapons], tag_injector_menu.tag_counts[_tag_injector_ai]);

	return TRUE;
}

void tag_injector_map_close(
	void)
{
	menu_tags_free();
	if (tag_injector_menu.opened != NONE)
		custom_edition_cache_bundle_close();
	tag_names_dispose();
	if (tag_injector_menu.names_made)
		free(tag_injector_menu.names_made);
	tag_injector_menu.names_made = NULL;
	tag_injector_menu.opened = NONE;
	tag_injector_menu.header = NULL;
	tag_injector_menu.path[0] = 0;
	tag_injector_menu.map_name[0] = 0;
}

short tag_injector_map_opened(
	void)
{
	return tag_injector_menu.opened;
}

char const *tag_injector_category_name(
	short category)
{
	return category >= 0 && category < NUMBER_OF_TAG_INJECTOR_CATEGORIES ? tag_injector_categories[category].name : "";
}

boolean tag_injector_category_addable(
	short category)
{
	return category >= 0 && category < NUMBER_OF_TAG_INJECTOR_CATEGORIES && tag_injector_categories[category].addable;
}

unsigned long tag_injector_category_group(
	short category)
{
	return category >= 0 && category < NUMBER_OF_TAG_INJECTOR_CATEGORIES ? tag_injector_categories[category].group_tag : 0;
}

long tag_injector_category_count(
	short category)
{
	return tag_injector_menu.opened != NONE && category >= 0 && category < NUMBER_OF_TAG_INJECTOR_CATEGORIES ?
		tag_injector_menu.tag_counts[category] : 0;
}

char const *tag_injector_category_tag(
	short category,
	long index)
{
	long absolute = menu_tag(category, index);

	return absolute == NONE ? "" : menu_tag_name(absolute);
}

boolean tag_injector_tag_name_made(
	short category,
	long index)
{
	long absolute = menu_tag(category, index);

	return absolute != NONE && tag_injector_menu.names_made && tag_injector_menu.names_made[absolute];
}

long tag_injector_tag_size(
	short category,
	long index,
	char *reason,
	long reason_size)
{
	long absolute = menu_tag(category, index);
	long *size;

	reason_set(reason, reason_size, "");
	if (absolute == NONE)
		return NONE;
	size = &tag_injector_menu.sizes[category][index];
	if (*size == TAG_SIZE_UNKNOWN)
	{
		struct tag_bundle_source source;
		long *indices = NULL;
		long count = 0;
		long problem = NONE;
		enum tag_bundle_status status;

		custom_edition_cache_bundle_source(&source);
		status = tag_bundle_tags(&source, absolute, &indices, &count, &problem);
		/* (the bundle's tags were taken from the game's malloc) */
		if (indices)
			free(indices);
		*size = status == _tag_bundle_status_ok ? count : NONE;
		tag_injector_menu.problems[category][index] = problem;
		if (status != _tag_bundle_status_ok)
			error(_error_silent, "tag injector: %s cannot be taken out: %s (%s)", menu_tag_name(absolute),
				tag_bundle_status_describe(status), menu_tag_name(problem));
	}
	if (*size == NONE)
	{
		char text[320];

		snprintf(text, sizeof(text), "It names a tag this build cannot take out: %s",
			menu_tag_name(tag_injector_menu.problems[category][index]));
		reason_set(reason, reason_size, text);
	}
	else if (category == _tag_injector_weapons &&
		!weapon_holdable(tag_injector_menu.header->instances[absolute].base_address))
	{
		reason_set(reason, reason_size, "It has no first-person model or animations: its map never lets a player hold "
			"it, and it cannot be added.");
		return NONE;
	}

	return *size;
}

boolean tag_injector_tag_stock(
	long profile,
	long tag)
{
	unsigned long group_tag = port_tag_profiles_tag_group(profile, tag);

	return (group_tag == WEAPON_DEFINITION_TAG || group_tag == ACTOR_VARIANT_GROUP_TAG ||
		group_tag == EQUIPMENT_GROUP_TAG) &&
		randomizer_stock_tag_exists((long)group_tag, port_tag_profiles_tag_name(profile, tag));
}

long tag_injector_tag_in_profile(
	long profile,
	short category,
	long index)
{
	long absolute = menu_tag(category, index);

	return absolute == NONE ? NONE :
		port_tag_profiles_tag_find(profile, tag_injector_categories[category].group_tag, menu_tag_name(absolute));
}

boolean tag_injector_tag_add(
	long profile,
	short category,
	long index,
	long *carried,
	char *reason,
	long reason_size)
{
	long absolute = menu_tag(category, index);
	struct injector_map_header *header;
	struct tag_bundle_source source;
	struct tag_bundle_report report;
	struct bundle_buffer buffer = { NULL, 0, 0 };
	char name[256];
	byte *sounds;
	long *indices = NULL;
	long count = 0;
	long problem = NONE;
	long tag_count;
	long tag;
	boolean added;

	reason_set(reason, reason_size, "");
	if (carried)
		*carried = 0;
	if (absolute == NONE || !tag_injector_category_addable(category))
		return FALSE;
	if (profile < 0 || profile >= port_tag_profiles_count())
	{
		reason_set(reason, reason_size, "Make a profile first.");
		return FALSE;
	}
	if (tag_injector_tag_size(category, index, reason, reason_size) == NONE)
		return FALSE;
	snprintf(name, sizeof(name), "%s", menu_tag_name(absolute));
	tag_count = tag_injector_menu.header->tag_count;

	/* the map again, with the sounds of the tags the bundle holds decoded
	(the tags are the same, at the same indices) */
	custom_edition_cache_bundle_source(&source);
	tag_bundle_tags(&source, absolute, &indices, &count, &problem);
	sounds = malloc((unsigned long)tag_count);
	if (!sounds || !indices)
	{
		if (sounds)
			free(sounds);
		if (indices)
			free(indices);
		reason_set(reason, reason_size, "Out of memory.");
		return FALSE;
	}
	memset(sounds, 0, (unsigned long)tag_count);
	for (tag = 0; tag < count; tag++)
	{
		if ((unsigned long)tag_injector_menu.header->instances[indices[tag]].group_tag == SOUND_DEFINITION_TAG)
			sounds[indices[tag]] = TRUE;
	}
	header = custom_edition_cache_bundle_open(tag_injector_menu.path, sounds, tag_count);
	free(sounds);
	tag_injector_menu.header = header;
	/* (its names made again: the same) */
	if (header)
		menu_names_restore(header);
	if (!header || header->tag_count != tag_count ||
		(unsigned long)header->instances[absolute].group_tag != tag_injector_categories[category].group_tag ||
		_stricmp(menu_tag_name(absolute), name))
	{
		/* (the file changed under the menu: what it lists is gone) */
		free(indices);
		reason_set(reason, reason_size, "The map cannot be read again.");
		tag_injector_map_close();
		return FALSE;
	}

	custom_edition_cache_bundle_source(&source);
	tag_bundle_write(&source, absolute, bundle_buffer_write, &buffer, &report);
	added = report.status == _tag_bundle_status_ok &&
		profile_bundle_add(profile, tag_injector_menu.map_name, tag_injector_categories[category].group_tag,
			name, buffer.bytes, buffer.size);
	if (report.status != _tag_bundle_status_ok)
	{
		error(_error_silent, "tag injector: %s cannot be taken out: %s", name, tag_bundle_status_describe(report.status));
		reason_set(reason, reason_size, tag_bundle_status_describe(report.status));
	}
	else if (!added)
	{
		reason_set(reason, reason_size, "The profile cannot be written (the log says why).");
	}
	else
	{
		error(_error_silent, "tag injector: took %s out of %s: %ld tags, %lu bytes", name, tag_injector_menu.map_name,
			report.tag_count, report.size);
	}
	if (buffer.bytes)
		free(buffer.bytes);

	/* an actor variant's weapons and equipment (those its bundle holds), each
	a tag of the profile of its own, unless it has one of that path */
	for (tag = 0; added && category == _tag_injector_ai && tag < count; tag++)
	{
		unsigned long group_tag = (unsigned long)header->instances[indices[tag]].group_tag;
		char const *carried_name = menu_tag_name(indices[tag]);
		struct bundle_buffer carried_buffer = { NULL, 0, 0 };

		if ((group_tag != WEAPON_DEFINITION_TAG && group_tag != EQUIPMENT_GROUP_TAG) || indices[tag] == absolute ||
			port_tag_profiles_tag_find(profile, group_tag, carried_name) != NONE)
			continue;
		if (group_tag == WEAPON_DEFINITION_TAG && !weapon_holdable(header->instances[indices[tag]].base_address))
		{
			error(_error_silent, "tag injector: %s, which %s carries, has no first-person model or animations, and is "
				"not added", carried_name, name);
			continue;
		}
		tag_bundle_write(&source, indices[tag], bundle_buffer_write, &carried_buffer, &report);
		if (report.status == _tag_bundle_status_ok && profile_bundle_add(profile, tag_injector_menu.map_name, group_tag,
			carried_name, carried_buffer.bytes, carried_buffer.size))
		{
			error(_error_silent, "tag injector: took %s out of %s with %s: %ld tags, %lu bytes", carried_name,
				tag_injector_menu.map_name, name, report.tag_count, report.size);
			if (carried)
				(*carried)++;
		}
		else
		{
			error(_error_silent, "tag injector: %s, which %s carries, cannot be taken out: %s", carried_name, name,
				report.status == _tag_bundle_status_ok ? "the profile cannot be written" :
				tag_bundle_status_describe(report.status));
		}
		if (carried_buffer.bytes)
			free(carried_buffer.bytes);
	}
	free(indices);

	return added;
}

/* ---------- the preview's (tag_preview.c) */

/* where a preview's tags are: the map open's (its tags by their tag
indices, its models' vertices and strips and its bitmaps' pixels in its
files), or a profile's bundle (its tags by the bundle's indices, the rest
in its file data) */
struct preview_source
{
	struct injected_bundle *bundle;
	struct tag_bundle_source map;
};

/* a preview's mesh as it is made: its source and model, the change colors
an object of it is drawn with (colored: of each, whether it has it), the
material of each shader of its model (PREVIEW_MATERIAL_UNMADE until a part
names it), and the bitmap tag each texture is of */
struct preview_build
{
	struct preview_source const *source;
	struct model const *model;
	boolean colored[PREVIEW_CHANGE_COLORS];
	float colors[PREVIEW_CHANGE_COLORS][3];
	short shader_materials[PREVIEW_MAXIMUM_SHADERS];
	long texture_tags[TAG_INJECTOR_MESH_MAXIMUM_TEXTURES];
};

/* what of a preview's tags' files is read: a gbxmodel part's vertices or
strip, or a bitmap's pixels */
enum
{
	_preview_vertices,
	_preview_strip,
	_preview_pixels,
};

/* the root of the tag a reference (or a tag index) names, and its group;
NULL for none */
static byte *preview_tag(
	struct preview_source const *source,
	long index,
	unsigned long *group_tag)
{
	if (source->bundle)
	{
		struct tag_bundle_loaded const *loaded = &source->bundle->loaded;

		if (index < 0 || index >= loaded->tag_count)
			return NULL;
		*group_tag = bundle_tag_group(loaded, index);
		return bundle_tag_root(loaded, index);
	}
	else
	{
		struct injector_map_header const *header = tag_injector_menu.header;
		long absolute = (long)(unsigned short)index;

		if (!header || index == NONE || absolute >= header->tag_count || header->instances[absolute].tag_index != index)
			return NULL;
		*group_tag = (unsigned long)header->instances[absolute].group_tag;
		return header->instances[absolute].base_address;
	}
}

/* reads `size` bytes of a gbxmodel part's vertices or strip, at its offset
of them, or of a bitmap's pixels, at its pixels' offset */
static boolean preview_read(
	struct preview_source const *source,
	short kind,
	unsigned long offset,
	unsigned long size,
	void *buffer)
{
	struct injector_map_header const *header = tag_injector_menu.header;
	unsigned long start;
	unsigned long end;

	if (source->bundle)
		return bundle_file_read(source->bundle, offset, size, buffer);
	if (!header || !source->map.read)
		return FALSE;
	if (kind == _preview_pixels)
		return source->map.read(source->map.context, offset, size, buffer);
	/* (the bounds of the vertices and of the strips, as tag_bundle.c's) */
	start = kind == _preview_strip ? header->model_index_data_offset : 0;
	end = kind == _preview_strip ? header->model_data_size : header->model_index_data_offset;
	if (start > end || end > header->model_data_size || offset > end - start || size > end - start - offset)
		return FALSE;
	return source->map.read(source->map.context, header->model_data_offset + start + offset, size, buffer);
}

/* the root of a model's shader, and its group; NULL for none */
static byte *preview_shader(
	struct preview_source const *source,
	struct model const *model,
	short shader_index,
	unsigned long *group_tag)
{
	struct preview_model_shader const *reference;

	if (shader_index < 0 || shader_index >= model->shaders.count)
		return NULL;
	reference = TAG_BLOCK_GET_ELEMENT(&model->shaders, shader_index, struct preview_model_shader);
	return preview_tag(source, reference->shader.index, group_tag);
}

/* whether a model's shader is opaque: a model's or an environment's (not
glass, nor a transparent one's effects) */
static boolean preview_shader_opaque(
	struct preview_source const *source,
	struct model const *model,
	short shader_index)
{
	unsigned long group_tag = 0;

	return preview_shader(source, model, shader_index, &group_tag) &&
		(group_tag == SHADER_MODEL_GROUP_TAG || group_tag == SHADER_ENVIRONMENT_GROUP_TAG);
}

/* the bytes of a mipmap level of a bitmap `width` by `height` of a format,
or 0 for a format the preview does not draw */
static unsigned long preview_level_size(
	short format,
	long width,
	long height)
{
	long blocks = MAX(1, (width + 3) / 4) * MAX(1, (height + 3) / 4);

	switch (format)
	{
	case _preview_bitmap_format_a8:
	case _preview_bitmap_format_y8:
	case _preview_bitmap_format_ay8:
		return (unsigned long)(width * height);
	case _preview_bitmap_format_a8y8:
	case _preview_bitmap_format_r5g6b5:
	case _preview_bitmap_format_a1r5g5b5:
	case _preview_bitmap_format_a4r4g4b4:
		return (unsigned long)(width * height * 2);
	case _preview_bitmap_format_x8r8g8b8:
	case _preview_bitmap_format_a8r8g8b8:
		return (unsigned long)(width * height * 4);
	case _preview_bitmap_format_dxt1:
		return (unsigned long)(blocks * 8);
	case _preview_bitmap_format_dxt3:
	case _preview_bitmap_format_dxt5:
		return (unsigned long)(blocks * 16);
	default:
		return 0;
	}
}

/* the change colors of the object of a tag (preview_build's): an actor
variant's own, the middle of each's bounds (where the game chooses at
random), and for those it has not, its unit's, the middle of each's first
permutation's (where the game chooses by the object's place) */
static void preview_change_colors(
	struct preview_source const *source,
	byte *root,
	unsigned long group_tag,
	struct preview_build *build)
{
	struct actor_variant_definition const *variant = NULL;
	struct object_definition const *object = (struct object_definition const *)root;
	short index;
	short channel;

	if (root && group_tag == ACTOR_VARIANT_GROUP_TAG)
	{
		variant = (struct actor_variant_definition const *)root;
		object = (struct object_definition const *)preview_tag(source, variant->unit_reference.index, &group_tag);
	}
	for (index = 0; object && index < MIN(object->object.change_colors.count, PREVIEW_CHANGE_COLORS); index++)
	{
		struct object_change_color_definition const *change_color = TAG_BLOCK_GET_ELEMENT(
			&object->object.change_colors, index, struct object_change_color_definition);
		struct object_change_color_permutation const *permutation;

		if (change_color->permutations.count <= 0)
			continue;
		permutation = TAG_BLOCK_GET_ELEMENT(&change_color->permutations, 0, struct object_change_color_permutation);
		build->colors[index][0] = (permutation->color_lower_bound.red + permutation->color_upper_bound.red) * 0.5f;
		build->colors[index][1] = (permutation->color_lower_bound.green + permutation->color_upper_bound.green) * 0.5f;
		build->colors[index][2] = (permutation->color_lower_bound.blue + permutation->color_upper_bound.blue) * 0.5f;
		build->colored[index] = TRUE;
	}
	for (index = 0; variant && index < MIN(variant->change_colors.count, PREVIEW_CHANGE_COLORS); index++)
	{
		struct preview_actor_variant_change_colors const *change_colors = TAG_BLOCK_GET_ELEMENT(
			&variant->change_colors, index, struct preview_actor_variant_change_colors);

		build->colors[index][0] = (change_colors->color_lower_bound.red + change_colors->color_upper_bound.red) * 0.5f;
		build->colors[index][1] = (change_colors->color_lower_bound.green + change_colors->color_upper_bound.green) *
			0.5f;
		build->colors[index][2] = (change_colors->color_lower_bound.blue + change_colors->color_upper_bound.blue) * 0.5f;
		build->colored[index] = TRUE;
	}
	for (index = 0; index < PREVIEW_CHANGE_COLORS; index++)
	{
		/* (a NaN fails every comparison) */
		for (channel = 0; channel < 3; channel++)
		{
			if (!(build->colors[index][channel] >= 0.0f))
				build->colors[index][channel] = 0.0f;
			build->colors[index][channel] = MIN(build->colors[index][channel], 1.0f);
		}
	}
}

/* The texture of a bitmap tag a shader names (its index in the mesh's
textures, read the first time one names it); NONE for none (no bitmap the
preview draws). */
static short preview_texture(
	struct preview_build *build,
	struct tag_reference const *reference,
	struct tag_injector_mesh *mesh)
{
	unsigned long group_tag = 0;
	struct bitmap_group const *group;
	struct bitmap_data const *bitmap;
	struct tag_injector_mesh_texture *texture;
	long width;
	long height;
	unsigned long offset = 0;
	unsigned long size;
	short level;
	short index;

	if (reference->index == NONE)
		return NONE;
	for (index = 0; index < mesh->texture_count; index++)
	{
		if (build->texture_tags[index] == reference->index)
			return index;
	}
	group = (struct bitmap_group const *)preview_tag(build->source, reference->index, &group_tag);
	if (!group || group_tag != BITMAP_GROUP_TAG || group->bitmaps.count <= 0 ||
		mesh->texture_count >= TAG_INJECTOR_MESH_MAXIMUM_TEXTURES)
	{
		return NONE;
	}
	bitmap = TAG_BLOCK_GET_ELEMENT(&group->bitmaps, 0, struct bitmap_data);
	width = bitmap->width;
	height = bitmap->height;
	if (bitmap->type != _preview_bitmap_type_2d || width <= 0 || height <= 0 ||
		width > PREVIEW_MAXIMUM_BITMAP_SIZE || height > PREVIEW_MAXIMUM_BITMAP_SIZE ||
		!preview_level_size(bitmap->format, width, height))
	{
		return NONE;
	}
	/* (its largest level no larger than the preview draws: Halo PC's
	levels one after another, the largest first) */
	for (level = 0; level < bitmap->mipmap_count && MAX(width, height) > PREVIEW_TEXTURE_SIZE; level++)
	{
		offset += preview_level_size(bitmap->format, width, height);
		width = MAX(1, width >> 1);
		height = MAX(1, height >> 1);
	}
	size = preview_level_size(bitmap->format, width, height);
	if (bitmap->pixels_size < 0 || offset > (unsigned long)bitmap->pixels_size ||
		size > (unsigned long)bitmap->pixels_size - offset)
	{
		return NONE;
	}
	texture = &mesh->textures[mesh->texture_count];
	texture->pixels = malloc(size);
	if (!texture->pixels)
		return NONE;
	if (!preview_read(build->source, _preview_pixels, (unsigned long)bitmap->pixels_offset + offset, size,
		texture->pixels))
	{
		free(texture->pixels);
		texture->pixels = NULL;
		return NONE;
	}
	texture->format = bitmap->format;
	texture->width = (short)width;
	texture->height = (short)height;
	texture->size = size;
	build->texture_tags[mesh->texture_count] = reference->index;

	return mesh->texture_count++;
}

/* The material of a model's shader (its index in the mesh's materials,
made the first time a part names the shader), and the shader's map scales
into `scale`; NONE for none (a shader not a model's or an environment's: its
parts are drawn grey). A model's shader's base map is tinted by the change
color it names (its source), where its multipurpose map's color change mask
(Halo PC's alpha) says. */
static short preview_material(
	struct preview_build *build,
	short shader_index,
	struct tag_injector_mesh *mesh,
	float *scale)
{
	unsigned long group_tag = 0;
	byte *shader = preview_shader(build->source, build->model, shader_index, &group_tag);
	struct tag_injector_mesh_material *material;
	short source = 0;
	short index;

	scale[0] = scale[1] = 1.0f;
	if (!shader || shader_index >= PREVIEW_MAXIMUM_SHADERS ||
		(group_tag != SHADER_MODEL_GROUP_TAG && group_tag != SHADER_ENVIRONMENT_GROUP_TAG))
	{
		return NONE;
	}
	if (group_tag == SHADER_MODEL_GROUP_TAG)
	{
		float u_scale;
		float v_scale;

		/* (none: 1) */
		memcpy(&u_scale, shader + SHADER_MODEL_MAP_U_SCALE_OFFSET, sizeof(u_scale));
		memcpy(&v_scale, shader + SHADER_MODEL_MAP_V_SCALE_OFFSET, sizeof(v_scale));
		if (u_scale > 0.0f && u_scale < PREVIEW_MAXIMUM_MAP_SCALE)
			scale[0] = u_scale;
		if (v_scale > 0.0f && v_scale < PREVIEW_MAXIMUM_MAP_SCALE)
			scale[1] = v_scale;
	}
	if (build->shader_materials[shader_index] != PREVIEW_MATERIAL_UNMADE)
		return build->shader_materials[shader_index];
	build->shader_materials[shader_index] = NONE;
	if (mesh->material_count >= TAG_INJECTOR_MESH_MAXIMUM_MATERIALS)
		return NONE;
	material = &mesh->materials[mesh->material_count];
	material->mask_texture = NONE;
	material->tinted = FALSE;
	if (group_tag == SHADER_MODEL_GROUP_TAG)
	{
		material->base_texture = preview_texture(build,
			(struct tag_reference const *)(shader + SHADER_MODEL_BASE_MAP_OFFSET), mesh);
		memcpy(&source, shader + SHADER_MODEL_CHANGE_COLOR_SOURCE_OFFSET, sizeof(source));
		/* (A to D: the object's change colors, of which it has this one) */
		if (source >= 1 && source <= PREVIEW_CHANGE_COLORS && build->colored[source - 1])
		{
			material->mask_texture = preview_texture(build,
				(struct tag_reference const *)(shader + SHADER_MODEL_MULTIPURPOSE_MAP_OFFSET), mesh);
			material->tinted = material->mask_texture != NONE;
			for (index = 0; index < 3; index++)
				material->tint[index] = build->colors[source - 1][index];
		}
	}
	else
	{
		material->base_texture = preview_texture(build,
			(struct tag_reference const *)(shader + SHADER_ENVIRONMENT_BASE_MAP_OFFSET), mesh);
	}
	build->shader_materials[shader_index] = mesh->material_count++;

	return build->shader_materials[shader_index];
}

/* a gbxmodel part's triangles and vertices onto the mesh (none when it
cannot be read, or there is no room for them) */
static void preview_mesh_part(
	struct preview_build *build,
	byte const *part,
	boolean opaque_only,
	struct tag_injector_mesh *mesh)
{
	struct model const *model = build->model;
	short shader_index;
	long strip_triangle_count = (long)read_unsigned_long(part + GBXMODEL_PART_STRIP_TRIANGLE_COUNT_OFFSET);
	long vertex_count = (long)read_unsigned_long(part + GBXMODEL_PART_VERTEX_COUNT_OFFSET);
	long first_vertex = mesh->vertex_count;
	long index_count = strip_triangle_count + 2;
	float scale[2];
	short material;
	byte *vertices;
	unsigned short *strip;
	long index;

	memcpy(&shader_index, part + GBXMODEL_PART_SHADER_INDEX_OFFSET, sizeof(shader_index));
	if ((opaque_only && !preview_shader_opaque(build->source, model, shader_index)) ||
		vertex_count <= 0 || vertex_count > PREVIEW_MAXIMUM_VERTICES - mesh->vertex_count ||
		strip_triangle_count <= 0 || strip_triangle_count > PREVIEW_MAXIMUM_STRIP_TRIANGLES)
	{
		return;
	}
	material = preview_material(build, shader_index, mesh, scale);
	/* (the model's base map scale, none of it 1) */
	if (model->base_map_scale.i > 0.0f && model->base_map_scale.i < PREVIEW_MAXIMUM_MAP_SCALE)
		scale[0] *= model->base_map_scale.i;
	if (model->base_map_scale.j > 0.0f && model->base_map_scale.j < PREVIEW_MAXIMUM_MAP_SCALE)
		scale[1] *= model->base_map_scale.j;
	vertices = malloc((unsigned long)vertex_count * GBXMODEL_VERTEX_BYTES);
	strip = malloc((unsigned long)index_count * sizeof(unsigned short));
	if (vertices && strip &&
		preview_read(build->source, _preview_vertices, read_unsigned_long(part + GBXMODEL_PART_VERTEX_OFFSET_OFFSET),
			(unsigned long)vertex_count * GBXMODEL_VERTEX_BYTES, vertices) &&
		preview_read(build->source, _preview_strip, read_unsigned_long(part + GBXMODEL_PART_STRIP_OFFSET_OFFSET),
			(unsigned long)index_count * sizeof(unsigned short), strip))
	{
		for (index = 0; index < vertex_count; index++)
		{
			byte const *vertex = vertices + index * GBXMODEL_VERTEX_BYTES;
			float *position = mesh->positions[first_vertex + index];
			float *normal = mesh->normals[first_vertex + index];
			float *texcoord = mesh->texcoords[first_vertex + index];
			short axis;

			memcpy(position, vertex + GBXMODEL_VERTEX_POSITION_OFFSET, 3 * sizeof(float));
			memcpy(normal, vertex + GBXMODEL_VERTEX_NORMAL_OFFSET, 3 * sizeof(float));
			memcpy(texcoord, vertex + GBXMODEL_VERTEX_TEXCOORD_OFFSET, 2 * sizeof(float));
			/* (a NaN fails every comparison) */
			for (axis = 0; axis < 3; axis++)
			{
				if (!(position[axis] >= -PREVIEW_MAXIMUM_COORDINATE && position[axis] <= PREVIEW_MAXIMUM_COORDINATE))
					position[axis] = 0.0f;
				if (!(normal[axis] >= -2.0f && normal[axis] <= 2.0f))
					normal[axis] = 0.0f;
			}
			for (axis = 0; axis < 2; axis++)
			{
				texcoord[axis] *= scale[axis];
				if (!(texcoord[axis] >= -PREVIEW_MAXIMUM_COORDINATE && texcoord[axis] <= PREVIEW_MAXIMUM_COORDINATE))
					texcoord[axis] = 0.0f;
			}
		}
		mesh->vertex_count += vertex_count;
		/* its strip's triangles (turning, its degenerate ones left out) */
		for (index = 0; index + 2 < index_count && mesh->triangle_count < PREVIEW_MAXIMUM_TRIANGLES; index++)
		{
			unsigned short a = strip[index];
			unsigned short b = strip[index + 1];
			unsigned short c = strip[index + 2];
			unsigned short *out = mesh->triangles[mesh->triangle_count];

			if (a == b || b == c || a == c || a >= vertex_count || b >= vertex_count || c >= vertex_count)
				continue;
			/* (a strip's odd triangles turn the other way) */
			if (index & 1)
			{
				unsigned short swap = b;

				b = c;
				c = swap;
			}
			out[0] = (unsigned short)(first_vertex + a);
			out[1] = (unsigned short)(first_vertex + b);
			out[2] = (unsigned short)(first_vertex + c);
			mesh->triangle_materials[mesh->triangle_count] = material;
			mesh->triangle_count++;
		}
	}
	if (vertices)
		free(vertices);
	if (strip)
		free(strip);
}

/* the mesh of the model of a tag of a preview's source (tag_injector_mesh's),
as the game draws an object of it: its change colors too (an actor
variant's, its unit's); FALSE when it has none (the mesh is then let go) */
static boolean preview_mesh_build(
	struct preview_source const *source,
	byte *root,
	unsigned long group_tag,
	struct tag_injector_mesh *mesh)
{
	struct preview_build build;
	struct object_definition const *object;
	short pass;
	short index;

	memset(&build, 0, sizeof(build));
	build.source = source;
	preview_change_colors(source, root, group_tag, &build);
	if (root && group_tag == ACTOR_VARIANT_GROUP_TAG)
		root = preview_tag(source, ((struct actor_variant_definition const *)root)->unit_reference.index, &group_tag);
	if (!root || (group_tag != WEAPON_DEFINITION_TAG && group_tag != EQUIPMENT_GROUP_TAG && group_tag != VEHICLE_GROUP_TAG &&
		group_tag != BIPED_GROUP_TAG && group_tag != SCENERY_GROUP_TAG))
	{
		return FALSE;
	}
	object = (struct object_definition const *)root;
	build.model = (struct model const *)preview_tag(source, object->object.model.index, &group_tag);
	if (!build.model || group_tag != GBXMODEL_GROUP_TAG || build.model->regions.count <= 0 ||
		build.model->geometries.count <= 0)
	{
		return FALSE;
	}
	mesh->positions = malloc(PREVIEW_MAXIMUM_VERTICES * sizeof(*mesh->positions));
	mesh->normals = malloc(PREVIEW_MAXIMUM_VERTICES * sizeof(*mesh->normals));
	mesh->texcoords = malloc(PREVIEW_MAXIMUM_VERTICES * sizeof(*mesh->texcoords));
	mesh->triangles = malloc(PREVIEW_MAXIMUM_TRIANGLES * sizeof(*mesh->triangles));
	mesh->triangle_materials = malloc(PREVIEW_MAXIMUM_TRIANGLES * sizeof(*mesh->triangle_materials));
	if (!mesh->positions || !mesh->normals || !mesh->texcoords || !mesh->triangles || !mesh->triangle_materials)
	{
		tag_injector_mesh_free(mesh);
		return FALSE;
	}
	for (index = 0; index < PREVIEW_MAXIMUM_SHADERS; index++)
		build.shader_materials[index] = PREVIEW_MATERIAL_UNMADE;
	/* (its opaque parts, else all of them: an energy sword's are all
	transparent) */
	for (pass = 0; pass < 2 && !mesh->triangle_count; pass++)
	{
		struct model const *model = build.model;
		short region_index;

		mesh->vertex_count = 0;
		for (region_index = 0; region_index < MIN(model->regions.count, PREVIEW_MAXIMUM_REGIONS); region_index++)
		{
			struct model_region const *region = TAG_BLOCK_GET_ELEMENT(&model->regions, region_index,
				struct model_region);
			struct model_region_permutation const *permutation = NULL;
			short permutation_index;
			short geometry_index = NONE;
			short level;
			long part_index;
			struct preview_model_geometry const *geometry;

			/* (its first permutation an object may be given, else its first:
			the others are its damaged ones, and the like) */
			for (permutation_index = 0; permutation_index < region->permutations.count; permutation_index++)
			{
				struct model_region_permutation const *candidate = TAG_BLOCK_GET_ELEMENT(&region->permutations,
					permutation_index, struct model_region_permutation);

				if (!permutation)
					permutation = candidate;
				if (!TEST_FLAG(candidate->flags, _model_region_permutation_cannot_be_chosen_randomly_bit))
				{
					permutation = candidate;
					break;
				}
			}
			if (!permutation)
				continue;
			/* (its most detailed geometry) */
			for (level = PREVIEW_DETAIL_LEVELS - 1;
				level >= 0 && (geometry_index < 0 || geometry_index >= model->geometries.count); level--)
			{
				geometry_index = permutation->geometry_indices[level];
			}
			if (geometry_index < 0 || geometry_index >= model->geometries.count)
				continue;
			geometry = TAG_BLOCK_GET_ELEMENT(&model->geometries, geometry_index, struct preview_model_geometry);
			for (part_index = 0; part_index < geometry->parts.count; part_index++)
			{
				preview_mesh_part(&build,
					(byte const *)TAG_BLOCK_GET_ELEMENT(&geometry->parts, part_index, struct preview_model_part),
					pass == 0, mesh);
			}
		}
	}
	if (!mesh->triangle_count)
	{
		tag_injector_mesh_free(mesh);
		return FALSE;
	}

	return TRUE;
}

/* the state of a tag a reference names that a tag needs (needed: it cannot
work without one), or might have */
static short preview_need_state(
	struct preview_source const *source,
	struct tag_reference const *reference,
	boolean needed)
{
	unsigned long group_tag;

	if (reference->index == NONE)
		return needed ? _tag_injector_need_missing : _tag_injector_need_none;
	return preview_tag(source, reference->index, &group_tag) ? _tag_injector_need_present :
		_tag_injector_need_missing;
}

static void preview_need(
	struct tag_injector_needs *needs,
	char const *name,
	short state)
{
	if (needs->count < TAG_INJECTOR_MAXIMUM_NEEDS)
	{
		needs->needs[needs->count].name = name;
		needs->needs[needs->count++].state = state;
	}
}

/* an object's model, its animations (when it moves by them) and its
collision (needed: else it may have none) */
static void preview_object_needs(
	struct preview_source const *source,
	struct object_definition const *object,
	boolean animated,
	boolean collision_needed,
	struct tag_injector_needs *needs)
{
	preview_need(needs, "model", preview_need_state(source, &object->object.model, TRUE));
	if (animated)
		preview_need(needs, "animations", preview_need_state(source, &object->object.animation_graph, TRUE));
	preview_need(needs, "collision", preview_need_state(source, &object->object.collision_model, collision_needed));
}

/* which of the first 64 of a block of animation indices are some animation
(not NONE) */
static unsigned long long preview_animation_bits(
	struct tag_block const *block)
{
	short const *indices = block->address;
	unsigned long long bits = 0;
	long index;

	for (index = 0; indices && index < block->count && index < 64; index++)
	{
		if (indices[index] != NONE)
			bits |= 1ULL << index;
	}
	return bits;
}

/* whether an animation's bit is set */
#define ANIMATION_BIT(bits, index) (((bits) >> (index) & 1) != 0)

static void usability_add(
	struct tag_injector_usability *usability,
	char const *name,
	boolean present)
{
	if (usability->count < TAG_INJECTOR_MAXIMUM_ANIMATIONS)
	{
		usability->animations[usability->count].name = name;
		usability->animations[usability->count++].state = present ? _tag_injector_need_present :
			_tag_injector_need_missing;
	}
}

/* the rounds a weapon's first magazine holds (none: it is a battery's or
heat's, and is not reloaded) */
static short weapon_magazine_rounds(
	struct weapon_definition const *weapon)
{
	return weapon->weapon.magazines.count > 0 && weapon->weapon.magazines.address ?
		((struct weapon_magazine_definition const *)weapon->weapon.magazines.address)->rounds_loaded_maximum : 0;
}

/* a weapon's: the first-person animations every weapon of the game's has
(but put-away, which none has, and fire, which the automatic ones show with
their overlays): idle, ready, reload (a weapon with a magazine of rounds),
melee and grenade (one that lets them be done); and whether the campaign's
player is animated holding it, by its label (the player's weapon types'),
else it is held as another */
static void preview_weapon_usability(
	struct preview_source const *source,
	struct weapon_definition const *weapon,
	struct tag_injector_usability *usability)
{
	unsigned long group_tag = 0;
	struct animation_graph const *graph = weapon->weapon.interface_definition.first_person_animations.index == NONE ?
		NULL : (struct animation_graph const *)preview_tag(source,
		weapon->weapon.interface_definition.first_person_animations.index, &group_tag);
	unsigned long long bits = 0;
	unsigned long long animations;
	unsigned long long overlays;
	int held;

	if (graph && group_tag == ANIMATION_GRAPH_TAG && graph->first_person_weapon_animations.count > 0 &&
		graph->first_person_weapon_animations.address)
	{
		bits = preview_animation_bits(&((struct preview_first_person_animations const *)
			graph->first_person_weapon_animations.address)->animations);
	}
	usability_add(usability, "idle", ANIMATION_BIT(bits, _first_person_idle));
	usability_add(usability, "ready", ANIMATION_BIT(bits, _first_person_ready));
	if (weapon_magazine_rounds(weapon) > 0)
	{
		usability_add(usability, "reload", ANIMATION_BIT(bits, _first_person_reload_empty) ||
			ANIMATION_BIT(bits, _first_person_reload_full));
	}
	if (!TEST_FLAG(weapon->weapon.flags, _weapon_prevents_melee_attack_bit))
		usability_add(usability, "melee", ANIMATION_BIT(bits, _first_person_melee));
	if (!TEST_FLAG(weapon->weapon.flags, _weapon_prevents_grenade_throwing_bit))
		usability_add(usability, "grenade", ANIMATION_BIT(bits, _first_person_throw_grenade));
	snprintf(usability->label, sizeof(usability->label), "%.*s", TAG_STRING_LENGTH, weapon->weapon.label);
	held = randomizer_stock_player_class(usability->label, &animations, &overlays);
	usability->third_person = held > 0 ? _tag_injector_need_present : held == 0 ? _tag_injector_need_missing :
		_tag_injector_need_none;
}

/* an actor variant's: whether its unit is animated holding its weapon (by
its label, as the game finds it: a weapon type's of a weapon class of its
standing seat), and its moves, that weapon class's and type's */
static void preview_actor_usability(
	struct preview_source const *source,
	struct actor_variant_definition const *variant,
	struct object_definition const *unit,
	struct tag_injector_usability *usability)
{
	unsigned long group_tag = 0;
	struct weapon_definition const *weapon = variant->ranged_combat.reference.index == NONE ? NULL :
		(struct weapon_definition const *)preview_tag(source, variant->ranged_combat.reference.index, &group_tag);
	struct animation_graph const *graph = NULL;
	struct animation_graph_unit_seat const *seats;
	struct animation_graph_weapon_class const *found = NULL;
	struct animation_graph_weapon_type const *found_type = NULL;
	unsigned long long animations = 0;
	unsigned long long overlays = 0;
	long index;
	long chosen = 0;

	usability->third_person = _tag_injector_need_none;
	if (!weapon || group_tag != WEAPON_DEFINITION_TAG || !unit || unit->object.animation_graph.index == NONE)
		return;
	snprintf(usability->label, sizeof(usability->label), "%.*s", TAG_STRING_LENGTH, weapon->weapon.label);
	graph = (struct animation_graph const *)preview_tag(source, unit->object.animation_graph.index, &group_tag);
	if (!graph || group_tag != ANIMATION_GRAPH_TAG || graph->unit_seats.count <= 0 || !graph->unit_seats.address)
		return;
	/* (its standing seat, else its first) */
	seats = graph->unit_seats.address;
	for (index = 0; index < graph->unit_seats.count; index++)
	{
		if (!_strnicmp(seats[index].label, "stand", 5))
		{
			chosen = index;
			break;
		}
	}
	for (index = 0; !found && seats[chosen].weapon_classes.address && index < seats[chosen].weapon_classes.count;
		index++)
	{
		struct animation_graph_weapon_class const *weapon_class = (struct animation_graph_weapon_class const *)
			seats[chosen].weapon_classes.address + index;
		long type_index;

		for (type_index = 0; weapon_class->weapon_types.address && type_index < weapon_class->weapon_types.count;
			type_index++)
		{
			struct animation_graph_weapon_type const *weapon_type = (struct animation_graph_weapon_type const *)
				weapon_class->weapon_types.address + type_index;

			if (!_strnicmp(weapon_type->label, usability->label, TAG_STRING_LENGTH))
			{
				found = weapon_class;
				found_type = weapon_type;
				break;
			}
		}
	}
	usability->third_person = found ? _tag_injector_need_present : _tag_injector_need_missing;
	usability->actor = TRUE;
	if (!found)
		return;
	animations = preview_animation_bits(&found->animations);
	overlays = preview_animation_bits(&found_type->animations);
	usability_add(usability, "idle", ANIMATION_BIT(animations, _class_idle));
	usability_add(usability, "walk", ANIMATION_BIT(animations, _class_move_front));
	usability_add(usability, "fire", ANIMATION_BIT(overlays, _type_fire));
	if (weapon_magazine_rounds(weapon) > 0)
		usability_add(usability, "reload", ANIMATION_BIT(overlays, _type_reload));
	usability_add(usability, "melee", ANIMATION_BIT(animations, _class_melee) || ANIMATION_BIT(overlays, _type_melee));
	usability_add(usability, "grenade", ANIMATION_BIT(animations, _class_throw_grenade));
	usability_add(usability, "evade", ANIMATION_BIT(animations, _class_evade_left) ||
		ANIMATION_BIT(animations, _class_evade_left + 1));
	usability_add(usability, "dive", (animations >> _class_dive_front & 0xF) != 0);
	usability_add(usability, "berserk", ANIMATION_BIT(animations, _class_berserk));
}

/* what a tag of a preview's source needs to work, by its group
(tag_injector_needs') */
static void preview_needs(
	struct preview_source const *source,
	byte *root,
	unsigned long group_tag,
	struct tag_injector_needs *needs)
{
	memset(needs, 0, sizeof(*needs));
	needs->usability.third_person = _tag_injector_need_none;
	if (group_tag == WEAPON_DEFINITION_TAG)
	{
		struct weapon_definition const *weapon = (struct weapon_definition const *)root;
		struct weapon_interface_definition const *interface = &weapon->weapon.interface_definition;
		short projectile = _tag_injector_need_none;
		long trigger_index;

		preview_object_needs(source, (struct object_definition const *)root, FALSE, TRUE, needs);
		preview_need(needs, "1st person model", preview_need_state(source, &interface->first_person_model, TRUE));
		preview_need(needs, "1st person animations",
			preview_need_state(source, &interface->first_person_animations, TRUE));
		/* (a trigger's that names one it has not got; none named: a melee
		weapon's) */
		for (trigger_index = 0; trigger_index < weapon->weapon.triggers.count; trigger_index++)
		{
			struct weapon_trigger_definition const *trigger = TAG_BLOCK_GET_ELEMENT(&weapon->weapon.triggers,
				trigger_index, struct weapon_trigger_definition);
			short state = preview_need_state(source, &trigger->projectile, FALSE);

			if (state == _tag_injector_need_missing || (state == _tag_injector_need_present &&
				projectile == _tag_injector_need_none))
			{
				projectile = state;
			}
		}
		preview_need(needs, "projectile", projectile);
		preview_need(needs, "HUD", preview_need_state(source, &interface->hud_interface, TRUE));
		preview_weapon_usability(source, weapon, &needs->usability);
	}
	else if (group_tag == ACTOR_VARIANT_GROUP_TAG)
	{
		struct actor_variant_definition const *variant = (struct actor_variant_definition const *)root;
		unsigned long unit_group = 0;
		byte *unit = variant->unit_reference.index == NONE ? NULL :
			preview_tag(source, variant->unit_reference.index, &unit_group);

		preview_need(needs, "behaviour", preview_need_state(source, &variant->actor_reference, TRUE));
		preview_need(needs, "unit", unit ? _tag_injector_need_present : _tag_injector_need_missing);
		if (unit)
		{
			preview_object_needs(source, (struct object_definition const *)unit, TRUE, TRUE, needs);
		}
		else
		{
			preview_need(needs, "model", _tag_injector_need_missing);
			preview_need(needs, "animations", _tag_injector_need_missing);
			preview_need(needs, "collision", _tag_injector_need_missing);
		}
		preview_need(needs, "weapon", preview_need_state(source, &variant->ranged_combat.reference, FALSE));
		preview_actor_usability(source, variant, unit_group == BIPED_GROUP_TAG || unit_group == VEHICLE_GROUP_TAG ?
			(struct object_definition const *)unit : NULL, &needs->usability);
	}
	else if (group_tag == EQUIPMENT_GROUP_TAG)
	{
		preview_object_needs(source, (struct object_definition const *)root, FALSE, TRUE, needs);
	}
	else if (group_tag == VEHICLE_GROUP_TAG || group_tag == BIPED_GROUP_TAG)
	{
		preview_object_needs(source, (struct object_definition const *)root, TRUE, TRUE, needs);
	}
	else if (group_tag == SCENERY_GROUP_TAG)
	{
		preview_object_needs(source, (struct object_definition const *)root, FALSE, FALSE, needs);
	}
}

boolean tag_injector_map_tag_preview(
	short category,
	long index,
	struct tag_injector_mesh *mesh,
	struct tag_injector_needs *needs)
{
	struct preview_source source;
	long absolute = menu_tag(category, index);
	byte *root;
	unsigned long group_tag;

	memset(mesh, 0, sizeof(*mesh));
	memset(needs, 0, sizeof(*needs));
	memset(&source, 0, sizeof(source));
	if (absolute == NONE)
		return FALSE;
	custom_edition_cache_bundle_source(&source.map);
	root = tag_injector_menu.header->instances[absolute].base_address;
	group_tag = (unsigned long)tag_injector_menu.header->instances[absolute].group_tag;
	preview_needs(&source, root, group_tag, needs);
	preview_mesh_build(&source, root, group_tag, mesh);

	return TRUE;
}

boolean tag_injector_profile_tag_preview(
	long profile,
	long tag,
	struct tag_injector_mesh *mesh,
	struct tag_injector_needs *needs)
{
	struct preview_source source;
	struct injected_bundle bundle;
	unsigned long total = 0;
	boolean read = FALSE;

	memset(mesh, 0, sizeof(*mesh));
	memset(needs, 0, sizeof(*needs));
	memset(&source, 0, sizeof(source));
	if (profile < 0 || profile >= port_tag_profiles_count() || tag < 0 || tag >= port_tag_profiles_tag_count(profile))
		return FALSE;
	/* (its tags read, its file data read from its file as it is wanted:
	the first of its tags the one taken out) */
	if (level_bundle_read(profile, tag, &bundle, &total) && bundle.loaded.tag_count > 0)
	{
		source.bundle = &bundle;
		preview_needs(&source, bundle_tag_root(&bundle.loaded, 0), bundle_tag_group(&bundle.loaded, 0), needs);
		preview_mesh_build(&source, bundle_tag_root(&bundle.loaded, 0), bundle_tag_group(&bundle.loaded, 0), mesh);
		read = TRUE;
	}
	if (bundle.bytes)
		free(bundle.bytes);
	if (bundle.datums)
		free(bundle.datums);
	if (bundle.handle != NONE)
		port_tag_profiles_bundle_close(bundle.handle);

	return read;
}

void tag_injector_mesh_free(
	struct tag_injector_mesh *mesh)
{
	short texture;

	/* (the game's free does not take NULL) */
	if (mesh->positions)
		free(mesh->positions);
	if (mesh->normals)
		free(mesh->normals);
	if (mesh->texcoords)
		free(mesh->texcoords);
	if (mesh->triangles)
		free(mesh->triangles);
	if (mesh->triangle_materials)
		free(mesh->triangle_materials);
	for (texture = 0; texture < mesh->texture_count; texture++)
	{
		if (mesh->textures[texture].pixels)
			free(mesh->textures[texture].pixels);
	}
	memset(mesh, 0, sizeof(*mesh));
}

long tag_injector_profiles_unstored(
	void)
{
	long profile;
	long count = 0;

	for (profile = 0; profile < port_tag_profiles_count(); profile++)
	{
		long tag;

		for (tag = 0; tag < port_tag_profiles_tag_count(profile); tag++)
		{
			if (!port_tag_profiles_tag_stored(profile, tag) && port_tag_profiles_tag_size(profile, tag) &&
				!tag_injector_store_failed(profile, tag, FALSE))
				count++;
		}
	}

	return count;
}

boolean tag_injector_profile_store_next(
	unsigned long *before,
	unsigned long *after)
{
	long profile;

	*before = 0;
	*after = 0;
	for (profile = 0; profile < port_tag_profiles_count(); profile++)
	{
		long tag;

		for (tag = 0; tag < port_tag_profiles_tag_count(profile); tag++)
		{
			char map[64];
			char name[256];
			unsigned long group_tag = port_tag_profiles_tag_group(profile, tag);
			unsigned long size = 0;
			unsigned long profile_before = port_tag_profiles_size(profile);
			long handle;
			byte *bytes;
			boolean stored;

			if (port_tag_profiles_tag_stored(profile, tag) || !port_tag_profiles_tag_size(profile, tag) ||
				tag_injector_store_failed(profile, tag, FALSE))
				continue;
			snprintf(map, sizeof(map), "%s", port_tag_profiles_tag_map(profile, tag));
			snprintf(name, sizeof(name), "%s", port_tag_profiles_tag_name(profile, tag));
			handle = port_tag_profiles_bundle_open(profile, tag, &size);
			bytes = handle != NONE && size ? malloc(size) : NULL;
			stored = bytes && port_tag_profiles_bundle_read_at(handle, 0, bytes, size);
			if (handle != NONE)
				port_tag_profiles_bundle_close(handle);
			stored = stored && profile_bundle_add(profile, map, group_tag, name, bytes, size);
			if (bytes)
				free(bytes);
			if (!stored)
			{
				error(_error_silent, "tag injector: %s of the profile %s is kept as it was taken out", name,
					port_tag_profiles_name(profile));
				tag_injector_store_failed(profile, tag, TRUE);
				return TRUE;
			}
			*before = profile_before;
			*after = port_tag_profiles_size(profile);
			error(_error_silent, "tag injector: stored %s of the profile %s: the profile is %lu bytes, of %lu",
				name, port_tag_profiles_name(profile), *after, *before);
			return TRUE;
		}
	}

	return FALSE;
}

long tag_injector_profile_playable(
	void)
{
	long profile;

	if (!config_boolean("game.tag_injection"))
		return NONE;
	port_tag_profiles_scan();
	profile = port_tag_profiles_find(config_string("game.randomizer_tag_profile"));

	return profile >= 0 && tag_injector_profile_enabled_count(profile, 0) > 0 ? profile : NONE;
}

long tag_injector_profile_enabled_count(
	long profile,
	unsigned long group_tag)
{
	long count = port_tag_profiles_tag_count(profile);
	long enabled = 0;
	long tag;

	for (tag = 0; tag < count; tag++)
	{
		if (port_tag_profiles_tag_enabled(profile, tag) &&
			(!group_tag || port_tag_profiles_tag_group(profile, tag) == group_tag))
			enabled++;
	}

	return enabled;
}

char const *tag_injector_tag_setting(
	long profile,
	long tag,
	char const *key)
{
	return port_tag_profiles_tag_option(profile, tag, key);
}

void tag_injector_tag_display_name(
	long profile,
	long tag,
	char *name,
	long name_size)
{
	char const *named = port_tag_profiles_tag_option(profile, tag, "name");
	char const *path = port_tag_profiles_tag_name(profile, tag);
	char const *leaf = strrchr(path, '\\');

	if (name_size > 0)
		snprintf(name, (size_t)name_size, "%s", named[0] ? named : leaf ? leaf + 1 : path);
}

void tag_injector_tag_custom_type(
	long profile,
	long tag,
	char *name,
	long name_size)
{
	char const *custom = port_tag_profiles_tag_option(profile, tag, "custom_type");
	long index;

	if (name_size <= 0)
		return;
	if (custom[0])
		snprintf(name, (size_t)name_size, "%s", custom);
	else
		tag_injector_tag_display_name(profile, tag, name, name_size);
	/* (in capitals, as the menus show it, and the randomizer compares it) */
	for (index = 0; name[index]; index++)
	{
		if (name[index] >= 'a' && name[index] <= 'z')
			name[index] = (char)(name[index] - 'a' + 'A');
	}
}

long tag_injector_custom_types(
	long profile,
	unsigned long group_tag,
	char (*names)[TAG_INJECTOR_CUSTOM_TYPE_LENGTH],
	long capacity)
{
	long count = port_tag_profiles_tag_count(profile);
	long found = 0;
	long tag;

	for (tag = 0; tag < count && found < capacity; tag++)
	{
		char name[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
		long index;

		if (!port_tag_profiles_tag_enabled(profile, tag) || port_tag_profiles_tag_group(profile, tag) != group_tag)
			continue;
		tag_injector_tag_custom_type(profile, tag, name, sizeof(name));
		for (index = 0; index < found && strcmp(names[index], name); index++)
		{
		}
		if (index < found)
			continue;
		/* (by name, alphabetically) */
		for (index = found; index > 0 && strcmp(names[index - 1], name) > 0; index--)
			memcpy(names[index], names[index - 1], sizeof(names[0]));
		memcpy(names[index], name, sizeof(names[0]));
		found++;
	}

	return found;
}

short tag_injector_stock_hud_count(
	void)
{
	return (short)NUMBEROF(stock_huds);
}

char const *tag_injector_stock_hud_value(
	short hud)
{
	return hud >= 0 && hud < (short)NUMBEROF(stock_huds) ? stock_huds[hud].value : "";
}

char const *tag_injector_stock_hud_tag(
	short hud)
{
	return hud >= 0 && hud < (short)NUMBEROF(stock_huds) ? stock_huds[hud].tag : "";
}

char const *tag_injector_stock_hud_map(
	short hud)
{
	return hud >= 0 && hud < (short)NUMBEROF(stock_huds) ? stock_huds[hud].map : "";
}

/* a line of the word of the tags that could not be played, kept */
static void level_notice_line(
	char const *text)
{
	if (tag_injector_level.notice_lines < TAG_INJECTOR_NOTICE_LINES)
	{
		snprintf(tag_injector_level.notice[tag_injector_level.notice_lines++], sizeof(tag_injector_level.notice[0]),
			"%s", text);
	}
}

/* the word of a profile's tags that could not be played (`tags`, `count`
of them), and of those past the most a level adds: how many, then their
names, as many as fit in the lines the HUD shows at once ("..." for the
rest) */
static void level_notice_failures(
	long profile,
	long const *tags,
	long count,
	long skipped)
{
	char line[TAG_INJECTOR_NOTICE_WIDTH + 4];
	long index;

	tag_injector_level.notice_lines = 0;
	if (count)
	{
		snprintf(line, sizeof(line), "%ld Tag Injector tag%s failed to load (turned off):", count,
			count == 1 ? "" : "s");
		level_notice_line(line);
		line[0] = 0;
		for (index = 0; index < count; index++)
		{
			char name[TAG_INJECTOR_NOTICE_WIDTH];
			char piece[TAG_INJECTOR_NOTICE_WIDTH + 2];
			boolean last_line = tag_injector_level.notice_lines == TAG_INJECTOR_NOTICE_LINES - 1 - (skipped != 0);

			tag_injector_tag_display_name(profile, tags[index], name, sizeof(name));
			snprintf(piece, sizeof(piece), "%s%s", name, index + 1 < count ? ", " : "");
			if (strlen(line) + strlen(piece) > TAG_INJECTOR_NOTICE_WIDTH)
			{
				/* (the last line there is room for: cut short) */
				if (last_line)
				{
					snprintf(line + strlen(line), sizeof(line) - strlen(line), "...");
					break;
				}
				level_notice_line(line);
				line[0] = 0;
			}
			snprintf(line + strlen(line), sizeof(line) - strlen(line), "%s", piece);
		}
		if (line[0])
			level_notice_line(line);
	}
	if (skipped)
	{
		snprintf(line, sizeof(line), "%ld Tag Injector tag%s left out: a level adds %d at most", skipped,
			skipped == 1 ? "" : "s", MAXIMUM_INJECTED_BUNDLES);
		/* (the last line, whatever came before) */
		if (tag_injector_level.notice_lines == TAG_INJECTOR_NOTICE_LINES)
			tag_injector_level.notice_lines--;
		level_notice_line(line);
	}
	if (count || skipped)
	{
		error(_error_silent, "tag injector: %ld tags could not be played and %ld were left out: the HUD says so once "
			"the level is played", count, skipped);
	}
}

long tag_injector_tags_read(
	long profile,
	short *huds,
	long capacity)
{
	unsigned long total = 0;
	long tag_count = port_tag_profiles_tag_count(profile);
	long found = 0;
	long bundle_index;
	long index;
	long failed[MAXIMUM_INJECTED_BUNDLES + 1];
	long failed_count = 0;
	long skipped = 0;

	tag_injector_tags_unloaded();
	tag_injector_level.bundles_read = TRUE;
	tag_injector_level.read_profile = profile;

	/* each bundle read, checked, and its models made (none of the level's
	tags is touched until all are) */
	for (index = 0; index < tag_count; index++)
	{
		struct injected_bundle *bundle = &tag_injector_level.bundles[tag_injector_level.bundle_count];

		/* (one turned off stays in the profile, and is not played) */
		if (!port_tag_profiles_tag_enabled(profile, index))
			continue;
		/* (past the most a level adds: left out, and said, but not turned
		off) */
		if (tag_injector_level.bundle_count >= MAXIMUM_INJECTED_BUNDLES)
		{
			error(_error_silent, "tag injector: %s is left out: a level adds %d tags of a profile at most",
				port_tag_profiles_tag_name(profile, index), MAXIMUM_INJECTED_BUNDLES);
			skipped++;
			continue;
		}
		/* (counted, so that what it holds is let go with the rest; the read
		clears it, and then it is told which tag it is) */
		tag_injector_level.bundle_count++;
		if (!level_bundle_read(profile, index, bundle, &total))
		{
			bundle->profile_tag = index;
			bundle->replaced = NONE;
			bundle->refused = TRUE;
			continue;
		}
		bundle->profile_tag = index;
		bundle->replace = port_tag_profiles_tag_option(profile, index, "replace")[0] != 0;
		bundle->replaced = NONE;
		bundle->failure = !bundle_weapons_holdable(bundle) ? "not holdable" :
			!bundle_bitmaps_drawable(bundle) ? "bitmaps" :
			!bundle_models_convert(bundle) ? "models" : NULL;
		if (bundle->failure)
		{
			error(_error_silent, "tag injector: %s is left out", port_tag_profiles_tag_name(profile, index));
			bundle->refused = TRUE;
		}
	}
	tag_injector_level.read_bytes = total;

	/* the tags that could not be played: each turned off, its "failed"
	setting saying why (the menu says it, and turning it on again clears
	it), and said once the level plays */
	for (bundle_index = 0; bundle_index < tag_injector_level.bundle_count; bundle_index++)
	{
		struct injected_bundle const *bundle = &tag_injector_level.bundles[bundle_index];

		if (!bundle->refused)
			continue;
		failed[failed_count++] = bundle->profile_tag;
		/* (a network game's shared profile is played as the host made it,
		the same on every machine: tag_multiplayer.c) */
		if (port_tag_profiles_shared(profile))
			continue;
		port_tag_profiles_tag_set_option(profile, bundle->profile_tag, "failed",
			bundle->failure ? bundle->failure : "unreadable");
		port_tag_profiles_tag_set_enabled(profile, bundle->profile_tag, FALSE);
	}
	level_notice_failures(profile, failed, failed_count, skipped);

	/* the stock HUDs of the weapons the bundles would add */
	for (bundle_index = 0; bundle_index < tag_injector_level.bundle_count; bundle_index++)
	{
		struct injected_bundle const *bundle = &tag_injector_level.bundles[bundle_index];
		long tag;

		for (tag = 0; !bundle->refused && tag < bundle->loaded.tag_count; tag++)
		{
			short hud;
			long known;

			if (bundle_tag_group(&bundle->loaded, tag) != WEAPON_DEFINITION_TAG ||
				bundle_tag_kept(bundle, WEAPON_DEFINITION_TAG, bundle_tag_name(&bundle->loaded, tag)) ||
				(bundle->drops_weapons && bundle_weapon_unholdable(bundle, tag)))
				continue;
			hud = weapon_stock_hud(bundle, tag);
			if (hud == NONE || tag_loaded(WEAPON_HUD_INTERFACE_GROUP_TAG, stock_huds[hud].tag) != NONE)
				continue;
			for (known = 0; known < found && huds[known] != hud; known++)
			{
			}
			if (known == found && found < capacity)
				huds[found++] = hud;
		}
	}

	return found;
}

long tag_injector_tags_load(
	long profile,
	long *roots,
	long *root_tags,
	long root_capacity)
{
	struct injector_tag_instance *instances;
	long existing = 0;
	unsigned long total;
	long added = 0;
	long replaced = 0;
	long root_count = 0;
	long salt = 0;
	long bundle_index;
	long index;
	short bitmap_limit = injected_bitmap_limit();
	long bitmaps_downsized = 0;
	unsigned long bitmap_bytes_skipped = 0;

	/* (read before the randomizer copied its tags, unless it did not) */
	if (!tag_injector_level.bundles_read || tag_injector_level.read_profile != profile)
		tag_injector_tags_read(profile, NULL, 0);
	total = tag_injector_level.read_bytes;
	instances = cache_files_tag_instances(&existing);
	if (!instances || !tag_injector_level.bundle_count)
		return 0;

	/* the tag of the level each bundle's tag is: the level's own, one added
	before, or a new one */
	for (index = 0; index < existing; index++)
	{
		long instance_salt = (long)((unsigned long)instances[index].tag_index >> 16);

		if (instance_salt > salt)
			salt = instance_salt;
	}
	salt++;
	tag_injector_level.salt = salt;
	tag_injector_level.grown_instances = malloc((unsigned long)existing * sizeof(*instances));
	if (!tag_injector_level.grown_instances)
	{
		level_bundles_free();
		custom_edition_injected_models_dispose();
		return 0;
	}
	memcpy(tag_injector_level.grown_instances, instances, (unsigned long)existing * sizeof(*instances));
	tag_injector_level.saved_instances = instances;
	tag_injector_level.saved_count = existing;
	tag_injector_level.first_injected = existing;

	/* the level's tags the replacing bundles take the place of, where they
	are (the first bundle of a path's: the others' are that one) */
	for (bundle_index = 0; bundle_index < tag_injector_level.bundle_count; bundle_index++)
	{
		struct injected_bundle *bundle = &tag_injector_level.bundles[bundle_index];
		long datum;
		long other;

		bundle->replaced = NONE;
		if (bundle->refused || !bundle->replace || bundle->loaded.tag_count <= 0)
			continue;
		datum = level_tag(bundle_tag_group(&bundle->loaded, 0), bundle_tag_name(&bundle->loaded, 0));
		if (datum == NONE)
		{
			error(_error_silent, "tag injector: %s replaces nothing: the level has not got it, and it is added",
				bundle_tag_name(&bundle->loaded, 0));
			continue;
		}
		for (other = 0; other < bundle_index && tag_injector_level.bundles[other].replaced != datum; other++)
		{
		}
		if (other < bundle_index)
		{
			error(_error_silent, "tag injector: %s is replaced already, by an earlier tag of the profile",
				bundle_tag_name(&bundle->loaded, 0));
			continue;
		}
		bundle->replaced = datum;
		tag_injector_level.grown_instances[DATUM_INDEX_TO_ABSOLUTE_INDEX(datum)].base_address =
			bundle_tag_root(&bundle->loaded, 0);
		replaced++;
		error(_error_silent, "tag injector: %s replaces the level's", bundle_tag_name(&bundle->loaded, 0));
	}

	for (bundle_index = 0; bundle_index < tag_injector_level.bundle_count; bundle_index++)
	{
		struct injected_bundle *bundle = &tag_injector_level.bundles[bundle_index];
		long tag;

		for (tag = 0; bundle->datums && tag < bundle->loaded.tag_count; tag++)
			bundle->datums[tag] = NONE;
		if (bundle->refused)
			continue;
		for (tag = 0; tag < bundle->loaded.tag_count; tag++)
		{
			unsigned long group_tag = bundle_tag_group(&bundle->loaded, tag);
			char const *name = bundle_tag_name(&bundle->loaded, tag);
			struct injector_tag_instance *grown;
			struct injector_tag_instance *instance;
			byte const *entry = bundle->loaded.tags + tag * TAG_BUNDLE_TAG_BYTES;
			long datum = level_tag(group_tag, name);
			long count = existing + added;

			if (tag == 0 && bundle->replaced != NONE)
			{
				bundle->datums[tag] = bundle->replaced;
				continue;
			}
			/* (an AI's weapon a player could not hold: left out, what names
			it naming none) */
			if (bundle->drops_weapons && bundle_weapon_unholdable(bundle, tag))
				continue;
			/* (a replacing bundle's are its own, but where a bundle's are:
			added, or replacing the level's) */
			if (group_left_out(group_tag) ||
				(datum != NONE && (!bundle->replace || tag_replaced(datum) ||
				DATUM_INDEX_TO_ABSOLUTE_INDEX(datum) >= existing)))
			{
				bundle->datums[tag] = datum;
				continue;
			}
			if (count >= MAXIMUM_TAG_COUNT || !schema_of(group_tag))
			{
				error(_error_silent, "tag injector: no room for %s, or no schema of its group: no tags are added", name);
				tag_injector_tags_unloaded();
				level_notice_line("No Tag Injector tags were added: no room for them");
				return 0;
			}
			grown = realloc(tag_injector_level.grown_instances, (unsigned long)(count + 1) * sizeof(*grown));
			if (!grown)
			{
				tag_injector_tags_unloaded();
				return 0;
			}
			tag_injector_level.grown_instances = grown;
			instance = &grown[count];
			memset(instance, 0, sizeof(*instance));
			instance->group_tag = (long)group_tag;
			instance->parent_group_tags[0] = (long)read_unsigned_long(entry + TAG_BUNDLE_TAG_PARENT_GROUPS_OFFSET);
			instance->parent_group_tags[1] = (long)read_unsigned_long(entry + TAG_BUNDLE_TAG_PARENT_GROUPS_OFFSET + 4);
			instance->tag_index = (salt << 16) | count;
			instance->name = (char *)name;
			instance->base_address = bundle_tag_root(&bundle->loaded, tag);
			bundle->datums[tag] = instance->tag_index;
			added++;
			/* (tag_loaded finds it now, for the bundles after this one) */
			cache_files_set_tag_instances(tag_injector_level.grown_instances, existing + added);
		}
	}
	tag_injector_level.injected_count = added;
	tag_injector_level.replaced_count = replaced;
	/* (the table the level plays: the grown one, with the tags replaced,
	though none was added) */
	cache_files_set_tag_instances(tag_injector_level.grown_instances, existing + added);
	tag_injector_level.injected_bundle = malloc((unsigned long)(added + 1) * sizeof(short));
	if (!tag_injector_level.injected_bundle)
	{
		tag_injector_tags_unloaded();
		return 0;
	}

	/* the tags added: each named tag the level's, and each sound's samples
	read for it */
	for (bundle_index = 0; bundle_index < tag_injector_level.bundle_count; bundle_index++)
	{
		struct injected_bundle *bundle = &tag_injector_level.bundles[bundle_index];
		long tag;

		for (tag = 0; !bundle->refused && tag < bundle->loaded.tag_count; tag++)
		{
			long datum = bundle->datums[tag];
			long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(datum);
			unsigned long group_tag = bundle_tag_group(&bundle->loaded, tag);
			byte *root = bundle_tag_root(&bundle->loaded, tag);
			/* (a root replacing the level's has no file data: none is read for
			it) */
			boolean replacing = tag == 0 && bundle->replaced != NONE;

			if (datum == NONE || (!replacing && absolute < existing) ||
				tag_injector_level.grown_instances[absolute].base_address != root)
				continue;
			if (!replacing)
				tag_injector_level.injected_bundle[absolute - existing] = (short)bundle_index;
			retarget_fields(bundle, root, schema_of(group_tag), 0);
			if (group_tag == SOUND_DEFINITION_TAG)
				sound_permutations_retarget((struct sound_definition *)root, datum);
			if (group_tag == WEAPON_DEFINITION_TAG)
				weapon_hud_swap(bundle, tag, (struct weapon_definition *)root);
			if (group_tag == BITMAP_GROUP_TAG)
			{
				struct bitmap_group *group = (struct bitmap_group *)root;
				long bitmap_index;

				tag_injector_level.bitmaps_added = TRUE;
				for (bitmap_index = 0; bitmap_limit && bitmap_index < group->bitmaps.count; bitmap_index++)
				{
					unsigned long skipped = bitmap_downsize(
						TAG_BLOCK_GET_ELEMENT(&group->bitmaps, bitmap_index, struct bitmap_data), bitmap_limit);

					bitmaps_downsized += skipped != 0;
					bitmap_bytes_skipped += skipped;
				}
			}
		}
		/* (the tag it was taken out for: the pools' weapon or actor) */
		if (!bundle->refused && bundle->loaded.tag_count > 0 && bundle->datums[0] != NONE && root_count < root_capacity)
		{
			roots[root_count] = bundle->datums[0];
			root_tags[root_count++] = bundle->profile_tag;
		}
	}
	/* (the profile's weapons given their edits: tag_editor.c) */
	tag_injector_weapons_edit(profile, existing);
	/* (the channels of every bitmap Halo PC keeps elsewhere, the bundles'
	among the level's, as a mod's bitmaps are found: mod_bitmaps.c) */
	if (tag_injector_level.bitmaps_added && !custom_edition_cache_tags_loaded())
		custom_edition_loaded_reordered_bitmaps_find();
	error(_error_silent, "tag injector: %s: %ld tags added and %ld replaced from %ld bundles (%lu bytes of tags), %ld "
		"roots", port_tag_profiles_name(profile), added, replaced, tag_injector_level.bundle_count, total, root_count);
	if (bitmaps_downsized)
	{
		error(_error_silent, "tag injector: %ld bitmaps drawn at %d pixels at most, %lu bytes fewer "
			"(game.tag_injection_textures)", bitmaps_downsized, bitmap_limit, bitmap_bytes_skipped);
	}

	return root_count;
}

long tag_injector_level_profile(
	void)
{
	return tag_injector_level.bundles_read && tag_injector_level.grown_instances ? tag_injector_level.read_profile :
		NONE;
}

void tag_injector_tags_unloaded(
	void)
{
	tag_editor_level_unloaded();
	if (tag_injector_level.saved_instances)
		cache_files_set_tag_instances(tag_injector_level.saved_instances, tag_injector_level.saved_count);
	if (tag_injector_level.grown_instances)
		free(tag_injector_level.grown_instances);
	if (tag_injector_level.injected_bundle)
		free(tag_injector_level.injected_bundle);
	/* (a Custom Edition map's channels are its own, which it forgets as it
	goes; a mod's go with the mods') */
	if (tag_injector_level.bitmaps_added && !custom_edition_cache_tags_loaded())
		custom_edition_bitmaps_dispose();
	custom_edition_injected_models_dispose();
	level_bundles_free();
	{
		long index;

		/* (the game's free does not take NULL) */
		for (index = 0; index < tag_injector_level.allocation_count; index++)
		{
			if (tag_injector_level.allocations[index])
				free(tag_injector_level.allocations[index]);
		}
		if (tag_injector_level.allocations)
			free(tag_injector_level.allocations);
	}
	tag_injector_level.allocations = NULL;
	tag_injector_level.allocation_count = 0;
	tag_injector_level.allocation_capacity = 0;
	tag_injector_level.copied_count = 0;
	tag_injector_level.salt = 0;
	tag_injector_level.saved_instances = NULL;
	tag_injector_level.saved_count = 0;
	tag_injector_level.grown_instances = NULL;
	tag_injector_level.injected_bundle = NULL;
	tag_injector_level.first_injected = 0;
	tag_injector_level.injected_count = 0;
	tag_injector_level.replaced_count = 0;
	tag_injector_level.bitmaps_added = FALSE;
	tag_injector_level.bundles_read = FALSE;
	tag_injector_level.read_profile = NONE;
	tag_injector_level.read_bytes = 0;
	tag_injector_level.notice_lines = 0;
	tag_injector_level.notice_ticks = 0;
}

void tag_injector_tick(
	void)
{
	short local_player_index;
	short line;

	if (!tag_injector_level.notice_lines)
		return;
	/* (the HUD is not drawn in a cutscene, and its messages go before it
	ends: counted while the level is played, its players in their bodies) */
	if (cinematic_in_progress())
		return;
	for (local_player_index = 0; local_player_index < local_player_count(); local_player_index++)
	{
		long player_index = local_player_get_player_index(local_player_index);

		if (player_index == NONE || player_get(player_index)->unit_index == NONE)
			return;
	}
	if (++tag_injector_level.notice_ticks < TAG_INJECTOR_NOTICE_TICKS)
		return;
	for (local_player_index = 0; local_player_index < local_player_count(); local_player_index++)
	{
		/* (the newest message is shown on top: the last line first) */
		for (line = tag_injector_level.notice_lines - 1; line >= 0; line--)
		{
			wchar_t text[TAG_INJECTOR_NOTICE_WIDTH + 4];
			short character;

			for (character = 0; tag_injector_level.notice[line][character] && character < (short)NUMBEROF(text) - 1;
				character++)
			{
				text[character] = (wchar_t)(unsigned char)tag_injector_level.notice[line][character];
			}
			text[character] = 0;
			hud_print_message(local_player_index, text);
		}
	}
	error(_error_silent, "tag injector: the HUD says %d lines of the tags that could not be played",
		tag_injector_level.notice_lines);
	tag_injector_level.notice_lines = 0;
}

char const *tag_injector_failure_describe(
	char const *failed)
{
	short index;

	if (!failed || !failed[0])
		return "";
	for (index = 0; index < (short)NUMBEROF(failures); index++)
	{
		if (!strcmp(failures[index].value, failed))
			return failures[index].described;
	}
	return "it could not be played";
}

boolean tag_injector_tag_injected(
	long tag_index)
{
	long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index);

	return tag_index != NONE && tag_injector_level.injected_count > 0 &&
		absolute >= tag_injector_level.first_injected &&
		absolute < tag_injector_level.first_injected + tag_injector_level.injected_count &&
		tag_injector_level.grown_instances[absolute].tag_index == tag_index;
}

boolean tag_injector_tags_loaded(
	void)
{
	return tag_injector_level.injected_count > 0 || tag_injector_level.replaced_count > 0 ||
		tag_injector_level.copied_count > 0;
}

int tag_injector_read(
	long tag_index,
	long offset,
	long size,
	void *buffer)
{
	struct injected_bundle const *bundle;
	struct injector_tag_instance const *instance;
	unsigned long available;

	if (!tag_injector_level.injected_count || !buffer || size < 0)
		return 0;
	if (!tag_injector_tag_injected(tag_index))
	{
		/* (the renderer keeps each address's channel order until it is told
		another: the level's bitmap where a bundle's was has the Xbox's) */
		if (tag_injector_level.bitmaps_added)
			halo_custom_edition_texels_channels(buffer, _custom_edition_channels_xbox);
		return 0;
	}
	instance = &tag_injector_level.grown_instances[DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index)];
	bundle = &tag_injector_level.bundles[tag_injector_level.injected_bundle[DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index) -
		tag_injector_level.first_injected]];
	/* (what is past its file data, or cannot be read, is none) */
	available = offset >= 0 && (unsigned long)offset < bundle->loaded.file_data_size ?
		MIN((unsigned long)size, bundle->loaded.file_data_size - (unsigned long)offset) : 0;
	if (!available || !bundle_file_read((void *)bundle, (unsigned long)offset, available, buffer))
	{
		error(_error_silent, "tag injector: cannot read 0x%lX bytes at 0x%08lX of '%s'", (unsigned long)size,
			(unsigned long)offset, instance->name);
		available = 0;
	}
	memset((byte *)buffer + available, 0, (unsigned long)size - available);
	if ((unsigned long)instance->group_tag == BITMAP_GROUP_TAG)
	{
		struct bitmap_group *group = instance->base_address;
		long bitmap_index;

		/* the texture cache sets the address its pixels go to before it
		reads them (as custom_edition_bitmap_pixels_arrived finds them) */
		for (bitmap_index = 0; bitmap_index < group->bitmaps.count; bitmap_index++)
		{
			struct bitmap_data *bitmap = TAG_BLOCK_GET_ELEMENT(&group->bitmaps, bitmap_index, struct bitmap_data);

			if (bitmap->base_address == buffer && bitmap->pixels_offset == offset)
			{
				halo_custom_edition_texels_channels(buffer,
					(unsigned char)custom_edition_bitmap_channel_order(tag_index, bitmap_index));
				/* (on failure it logs, and the texture shows the pixels as read) */
				rasterizer_xbox_bitmap_rebuild_hardware_format(bitmap);
				break;
			}
		}
	}

	return 1;
}
