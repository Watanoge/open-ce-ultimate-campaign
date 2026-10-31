/*
SKULLS_OUTBREAK.C

Outbreak, a skull of this port's own (skulls.c has the rest): a player's
kill of an enemy may let loose Flood spores, which run to the bodies about
and raise each as the Flood its body makes. An elite's body (a combat
form's too) an elite combat form, a marine's or a crewman's a human combat
form, a grunt's, a jackal's or a hunter's a carrier. The Flood's tags are
Two Betrayals' (c40), copied into the level as it loads (skulls.c's
skulls_tag_import, from watanoge_skulls.pack); a level of the Flood has its own.

A kill lets loose spores half the time, one to four (rolled), one for each
body within three frag grenades' reach (as many as there are bodies), never
the killed enemy's own, in a carrier's burst half the size, harmless. A spore
is an infection form's body, with no actor: this runs it straight to the
body it claimed, as an actor would, each tick. It reaching the body, both
go, and the Flood stands where the body lay, in a carrier's burst of its
particles alone (no blast, no damage). A spore shot pops, as an infection
form does; one that has raised none in six seconds pops itself.

The Flood a spore raised carries no skull to be found, lets loose no spores,
and its death is not counted in the skulls' odds (skulls_outbreak_raised).
The spores and the Flood raised are the game state's: a checkpoint's revert
brings them back as they were.

In co-op every machine copies in the Flood's tags alike (skulls.c's
skull_tags_wanted, by the host's advertisement), and the host alone lets
spores loose and raises the Flood: its spores, bodies and raised Flood are
its objects and actors, which its clients have and move as it does
(network_objects.c, network_actors.c), and it sends each burst to its
clients, which play it (_distributed_message_skulls_outbreak_burst).
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/errors.h"
#include "ai/actors.h"
#include "ai/actor_definitions.h"
#include "effects/effects.h"
#include "game/game.h"
#include "game/game_globals.h"
#include "math/real_math.h"
#include "objects/damage_effect_definitions.h"
#include "objects/objects.h"
#include "saved games/game_state.h"
#include "tag_files/tag_groups.h"
#include "units/units.h"
#include "infection.h"
#include "network_distributed.h"

/* cseries_windows.h's */
unsigned long system_milliseconds(void);

int skulls_effect_quiet;
int skulls_effect_harmless;
int skulls_effect_no_objects;
float skulls_effect_size = 1.0f;

#define OUTBREAK_SPORES_MAX 8
/* the Flood raised that are known as such (the oldest forgotten past it),
and how many of them may stand at once */
#define OUTBREAK_RAISED_MAX 32
#define OUTBREAK_RAISED_ALIVE_MAX 12
#define OUTBREAK_SPORES_PER_KILL 4
/* the burst at a killed enemy that lets spores loose: the carrier's, this size */
#define OUTBREAK_KILL_BURST_SIZE 0.5f
#define OUTBREAK_SPORE_TICKS (6 * TICKS_PER_SECOND)
/* how close a spore comes to its body to raise it, across and up (world
units) */
#define OUTBREAK_REACH 0.5f
#define OUTBREAK_REACH_UP 1.5f
/* the reach of the spores: the frag grenade's damage, three times (2.5
world units: 7.5) */
#define OUTBREAK_FRAG_DAMAGE_TAG "weapons\\frag grenade\\explosion"
#define OUTBREAK_FRAG_REACHES 3.0f
#define OUTBREAK_RANGE_DEFAULT 7.5f
/* where a spore is let loose, above the killed enemy's origin */
#define OUTBREAK_SPORE_HEIGHT 0.2f

enum
{
	_outbreak_body_none = NONE,
	_outbreak_body_elite = 0,
	_outbreak_body_human,
	_outbreak_body_carrier
};

/* in the game state */
struct skulls_outbreak
{
	struct
	{
		long unit_index;
		long body_index;
		long born_time;
	} spores[OUTBREAK_SPORES_MAX];
	long raised[OUTBREAK_RAISED_MAX];
	long raised_next;
};

static struct skulls_outbreak *outbreak;

/* the level's Flood: its spore's body, the variants each body is raised as,
and the burst; NONE, those the level has not */
static long spore_definition = NONE;
static long burst_definition = NONE;
static long elite_variants[3];
static long human_variants[3];
static long carrier_variant = NONE;
static short elite_variant_count;
static short human_variant_count;
static real outbreak_range = OUTBREAK_RANGE_DEFAULT;
static unsigned long outbreak_roll;

