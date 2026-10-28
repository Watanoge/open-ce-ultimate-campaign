/*
FIRING_RANGE.C

The firing range (firing_range.h; docs/tag_injector.md, "The firing
range"): a level played alone to try a Tag Injector profile's weapons and
AI, opened from the profile in the Tag Injector menu.

It is Blood Gulch, for all a level needs but its ground (the globals, the
sky, the sounds and the shaders), with its structure bsp never read: one is
written where it would be (firing_range_structure_bsp_write), a square
floor walled round, its materials' vertices uncompressed as a Custom Edition
map's are, which custom_edition_geometry.c compresses and gives buffers. It
has no lightmap: structure_render.c draws a bsp without one by its own
directional light, and objects are lit by the bsp's default lighting
(object_lights.c). While it is loaded the map's own objects are taken out of
its scenario and its players start at the middle (put back as its tags go,
for the next level of the map).

Its sides are the range's parts: west the profile's weapons, laid on benches
(the bsp's own, as many as they need), each put back a moment after it is
taken; south a mirror, a frame on the floor (drawn alone: no collision), past
which a copy of the player (its biped, holding its weapons) moves as the
player does, reflected, while the player stands before it: its controls are
the player's each tick (firing_range_player_control), turned about the
frame, so that it plays every animation the player does, seen from the
front; its
shots and grenades hurt no one and nothing hurts it. It is made as the
player steps up, and let go as the player steps back; its weapons are the
player's slot for slot (units.c's unit_network_* helpers, as a network
client's are: unit_delete_all_weapons keeps the one in hand, which left the
copy holding the weapon it had). North, the profile's AI: each a unit of
its actor variant standing still on its place, unarmed, playing every base
animation of its graph in turn, which nothing hurts; action by one (the HUD
names it as the player comes near) puts a live actor of it, of no encounter
(actors.c's actor_port_place_free, as the Outbreak skull's), in the arena
further north, holding one of the profile's weapons by chance.

The bsp, as the engine reads one:

- its collision bsp: the floor, the four walls and a ceiling, each a
  surface of its own plane facing in, and each bench's top and sides, each
  of its own plane facing out. Its bsp3d is a tree (collision_bsp.c walks no
  node twice in a test): the room's nodes a chain, each with solid behind it
  and the next in front, the last's front the benches', split in halves by
  planes between them (planes of no surface) until each bench has its own
  part, whose nodes have the one leaf in front and behind the bench's next
  node, its last's solid (a point is in the bench when it is behind all of
  its planes). The one leaf names every surface (each its plane's bsp2d:
  the surface itself). The
  surfaces' edge rings run counter-clockwise about their planes' normals,
  as the stock maps' do; a bench's bottom edges have one surface;
- its render surfaces: the floor in tiles, the walls in panels, the benches'
  faces, the mirror's line and frame, and its pane, two triangles each,
  clockwise seen from the front (the stock maps' are), in four materials
  (three when the map has not got the pane's shader) of one lightmap without
  a bitmap;
- one cluster, which sees itself, whose subclusters (the floor's blocks of
  tiles, each wall's quarters, the benches and the line) list the surfaces
  the renderer culls by their bounds; one structure leaf of that cluster
  naming every surface, and one structure node of whole bounds for each
  bsp3d node.
*/

/* ---------- headers */

#include "cseries.h"
#include "errors.h"
#include "cache/cache_files.h"
#include "game/game.h"
#include "ai/actor_definitions.h"
#include "ai/actors.h"
#include "game/players.h"
#include "interface/hud_messaging.h"
#include "items/weapons.h"
#include "math/integer_math.h"
#include "models/model_animation_definitions.h"
#include "objects/objects.h"
#include "physics/collision_bsp_definitions.h"
#include "scenario/scenario_definitions.h"
#include "shaders/shader_definitions.h"
#include "structures/structure_bsp_definitions.h"
#include "tag_files/tag_groups.h"
#include "units/unit_control_data.h"
#include "units/units.h"
#include "firing_range.h"
#include "tag_injector.h"

/* units.c's: a weapon put in a unit's slot, and a slot emptied (its weapon
about to be deleted), as a network client's units are given the host's */
void unit_network_add_weapon(long unit_index, long weapon_index, short slot);
void unit_network_forget_weapon(long unit_index, short slot);

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ---------- constants */

enum
{
	SCENARIO_GROUP_TAG = 'scnr',
	SHADER_ENVIRONMENT_GROUP_TAG = 'senv',
	BITMAP_GROUP_TAG = 'bitm',
	WEAPON_GROUP_TAG = 'weap',
	ACTOR_VARIANT_GROUP_TAG = 'actv',

	/* the AI stood north (each once), in rows of so many, and the live
	ones in the arena at once (the oldest let go for another) */
	MAXIMUM_RANGE_AIS = 32,
	RANGE_AIS_PER_ROW = 8,
	MAXIMUM_RANGE_FIGHTERS = 8,
	/* the longest name kept */
	RANGE_NAME_LENGTH = 48,
	STRUCTURE_BSP_HEADER_SIGNATURE = 'sbsp',

	/* the floor's tiles a side, the walls' panels a side, and the
	subclusters a side of the floor (and of each wall) */
	RANGE_TILES = 16,
	RANGE_SUBCLUSTERS = 4,

	/* the room's faces (the floor, the walls east, west, north and south,
	and the ceiling), and a bench's (its top and its sides) */
	_range_face_floor = 0,
	_range_face_east,
	_range_face_west,
	_range_face_north,
	_range_face_south,
	_range_face_ceiling,
	NUMBER_OF_ROOM_FACES,
	NUMBER_OF_RANGE_WALLS = 4,
	NUMBER_OF_BENCH_FACES = 5,
	BOX_CORNERS = 8,

	/* the weapons laid out (the profile's, each once), on how many
	benches at most, and how many a bench */
	MAXIMUM_RANGE_WEAPONS = 96,
	RANGE_WEAPONS_PER_BENCH = 12,
	MAXIMUM_RANGE_BENCHES = MAXIMUM_RANGE_WEAPONS / RANGE_WEAPONS_PER_BENCH,

	/* the blocks standing on the floor: the benches, then the arena's walls
	(north, west, east, and the south's either side of its door) */
	NUMBER_OF_ARENA_WALLS = 5,
	MAXIMUM_RANGE_BLOCKS = MAXIMUM_RANGE_BENCHES + NUMBER_OF_ARENA_WALLS,

	MAXIMUM_RANGE_FACES = NUMBER_OF_ROOM_FACES + MAXIMUM_RANGE_BLOCKS * NUMBER_OF_BENCH_FACES,
	/* the bsp3d's planes and nodes: the faces', and those between the
	blocks */
	MAXIMUM_RANGE_PLANES = MAXIMUM_RANGE_FACES + MAXIMUM_RANGE_BLOCKS,
	MAXIMUM_RANGE_NODES = MAXIMUM_RANGE_PLANES,
	MAXIMUM_RANGE_COLLISION_VERTICES = BOX_CORNERS * (1 + MAXIMUM_RANGE_BLOCKS),
	MAXIMUM_RANGE_EDGES = 12 * (1 + MAXIMUM_RANGE_BLOCKS),

	/* the render surfaces: the floor's, the walls', and the details' (each
	bench's faces, and the mirror's line) */
	RANGE_FLOOR_SURFACES = RANGE_TILES * RANGE_TILES * 2,
	RANGE_WALL_SURFACES = RANGE_TILES * 2,
	RANGE_FLOOR_VERTICES = (RANGE_TILES + 1) * (RANGE_TILES + 1),
	RANGE_WALL_VERTICES = (RANGE_TILES + 1) * 2,
	/* the details: each bench's faces, the mirror's line, and its frame (two
	posts of five faces, a bar of six) */
	RANGE_MIRROR_FRAME_QUADS = 2 * 5 + 6,
	MAXIMUM_RANGE_DETAIL_QUADS = MAXIMUM_RANGE_BLOCKS * NUMBER_OF_BENCH_FACES + 1 + RANGE_MIRROR_FRAME_QUADS,
	/* the materials: the floor's, the walls' and the details' (the
	collision's too) */
	NUMBER_OF_RANGE_COLLISION_MATERIALS = 3,
	NUMBER_OF_RANGE_MATERIALS = 3,
	RANGE_ROOM_SUBCLUSTERS = RANGE_SUBCLUSTERS * RANGE_SUBCLUSTERS + NUMBER_OF_RANGE_WALLS * RANGE_SUBCLUSTERS,

	/* the map's players' starting locations kept to put back */
	MAXIMUM_RANGE_STARTING_LOCATIONS = 64,

	/* a weapon taken is put back this long after its bench is empty, and
	the benches are looked at this often */
	RANGE_REFILL_TICKS = 3 * 30,
	RANGE_CHECK_TICKS = 15,
};

/* the plane's half width, the walls' height drawn and the ceiling's (its
collision's top, and the bsp's bounds'), in world units (one is ten feet:
the player stands 0.7 tall) */
#define RANGE_HALF_WIDTH 80.0f
#define RANGE_WALL_HEIGHT 8.0f
#define RANGE_CEILING 100.0f
/* texture coordinates a world unit */
#define RANGE_TEXTURE_SCALE 0.5f
/* where the players start: at the middle, a little above the floor, facing
north (+y) */
#define RANGE_START_HEIGHT 0.25f
#define RANGE_START_SPACING 1.5f
#define RANGE_START_FACING 1.5707963f
/* the benches: the first's middle west of the start, the next further
west each; their width, height and length (north to south), the room a
weapon has on one, how high above it one is laid and how near its place
one must lie to be there */
#define RANGE_BENCH_FIRST_X -6.0f
#define RANGE_BENCH_SPACING 3.0f
#define RANGE_BENCH_WIDTH 0.6f
#define RANGE_BENCH_HEIGHT 0.3f
#define RANGE_WEAPON_SPACING 0.9f
#define RANGE_BENCH_LENGTH (RANGE_WEAPONS_PER_BENCH * RANGE_WEAPON_SPACING)
#define RANGE_WEAPON_DROP 0.08f
#define RANGE_WEAPON_NEAR 0.6f
/* the mirror: its line (y) south of the start, how far across it reaches
each way and how far before it the player is mirrored; the line's width,
and how far off the copy may be before it is put where it should be */
#define RANGE_MIRROR_Y -8.0f
#define RANGE_MIRROR_HALF_WIDTH 6.0f
#define RANGE_MIRROR_DEPTH 6.0f
#define RANGE_MIRROR_LINE_WIDTH 0.1f
#define RANGE_MIRROR_LINE_HEIGHT 0.01f
#define RANGE_MIRROR_DRIFT 0.25f
/* the arena north of the start, where the AI fought are put: its inside's
bounds, its walls' height and thickness, its south wall's door's width, and
the gap left at its walls' ends (the collision bsp parts them by planes
between them: collision_bsp.c finds no surface on a plane of none, so
touching walls would be passed through there) */
#define RANGE_ARENA_X0 -8.0f
#define RANGE_ARENA_X1 8.0f
#define RANGE_ARENA_Y0 26.0f
#define RANGE_ARENA_Y1 40.0f
#define RANGE_ARENA_WALL_HEIGHT 1.0f
#define RANGE_ARENA_WALL_THICKNESS 0.3f
#define RANGE_ARENA_DOOR_WIDTH 2.0f
#define RANGE_BLOCK_GAP 0.05f
/* the AI's places: the first row's middle north of the start, the room
each has, and how near one the player is to fight it; how far off its place
one may move as it plays before it is put back */
#define RANGE_AI_FIRST_Y 9.0f
#define RANGE_AI_SPACING 2.5f
#define RANGE_AI_NEAR 1.5f
#define RANGE_AI_DRIFT 0.05f
/* the frame: its posts' width and height, and its bar's height */
#define RANGE_MIRROR_POST_WIDTH 0.15f
#define RANGE_MIRROR_FRAME_HEIGHT 1.4f
#define RANGE_MIRROR_BAR_HEIGHT 0.12f

