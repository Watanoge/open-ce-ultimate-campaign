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

Its sides are the range's parts, each to come: north the AI's range, east
their animations, west the weapons and south the player's own animations
(a copy that mirrors the player's moves). Today it is the plane alone.

The bsp, as the engine reads one:

- its collision bsp: the floor, the four walls and a ceiling, each a
  surface of its own plane facing in, a bsp3d whose every node's back is
  solid and whose last front is its one leaf, which names the six surfaces
  (each its plane's bsp2d, the surface itself); the surfaces' edge rings run
  counter-clockwise about their planes' normals, as the stock maps' do;
- its render surfaces: the floor in tiles and the walls in panels, two
  triangles each, clockwise seen from the front (the stock maps' are), in
  two materials of one lightmap without a bitmap;
- one cluster, which sees itself, whose subclusters (the floor's blocks of
  tiles, each wall's quarters) list the surfaces the renderer culls by
  their bounds; one structure leaf of that cluster naming every surface,
  and one structure node of whole bounds for each bsp3d node.
*/

/* ---------- headers */

#include "cseries.h"
#include "errors.h"
#include "math/integer_math.h"
#include "physics/collision_bsp_definitions.h"
#include "scenario/scenario_definitions.h"
#include "shaders/shader_definitions.h"
#include "structures/structure_bsp_definitions.h"
#include "tag_files/tag_groups.h"
#include "firing_range.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

/* ---------- constants */

enum
{
	SCENARIO_GROUP_TAG = 'scnr',
	SHADER_ENVIRONMENT_GROUP_TAG = 'senv',
	BITMAP_GROUP_TAG = 'bitm',
	STRUCTURE_BSP_HEADER_SIGNATURE = 'sbsp',

	/* the floor's tiles a side, the walls' panels a side, and the
	subclusters a side of the floor (and of each wall) */
	RANGE_TILES = 16,
	RANGE_SUBCLUSTERS = 4,

	/* the collision bsp: the floor, the walls (east, west, north, south)
	and the ceiling */
	_range_face_floor = 0,
	_range_face_east,
	_range_face_west,
	_range_face_north,
	_range_face_south,
	_range_face_ceiling,
	NUMBER_OF_RANGE_FACES,
	NUMBER_OF_RANGE_WALLS = 4,
	RANGE_BOX_VERTICES = 8,
	RANGE_BOX_EDGES = 12,

	/* the render surfaces: the floor's, then the walls' */
	RANGE_FLOOR_SURFACES = RANGE_TILES * RANGE_TILES * 2,
	RANGE_WALL_SURFACES = RANGE_TILES * 2,
	RANGE_SURFACES = RANGE_FLOOR_SURFACES + NUMBER_OF_RANGE_WALLS * RANGE_WALL_SURFACES,
	RANGE_FLOOR_VERTICES = (RANGE_TILES + 1) * (RANGE_TILES + 1),
	RANGE_WALL_VERTICES = (RANGE_TILES + 1) * 2,
	RANGE_SUBCLUSTER_COUNT = RANGE_SUBCLUSTERS * RANGE_SUBCLUSTERS + NUMBER_OF_RANGE_WALLS * RANGE_SUBCLUSTERS,

	/* the map's players' starting locations kept to put back */
	MAXIMUM_RANGE_STARTING_LOCATIONS = 64,
};

/* the plane's half width, the walls' height drawn and the ceiling's (its
collision's top, and the bsp's bounds'), in world units */
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

/* the map the range is played on, and its shaders (Blood Gulch's own) */
static char const range_map_name[] = "levels\\test\\bloodgulch\\bloodgulch";
static char const range_floor_shader[] = "levels\\b30\\shaders\\metal flat generic";
static char const range_wall_shader[] = "levels\\b30\\shaders\\metal panels generic ext";

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
	range_vector(&v, normal->j * u.k - normal->k * u.j, normal->k * u.i - normal->i * u.k,
		normal->i * u.j - normal->j * u.i);
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

/* each face's inward normal (its plane's: n.p = d), and the box's corner
(bit 0: east, 1: north, 2: the ceiling's) a face's corners are */
static void range_face_normal(
	short face,
	real_vector3d *normal)
{
	switch (face)
	{
	case _range_face_floor: range_vector(normal, 0.0f, 0.0f, 1.0f); break;
	case _range_face_east: range_vector(normal, -1.0f, 0.0f, 0.0f); break;
	case _range_face_west: range_vector(normal, 1.0f, 0.0f, 0.0f); break;
	case _range_face_north: range_vector(normal, 0.0f, -1.0f, 0.0f); break;
	case _range_face_south: range_vector(normal, 0.0f, 1.0f, 0.0f); break;
	default: range_vector(normal, 0.0f, 0.0f, -1.0f); break;
	}
}

static boolean range_face_has_corner(
	short face,
	short corner)
{
	switch (face)
	{
	case _range_face_floor: return !(corner & 4);
	case _range_face_east: return (corner & 1) != 0;
	case _range_face_west: return !(corner & 1);
	case _range_face_north: return (corner & 2) != 0;
	case _range_face_south: return !(corner & 2);
	default: return (corner & 4) != 0;
	}
}

static void range_corner(
	short corner,
	real_point3d *point)
{
	range_point(point, corner & 1 ? RANGE_HALF_WIDTH : -RANGE_HALF_WIDTH,
		corner & 2 ? RANGE_HALF_WIDTH : -RANGE_HALF_WIDTH, corner & 4 ? RANGE_CEILING : 0.0f);
}

/* ---------- private code: the collision bsp */

static boolean range_collision_bsp_write(
	struct range_writer *writer,
	struct collision_bsp *bsp)
{
	struct bsp3d_node *nodes = range_block(writer, &bsp->bsp3d.nodes, NUMBER_OF_RANGE_FACES, sizeof(*nodes));
	real_plane3d *planes = range_block(writer, &bsp->bsp3d.planes, NUMBER_OF_RANGE_FACES, sizeof(*planes));
	struct range_collision_leaf *leaf = range_block(writer, &bsp->leaves, 1, sizeof(*leaf));
	struct range_bsp2d_reference *references = range_block(writer, &bsp->bsp2d_references, NUMBER_OF_RANGE_FACES,
		sizeof(*references));
	struct collision_surface *surfaces = range_block(writer, &bsp->surfaces, NUMBER_OF_RANGE_FACES,
		sizeof(*surfaces));
	struct collision_edge *edges = range_block(writer, &bsp->edges, RANGE_BOX_EDGES, sizeof(*edges));
	struct collision_vertex *vertices = range_block(writer, &bsp->vertices, RANGE_BOX_VERTICES, sizeof(*vertices));
	long face_edges[NUMBER_OF_RANGE_FACES][4];
	long edge_count = 0;
	short face;
	short corner;

	if (writer->full)
		return FALSE;
	bsp->bsp2d.nodes.count = 0;
	for (corner = 0; corner < RANGE_BOX_VERTICES; corner++)
	{
		range_corner(corner, &vertices[corner].point);
		vertices[corner].first_edge_index = NONE;
	}
	for (face = 0; face < NUMBER_OF_RANGE_FACES; face++)
	{
		real_point3d points[4];
		short corners[4];
		short order[4];
		short count = 0;
		short index;

		range_face_normal(face, &planes[face].n);
		for (corner = 0; corner < RANGE_BOX_VERTICES; corner++)
		{
			if (range_face_has_corner(face, corner))
			{
				corners[count] = corner;
				range_corner(corner, &points[count++]);
			}
		}
		planes[face].d = planes[face].n.i * points[0].x + planes[face].n.j * points[0].y +
			planes[face].n.k * points[0].z;
		/* (its back solid, its front the next plane's, the last's the leaf) */
		nodes[face].plane_designator = face;
		nodes[face].children[0] = NONE;
		nodes[face].children[1] = face + 1 < NUMBER_OF_RANGE_FACES ? face + 1 : (long)(0 | LONG_MIN);
		references[face].plane_designator = face;
		references[face].root_index = (long)((unsigned long)face | (unsigned long)LONG_MIN);
		surfaces[face].plane_designator = face;
		surfaces[face].flags = face == _range_face_ceiling ? FLAG(1) : 0;
		surfaces[face].breakable_surface_index = (byte)NONE;
		surfaces[face].material_index = face == _range_face_floor ? 0 : 1;

		/* (its edges, each shared with the face across it, which walks it
		the other way) */
		range_ring_order(points, 4, &planes[face].n, order);
		for (index = 0; index < 4; index++)
		{
			long from = corners[order[index]];
			long to = corners[order[(index + 1) % 4]];
			long edge;

			for (edge = 0; edge < edge_count; edge++)
			{
				if (edges[edge].vertex_indices[0] == to && edges[edge].vertex_indices[1] == from)
					break;
			}
			if (edge == edge_count)
			{
				if (edge_count == RANGE_BOX_EDGES)
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
	if (edge_count != RANGE_BOX_EDGES)
		return FALSE;
	for (face = 0; face < NUMBER_OF_RANGE_FACES; face++)
	{
		short index;

		for (index = 0; index < 4; index++)
		{
			struct collision_edge *edge = &edges[face_edges[face][index]];

			edge->edge_indices[edge->surface_indices[0] == face ? 0 : 1] = face_edges[face][(index + 1) % 4];
		}
	}
	leaf->flags = 0;
	leaf->bsp2d_reference_count = NUMBER_OF_RANGE_FACES;
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
	range_vector(&vertex->binormal, normal->j * tangent->k - normal->k * tangent->j,
		normal->k * tangent->i - normal->i * tangent->k, normal->i * tangent->j - normal->j * tangent->i);
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
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index)
{
	real panel = 2.0f * RANGE_HALF_WIDTH / RANGE_TILES;
	short wall;

	build->first_surface_index = *surface_index;
	for (wall = 0; wall < NUMBER_OF_RANGE_WALLS; wall++)
	{
		short face = (short)(_range_face_east + wall);
		real_vector3d normal;
		real_vector3d tangent;
		long first_vertex = build->vertex_count;
		short step;

		range_face_normal(face, &normal);
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

/* a subcluster of `count` surfaces from `first` (of a material's) */
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
	char const *name)
{
	long shader = tag_loaded(SHADER_ENVIRONMENT_GROUP_TAG, name);

	if (shader == NONE)
		error(_error_silent, "firing range: the map has no shader %s", name);
	return shader;
}

static short range_shader_material_type(
	long shader)
{
	struct shader *definition = shader == NONE ? NULL : (struct shader *)tag_get(SHADER_ENVIRONMENT_GROUP_TAG, shader);

	return definition ? definition->base.physics.material_type : 0;
}

static boolean range_structure_bsp_build(
	struct range_writer *writer,
	struct structure_bsp **result)
{
	struct structure_bsp *bsp = range_allocate(writer, sizeof(*bsp));
	struct collision_bsp *collision = bsp ? range_block(writer, &bsp->collision_bsp, 1, sizeof(*collision)) : NULL;
	long shaders[2];
	struct structure_collision_material *collision_materials;
	struct structure_surface *surfaces;
	struct structure_lightmap *lightmap;
	struct structure_material *materials;
	struct range_material_build builds[2];
	struct range_cluster *cluster;
	struct range_subcluster *subclusters;
	struct structure_leaf *leaf;
	struct range_surface_reference *references;
	byte_rectangle3d *nodes;
	unsigned long *visibility;
	long surface_index = 0;
	short material_index;
	short index;

	*result = bsp;
	if (!collision || !range_collision_bsp_write(writer, collision))
		return FALSE;
	shaders[0] = range_shader(range_floor_shader);
	shaders[1] = range_shader(range_wall_shader);
	if (shaders[0] == NONE || shaders[1] == NONE)
		return FALSE;

	/* (no lightmap: drawn by the renderer's own light, objects lit by the
	default lighting) */
	range_reference(&bsp->lightmap_group, BITMAP_GROUP_TAG, NONE);
	bsp->default_lighting.ambient_color.red = 0.35f;
	bsp->default_lighting.ambient_color.green = 0.35f;
	bsp->default_lighting.ambient_color.blue = 0.38f;
	bsp->default_lighting.distant_light_count = 2;
	bsp->default_lighting.distant_lights[0].color.red = 0.9f;
	bsp->default_lighting.distant_lights[0].color.green = 0.88f;
	bsp->default_lighting.distant_lights[0].color.blue = 0.8f;
	range_vector(&bsp->default_lighting.distant_lights[0].direction, 0.38f, 0.3f, -0.87f);
	bsp->default_lighting.distant_lights[1].color.red = 0.25f;
	bsp->default_lighting.distant_lights[1].color.green = 0.27f;
	bsp->default_lighting.distant_lights[1].color.blue = 0.32f;
	range_vector(&bsp->default_lighting.distant_lights[1].direction, -0.38f, -0.3f, -0.87f);
	bsp->default_lighting.reflection_tint_color.alpha = 1.0f;
	bsp->default_lighting.reflection_tint_color.red = 0.5f;
	bsp->default_lighting.reflection_tint_color.green = 0.5f;
	bsp->default_lighting.reflection_tint_color.blue = 0.5f;
	range_vector(&bsp->default_lighting.shadow_vector, 0.38f, 0.3f, -0.87f);
	bsp->default_lighting.shadow_color.red = 0.4f;
	bsp->default_lighting.shadow_color.green = 0.4f;
	bsp->default_lighting.shadow_color.blue = 0.45f;
	bsp->world_bounds.x0 = -RANGE_HALF_WIDTH;
	bsp->world_bounds.x1 = RANGE_HALF_WIDTH;
	bsp->world_bounds.y0 = -RANGE_HALF_WIDTH;
	bsp->world_bounds.y1 = RANGE_HALF_WIDTH;
	bsp->world_bounds.z0 = 0.0f;
	bsp->world_bounds.z1 = RANGE_CEILING;

	collision_materials = range_block(writer, &bsp->collision_materials, 2, sizeof(*collision_materials));
	surfaces = range_block(writer, &bsp->surfaces, RANGE_SURFACES, sizeof(*surfaces));
	lightmap = range_block(writer, &bsp->lightmaps, 1, sizeof(*lightmap));
	materials = lightmap ? range_block(writer, &lightmap->materials, 2, sizeof(*materials)) : NULL;
	if (!collision_materials || !surfaces || !materials)
		return FALSE;
	for (material_index = 0; material_index < 2; material_index++)
	{
		long vertex_count = material_index ? NUMBER_OF_RANGE_WALLS * RANGE_WALL_VERTICES : RANGE_FLOOR_VERTICES;
		unsigned long vertex_bytes = (unsigned long)vertex_count *
			(sizeof(struct range_vertex) + sizeof(struct range_lightmap_vertex));
		byte *vertex_data = range_allocate(writer, vertex_bytes);

		if (!vertex_data)
			return FALSE;
		range_reference(&collision_materials[material_index].shader, SHADER_ENVIRONMENT_GROUP_TAG,
			shaders[material_index]);
		collision_materials[material_index].runtime_physics_material_type =
			range_shader_material_type(shaders[material_index]);
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
	range_walls_build(&builds[1], surfaces, &surface_index);
	lightmap->bitmap_index = NONE;
	for (material_index = 0; material_index < 2; material_index++)
	{
		struct structure_material *material = &materials[material_index];

		range_reference(&material->shader, SHADER_ENVIRONMENT_GROUP_TAG, shaders[material_index]);
		material->first_surface_index = builds[material_index].first_surface_index;
		material->surface_count = builds[material_index].surface_count;
		material->breakable_surface_index = NONE;
		material->vertices.type = _rasterizer_vertex_type_environment_uncompressed;
		material->vertices.count = builds[material_index].vertex_count;
		material->lightmap_vertices.type = _rasterizer_vertex_type_environment_lightmap_uncompressed;
		material->lightmap_vertices.count = builds[material_index].vertex_count;
		range_point(&material->centroid, 0.0f, 0.0f, material_index ? RANGE_WALL_HEIGHT * 0.5f : 0.0f);
		material->lighting = bsp->default_lighting;
	}

	/* (one cluster, which sees itself, its subclusters the floor's blocks
	of tiles and each wall's quarters) */
	cluster = range_block(writer, &bsp->clusters, 1, sizeof(*cluster));
	subclusters = cluster ? range_block(writer, &cluster->subclusters, RANGE_SUBCLUSTER_COUNT,
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
		/* (a wall's vertices are its material's, as its surfaces name them:
		the walls' build) */
		if (!range_subcluster_write(writer, &subclusters[RANGE_SUBCLUSTERS * RANGE_SUBCLUSTERS + index], surfaces,
			&builds[1], list, count))
			return FALSE;
	}

	/* (one leaf, the collision bsp's, of the cluster, naming every surface
	by its plane's node; one node of whole bounds for each bsp3d node) */
	leaf = range_block(writer, &bsp->leaves, 1, sizeof(*leaf));
	references = range_block(writer, &bsp->surface_references, RANGE_SURFACES, sizeof(*references));
	nodes = range_block(writer, &bsp->nodes, NUMBER_OF_RANGE_FACES, sizeof(*nodes));
	if (!leaf || !references || !nodes)
		return FALSE;
	leaf->bounds.x0 = leaf->bounds.y0 = leaf->bounds.z0 = 0;
	leaf->bounds.x1 = leaf->bounds.y1 = leaf->bounds.z1 = 0xFF;
	leaf->cluster_index = 0;
	leaf->surface_reference_count = RANGE_SURFACES;
	leaf->first_surface_reference_index = 0;
	for (index = 0; index < RANGE_SURFACES; index++)
	{
		references[index].surface_index = index;
		references[index].bsp3d_node_index = index < RANGE_FLOOR_SURFACES ? _range_face_floor :
			_range_face_east + (index - RANGE_FLOOR_SURFACES) / RANGE_WALL_SURFACES;
	}
	for (index = 0; index < NUMBER_OF_RANGE_FACES; index++)
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
		"locations moved)", range_map_name, firing_range.emptied_count, firing_range.starting_location_count);
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

void firing_range_tags_loaded(
	char const *scenario_name,
	long scenario_tag_index)
{
	struct scenario *scenario;

	firing_range.active = FALSE;
	/* (another map loaded: the range is left) */
	if (!firing_range.requested || !range_map_is(scenario_name))
	{
		firing_range.requested = FALSE;
		return;
	}
	scenario = scenario_tag_index == NONE ? NULL : (struct scenario *)tag_get(SCENARIO_GROUP_TAG, scenario_tag_index);
	if (!scenario || scenario->structure_bsp_references.count <= 0)
	{
		error(_error_silent, "firing range: %s has no structure bsp to stand for", range_map_name);
		firing_range.requested = FALSE;
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
}

boolean firing_range_structure_bsp_write(
	void *base_address,
	long size)
{
	struct range_writer writer;
	struct range_structure_bsp_header *header;
	struct structure_bsp *bsp = NULL;

	if (!firing_range.active || !base_address || size <= 0)
		return FALSE;
	writer.base = base_address;
	writer.size = (unsigned long)size;
	writer.used = 0;
	writer.full = FALSE;
	header = range_allocate(&writer, sizeof(*header));
	if (!header || !range_structure_bsp_build(&writer, &bsp))
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
	error(_error_silent, "firing range: the plane's structure bsp written (%lu bytes of %ld): %d surfaces, %d "
		"collision surfaces", writer.used, size, RANGE_SURFACES, NUMBER_OF_RANGE_FACES);

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