static void outbreak_reset(void)
{
	short index;

	if (!outbreak)
		return;
	for (index = 0; index < OUTBREAK_SPORES_MAX; index++)
	{
		outbreak->spores[index].unit_index = NONE;
		outbreak->spores[index].body_index = NONE;
		outbreak->spores[index].born_time = 0;
	}
	for (index = 0; index < OUTBREAK_RAISED_MAX; index++)
		outbreak->raised[index] = NONE;
	outbreak->raised_next = 0;
}

void skulls_outbreak_initialize(void)
{
	/* (a multiple of four bytes, as the game state allocates) */
	outbreak = (struct skulls_outbreak *)game_state_malloc("skulls outbreak", NULL,
		(long)((sizeof(*outbreak) + 3) & ~(unsigned long)3));
	outbreak_reset();
}

void skulls_outbreak_initialize_for_new_map(void)
{
	outbreak_reset();
	/* (the Flood's fog, Outbreak's look in the campaign: infection.c) */
	infection_fog_reset();
}

/* the variants of the names the level has */
static short variants_found(char const *const *names, short count, long *variants)
{
	short found = 0;
	short index;

	for (index = 0; index < count; index++)
	{
		long variant = tag_loaded(ACTOR_VARIANT_DEFINITION_TAG, names[index]);

		if (variant != NONE)
			variants[found++] = variant;
	}
	return found;
}

void skulls_outbreak_level_loaded(void)
{
	static char const *const elite_names[] = { SKULLS_OUTBREAK_ELITE_TAGS };
	static char const *const human_names[] = { SKULLS_OUTBREAK_HUMAN_TAGS };
	long frag_damage;

	spore_definition = tag_loaded('bipd', SKULLS_OUTBREAK_SPORE_TAG);
	burst_definition = tag_loaded('effe', SKULLS_OUTBREAK_BURST_TAG);
	carrier_variant = tag_loaded(ACTOR_VARIANT_DEFINITION_TAG, SKULLS_OUTBREAK_CARRIER_TAG);
	elite_variant_count = variants_found(elite_names, (short)NUMBEROF(elite_names), elite_variants);
	human_variant_count = variants_found(human_names, (short)NUMBEROF(human_names), human_variants);
	frag_damage = tag_loaded(DAMAGE_EFFECT_DEFINITION_TAG, OUTBREAK_FRAG_DAMAGE_TAG);
	outbreak_range = OUTBREAK_RANGE_DEFAULT;
	if (frag_damage != NONE)
	{
		struct damage_effect_definition const *damage =
			(struct damage_effect_definition const *)tag_get(DAMAGE_EFFECT_DEFINITION_TAG, frag_damage);

		if (damage->cutoff_radius > 0.0f)
			outbreak_range = damage->cutoff_radius * OUTBREAK_FRAG_REACHES;
	}
	outbreak_roll = system_milliseconds() * 2654435761UL ^ 0x0B7EA4UL;
	if (skull_active(_skull_outbreak) || skulls_outbreak_wanted())
	{
		error(_error_silent, "skulls: outbreak's flood: spore %s, %d elite and %d human forms, carrier %s, "
			"burst %s; spores reach %.1f world units", spore_definition != NONE ? "yes" : "no",
			elite_variant_count, human_variant_count, carrier_variant != NONE ? "yes" : "no",
			burst_definition != NONE ? "yes" : "no", (double)outbreak_range);
	}
}

/* Outbreak's on, and the level's Flood is all there; in this machine's
game, or the one it hosts (a co-op client's are the host's) */
static boolean outbreak_on(void)
{
	return outbreak && skull_active(_skull_outbreak) && spore_definition != NONE &&
		carrier_variant != NONE && elite_variant_count > 0 && human_variant_count > 0 &&
		game_connection() != _game_connection_network_client;
}

static unsigned long outbreak_random(void)
{
	outbreak_roll = outbreak_roll * 1664525UL + 1013904223UL;
	return (outbreak_roll >> 8) & 0xFFFFFFUL;
}

int skulls_outbreak_raised(long unit_index)
{
	short index;

	if (!outbreak || unit_index == NONE)
		return FALSE;
	for (index = 0; index < OUTBREAK_RAISED_MAX; index++)
	{
		if (outbreak->raised[index] == unit_index)
			return TRUE;
	}
	return FALSE;
}