/* the map the range is played on, and its materials' shaders (Blood
Gulch's own): their groups and paths */
static char const range_map_name[] = "levels\\test\\bloodgulch\\bloodgulch";
static struct
{
	unsigned long group_tag;
	char const *path;
} const range_shaders[NUMBER_OF_RANGE_MATERIALS] =
{
	{ SHADER_ENVIRONMENT_GROUP_TAG, "levels\\b30\\shaders\\metal flat generic" },
	{ SHADER_ENVIRONMENT_GROUP_TAG, "levels\\b30\\shaders\\metal panels generic ext" },
	{ SHADER_ENVIRONMENT_GROUP_TAG, "levels\\test\\bloodgulch\\shaders\\cap_metal" },
};

/* ---------- structures */

/* cache_files.c's struct cache_file_structure_bsp_header */
struct range_structure_bsp_header
{
	void *base_address;
	long vertex_buffer_count;
	void *vertex_buffers;
	long index_buffer_count;
	void *index_buffers;
	unsigned long signature;
};

/* collision_bsp.c's */
struct range_collision_leaf
{
	word flags;
	short bsp2d_reference_count;
	long first_bsp2d_reference_index;
};

struct range_bsp2d_reference
{
	long plane_designator;
	long root_index;
};

/* structure_visibility.c's cluster and subcluster, and a structure leaf's
surface reference (tag_schema_collision.c's) */
struct range_cluster
{
	short sky_index;
	short fog_designator;
	short background_sound_palette_index;
	short sound_environment_palette_index;
	short weather_palette_index;
	short transition_structure_bsp_index;
	short first_runtime_decal_index;
	word runtime_decal_count;
	long unused[6];
	struct tag_block predicted_resources;
	struct tag_block subclusters;
	word first_lens_flare_marker_index;
	word lens_flare_marker_count;
	struct tag_block surface_indices;
	struct tag_block mirrors;
	struct tag_block portal_indices;
};

struct range_subcluster
{
	real_rectangle3d world_bounds;
	struct tag_block surface_indices;
};

struct range_surface_reference
{
	long surface_index;
	long bsp3d_node_index;
};

/* rasterizer_geometry.c's uncompressed environment vertices */
struct range_vertex
{
	real_point3d position;
	real_vector3d normal;
	real_vector3d binormal;
	real_vector3d tangent;
	real_point2d texcoord;
};

struct range_lightmap_vertex
{
	real_vector3d incident_radiosity;
	real_point2d texcoord;
};

typedef char range_structure_bsp_header_size_assert[sizeof(struct range_structure_bsp_header) == 0x18 ? 1 : -1];
typedef char range_collision_leaf_size_assert[sizeof(struct range_collision_leaf) == 0x08 ? 1 : -1];
typedef char range_bsp2d_reference_size_assert[sizeof(struct range_bsp2d_reference) == 0x08 ? 1 : -1];
typedef char range_cluster_size_assert[sizeof(struct range_cluster) == 0x68 ? 1 : -1];
typedef char range_subcluster_size_assert[sizeof(struct range_subcluster) == 0x24 ? 1 : -1];
typedef char range_vertex_size_assert[sizeof(struct range_vertex) == 0x38 ? 1 : -1];
typedef char range_lightmap_vertex_size_assert[sizeof(struct range_lightmap_vertex) == 0x14 ? 1 : -1];

/* the bsp written, as it is: what is left of the room, and whether it ran
out */
struct range_writer
{
	byte *base;
	unsigned long size;
	unsigned long used;
	boolean full;
};

/* a collision face: its normal (towards the room's empty space), its
corners (the collision bsp's vertices), its material and flags */
struct range_face
{
	real_vector3d normal;
	long corners[4];
	short material;
	byte flags;
};

/* the collision bsp's faces and vertices, as they are laid out, and its
bsp3d's planes (the faces', then those between the benches) and nodes, and
each face's node */
struct range_layout
{
	struct range_face faces[MAXIMUM_RANGE_FACES];
	short face_count;
	real_point3d vertices[MAXIMUM_RANGE_COLLISION_VERTICES];
	short vertex_count;
	short bench_count;
	real_rectangle3d blocks[MAXIMUM_RANGE_BLOCKS];
	short block_count;
	real_plane3d planes[MAXIMUM_RANGE_PLANES];
	short plane_count;
	struct bsp3d_node nodes[MAXIMUM_RANGE_NODES];
	short node_count;
	short face_nodes[MAXIMUM_RANGE_FACES];
};

/* a material's vertices as they are made, and its surfaces' */
struct range_material_build
{
	struct range_vertex *vertices;
	struct range_lightmap_vertex *lightmap_vertices;
	long vertex_count;
	long first_surface_index;
	long surface_count;
};

/* ---------- globals */

static struct
{
	/* the range asked for (the next level of its map is it), and whether
	the level loaded is it, with its profile */
	boolean requested;
	boolean active;
	char profile[FIRING_RANGE_NAME_LENGTH];
	/* whether the bsp loaded is the one written */
	boolean bsp_written;

	/* the scenario's blocks taken out (their counts as they were), and its
	players' starting locations as they were */
	struct scenario *scenario;
	long emptied_counts[16];
	short emptied_count;
	struct player_starting_location starting_locations[MAXIMUM_RANGE_STARTING_LOCATIONS];
	short starting_location_count;
	long players_count;

	/* the profile's weapons the level added, each laid on a bench: the
	weapon lying there, and when its place was last seen empty (NONE: not
	empty) */
	long weapons[MAXIMUM_RANGE_WEAPONS];
	short weapon_count;
	long weapon_objects[MAXIMUM_RANGE_WEAPONS];
	long weapon_empty_since[MAXIMUM_RANGE_WEAPONS];
	long next_check_time;

	/* the mirror's copy of the player (NONE: none), the player's unit it
	copies, and whether the player stood before the mirror (the HUD says
	what it is as the player steps up) */
	long mirror_unit;
	long mirrored_unit;
	boolean before_mirror;

	/* the weapons' names (as the profile names them) */
	char weapon_names[MAXIMUM_RANGE_WEAPONS][RANGE_NAME_LENGTH];

	/* the profile's AI: each's actor variant and name, the unit standing
	for it and the animation it plays; the live ones in the arena, the
	next place among them, and the AI the player is near (NONE: none) and
	whether its action was held */
	struct
	{
		long variant;
		char name[RANGE_NAME_LENGTH];
		long unit;
		short animation;
	} ais[MAXIMUM_RANGE_AIS];
	short ai_count;
	long fighters[MAXIMUM_RANGE_FIGHTERS];
	short next_fighter;
	short near_ai;
	boolean action_held;
	unsigned long random;
} firing_range;

/* ---------- private code: the bsp's room */

static void *range_allocate(
	struct range_writer *writer,
	unsigned long size)
{
	unsigned long start = (writer->used + 15UL) & ~15UL;
	void *result;

	if (writer->full || start > writer->size || size > writer->size - start)
	{
		writer->full = TRUE;
		return NULL;
	}
	result = writer->base + start;
	csmemset(result, 0, size);
	writer->used = start + size;

	return result;
}

/* a block of `count` elements, zeroed (NULL, and the writer full, when
there is no room) */
static void *range_block(
	struct range_writer *writer,
	struct tag_block *block,
	long count,
	unsigned long element_size)
{
	void *elements = count > 0 ? range_allocate(writer, (unsigned long)count * element_size) : NULL;

	block->count = elements ? count : 0;
	block->address = elements;
	block->definition = NULL;

	return elements;
}

static void range_reference(
	struct tag_reference *reference,
	unsigned long group_tag,
	long index)
{
	reference->group_tag = group_tag;
	reference->index = index;
	reference->name = index == NONE ? NULL : tag_get_name(index);
	reference->name_length = reference->name ? (long)strlen(reference->name) : 0;
}

/* ---------- private code: geometry */

static void range_point(
	real_point3d *point,
	real x,
	real y,
	real z)
{
	point->x = x;
	point->y = y;
	point->z = z;
}

static void range_vector(
	real_vector3d *vector,
	real i,
	real j,
	real k)
{
	vector->i = i;
	vector->j = j;
	vector->k = k;
}

static void range_cross(
	real_vector3d const *a,
	real_vector3d const *b,
	real_vector3d *result)
{
	range_vector(result, a->j * b->k - a->k * b->j, a->k * b->i - a->i * b->k, a->i * b->j - a->j * b->i);
}

/* `count` points' order counter-clockwise about `normal`, seen from its
front (each's angle about their middle) */
static void range_ring_order(
	real_point3d const *points,
	short count,
	real_vector3d const *normal,
	short *order)
{
	real_vector3d u;
	real_vector3d v;
	real_point3d middle;
	real angles[8];
	short index;

	range_point(&middle, 0.0f, 0.0f, 0.0f);
	/* (u across the normal, v = normal x u: u x v is the normal) */
	if (fabsf(normal->k) < 0.9f)
		range_vector(&u, normal->j, -normal->i, 0.0f);
	else
		range_vector(&u, 1.0f, 0.0f, 0.0f);
	{
		real length = sqrtf(u.i * u.i + u.j * u.j + u.k * u.k);

		u.i /= length;
		u.j /= length;
		u.k /= length;
	}
	range_cross(normal, &u, &v);
	for (index = 0; index < count; index++)
	{
		middle.x += points[index].x / count;
		middle.y += points[index].y / count;
		middle.z += points[index].z / count;
	}
	for (index = 0; index < count; index++)
	{
		real dx = points[index].x - middle.x;
		real dy = points[index].y - middle.y;
		real dz = points[index].z - middle.z;

		angles[index] = atan2f(dx * v.i + dy * v.j + dz * v.k, dx * u.i + dy * u.j + dz * u.k);
		order[index] = index;
	}
	/* (a few points: sorted by insertion) */
	for (index = 1; index < count; index++)
	{
		short moved = order[index];
		short place = index;

		while (place > 0 && angles[order[place - 1]] > angles[moved])
		{
			order[place] = order[place - 1];
			place--;
		}
		order[place] = moved;
	}
}

/* ---------- private code: the layout */

static void range_tree_make(
	struct range_layout *layout);

/* a box's face on an axis (0: x, 1: y, 2: z) at its low or high side: its
outward normal, and its corners (a box's corner: bit 0 its high x, bit 1
its high y, bit 2 its high z) */
static void range_box_face(
	struct range_layout *layout,
	long first_corner,
	short axis,
	boolean high,
	boolean inward,
	short material,
	byte flags)
{
	struct range_face *face = &layout->faces[layout->face_count++];
	real sign = (high ? 1.0f : -1.0f) * (inward ? -1.0f : 1.0f);
	short corner;
	short count = 0;

	range_vector(&face->normal, axis == 0 ? sign : 0.0f, axis == 1 ? sign : 0.0f, axis == 2 ? sign : 0.0f);
	for (corner = 0; corner < BOX_CORNERS; corner++)
	{
		if (((corner >> axis) & 1) == (high ? 1 : 0))
			face->corners[count++] = first_corner + corner;
	}
	face->material = material;
	face->flags = flags;
}