/* the Flood a body is raised as, by its unit's name; NONE, a body raised
as none (a player's, an infection form's, a carrier's, a cinematic's) */
static int body_kind(struct unit_datum const *unit)
{
	char const *name = tag_get_name(unit->definition_index);
	char lower[256];
	short index;

	if (!name || unit->unit.player_index != NONE)
		return _outbreak_body_none;
	for (index = 0; name[index] && index + 1 < (short)sizeof(lower); index++)
		lower[index] = (char)(name[index] >= 'A' && name[index] <= 'Z' ? name[index] - 'A' + 'a' : name[index]);
	lower[index] = 0;
	if (strstr(lower, "cinematic") || strstr(lower, "cyborg"))
		return _outbreak_body_none;
	if (!strncmp(lower, "characters\\elite\\", 17) || !strncmp(lower, "characters\\floodcombat elite\\", 29))
		return _outbreak_body_elite;
	if (!strncmp(lower, "characters\\marine", 17) || !strncmp(lower, "characters\\crewman\\", 19) ||
		!strncmp(lower, "characters\\floodcombat_human\\", 29))
	{
		return _outbreak_body_human;
	}
	if (!strncmp(lower, "characters\\grunt\\", 17) || !strncmp(lower, "characters\\jackal\\", 18) ||
		!strncmp(lower, "characters\\hunter\\", 18))
	{
		return _outbreak_body_carrier;
	}
	return _outbreak_body_none;
}

/* whether the unit is one a kill of lets loose spores: an infection form,
a carrier, and the Flood raised do not */
static boolean lets_loose_spores(struct unit_datum const *unit, long unit_index)
{
	char const *name = tag_get_name(unit->definition_index);

	if (skulls_outbreak_raised(unit_index) || !name)
		return FALSE;
	return !strstr(name, "flood_infection") && !strstr(name, "floodcarrier");
}

/* whether a spore of the state has claimed the body */
static boolean body_claimed(long body_index)
{
	short index;

	for (index = 0; index < OUTBREAK_SPORES_MAX; index++)
	{
		if (outbreak->spores[index].unit_index != NONE && outbreak->spores[index].body_index == body_index)
			return TRUE;
	}
	return FALSE;
}

/* a body a spore may run to: dead, raised as a Flood, unclaimed */
static boolean body_valid(long body_index)
{
	struct unit_datum *unit = body_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(body_index, _object_mask_unit);

	return unit && TEST_FLAG(unit->object.damage_flags, _object_dead_bit) &&
		body_kind(unit) != _outbreak_body_none;
}

/* the nearest bodies within reach of the point, unclaimed, but the one
left out: as many as fit, nearest first; their count */
static short bodies_near(real_point3d const *point, long left_out, long *bodies, short room)
{
	struct object_iterator iterator;
	struct unit_datum *unit;
	real distances[OUTBREAK_SPORES_PER_KILL];
	short count = 0;

	object_iterator_new(&iterator, _object_mask_unit, 0);
	while ((unit = object_iterator_next(&iterator)) != NULL)
	{
		real_point3d origin;
		real distance;
		short place;

		if (iterator.index == left_out || !body_valid(iterator.index) || body_claimed(iterator.index))
			continue;
		object_get_origin(iterator.index, &origin);
		distance = distance3d(point, &origin);
		if (distance > outbreak_range)
			continue;
		/* (kept nearest first) */
		for (place = count; place > 0 && distances[place - 1] > distance; place--)
		{
			if (place < room)
			{
				distances[place] = distances[place - 1];
				bodies[place] = bodies[place - 1];
			}
		}
		if (place < room)
		{
			distances[place] = distance;
			bodies[place] = iterator.index;
			if (count < room)
				count++;
		}
	}
	return count;
}

static short free_spore_slot(void)
{
	short index;

	for (index = 0; index < OUTBREAK_SPORES_MAX; index++)
	{
		if (outbreak->spores[index].unit_index == NONE)
			return index;
	}
	return NONE;
}

/* the level way from the point to the other, normalized; FALSE, none */
static boolean level_direction(real_point3d const *from, real_point3d const *to, real_vector3d *direction)
{
	direction->i = to->x - from->x;
	direction->j = to->y - from->y;
	direction->k = 0.0f;
	return normalize3d(direction) > _real_epsilon;
}

static void burst_at(long unit_index, boolean quiet, float size);