static long range_box_corners(
	struct range_layout *layout,
	real x0,
	real x1,
	real y0,
	real y1,
	real z0,
	real z1)
{
	long first = layout->vertex_count;
	short corner;

	for (corner = 0; corner < BOX_CORNERS; corner++)
		range_point(&layout->vertices[layout->vertex_count++], corner & 1 ? x1 : x0, corner & 2 ? y1 : y0,
			corner & 4 ? z1 : z0);
	return first;
}

/* a bench's bounds (its index of the benches) */
static void range_bench_bounds(
	short bench,
	real_rectangle3d *bounds)
{
	real x = RANGE_BENCH_FIRST_X - bench * RANGE_BENCH_SPACING;

	bounds->x0 = x - RANGE_BENCH_WIDTH * 0.5f;
	bounds->x1 = x + RANGE_BENCH_WIDTH * 0.5f;
	bounds->y0 = -RANGE_BENCH_LENGTH * 0.5f;
	bounds->y1 = RANGE_BENCH_LENGTH * 0.5f;
	bounds->z0 = 0.0f;
	bounds->z1 = RANGE_BENCH_HEIGHT;
}

/* where a weapon of the profile's is laid (its index of the weapons) */
static void range_weapon_place(
	short weapon,
	real_point3d *place)
{
	real_rectangle3d bounds;

	range_bench_bounds((short)(weapon / RANGE_WEAPONS_PER_BENCH), &bounds);
	range_point(place, (bounds.x0 + bounds.x1) * 0.5f,
		bounds.y1 - RANGE_WEAPON_SPACING * ((weapon % RANGE_WEAPONS_PER_BENCH) + 0.5f),
		bounds.z1 + RANGE_WEAPON_DROP);
}

static void range_layout_make(
	struct range_layout *layout,
	short weapon_count)
{
	long first;
	short bench;

	csmemset(layout, 0, sizeof(*layout));
	/* (the room: in this order, its faces' nodes the first) */
	first = range_box_corners(layout, -RANGE_HALF_WIDTH, RANGE_HALF_WIDTH, -RANGE_HALF_WIDTH, RANGE_HALF_WIDTH,
		0.0f, RANGE_CEILING);
	range_box_face(layout, first, 2, FALSE, TRUE, 0, 0);
	range_box_face(layout, first, 0, TRUE, TRUE, 1, 0);
	range_box_face(layout, first, 0, FALSE, TRUE, 1, 0);
	range_box_face(layout, first, 1, TRUE, TRUE, 1, 0);
	range_box_face(layout, first, 1, FALSE, TRUE, 1, 0);
	/* (the ceiling unseen: the collision's alone) */
	range_box_face(layout, first, 2, TRUE, TRUE, 1, FLAG(1));
	/* (the benches, then the arena's walls: a gap between each two that
meet) */
	layout->bench_count = (short)((weapon_count + RANGE_WEAPONS_PER_BENCH - 1) / RANGE_WEAPONS_PER_BENCH);
	for (bench = 0; bench < layout->bench_count; bench++)
		range_bench_bounds(bench, &layout->blocks[layout->block_count++]);
	{
		real t = RANGE_ARENA_WALL_THICKNESS;
		real g = RANGE_BLOCK_GAP;
		real h = RANGE_ARENA_WALL_HEIGHT;
		real door = RANGE_ARENA_DOOR_WIDTH * 0.5f;
		real_rectangle3d walls[NUMBER_OF_ARENA_WALLS] =
		{
			{ RANGE_ARENA_X0 - t, RANGE_ARENA_X1 + t, RANGE_ARENA_Y1, RANGE_ARENA_Y1 + t, 0.0f, h },
			{ RANGE_ARENA_X0 - t, RANGE_ARENA_X0, RANGE_ARENA_Y0 - t, RANGE_ARENA_Y1 - g, 0.0f, h },
			{ RANGE_ARENA_X1, RANGE_ARENA_X1 + t, RANGE_ARENA_Y0 - t, RANGE_ARENA_Y1 - g, 0.0f, h },
			{ RANGE_ARENA_X0 + g, -door, RANGE_ARENA_Y0 - t, RANGE_ARENA_Y0, 0.0f, h },
			{ door, RANGE_ARENA_X1 - g, RANGE_ARENA_Y0 - t, RANGE_ARENA_Y0, 0.0f, h },
		};
		short wall;

		for (wall = 0; wall < NUMBER_OF_ARENA_WALLS; wall++)
			layout->blocks[layout->block_count++] = walls[wall];
	}
	for (bench = 0; bench < layout->block_count; bench++)
	{
		real_rectangle3d const *bounds = &layout->blocks[bench];

		first = range_box_corners(layout, bounds->x0, bounds->x1, bounds->y0, bounds->y1, bounds->z0, bounds->z1);
		range_box_face(layout, first, 2, TRUE, FALSE, 2, 0);
		range_box_face(layout, first, 0, TRUE, FALSE, 2, 0);
		range_box_face(layout, first, 0, FALSE, FALSE, 2, 0);
		range_box_face(layout, first, 1, TRUE, FALSE, 2, 0);
		range_box_face(layout, first, 1, FALSE, FALSE, 2, 0);
	}
	range_tree_make(layout);
}

/* ---------- private code: the collision bsp */

/* a node of a plane, and its children */
static short range_node_add(
	struct range_layout *layout,
	short plane,
	long back,
	long front)
{
	short node = layout->node_count++;

	layout->nodes[node].plane_designator = plane;
	layout->nodes[node].children[0] = back;
	layout->nodes[node].children[1] = front;
	return node;
}

/* a block's bounds on an axis */
static void range_block_span(
	real_rectangle3d const *bounds,
	short axis,
	real *low,
	real *high)
{
	*low = axis == 0 ? bounds->x0 : axis == 1 ? bounds->y0 : bounds->z0;
	*high = axis == 0 ? bounds->x1 : axis == 1 ? bounds->y1 : bounds->z1;
}

/* the blocks listed: one's nodes (the leaf in front of each, the next
behind, solid behind the last), else a plane parting them in two, on an
axis where those below it end before those above begin (the first found,
nearest the middle of the list), the upper part in front; NONE (solid)
when they cannot be parted (a layout's blocks always can) */
static long range_blocks_tree(
	struct range_layout *layout,
	short const *blocks,
	short count)
{
	long leaf = (long)(0 | LONG_MIN);
	short axis;

	if (count == 1)
	{
		short face = (short)(NUMBER_OF_ROOM_FACES + blocks[0] * NUMBER_OF_BENCH_FACES);
		short node = layout->node_count;
		short index;

		for (index = 0; index < NUMBER_OF_BENCH_FACES; index++)
		{
			layout->face_nodes[face + index] = range_node_add(layout, (short)(face + index),
				index + 1 < NUMBER_OF_BENCH_FACES ? node + index + 1 : NONE, leaf);
		}
		return node;
	}
	for (axis = 0; axis < 3; axis++)
	{
		short sorted[MAXIMUM_RANGE_BLOCKS];
		short best = NONE;
		real best_plane = 0.0f;
		short index;

		/* (sorted by where they begin on the axis) */
		for (index = 0; index < count; index++)
		{
			short place = index;
			real low;
			real high;
			real other_low;

			range_block_span(&layout->blocks[blocks[index]], axis, &low, &high);
			while (place > 0)
			{
				range_block_span(&layout->blocks[sorted[place - 1]], axis, &other_low, &high);
				if (other_low <= low)
					break;
				sorted[place] = sorted[place - 1];
				place--;
			}
			sorted[place] = blocks[index];
		}
		for (index = 1; index < count; index++)
		{
			real below_high = -1.0e9f;
			real above_low;
			real low;
			real high;
			short other;

			for (other = 0; other < index; other++)
			{
				range_block_span(&layout->blocks[sorted[other]], axis, &low, &high);
				below_high = MAX(below_high, high);
			}
			range_block_span(&layout->blocks[sorted[index]], axis, &above_low, &high);
			if (below_high < above_low &&
				(best == NONE || abs(index * 2 - count) < abs(best * 2 - count)))
			{
				best = index;
				best_plane = (below_high + above_low) * 0.5f;
			}
		}
		if (best != NONE)
		{
			short plane = layout->plane_count++;
			short node;

			range_vector(&layout->planes[plane].n, axis == 0 ? 1.0f : 0.0f, axis == 1 ? 1.0f : 0.0f,
				axis == 2 ? 1.0f : 0.0f);
			layout->planes[plane].d = best_plane;
			node = range_node_add(layout, plane, NONE, NONE);
			layout->nodes[node].children[1] = range_blocks_tree(layout, sorted + best, (short)(count - best));
			layout->nodes[node].children[0] = range_blocks_tree(layout, sorted, best);
			return node;
		}
	}
	error(_error_silent, "firing range: %d of the plane's blocks cannot be parted", count);
	return NONE;
}

/* the bsp3d: the faces' planes, the room's chain, then the benches' tree */
static void range_tree_make(
	struct range_layout *layout)
{
	short face;

	layout->plane_count = layout->face_count;
	layout->node_count = 0;
	for (face = 0; face < layout->face_count; face++)
	{
		struct range_face const *source = &layout->faces[face];
		real_point3d const *point = &layout->vertices[source->corners[0]];

		layout->planes[face].n = source->normal;
		layout->planes[face].d = source->normal.i * point->x + source->normal.j * point->y +
			source->normal.k * point->z;
	}
	for (face = 0; face < NUMBER_OF_ROOM_FACES; face++)
		layout->face_nodes[face] = range_node_add(layout, face, NONE, face + 1);
	{
		short blocks[MAXIMUM_RANGE_BLOCKS];
		short block;

		for (block = 0; block < layout->block_count; block++)
			blocks[block] = block;
		layout->nodes[NUMBER_OF_ROOM_FACES - 1].children[1] = layout->block_count ?
			range_blocks_tree(layout, blocks, layout->block_count) : (long)(0 | LONG_MIN);
	}
}

static boolean range_collision_bsp_write(
	struct range_writer *writer,
	struct range_layout const *layout,
	struct collision_bsp *bsp)
{
	short face_count = layout->face_count;
	struct bsp3d_node *nodes = range_block(writer, &bsp->bsp3d.nodes, layout->node_count, sizeof(*nodes));
	real_plane3d *planes = range_block(writer, &bsp->bsp3d.planes, layout->plane_count, sizeof(*planes));
	struct range_collision_leaf *leaf = range_block(writer, &bsp->leaves, 1, sizeof(*leaf));
	struct range_bsp2d_reference *references = range_block(writer, &bsp->bsp2d_references, face_count,
		sizeof(*references));
	struct collision_surface *surfaces = range_block(writer, &bsp->surfaces, face_count, sizeof(*surfaces));
	struct collision_edge *edges = range_allocate(writer, MAXIMUM_RANGE_EDGES * sizeof(*edges));
	struct collision_vertex *vertices = range_block(writer, &bsp->vertices, layout->vertex_count, sizeof(*vertices));
	long face_edges[MAXIMUM_RANGE_FACES][4];
	long edge_count = 0;
	short face;
	short vertex;

	if (writer->full)
		return FALSE;
	csmemcpy(nodes, layout->nodes, layout->node_count * sizeof(*nodes));
	csmemcpy(planes, layout->planes, layout->plane_count * sizeof(*planes));
	bsp->bsp2d.nodes.count = 0;
	for (vertex = 0; vertex < layout->vertex_count; vertex++)
	{
		vertices[vertex].point = layout->vertices[vertex];
		vertices[vertex].first_edge_index = NONE;
	}
	for (face = 0; face < face_count; face++)
	{
		struct range_face const *source = &layout->faces[face];
		real_point3d points[4];
		short order[4];
		short index;

		for (index = 0; index < 4; index++)
			points[index] = layout->vertices[source->corners[index]];
		references[face].plane_designator = face;
		references[face].root_index = (long)((unsigned long)face | (unsigned long)LONG_MIN);
		surfaces[face].plane_designator = face;
		surfaces[face].flags = source->flags;
		surfaces[face].breakable_surface_index = (byte)NONE;
		surfaces[face].material_index = source->material;

		/* (its edges, each shared with the face across it, which walks it
		the other way: a bench's bottom edges have none) */
		range_ring_order(points, 4, &source->normal, order);
		for (index = 0; index < 4; index++)
		{
			long from = source->corners[order[index]];
			long to = source->corners[order[(index + 1) % 4]];
			long edge;

			for (edge = 0; edge < edge_count; edge++)
			{
				if (edges[edge].vertex_indices[0] == to && edges[edge].vertex_indices[1] == from)
					break;
			}
			if (edge == edge_count)
			{
				if (edge_count == MAXIMUM_RANGE_EDGES)
					return FALSE;
				edges[edge].vertex_indices[0] = from;
				edges[edge].vertex_indices[1] = to;
				edges[edge].surface_indices[0] = face;
				edges[edge].surface_indices[1] = NONE;
				edges[edge].edge_indices[0] = NONE;
				edges[edge].edge_indices[1] = NONE;
				edge_count++;
			}
			else
			{
				edges[edge].surface_indices[1] = face;
			}
			face_edges[face][index] = edge;
			if (vertices[from].first_edge_index == NONE)
				vertices[from].first_edge_index = edge;
		}
		surfaces[face].first_edge_index = face_edges[face][0];
	}
	for (face = 0; face < face_count; face++)
	{
		short index;

		for (index = 0; index < 4; index++)
		{
			struct collision_edge *edge = &edges[face_edges[face][index]];

			edge->edge_indices[edge->surface_indices[0] == face ? 0 : 1] = face_edges[face][(index + 1) % 4];
		}
	}
	bsp->edges.count = edge_count;
	bsp->edges.address = edges;
	bsp->edges.definition = NULL;
	leaf->flags = 0;
	leaf->bsp2d_reference_count = face_count;
	leaf->first_bsp2d_reference_index = 0;

	return TRUE;
}

/* ---------- private code: the render surfaces */

static long range_vertex_add(
	struct range_material_build *build,
	real_point3d const *position,
	real_vector3d const *normal,
	real_vector3d const *tangent,
	real u,
	real v)
{
	struct range_vertex *vertex = &build->vertices[build->vertex_count];
	struct range_lightmap_vertex *lightmap_vertex = &build->lightmap_vertices[build->vertex_count];

	vertex->position = *position;
	vertex->normal = *normal;
	vertex->tangent = *tangent;
	/* (binormal = normal x tangent) */
	range_cross(normal, tangent, &vertex->binormal);
	vertex->texcoord.x = u;
	vertex->texcoord.y = v;
	lightmap_vertex->incident_radiosity = *normal;

	return build->vertex_count++;
}

/* a quad's two triangles (its corners, of the material's vertices, in any
order), clockwise seen from the front of `normal` */
static void range_quad_add(
	struct structure_surface *surfaces,
	long *surface_index,
	struct range_material_build const *build,
	long const *corners,
	real_vector3d const *normal)
{
	real_point3d points[4];
	short order[4];
	long ring[4];
	short index;

	for (index = 0; index < 4; index++)
		points[index] = build->vertices[corners[index]].position;
	range_ring_order(points, 4, normal, order);
	for (index = 0; index < 4; index++)
		ring[index] = corners[order[index]];
	/* (a counter-clockwise ring's 0, 2, 1 and 0, 3, 2 are clockwise) */
	surfaces[*surface_index].vertex_indices[0] = (word)ring[0];
	surfaces[*surface_index].vertex_indices[1] = (word)ring[2];
	surfaces[*surface_index].vertex_indices[2] = (word)ring[1];
	(*surface_index)++;
	surfaces[*surface_index].vertex_indices[0] = (word)ring[0];
	surfaces[*surface_index].vertex_indices[1] = (word)ring[3];
	surfaces[*surface_index].vertex_indices[2] = (word)ring[2];
	(*surface_index)++;
}

/* a quad of four corners of its own, textured by the two axes across its
normal */
static void range_flat_quad_add(
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index,
	real_point3d const *points,
	real_vector3d const *normal)
{
	real_vector3d tangent;
	long corners[4];
	short index;

	if (fabsf(normal->k) > 0.5f)
		range_vector(&tangent, 1.0f, 0.0f, 0.0f);
	else
		range_vector(&tangent, -normal->j, normal->i, 0.0f);
	for (index = 0; index < 4; index++)
	{
		real u = (points[index].x * tangent.i + points[index].y * tangent.j) * RANGE_TEXTURE_SCALE * 4.0f;
		real v = (fabsf(normal->k) > 0.5f ? points[index].y : points[index].z) * RANGE_TEXTURE_SCALE * 4.0f;

		corners[index] = range_vertex_add(build, &points[index], normal, &tangent, u, v);
	}
	range_quad_add(surfaces, surface_index, build, corners, normal);
}

/* the floor's tiles (surfaces from 0, a row of tiles at a time) */
static void range_floor_build(
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index)
{
	real_vector3d normal;
	real_vector3d tangent;
	real tile = 2.0f * RANGE_HALF_WIDTH / RANGE_TILES;
	short row;
	short column;

	range_vector(&normal, 0.0f, 0.0f, 1.0f);
	range_vector(&tangent, 1.0f, 0.0f, 0.0f);
	build->first_surface_index = *surface_index;
	for (row = 0; row <= RANGE_TILES; row++)
	{
		for (column = 0; column <= RANGE_TILES; column++)
		{
			real_point3d position;

			range_point(&position, -RANGE_HALF_WIDTH + column * tile, -RANGE_HALF_WIDTH + row * tile, 0.0f);
			range_vertex_add(build, &position, &normal, &tangent, position.x * RANGE_TEXTURE_SCALE,
				position.y * RANGE_TEXTURE_SCALE);
		}
	}
	for (row = 0; row < RANGE_TILES; row++)
	{
		for (column = 0; column < RANGE_TILES; column++)
		{
			long corners[4];

			corners[0] = row * (RANGE_TILES + 1) + column;
			corners[1] = corners[0] + 1;
			corners[2] = corners[0] + RANGE_TILES + 1;
			corners[3] = corners[2] + 1;
			range_quad_add(surfaces, surface_index, build, corners, &normal);
		}
	}
	build->surface_count = *surface_index - build->first_surface_index;
}

/* the walls' panels (each wall's surfaces in turn, east, west, north and
south, along it) */
static void range_walls_build(
	struct range_layout const *layout,
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index)
{
	real panel = 2.0f * RANGE_HALF_WIDTH / RANGE_TILES;
	short wall;

	build->first_surface_index = *surface_index;
	for (wall = 0; wall < NUMBER_OF_RANGE_WALLS; wall++)
	{
		real_vector3d normal = layout->faces[_range_face_east + wall].normal;
		real_vector3d tangent;
		long first_vertex = build->vertex_count;
		short step;

		/* (along the wall: the normal turned a quarter about up) */
		range_vector(&tangent, -normal.j, normal.i, 0.0f);
		for (step = 0; step <= RANGE_TILES; step++)
		{
			real along = -RANGE_HALF_WIDTH + step * panel;
			real_point3d bottom;
			real_point3d top;

			/* (the wall's plane, at `along` across it) */
			range_point(&bottom, -normal.i * RANGE_HALF_WIDTH + tangent.i * along,
				-normal.j * RANGE_HALF_WIDTH + tangent.j * along, 0.0f);
			top = bottom;
			top.z = RANGE_WALL_HEIGHT;
			range_vertex_add(build, &bottom, &normal, &tangent, along * RANGE_TEXTURE_SCALE, 0.0f);
			range_vertex_add(build, &top, &normal, &tangent, along * RANGE_TEXTURE_SCALE,
				RANGE_WALL_HEIGHT * RANGE_TEXTURE_SCALE);
		}
		for (step = 0; step < RANGE_TILES; step++)
		{
			long corners[4];

			corners[0] = first_vertex + step * 2;
			corners[1] = corners[0] + 1;
			corners[2] = corners[0] + 2;
			corners[3] = corners[0] + 3;
			range_quad_add(surfaces, surface_index, build, corners, &normal);
		}
	}
	build->surface_count = *surface_index - build->first_surface_index;
}

/* a box of the mirror's frame (from x0 to x1 across, its depth the posts'
width about the pane): a post's top and sides up to `height`, or (bar) the
bar's six faces below it */
static void range_box_quads_add(
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index,
	real x0,
	real x1,
	real height,
	boolean bar)
{
	real y0 = RANGE_MIRROR_Y - RANGE_MIRROR_POST_WIDTH * 0.5f;
	real y1 = RANGE_MIRROR_Y + RANGE_MIRROR_POST_WIDTH * 0.5f;
	real z0 = bar ? height - RANGE_MIRROR_BAR_HEIGHT : 0.0f;
	real z1 = height;
	short face;

	for (face = 0; face < 6; face++)
	{
		short axis = (short)(face / 2);
		boolean high = (face & 1) != 0;
		real_point3d corners[8];
		real_point3d points[4];
		real_vector3d normal;
		short corner;
		short count = 0;

		/* (a post's bottom is on the floor: not drawn) */
		if (!bar && axis == 2 && !high)
			continue;
		range_vector(&normal, axis == 0 ? (high ? 1.0f : -1.0f) : 0.0f, axis == 1 ? (high ? 1.0f : -1.0f) : 0.0f,
			axis == 2 ? (high ? 1.0f : -1.0f) : 0.0f);
		for (corner = 0; corner < BOX_CORNERS; corner++)
		{
			range_point(&corners[corner], corner & 1 ? x1 : x0, corner & 2 ? y1 : y0, corner & 4 ? z1 : z0);
			if (((corner >> axis) & 1) == (high ? 1 : 0))
				points[count++] = corners[corner];
		}
		range_flat_quad_add(build, surfaces, surface_index, points, &normal);
	}
}