void skulls_outbreak_kill(long unit_index, long killer_player_index)
{
	struct unit_datum *unit;
	long bodies[OUTBREAK_SPORES_PER_KILL];
	real_point3d origin;
	short count;
	short wanted;
	short index;

	if (killer_player_index == NONE || !outbreak_on() || !skulls_unit_is_enemy(unit_index))
		return;
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	if (!unit || !lets_loose_spores(unit, unit_index))
		return;
	object_get_origin(unit_index, &origin);
	/* (no body near: no spores, and no roll spent; then half the time, one
	to four, as many as there are bodies for) */
	count = bodies_near(&origin, unit_index, bodies, OUTBREAK_SPORES_PER_KILL);
	if (!count || (outbreak_random() & 1))
		return;
	wanted = (short)(1 + outbreak_random() % OUTBREAK_SPORES_PER_KILL);
	if (count > wanted)
		count = wanted;
	burst_at(unit_index, FALSE, OUTBREAK_KILL_BURST_SIZE);
	for (index = 0; index < count; index++)
	{
		struct object_placement_data placement_data;
		real_point3d body_origin;
		short slot = free_spore_slot();
		long spore_index;

		if (slot == NONE)
			break;
		object_get_origin(bodies[index], &body_origin);
		object_placement_data_new(&placement_data, spore_definition, NONE);
		placement_data.position = origin;
		placement_data.position.z += OUTBREAK_SPORE_HEIGHT;
		if (!level_direction(&origin, &body_origin, &placement_data.forward))
			placement_data.forward = *global_forward3d;
		spore_index = object_new(&placement_data);
		if (spore_index == NONE)
			break;
		outbreak->spores[slot].unit_index = spore_index;
		outbreak->spores[slot].body_index = bodies[index];
		outbreak->spores[slot].born_time = game_time_get();
	}
	error(_error_silent, "skulls: outbreak, %d spore%s let loose", count, count == 1 ? "" : "s");
}

/* how many of the Flood raised stand */
static short raised_alive(void)
{
	short alive = 0;
	short index;

	for (index = 0; index < OUTBREAK_RAISED_MAX; index++)
	{
		struct unit_datum *unit = outbreak->raised[index] == NONE ? NULL :
			(struct unit_datum *)object_try_and_get_and_verify_type(outbreak->raised[index], _object_mask_unit);

		if (unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			alive++;
	}
	return alive;
}

/* a carrier's burst at the unit, harmless: quiet, its particles alone (a
raised Flood's), else with its sound; of the size */
/* a burst, sent to a co-op host's clients: where, how big, and whether
quiet */
struct skulls_outbreak_burst_message
{
	struct distributed_message_header header;
	real_point3d point;
	real size;
	long quiet;
};

static void burst_at_point(real_point3d const *point, long owner_index, boolean quiet, float size)
{
	char const *marker_names[1];
	real_point3d marker_points[1];
	real_vector3d marker_forwards[1];

	if (burst_definition == NONE)
		return;
	marker_names[0] = "";
	marker_points[0] = *point;
	marker_forwards[0] = *global_up3d;
	skulls_effect_quiet = quiet;
	skulls_effect_harmless = TRUE;
	skulls_effect_size = size;
	effect_new_unattached_from_markers(burst_definition, owner_index, NULL, 1, marker_names, marker_points,
		marker_forwards, 0.0f, 0.0f, NULL, NULL, TRUE);
	skulls_effect_quiet = FALSE;
	skulls_effect_harmless = FALSE;
	skulls_effect_size = 1.0f;
}

static void burst_at(long unit_index, boolean quiet, float size)
{
	struct skulls_outbreak_burst_message message;
	real_point3d point;

	if (burst_definition == NONE)
		return;
	object_get_origin(unit_index, &point);
	burst_at_point(&point, unit_index, quiet, size);
	/* (a co-op host's clients play it too: effects are each machine's) */
	if (game_connection() == _game_connection_network_server)
	{
		csmemset(&message, 0, sizeof(message));
		message.point = point;
		message.size = size;
		message.quiet = quiet ? 1 : 0;
		distributed_send(&message, _distributed_message_skulls_outbreak_burst, 1, sizeof(message),
			_distributed_to_clients);
	}
}

int skulls_outbreak_handles_message(unsigned short const *message, unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return FALSE;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_skulls_outbreak_burst;
}

/* a co-op client: the host's burst, played (no owner: the unit may not be
here yet) */
void skulls_outbreak_handle_message(long machine_index, unsigned short const *data, unsigned short size)
{
	struct skulls_outbreak_burst_message message;

	if (machine_index != NONE || game_connection() != _game_connection_network_client || !game_in_progress() ||
		size != sizeof(message))
	{
		return;
	}
	memcpy(&message, data, sizeof(message));
	if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.type != _distributed_message_skulls_outbreak_burst ||
		message.header.count != 1 || message.quiet > 1 || !(message.size > 0.0f && message.size <= 4.0f) ||
		!valid_real_point3d(&message.point))
	{
		return;
	}
	burst_at_point(&message.point, NONE, (boolean)message.quiet, message.size);
}