/* the details: each bench's faces (the collision's, drawn), then the
mirror's line, a little above the floor */
static void range_details_build(
	struct range_layout const *layout,
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index)
{
	real_point3d points[4];
	real_vector3d up;
	short face;

	build->first_surface_index = *surface_index;
	for (face = NUMBER_OF_ROOM_FACES; face < layout->face_count; face++)
	{
		short index;

		for (index = 0; index < 4; index++)
			points[index] = layout->vertices[layout->faces[face].corners[index]];
		range_flat_quad_add(build, surfaces, surface_index, points, &layout->faces[face].normal);
	}
	range_vector(&up, 0.0f, 0.0f, 1.0f);
	range_point(&points[0], -RANGE_MIRROR_HALF_WIDTH, RANGE_MIRROR_Y - RANGE_MIRROR_LINE_WIDTH * 0.5f,
		RANGE_MIRROR_LINE_HEIGHT);
	range_point(&points[1], RANGE_MIRROR_HALF_WIDTH, RANGE_MIRROR_Y - RANGE_MIRROR_LINE_WIDTH * 0.5f,
		RANGE_MIRROR_LINE_HEIGHT);
	range_point(&points[2], RANGE_MIRROR_HALF_WIDTH, RANGE_MIRROR_Y + RANGE_MIRROR_LINE_WIDTH * 0.5f,
		RANGE_MIRROR_LINE_HEIGHT);
	range_point(&points[3], -RANGE_MIRROR_HALF_WIDTH, RANGE_MIRROR_Y + RANGE_MIRROR_LINE_WIDTH * 0.5f,
		RANGE_MIRROR_LINE_HEIGHT);
	range_flat_quad_add(build, surfaces, surface_index, points, &up);
	/* (the frame: a post each side, and the bar between their tops) */
	range_box_quads_add(build, surfaces, surface_index, -RANGE_MIRROR_HALF_WIDTH - RANGE_MIRROR_POST_WIDTH,
		-RANGE_MIRROR_HALF_WIDTH, RANGE_MIRROR_FRAME_HEIGHT, FALSE);
	range_box_quads_add(build, surfaces, surface_index, RANGE_MIRROR_HALF_WIDTH,
		RANGE_MIRROR_HALF_WIDTH + RANGE_MIRROR_POST_WIDTH, RANGE_MIRROR_FRAME_HEIGHT, FALSE);
	range_box_quads_add(build, surfaces, surface_index, -RANGE_MIRROR_HALF_WIDTH, RANGE_MIRROR_HALF_WIDTH,
		RANGE_MIRROR_FRAME_HEIGHT, TRUE);
	build->surface_count = *surface_index - build->first_surface_index;
}

static void range_bounds_add(
	real_rectangle3d *bounds,
	real_point3d const *point,
	boolean first)
{
	if (first)
	{
		bounds->x0 = bounds->x1 = point->x;
		bounds->y0 = bounds->y1 = point->y;
		bounds->z0 = bounds->z1 = point->z;
		return;
	}
	bounds->x0 = MIN(bounds->x0, point->x);
	bounds->x1 = MAX(bounds->x1, point->x);
	bounds->y0 = MIN(bounds->y0, point->y);
	bounds->y1 = MAX(bounds->y1, point->y);
	bounds->z0 = MIN(bounds->z0, point->z);
	bounds->z1 = MAX(bounds->z1, point->z);
}

/* a subcluster of the surfaces listed (of a material's) */
static boolean range_subcluster_write(
	struct range_writer *writer,
	struct range_subcluster *subcluster,
	struct structure_surface const *surfaces,
	struct range_material_build const *build,
	long const *surface_list,
	long count)
{
	long *indices = range_block(writer, &subcluster->surface_indices, count, sizeof(long));
	long index;

	if (!indices)
		return FALSE;
	for (index = 0; index < count; index++)
	{
		struct structure_surface const *surface = &surfaces[surface_list[index]];
		short corner;

		indices[index] = surface_list[index];
		for (corner = 0; corner < 3; corner++)
		{
			range_bounds_add(&subcluster->world_bounds, &build->vertices[surface->vertex_indices[corner]].position,
				index == 0 && corner == 0);
		}
	}
	return TRUE;
}

/* ---------- private code: the bsp */

static long range_shader(
	short material)
{
	long shader = tag_loaded(range_shaders[material].group_tag, range_shaders[material].path);

	/* (the details' is the walls' when the map has not got its own) */
	if (shader == NONE && material == 2)
		shader = tag_loaded(range_shaders[1].group_tag, range_shaders[1].path);
	if (shader == NONE)
		error(_error_silent, "firing range: the map has no shader %s", range_shaders[material].path);
	return shader;
}

static short range_shader_material_type(
	long shader)
{
	struct shader *definition = shader == NONE ? NULL : (struct shader *)tag_get(SHADER_ENVIRONMENT_GROUP_TAG, shader);

	return definition ? definition->base.physics.material_type : 0;
}

static void range_default_lighting(
	struct render_lighting *lighting)
{
	lighting->ambient_color.red = 0.35f;
	lighting->ambient_color.green = 0.35f;
	lighting->ambient_color.blue = 0.38f;
	lighting->distant_light_count = 2;
	lighting->distant_lights[0].color.red = 0.9f;
	lighting->distant_lights[0].color.green = 0.88f;
	lighting->distant_lights[0].color.blue = 0.8f;
	range_vector(&lighting->distant_lights[0].direction, 0.38f, 0.3f, -0.87f);
	lighting->distant_lights[1].color.red = 0.25f;
	lighting->distant_lights[1].color.green = 0.27f;
	lighting->distant_lights[1].color.blue = 0.32f;
	range_vector(&lighting->distant_lights[1].direction, -0.38f, -0.3f, -0.87f);
	lighting->reflection_tint_color.alpha = 1.0f;
	lighting->reflection_tint_color.red = 0.5f;
	lighting->reflection_tint_color.green = 0.5f;
	lighting->reflection_tint_color.blue = 0.5f;
	range_vector(&lighting->shadow_vector, 0.38f, 0.3f, -0.87f);
	lighting->shadow_color.red = 0.4f;
	lighting->shadow_color.green = 0.4f;
	lighting->shadow_color.blue = 0.45f;
}

/* how many vertices and surfaces a material has */
static void range_material_counts(
	struct range_layout const *layout,
	short material,
	long *vertices,
	long *surfaces)
{
	long quads = (layout->face_count - NUMBER_OF_ROOM_FACES) + 1;

	switch (material)
	{
	case 0:
		*vertices = RANGE_FLOOR_VERTICES;
		*surfaces = RANGE_FLOOR_SURFACES;
		break;
	case 1:
		*vertices = NUMBER_OF_RANGE_WALLS * RANGE_WALL_VERTICES;
		*surfaces = NUMBER_OF_RANGE_WALLS * RANGE_WALL_SURFACES;
		break;
	case 2:
		*vertices = (quads + RANGE_MIRROR_FRAME_QUADS) * 4;
		*surfaces = (quads + RANGE_MIRROR_FRAME_QUADS) * 2;
		break;
	default:
		*vertices = 4;
		*surfaces = 2;
		break;
	}
}

static boolean range_structure_bsp_build(
	struct range_writer *writer,
	struct range_layout const *layout,
	struct structure_bsp **result)
{
	struct structure_bsp *bsp = range_allocate(writer, sizeof(*bsp));
	struct collision_bsp *collision = bsp ? range_block(writer, &bsp->collision_bsp, 1, sizeof(*collision)) : NULL;
	long shaders[NUMBER_OF_RANGE_MATERIALS];
	struct structure_collision_material *collision_materials;
	struct structure_surface *surfaces;
	struct structure_lightmap *lightmap;
	struct structure_material *materials;
	struct range_material_build builds[NUMBER_OF_RANGE_MATERIALS];
	struct range_cluster *cluster;
	struct range_subcluster *subclusters;
	struct structure_leaf *leaf;
	struct range_surface_reference *references;
	byte_rectangle3d *nodes;
	unsigned long *visibility;
	long surface_count = 0;
	long surface_index = 0;
	short material_count = NUMBER_OF_RANGE_MATERIALS;
	short material_index;
	long index;

	*result = bsp;
	if (!collision || !range_collision_bsp_write(writer, layout, collision))
		return FALSE;
	for (material_index = 0; material_index < NUMBER_OF_RANGE_MATERIALS; material_index++)
	{
		if ((shaders[material_index] = range_shader(material_index)) == NONE)
			return FALSE;
	}

	/* (no lightmap: drawn by the renderer's own light, objects lit by the
	default lighting) */
	range_reference(&bsp->lightmap_group, BITMAP_GROUP_TAG, NONE);
	range_default_lighting(&bsp->default_lighting);
	bsp->world_bounds.x0 = -RANGE_HALF_WIDTH;
	bsp->world_bounds.x1 = RANGE_HALF_WIDTH;
	bsp->world_bounds.y0 = -RANGE_HALF_WIDTH;
	bsp->world_bounds.y1 = RANGE_HALF_WIDTH;
	bsp->world_bounds.z0 = 0.0f;
	bsp->world_bounds.z1 = RANGE_CEILING;

	for (material_index = 0; material_index < material_count; material_index++)
	{
		long vertex_count;
		long material_surfaces;

		range_material_counts(layout, material_index, &vertex_count, &material_surfaces);
		surface_count += material_surfaces;
	}
	collision_materials = range_block(writer, &bsp->collision_materials, NUMBER_OF_RANGE_COLLISION_MATERIALS,
		sizeof(*collision_materials));
	surfaces = range_block(writer, &bsp->surfaces, surface_count, sizeof(*surfaces));
	lightmap = range_block(writer, &bsp->lightmaps, 1, sizeof(*lightmap));
	materials = lightmap ? range_block(writer, &lightmap->materials, material_count, sizeof(*materials)) : NULL;
	if (!collision_materials || !surfaces || !materials)
		return FALSE;
	for (material_index = 0; material_index < material_count; material_index++)
	{
		long vertex_count;
		long material_surfaces;
		unsigned long vertex_bytes;
		byte *vertex_data;

		range_material_counts(layout, material_index, &vertex_count, &material_surfaces);
		vertex_bytes = (unsigned long)vertex_count *
			(sizeof(struct range_vertex) + sizeof(struct range_lightmap_vertex));
		if (!(vertex_data = range_allocate(writer, vertex_bytes)))
			return FALSE;
		if (material_index < NUMBER_OF_RANGE_COLLISION_MATERIALS)
		{
			range_reference(&collision_materials[material_index].shader, SHADER_ENVIRONMENT_GROUP_TAG,
				shaders[material_index]);
			collision_materials[material_index].runtime_physics_material_type =
				range_shader_material_type(shaders[material_index]);
		}
		/* (the lightmap vertices follow the vertices, as
		custom_edition_geometry.c reads them) */
		builds[material_index].vertices = (struct range_vertex *)vertex_data;
		builds[material_index].lightmap_vertices = (struct range_lightmap_vertex *)(vertex_data +
			(unsigned long)vertex_count * sizeof(struct range_vertex));
		builds[material_index].vertex_count = 0;
		materials[material_index].uncompressed_vertex_data.size = (long)vertex_bytes;
		materials[material_index].uncompressed_vertex_data.address = vertex_data;
	}
	range_floor_build(&builds[0], surfaces, &surface_index);
	range_walls_build(layout, &builds[1], surfaces, &surface_index);
	range_details_build(layout, &builds[2], surfaces, &surface_index);
	if (surface_index != surface_count)
		return FALSE;
	lightmap->bitmap_index = NONE;
	for (material_index = 0; material_index < material_count; material_index++)
	{
		struct structure_material *material = &materials[material_index];

		range_reference(&material->shader, range_shaders[material_index].group_tag, shaders[material_index]);
		material->first_surface_index = builds[material_index].first_surface_index;
		material->surface_count = builds[material_index].surface_count;
		material->breakable_surface_index = NONE;
		material->vertices.type = _rasterizer_vertex_type_environment_uncompressed;
		material->vertices.count = builds[material_index].vertex_count;
		material->lightmap_vertices.type = _rasterizer_vertex_type_environment_lightmap_uncompressed;
		material->lightmap_vertices.count = builds[material_index].vertex_count;
		range_point(&material->centroid, 0.0f, 0.0f, material_index == 1 ? RANGE_WALL_HEIGHT * 0.5f : 0.0f);
		material->lighting = bsp->default_lighting;
	}

	/* (one cluster, which sees itself, its subclusters the floor's blocks
	of tiles, each wall's quarters and the details) */
	cluster = range_block(writer, &bsp->clusters, 1, sizeof(*cluster));
	subclusters = cluster ? range_block(writer, &cluster->subclusters, RANGE_ROOM_SUBCLUSTERS + 1,
		sizeof(*subclusters)) : NULL;
	visibility = range_allocate(writer, sizeof(unsigned long));
	if (!subclusters || !visibility)
		return FALSE;
	cluster->sky_index = 0;
	cluster->fog_designator = NONE;
	cluster->background_sound_palette_index = NONE;
	cluster->sound_environment_palette_index = NONE;
	cluster->weather_palette_index = NONE;
	cluster->transition_structure_bsp_index = NONE;
	cluster->first_runtime_decal_index = NONE;
	*visibility = 1;
	bsp->cluster_data.size = sizeof(unsigned long);
	bsp->cluster_data.address = visibility;
	for (index = 0; index < RANGE_SUBCLUSTERS * RANGE_SUBCLUSTERS; index++)
	{
		long list[(RANGE_TILES / RANGE_SUBCLUSTERS) * (RANGE_TILES / RANGE_SUBCLUSTERS) * 2];
		long count = 0;
		short block_row = (short)(index / RANGE_SUBCLUSTERS);
		short block_column = (short)(index % RANGE_SUBCLUSTERS);
		short row;
		short column;

		for (row = 0; row < RANGE_TILES / RANGE_SUBCLUSTERS; row++)
		{
			for (column = 0; column < RANGE_TILES / RANGE_SUBCLUSTERS; column++)
			{
				long tile = (block_row * (RANGE_TILES / RANGE_SUBCLUSTERS) + row) * RANGE_TILES +
					block_column * (RANGE_TILES / RANGE_SUBCLUSTERS) + column;

				list[count++] = builds[0].first_surface_index + tile * 2;
				list[count++] = builds[0].first_surface_index + tile * 2 + 1;
			}
		}
		if (!range_subcluster_write(writer, &subclusters[index], surfaces, &builds[0], list, count))
			return FALSE;
	}
	for (index = 0; index < NUMBER_OF_RANGE_WALLS * RANGE_SUBCLUSTERS; index++)
	{
		long list[RANGE_WALL_SURFACES / RANGE_SUBCLUSTERS];
		long first = builds[1].first_surface_index + index * (RANGE_WALL_SURFACES / RANGE_SUBCLUSTERS);
		long count;

		for (count = 0; count < RANGE_WALL_SURFACES / RANGE_SUBCLUSTERS; count++)
			list[count] = first + count;
		if (!range_subcluster_write(writer, &subclusters[RANGE_SUBCLUSTERS * RANGE_SUBCLUSTERS + index], surfaces,
			&builds[1], list, count))
			return FALSE;
	}
	{
		long list[MAXIMUM_RANGE_DETAIL_QUADS * 2];
		long count;

		for (count = 0; count < builds[2].surface_count; count++)
			list[count] = builds[2].first_surface_index + count;
		if (!range_subcluster_write(writer, &subclusters[RANGE_ROOM_SUBCLUSTERS], surfaces, &builds[2], list, count))
			return FALSE;
	}

	/* (the AI's paths: a byte of each collision surface, the floor's
walkable, path.c's _pathfinding_surface_walkable_bit) */
	{
		byte *pathfinding = range_block(writer, &bsp->pathfinding_surfaces, layout->face_count, sizeof(byte));

		if (!pathfinding)
			return FALSE;
		pathfinding[_range_face_floor] = FLAG(6);
	}

	/* (one leaf, the collision bsp's, of the cluster, naming every surface
	by its face's node, the line by the floor's; one node of whole bounds
	for each bsp3d node) */
	leaf = range_block(writer, &bsp->leaves, 1, sizeof(*leaf));
	references = range_block(writer, &bsp->surface_references, surface_count, sizeof(*references));
	nodes = range_block(writer, &bsp->nodes, layout->node_count, sizeof(*nodes));
	if (!leaf || !references || !nodes)
		return FALSE;
	leaf->bounds.x0 = leaf->bounds.y0 = leaf->bounds.z0 = 0;
	leaf->bounds.x1 = leaf->bounds.y1 = leaf->bounds.z1 = 0xFF;
	leaf->cluster_index = 0;
	leaf->surface_reference_count = (short)surface_count;
	leaf->first_surface_reference_index = 0;
	for (index = 0; index < surface_count; index++)
	{
		long detail = index - builds[2].first_surface_index;

		references[index].surface_index = index;
		if (index < RANGE_FLOOR_SURFACES)
			references[index].bsp3d_node_index = layout->face_nodes[_range_face_floor];
		else if (detail < 0)
			references[index].bsp3d_node_index = layout->face_nodes[_range_face_east +
				(index - RANGE_FLOOR_SURFACES) / RANGE_WALL_SURFACES];
		else if (detail < (layout->face_count - NUMBER_OF_ROOM_FACES) * 2)
			references[index].bsp3d_node_index = layout->face_nodes[NUMBER_OF_ROOM_FACES + detail / 2];
		else
			references[index].bsp3d_node_index = layout->face_nodes[_range_face_floor];
	}
	for (index = 0; index < layout->node_count; index++)
	{
		nodes[index].x0 = nodes[index].y0 = nodes[index].z0 = 0;
		nodes[index].x1 = nodes[index].y1 = nodes[index].z1 = 0xFF;
	}

	return !writer->full;
}

/* ---------- private code: the scenario */

/* the scenario's blocks of objects the map places, taken out while the
range plays */
static struct tag_block *range_emptied_block(
	struct scenario *scenario,
	short index)
{
	switch (index)
	{
	case 0: return &scenario->scenery;
	case 1: return &scenario->bipeds;
	case 2: return &scenario->vehicles;
	case 3: return &scenario->equipment;
	case 4: return &scenario->weapons;
	case 5: return &scenario->machines;
	case 6: return &scenario->controls;
	case 7: return &scenario->light_fixtures;
	case 8: return &scenario->sound_scenery;
	case 9: return &scenario->netgame_flags;
	case 10: return &scenario->netgame_equipment;
	case 11: return &scenario->decals;
	}
	return NULL;
}

static void range_scenario_patch(
	struct scenario *scenario)
{
	struct tag_block *block;
	short index;

	firing_range.scenario = scenario;
	firing_range.emptied_count = 0;
	while ((block = range_emptied_block(scenario, firing_range.emptied_count)) != NULL &&
		firing_range.emptied_count < (short)NUMBEROF(firing_range.emptied_counts))
	{
		firing_range.emptied_counts[firing_range.emptied_count++] = block->count;
		block->count = 0;
	}
	/* (its players start at the middle, side by side, facing north: those
	kept to put back, the rest left out) */
	firing_range.players_count = scenario->players.count;
	firing_range.starting_location_count = (short)MIN(scenario->players.count, MAXIMUM_RANGE_STARTING_LOCATIONS);
	scenario->players.count = firing_range.starting_location_count;
	for (index = 0; index < firing_range.starting_location_count; index++)
	{
		struct player_starting_location *location = TAG_BLOCK_GET_ELEMENT(&scenario->players, index,
			struct player_starting_location);

		firing_range.starting_locations[index] = *location;
		range_point(&location->position, ((index % 8) - 3.5f) * RANGE_START_SPACING,
			-(index / 8) * RANGE_START_SPACING, RANGE_START_HEIGHT);
		location->facing = RANGE_START_FACING;
		location->structure_bsp_reference_index = 0;
	}
	error(_error_silent, "firing range: %s plays the range (%d of its blocks of objects taken out, %d starting "
		"locations moved, %d weapons laid out)", range_map_name, firing_range.emptied_count,
		firing_range.starting_location_count, firing_range.weapon_count);
}

static void range_scenario_restore(
	void)
{
	struct scenario *scenario = firing_range.scenario;
	short index;

	if (!scenario)
		return;
	for (index = 0; index < firing_range.emptied_count; index++)
		range_emptied_block(scenario, index)->count = firing_range.emptied_counts[index];
	for (index = 0; index < firing_range.starting_location_count; index++)
		*TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location) =
			firing_range.starting_locations[index];
	scenario->players.count = firing_range.players_count;
	firing_range.scenario = NULL;
	firing_range.emptied_count = 0;
	firing_range.starting_location_count = 0;
}

/* whether a scenario's name is the range's map's (its last part, any case) */
static boolean range_map_is(
	char const *scenario_name)
{
	char const *leaf = scenario_name ? strrchr(scenario_name, '\\') : NULL;
	char const *range_leaf = strrchr(range_map_name, '\\') + 1;

	leaf = leaf ? leaf + 1 : scenario_name;
	return leaf && !strcasecmp(leaf, range_leaf);
}

/* ---------- private code: the weapons */

/* whether a weapon of a definition lies (on the map, held by no one) near
a place */
static long range_weapon_lying(
	long definition_index,
	real_point3d const *place)
{
	struct object_iterator iterator;

	object_iterator_new(&iterator, _object_mask_weapon, 0);
	while (object_iterator_next(&iterator))
	{
		struct weapon_datum *weapon = weapon_get(iterator.index);
		real dx = weapon->object.position.x - place->x;
		real dy = weapon->object.position.y - place->y;
		real dz = weapon->object.position.z - place->z;

		if (weapon->definition_index == definition_index && weapon->object.parent_object_index == NONE &&
			TEST_FLAG(weapon->object.flags, _object_connected_to_map_bit) &&
			dx * dx + dy * dy + dz * dz < RANGE_WEAPON_NEAR * RANGE_WEAPON_NEAR)
		{
			return iterator.index;
		}
	}
	return NONE;
}

/* a weapon laid in its place, its barrel to the east (towards the start) */
static long range_weapon_lay(
	short weapon)
{
	struct object_placement_data placement;
	real_point3d place;

	range_weapon_place(weapon, &place);
	object_placement_data_new(&placement, firing_range.weapons[weapon], NONE);
	placement.position = place;
	range_vector(&placement.forward, 1.0f, 0.0f, 0.0f);
	range_vector(&placement.up, 0.0f, 0.0f, 1.0f);
	return object_new(&placement);
}

/* each weapon's place looked at: one empty a moment has its weapon laid
there again */
static void range_weapons_update(
	long now)
{
	short weapon;

	for (weapon = 0; weapon < firing_range.weapon_count; weapon++)
	{
		real_point3d place;

		range_weapon_place(weapon, &place);
		firing_range.weapon_objects[weapon] = range_weapon_lying(firing_range.weapons[weapon], &place);
		if (firing_range.weapon_objects[weapon] != NONE)
		{
			firing_range.weapon_empty_since[weapon] = NONE;
			continue;
		}
		/* (laid at once the first time, else a moment after it was taken) */
		if (firing_range.weapon_empty_since[weapon] == NONE)
			firing_range.weapon_empty_since[weapon] = now - (firing_range.next_check_time ? 0 : RANGE_REFILL_TICKS);
		if (now - firing_range.weapon_empty_since[weapon] >= RANGE_REFILL_TICKS)
		{
			firing_range.weapon_objects[weapon] = range_weapon_lay(weapon);
			firing_range.weapon_empty_since[weapon] = NONE;
		}
	}
}

/* ---------- private code: the mirror */

/* a point, and a vector, turned about the mirror's line */
static void range_mirror_point(
	real_point3d const *point,
	real_point3d *result)
{
	range_point(result, point->x, 2.0f * RANGE_MIRROR_Y - point->y, point->z);
}

static void range_mirror_vector(
	real_vector3d const *vector,
	real_vector3d *result)
{
	range_vector(result, vector->i, -vector->j, vector->k);
}