/* the body raised as its Flood, the spore gone into it: TRUE, done */
static boolean body_raise(long body_index)
{
	struct unit_datum *body = (struct unit_datum *)object_get_and_verify_type(body_index, _object_mask_unit);
	int kind = body_kind(body);
	real_point3d position;
	real_vector3d forward;
	long variant;
	long unit_index;

	if (raised_alive() >= OUTBREAK_RAISED_ALIVE_MAX)
		return FALSE;
	switch (kind)
	{
	case _outbreak_body_elite:
		variant = elite_variants[outbreak_random() % (unsigned long)elite_variant_count];
		break;
	case _outbreak_body_human:
		variant = human_variants[outbreak_random() % (unsigned long)human_variant_count];
		break;
	case _outbreak_body_carrier:
		variant = carrier_variant;
		break;
	default:
		return FALSE;
	}
	object_get_origin(body_index, &position);
	forward = body->object.forward;
	forward.k = 0.0f;
	if (normalize3d(&forward) <= _real_epsilon)
		forward = *global_forward3d;
	error(_error_silent, "skulls: outbreak raises %s as %s", tag_get_name(body->definition_index),
		tag_get_name(variant));
	object_delete(body_index);
	unit_index = actor_port_place_free(variant, &position, &forward);
	if (unit_index == NONE)
		return TRUE;
	outbreak->raised[outbreak->raised_next] = unit_index;
	outbreak->raised_next = (outbreak->raised_next + 1) % OUTBREAK_RAISED_MAX;
	burst_at(unit_index, TRUE, 1.0f);
	return TRUE;
}

static void spore_forget(short slot)
{
	outbreak->spores[slot].unit_index = NONE;
	outbreak->spores[slot].body_index = NONE;
}

static void spore_pop(short slot)
{
	unit_kill(outbreak->spores[slot].unit_index);
	spore_forget(slot);
}

void skulls_outbreak_tick(void)
{
	long now = game_time_get();
	short slot;

	if (!outbreak)
		return;
	for (slot = 0; slot < OUTBREAK_SPORES_MAX; slot++)
	{
		long spore_index = outbreak->spores[slot].unit_index;
		struct unit_datum *spore;
		real_point3d spore_origin, body_origin;
		real_vector3d facing;
		real across;

		if (spore_index == NONE)
			continue;
		spore = (struct unit_datum *)object_try_and_get_and_verify_type(spore_index, _object_mask_unit);
		/* (gone, another object in its place, or popped: shot) */
		if (!spore || spore->definition_index != spore_definition ||
			TEST_FLAG(spore->object.damage_flags, _object_dead_bit))
		{
			spore_forget(slot);
			continue;
		}
		/* (hurt: popped, as an infection form's swarm pops it at any hit. Its
		body alone, with no actor, would take the whole of its collision
		model's 80 points, twice a grunt's, the swarm's to be popped by) */
		if (spore->object.body_vitality < 1.0f)
		{
			spore_pop(slot);
			continue;
		}
		/* (Outbreak off now, or the time up) */
		if (!outbreak_on() || now - outbreak->spores[slot].born_time > OUTBREAK_SPORE_TICKS ||
			now < outbreak->spores[slot].born_time)
		{
			spore_pop(slot);
			continue;
		}
		object_get_origin(spore_index, &spore_origin);
		/* (its body gone, or raised by another: the nearest other, if any) */
		if (!body_valid(outbreak->spores[slot].body_index))
		{
			long body;

			outbreak->spores[slot].body_index = NONE;
			if (!bodies_near(&spore_origin, NONE, &body, 1))
			{
				spore_pop(slot);
				continue;
			}
			outbreak->spores[slot].body_index = body;
		}
		object_get_origin(outbreak->spores[slot].body_index, &body_origin);
		across = (body_origin.x - spore_origin.x) * (body_origin.x - spore_origin.x) +
			(body_origin.y - spore_origin.y) * (body_origin.y - spore_origin.y);
		if (across <= OUTBREAK_REACH * OUTBREAK_REACH &&
			fabs(body_origin.z - spore_origin.z) <= OUTBREAK_REACH_UP)
		{
			long body_index = outbreak->spores[slot].body_index;

			/* (too many raised standing: the spore pops on the body) */
			if (!body_raise(body_index))
			{
				spore_pop(slot);
				continue;
			}
			spore_forget(slot);
			object_delete(spore_index);
			continue;
		}
		if (!level_direction(&spore_origin, &body_origin, &facing))
			facing = spore->object.forward;
		unit_port_run(spore_index, &facing);
	}
}