/* whether the player's unit stands before the mirror */
static boolean range_before_mirror(
	struct unit_datum const *unit)
{
	real_point3d const *position = &unit->object.position;

	return position->y > RANGE_MIRROR_Y && position->y < RANGE_MIRROR_Y + RANGE_MIRROR_DEPTH &&
		fabsf(position->x) < RANGE_MIRROR_HALF_WIDTH;
}

static void range_mirror_delete(
	void)
{
	if (firing_range.mirror_unit != NONE &&
		object_try_and_get_and_verify_type(firing_range.mirror_unit, _object_mask_unit))
	{
		unit_delete_all_weapons(firing_range.mirror_unit);
		object_delete(firing_range.mirror_unit);
	}
	firing_range.mirror_unit = NONE;
	firing_range.mirrored_unit = NONE;
}

static void range_mirror_create(
	long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	struct object_placement_data placement;
	long mirror;

	object_placement_data_new(&placement, unit->definition_index, NONE);
	range_mirror_point(&unit->object.position, &placement.position);
	range_mirror_vector(&unit->object.forward, &placement.forward);
	range_vector(&placement.up, 0.0f, 0.0f, 1.0f);
	mirror = object_new(&placement);
	if (mirror == NONE || !object_try_and_get_and_verify_type(mirror, _object_mask_unit))
	{
		error(_error_silent, "firing range: the mirror's copy of %s could not be made",
			tag_get_name(unit->definition_index));
		if (mirror != NONE)
			object_delete(mirror);
		return;
	}
	unit_set_actively_controlled(mirror, TRUE);
	firing_range.mirror_unit = mirror;
	firing_range.mirrored_unit = unit_index;
}

/* the copy's weapons the player's, slot for slot (a slot of another kind
of weapon emptied and given a new one of the player's kind), the one in
hand the player's, their rounds, and its grenades */
static void range_mirror_equip(
	long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	struct unit_datum *mirror = unit_get(firing_range.mirror_unit);
	short slot;

	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long held = unit->unit.weapon_object_indices[slot];
		long copied = mirror->unit.weapon_object_indices[slot];
		long wanted = held == NONE ? NONE : weapon_get(held)->definition_index;
		struct object_placement_data placement;
		long copy;

		if (wanted == (copied == NONE ? NONE : weapon_get(copied)->definition_index))
			continue;
		if (copied != NONE)
		{
			unit_network_forget_weapon(firing_range.mirror_unit, slot);
			object_delete(copied);
		}
		if (wanted == NONE)
			continue;
		object_placement_data_new(&placement, wanted, NONE);
		placement.position = mirror->object.position;
		copy = object_new(&placement);
		if (copy != NONE)
			unit_network_add_weapon(firing_range.mirror_unit, copy, slot);
	}
	/* (the one in hand gone: the player's readied, as the copy readies any) */
	if (unit->unit.current_weapon_index >= 0 && unit->unit.current_weapon_index < MAXIMUM_WEAPONS_PER_UNIT &&
		(mirror->unit.current_weapon_index == NONE ||
		mirror->unit.weapon_object_indices[mirror->unit.current_weapon_index] == NONE))
	{
		mirror->unit.desired_weapon_index = unit->unit.current_weapon_index;
	}
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long held = unit->unit.weapon_object_indices[slot];
		long copied = mirror->unit.weapon_object_indices[slot];

		if (held != NONE && copied != NONE)
		{
			struct weapon_datum *from = weapon_get(held);
			struct weapon_datum *to = weapon_get(copied);

			csmemcpy(to->weapon.magazines, from->weapon.magazines, sizeof(to->weapon.magazines));
			to->weapon.heat = from->weapon.heat;
		}
	}
	csmemcpy(mirror->unit.grenade_counts, unit->unit.grenade_counts, sizeof(mirror->unit.grenade_counts));
}

static void range_mirror_update(
	void)
{
	long player_index = local_player_get_player_index(0);
	long unit_index = player_index == NONE ? NONE : player_get(player_index)->unit_index;
	struct unit_datum *unit = unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	boolean before = unit && unit->object.parent_object_index == NONE && range_before_mirror(unit);

	if (firing_range.mirror_unit != NONE &&
		(!object_try_and_get_and_verify_type(firing_range.mirror_unit, _object_mask_unit) ||
		firing_range.mirrored_unit != unit_index))
	{
		range_mirror_delete();
	}
	/* (before the mirror, on foot, the copy, and the HUD saying what it is
	as the player steps up; else none) */
	if (!before)
	{
		range_mirror_delete();
		firing_range.before_mirror = FALSE;
		return;
	}
	if (!firing_range.before_mirror)
		hud_print_message(0, L"MIRROR: your copy moves as you do. Step back to leave it.");
	firing_range.before_mirror = TRUE;
	if (firing_range.mirror_unit == NONE)
		range_mirror_create(unit_index);
	if (firing_range.mirror_unit != NONE)
		range_mirror_equip(unit_index);
}

/* ---------- private code: the AI */

/* an AI's place north (its index of the AI's), facing south */
static void range_ai_place(
	short ai,
	real_point3d *place)
{
	range_point(place, ((ai % RANGE_AIS_PER_ROW) - (RANGE_AIS_PER_ROW - 1) * 0.5f) * RANGE_AI_SPACING,
		RANGE_AI_FIRST_Y + (ai / RANGE_AIS_PER_ROW) * RANGE_AI_SPACING, RANGE_START_HEIGHT);
}

/* a unit's weapons taken from it, slot for slot */
static void range_unit_disarm(
	long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	short slot;

	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long weapon = unit->unit.weapon_object_indices[slot];

		if (weapon == NONE)
			continue;
		unit_network_forget_weapon(unit_index, slot);
		object_delete(weapon);
	}
}

/* an AI's unit standing for it: its variant's, as its actors are given
(their colours), with no weapon */
static long range_ai_stand(
	short ai)
{
	struct actor_variant_definition *variant = actor_variant_definition_get(firing_range.ais[ai].variant);
	struct object_placement_data placement;
	long unit_index;

	if (!variant || variant->unit_reference.index == NONE)
		return NONE;
	object_placement_data_new(&placement, variant->unit_reference.index, NONE);
	range_ai_place(ai, &placement.position);
	range_vector(&placement.forward, 0.0f, -1.0f, 0.0f);
	range_vector(&placement.up, 0.0f, 0.0f, 1.0f);
	unit_index = object_new(&placement);
	if (unit_index == NONE || !object_try_and_get_and_verify_type(unit_index, _object_mask_unit))
	{
		error(_error_silent, "firing range: %s could not be stood", firing_range.ais[ai].name);
		if (unit_index != NONE)
			object_delete(unit_index);
		return NONE;
	}
	actor_customize_unit(firing_range.ais[ai].variant, unit_index);
	range_unit_disarm(unit_index);
	firing_range.ais[ai].animation = NONE;
	return unit_index;
}

/* an AI's unit given its next animation when it has played the last: the
graph's base animations in turn, each name once (its first of the name:
the others are its permutations) */
static void range_ai_animate(
	short ai)
{
	struct unit_datum *unit = unit_get(firing_range.ais[ai].unit);
	long graph_index = unit->object.animation.animation_graph_index;
	struct animation_graph *graph = graph_index == NONE ? NULL : animation_graph_definition_get(graph_index);
	short tries;

	if (!graph || graph->animations.count <= 0 || unit_is_playing_custom_animation(firing_range.ais[ai].unit))
		return;
	for (tries = 0; tries < graph->animations.count; tries++)
	{
		short next = (short)((firing_range.ais[ai].animation + 1 + graph->animations.count) % graph->animations.count);
		struct animation *animation = TAG_BLOCK_GET_ELEMENT(&graph->animations, next, struct animation);

		firing_range.ais[ai].animation = next;
		if (animation->type != _animation_base ||
			animation_graph_get_animation_by_name(graph_index, animation->name) != next)
			continue;
		if (unit_start_user_animation(firing_range.ais[ai].unit, graph_index, animation->name, TRUE))
			return;
	}
}

/* each AI stood (once the player is on its feet), playing, on its place */
static void range_ais_update(
	void)
{
	short ai;

	for (ai = 0; ai < firing_range.ai_count; ai++)
	{
		struct unit_datum *unit;
		real_point3d place;
		real dx;
		real dy;

		if (firing_range.ais[ai].unit == NONE ||
			!object_try_and_get_and_verify_type(firing_range.ais[ai].unit, _object_mask_unit))
		{
			firing_range.ais[ai].unit = range_ai_stand(ai);
			if (firing_range.ais[ai].unit == NONE)
				continue;
		}
		range_ai_animate(ai);
		/* (an animation that walks stays where it is) */
		unit = unit_get(firing_range.ais[ai].unit);
		range_ai_place(ai, &place);
		dx = unit->object.position.x - place.x;
		dy = unit->object.position.y - place.y;
		if (dx * dx + dy * dy > RANGE_AI_DRIFT * RANGE_AI_DRIFT)
		{
			real_vector3d forward;
			real_vector3d up;

			place.z = unit->object.position.z;
			range_vector(&forward, 0.0f, -1.0f, 0.0f);
			range_vector(&up, 0.0f, 0.0f, 1.0f);
			object_set_position(firing_range.ais[ai].unit, &place, &forward, &up);
		}
	}
}

static void range_hud_say(
	char const *text)
{
	wchar_t wide[128];
	short index;

	for (index = 0; text[index] && index < (short)NUMBEROF(wide) - 1; index++)
		wide[index] = (wchar_t)(unsigned char)text[index];
	wide[index] = 0;
	hud_print_message(0, wide);
}

/* a live actor of an AI put in the arena, facing its door, holding one of
the profile's weapons by chance (its own when the profile has none); the
oldest let go when there are as many as there may be */
static void range_ai_fight(
	short ai)
{
	real_point3d place;
	real_vector3d forward;
	long unit_index;
	long weapon_definition = NONE;
	char const *weapon_name = "its own weapon";
	long previous = firing_range.fighters[firing_range.next_fighter];
	char text[160];

	if (previous != NONE && object_try_and_get_and_verify_type(previous, _object_mask_unit))
	{
		range_unit_disarm(previous);
		object_delete(previous);
	}
	firing_range.fighters[firing_range.next_fighter] = NONE;
	range_point(&place, 0.0f, (RANGE_ARENA_Y0 + RANGE_ARENA_Y1) * 0.5f, RANGE_START_HEIGHT);
	range_vector(&forward, 0.0f, -1.0f, 0.0f);
	unit_index = actor_port_place_free(firing_range.ais[ai].variant, &place, &forward);
	if (unit_index == NONE)
	{
		error(_error_silent, "firing range: %s could not be put in the arena (the AI is not running?)",
			firing_range.ais[ai].name);
		range_hud_say("This AI could not be put in the arena.");
		return;
	}
	if (firing_range.weapon_count)
	{
		short weapon;
		struct object_placement_data placement;
		long weapon_index;

		firing_range.random = firing_range.random * 1103515245UL + 12345UL + (unsigned long)game_time_get();
		weapon = (short)((firing_range.random >> 16) % (unsigned long)firing_range.weapon_count);
		weapon_definition = firing_range.weapons[weapon];
		weapon_name = firing_range.weapon_names[weapon];
		range_unit_disarm(unit_index);
		object_placement_data_new(&placement, weapon_definition, NONE);
		placement.position = place;
		weapon_index = object_new(&placement);
		if (weapon_index != NONE)
		{
			unit_network_add_weapon(unit_index, weapon_index, 0);
			unit_get(unit_index)->unit.desired_weapon_index = 0;
		}
	}
	firing_range.fighters[firing_range.next_fighter] = unit_index;
	firing_range.next_fighter = (short)((firing_range.next_fighter + 1) % MAXIMUM_RANGE_FIGHTERS);
	snprintf(text, sizeof(text), "%s is in the arena (north) with %s.", firing_range.ais[ai].name, weapon_name);
	range_hud_say(text);
	error(_error_silent, "firing range: %s put in the arena with %s", firing_range.ais[ai].name,
		weapon_definition == NONE ? weapon_name : tag_get_name(weapon_definition));
}

/* the AI the player stands near (NONE: none) */
static short range_ai_near(
	struct unit_datum const *unit)
{
	short best = NONE;
	real best_distance = RANGE_AI_NEAR * RANGE_AI_NEAR;
	short ai;

	for (ai = 0; ai < firing_range.ai_count; ai++)
	{
		real_point3d place;
		real dx;
		real dy;

		range_ai_place(ai, &place);
		dx = unit->object.position.x - place.x;
		dy = unit->object.position.y - place.y;
		if (dx * dx + dy * dy < best_distance)
		{
			best = ai;
			best_distance = dx * dx + dy * dy;
		}
	}
	return best;
}

/* ---------- public code */

char const *firing_range_map_name(
	void)
{
	return range_map_name;
}

void firing_range_request(
	char const *profile)
{
	firing_range.requested = TRUE;
	snprintf(firing_range.profile, sizeof(firing_range.profile), "%s", profile ? profile : "");
	error(_error_silent, "firing range: asked for, with the profile %s", firing_range.profile);
}

boolean firing_range_active(
	void)
{
	return firing_range.active;
}

char const *firing_range_profile(
	void)
{
	return firing_range.active ? firing_range.profile : "";
}

char const *firing_range_level_profile(
	char const *scenario_name)
{
	return firing_range.requested && range_map_is(scenario_name) && firing_range.profile[0] ? firing_range.profile :
		NULL;
}

void firing_range_tags_injected(
	long profile,
	long const *roots,
	long const *root_tags,
	long count)
{
	long index;

	firing_range.weapon_count = 0;
	firing_range.ai_count = 0;
	for (index = 0; index < count; index++)
	{
		unsigned long group = roots[index] == NONE ? 0 : tag_get_group_tag(roots[index]);
		char name[RANGE_NAME_LENGTH];
		short known;

		if (group != WEAPON_GROUP_TAG && group != ACTOR_VARIANT_GROUP_TAG)
			continue;
		tag_injector_tag_display_name(profile, root_tags[index], name, sizeof(name));
		if (group == WEAPON_GROUP_TAG)
		{
			for (known = 0; known < firing_range.weapon_count && firing_range.weapons[known] != roots[index]; known++)
				;
			if (known < firing_range.weapon_count || firing_range.weapon_count == MAXIMUM_RANGE_WEAPONS)
				continue;
			snprintf(firing_range.weapon_names[firing_range.weapon_count], RANGE_NAME_LENGTH, "%s", name);
			firing_range.weapons[firing_range.weapon_count++] = roots[index];
			continue;
		}
		for (known = 0; known < firing_range.ai_count && firing_range.ais[known].variant != roots[index]; known++)
			;
		if (known < firing_range.ai_count || firing_range.ai_count == MAXIMUM_RANGE_AIS)
			continue;
		firing_range.ais[firing_range.ai_count].variant = roots[index];
		snprintf(firing_range.ais[firing_range.ai_count].name, RANGE_NAME_LENGTH, "%s", name);
		firing_range.ais[firing_range.ai_count].unit = NONE;
		firing_range.ais[firing_range.ai_count].animation = NONE;
		firing_range.ai_count++;
	}
	error(_error_silent, "firing range: %d weapons laid out and %d AI stood (the most: %d and %d)",
		firing_range.weapon_count, firing_range.ai_count, MAXIMUM_RANGE_WEAPONS, MAXIMUM_RANGE_AIS);
}

void firing_range_tags_loaded(
	char const *scenario_name,
	long scenario_tag_index)
{
	struct scenario *scenario;

	firing_range.active = FALSE;
	firing_range.mirror_unit = NONE;
	firing_range.mirrored_unit = NONE;
	firing_range.next_check_time = 0;
	firing_range.before_mirror = FALSE;
	firing_range.near_ai = NONE;
	firing_range.action_held = FALSE;
	firing_range.next_fighter = 0;
	{
		short slot;

		for (slot = 0; slot < MAXIMUM_RANGE_FIGHTERS; slot++)
			firing_range.fighters[slot] = NONE;
		for (slot = 0; slot < firing_range.ai_count; slot++)
			firing_range.ais[slot].unit = NONE;
	}
	{
		short weapon;

		for (weapon = 0; weapon < MAXIMUM_RANGE_WEAPONS; weapon++)
		{
			firing_range.weapon_objects[weapon] = NONE;
			firing_range.weapon_empty_since[weapon] = NONE;
		}
	}
	/* (another map loaded: the range is left) */
	if (!firing_range.requested || !range_map_is(scenario_name))
	{
		firing_range.requested = FALSE;
		firing_range.weapon_count = 0;
		firing_range.ai_count = 0;
		return;
	}
	scenario = scenario_tag_index == NONE ? NULL : (struct scenario *)tag_get(SCENARIO_GROUP_TAG, scenario_tag_index);
	if (!scenario || scenario->structure_bsp_references.count <= 0)
	{
		error(_error_silent, "firing range: %s has no structure bsp to stand for", range_map_name);
		firing_range.requested = FALSE;
		firing_range.weapon_count = 0;
		return;
	}
	firing_range.active = TRUE;
	range_scenario_patch(scenario);
}

void firing_range_tags_unloaded(
	void)
{
	range_scenario_restore();
	firing_range.active = FALSE;
	firing_range.weapon_count = 0;
	firing_range.ai_count = 0;
	firing_range.mirror_unit = NONE;
	firing_range.mirrored_unit = NONE;
}

boolean firing_range_structure_bsp_write(
	void *base_address,
	long size)
{
	static struct range_layout layout;
	struct range_writer writer;
	struct range_structure_bsp_header *header;
	struct structure_bsp *bsp = NULL;

	if (!firing_range.active || !base_address || size <= 0)
		return FALSE;
	range_layout_make(&layout, firing_range.weapon_count);
	writer.base = base_address;
	writer.size = (unsigned long)size;
	writer.used = 0;
	writer.full = FALSE;
	header = range_allocate(&writer, sizeof(*header));
	if (!header || !range_structure_bsp_build(&writer, &layout, &bsp))
	{
		error(_error_silent, "firing range: the plane does not fit the map's %ld bytes of structure bsp (or its "
			"shaders are missing): the map's is played", size);
		return FALSE;
	}
	/* (no buffers: its materials are given theirs as a Custom Edition
	bsp's are) */
	header->base_address = bsp;
	header->vertex_buffer_count = 0;
	header->vertex_buffers = header;
	header->index_buffer_count = 0;
	header->index_buffers = header;
	header->signature = STRUCTURE_BSP_HEADER_SIGNATURE;
	firing_range.bsp_written = TRUE;
	error(_error_silent, "firing range: the plane's structure bsp written (%lu bytes of %ld): %ld surfaces, %d "
		"collision surfaces, %d benches", writer.used, size, bsp->surfaces.count, layout.face_count,
		layout.bench_count);

	return TRUE;
}

boolean firing_range_structure_bsp_written(
	void)
{
	return firing_range.bsp_written;
}

void firing_range_structure_bsp_unloaded(
	void)
{
	firing_range.bsp_written = FALSE;
}

void firing_range_tick(
	void)
{
	long now;

	if (!firing_range.active || !firing_range.bsp_written)
		return;
	now = game_time_get();
	/* (the weapons once the player is on its feet, then a few times a
	second) */
	if (firing_range.weapon_count && local_player_get_player_index(0) != NONE &&
		player_get(local_player_get_player_index(0))->unit_index != NONE &&
		now >= firing_range.next_check_time)
	{
		range_weapons_update(now);
		firing_range.next_check_time = now + RANGE_CHECK_TICKS;
	}
	if (firing_range.ai_count && local_player_get_player_index(0) != NONE &&
		player_get(local_player_get_player_index(0))->unit_index != NONE)
	{
		range_ais_update();
	}
	range_mirror_update();
}

void firing_range_player_control(
	long unit_index,
	struct unit_control_data const *control)
{
	struct unit_control_data mirrored;
	struct unit_datum *unit;
	struct unit_datum *mirror;
	real_point3d place;
	real dx;
	real dy;

	if (!firing_range.active || local_player_get_player_index(0) == NONE ||
		player_get(local_player_get_player_index(0))->unit_index != unit_index)
		return;
	unit = unit_get(unit_index);
	/* (near an AI: the HUD names it as the player comes, and action puts it
	in the arena) */
	{
		short near = unit->object.parent_object_index == NONE ? range_ai_near(unit) : NONE;
		boolean action = TEST_FLAG(control->control_flags, _unit_control_action_bit);

		if (near != NONE && near != firing_range.near_ai)
		{
			char text[160];

			snprintf(text, sizeof(text), "%s: press action to fight it in the arena.", firing_range.ais[near].name);
			range_hud_say(text);
		}
		if (near != NONE && action && !firing_range.action_held)
			range_ai_fight(near);
		firing_range.near_ai = near;
		firing_range.action_held = action;
	}
	if (firing_range.mirror_unit == NONE || unit_index != firing_range.mirrored_unit ||
		!object_try_and_get_and_verify_type(firing_range.mirror_unit, _object_mask_unit))
		return;
	mirror = unit_get(firing_range.mirror_unit);
	/* (its controls the player's turned about the pane: the way it faces
	and aims reflected, its sideways throttle the other way; it picks up
	nothing) */
	mirrored = *control;
	mirrored.control_flags &= (word)~FLAG(_unit_control_action_bit);
	range_mirror_vector(&control->facing_vector, &mirrored.facing_vector);
	range_mirror_vector(&control->aiming_vector, &mirrored.aiming_vector);
	range_mirror_vector(&control->looking_vector, &mirrored.looking_vector);
	mirrored.throttle.j = -control->throttle.j;
	unit_control(firing_range.mirror_unit, &mirrored);
	/* (put where it should be when it has drifted: the floor is the same
	both sides, but a step's push differs) */
	range_mirror_point(&unit->object.position, &place);
	dx = mirror->object.position.x - place.x;
	dy = mirror->object.position.y - place.y;
	if (dx * dx + dy * dy > RANGE_MIRROR_DRIFT * RANGE_MIRROR_DRIFT)
	{
		real_vector3d forward;

		range_mirror_vector(&unit->object.forward, &forward);
		object_set_position(firing_range.mirror_unit, &place, &forward, &unit->object.up);
	}
}

boolean firing_range_damage_ignored(
	long owner_object_index,
	long object_index)
{
	short ai;

	if (!firing_range.active)
		return FALSE;
	if (firing_range.mirror_unit != NONE &&
		(owner_object_index == firing_range.mirror_unit || object_index == firing_range.mirror_unit))
		return TRUE;
	/* (the AI standing north: nothing hurts them) */
	for (ai = 0; ai < firing_range.ai_count; ai++)
	{
		if (object_index != NONE && firing_range.ais[ai].unit == object_index)
			return TRUE;
	}
	return FALSE;
}
