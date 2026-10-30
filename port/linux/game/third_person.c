/*
THIRD_PERSON.C

The view over the shoulder, on foot: the following camera
(following_camera.c) pivoting a little right of (or left of, or above) the
player's eye, back a distance. The player's aim, and the shot, stay the
eye's (first_person_camera_deterministic, as aim assist's), so nothing about
the game or the network changes: the view alone is turned to the point the
eye's line hits, which puts the screen's middle there.

Whether a player sees it is the game's (game_modes_third_person: the
campaign leaves it to the setting, a multiplayer game's host may
(network_third_person.c), a mode may force or forbid it), then the player's
(game.third_person). A scope's zoom is first person: the view
comes back once the zoom is let go.
*/

#include "cseries.h"
#include "third_person.h"
#include "camera/director.h"
#include "camera/observer.h"
#include "camera/static_camera.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/players.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "units/units.h"
#include "game_modes.h"

#include <string.h>

int config_boolean(const char *name);
const char *config_string(const char *name);
unsigned long config_changes(void);

/* the shoulder: how far right (or left) of the eye, and above it; a view
from the middle is higher, to see over the head */
#define THIRD_PERSON_SIDE 0.42f
#define THIRD_PERSON_RISE 0.08f
#define THIRD_PERSON_CENTER_RISE 0.3f
/* how far short of a wall the shoulder stops */
#define THIRD_PERSON_WALL_MARGIN 0.15f
/* how far the eye's line is followed, and the nearest point the view turns
to (closer, it turns too far) */
#define THIRD_PERSON_AIM_RANGE 200.f
#define THIRD_PERSON_AIM_NEAREST 1.f
/* a tick's share of the way to the shoulder chosen, and to what the line hits */
#define THIRD_PERSON_SIDE_GLIDE 0.25f
#define THIRD_PERSON_AIM_GLIDE 0.35f

#define THIRD_PERSON_WALL_FLAGS (FLAG(_collision_test_front_facing_surfaces_bit) | \
	FLAG(_collision_test_structure_bit) | FLAG(_collision_test_objects_bit) | \
	FLAG(_collision_test_objects_scenery_bit))

enum
{
	_shoulder_right = 0,
	_shoulder_left,
	_shoulder_center
};

/* (in the order of game.third_person_distance's values: normal, near, far) */
static real const third_person_distances[] = { 1.9f, 1.3f, 2.7f };

static struct
{
	unsigned long read_at;
	boolean on;
	short shoulder;
	short distance;
} settings = { (unsigned long)-1 };

/* each local player's view: where it is between the shoulders (-1 left, 1
right), and how far along the eye's line it looks, each gliding a tick at a
time; the eye and its line as of the last command */
static struct
{
	boolean valid;
	long commanded_at;
	long glided_at;
	long unit_index;
	real side;
	real aim_distance;
	real_point3d eye;
	real_vector3d forward;
} views[MAXIMUM_LOCAL_PLAYERS];

static void settings_read(void)
{
	char const *value;

	if (settings.read_at == config_changes())
		return;
	settings.read_at = config_changes();
	settings.on = config_boolean("game.third_person") != 0;
	value = config_string("game.third_person_shoulder");
	settings.shoulder = value && !strcmp(value, "left") ? _shoulder_left :
		value && !strcmp(value, "center") ? _shoulder_center : _shoulder_right;
	value = config_string("game.third_person_distance");
	settings.distance = value && !strcmp(value, "near") ? 1 : value && !strcmp(value, "far") ? 2 : 0;
}

boolean third_person_active(short local_player_index)
{
	long player_index, unit_index;
	struct unit_datum *unit;
	short rule;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS ||
		cinematic_in_progress() || (director_camera_scripted && *director_camera_scripted))
		return FALSE;
	player_index = local_player_get_player_index(local_player_index);
	if (player_index == NONE)
		return FALSE;
	unit_index = player_control_get_unit_index(local_player_index);
	if (unit_index == NONE || player_get(player_index)->unit_index != unit_index)
		return FALSE;
	unit = unit_try_and_get(unit_index);
	if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ||
		unit->object.parent_object_index != NONE ||
		unit->unit.current_zoom_level != NONE || unit->unit.desired_zoom_level != NONE)
		return FALSE;
	rule = game_modes_third_person(player_index);
	if (rule == _third_person_rule_choice)
	{
		settings_read();
		return settings.on;
	}
	return rule == _third_person_rule_forced;
}

/* the view of this tick's command, still over the shoulder */
static boolean third_person_viewing(short local_player_index)
{
	return local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS &&
		views[local_player_index].valid && views[local_player_index].commanded_at == game_time_get() &&
		director_get_perspective(local_player_index) == _director_perspective_third_person &&
		third_person_active(local_player_index);
}

void third_person_command(short local_player_index, real_euler_angles2d const *facing,
	struct camera_command *command)
{
	long unit_index = player_control_get_unit_index(local_player_index);
	real side;
	real distance = THIRD_PERSON_AIM_RANGE;
	real_vector3d ray;
	struct collision_result collision;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS || unit_index == NONE)
		return;
	settings_read();
	side = settings.shoulder == _shoulder_left ? -1.f : settings.shoulder == _shoulder_center ? 0.f : 1.f;

	/* what the eye's line hits, as a shot would */
	unit_get_camera_position(unit_index, &views[local_player_index].eye);
	vector3d_from_euler_angles2d(&views[local_player_index].forward, facing);
	scale_vector3d(&views[local_player_index].forward, THIRD_PERSON_AIM_RANGE, &ray);
	if (collision_test_vector(_collision_test_for_projectiles_flags, &views[local_player_index].eye, &ray,
		unit_index, &collision))
	{
		distance = MAX(collision.t * THIRD_PERSON_AIM_RANGE, THIRD_PERSON_AIM_NEAREST);
	}

	/* (afresh for another unit: a spawn) */
	if (!views[local_player_index].valid || views[local_player_index].unit_index != unit_index)
	{
		views[local_player_index].side = side;
		views[local_player_index].aim_distance = distance;
		views[local_player_index].glided_at = game_time_get();
	}
	else if (views[local_player_index].glided_at != game_time_get())
	{
		views[local_player_index].side += (side - views[local_player_index].side) * THIRD_PERSON_SIDE_GLIDE;
		views[local_player_index].aim_distance +=
			(distance - views[local_player_index].aim_distance) * THIRD_PERSON_AIM_GLIDE;
		views[local_player_index].glided_at = game_time_get();
	}
	views[local_player_index].valid = TRUE;
	views[local_player_index].unit_index = unit_index;
	views[local_player_index].commanded_at = game_time_get();

	/* (the observer's offset: i forward, j right, k up of the eye) */
	side = views[local_player_index].side;
	command->offset.i = 0.f;
	command->offset.j = side * THIRD_PERSON_SIDE;
	command->offset.k = THIRD_PERSON_RISE + (1.f - (side < 0.f ? -side : side)) *
		(THIRD_PERSON_CENTER_RISE - THIRD_PERSON_RISE);
	command->depth = third_person_distances[settings.distance];
}

void third_person_clip_focus(short local_player_index, real_point3d const *eye, real_point3d *focus)
{
	real_vector3d vector;
	real length;
	struct collision_result collision;

	if (!third_person_viewing(local_player_index))
		return;
	vector_from_points3d(eye, focus, &vector);
	length = magnitude3d(&vector);
	if (length < 0.01f)
		return;
	if (collision_test_vector(THIRD_PERSON_WALL_FLAGS, eye, &vector, views[local_player_index].unit_index,
		&collision))
	{
		point_from_line3d(eye, &vector, MAX(collision.t - THIRD_PERSON_WALL_MARGIN / length, 0.f), focus);
	}
}

void third_person_converge(short local_player_index, struct observer_result *result)
{
	real_point3d target;
	real_vector3d forward;

	if (!third_person_viewing(local_player_index))
		return;
	point_from_line3d(&views[local_player_index].eye, &views[local_player_index].forward,
		views[local_player_index].aim_distance, &target);
	vector_from_points3d(&result->position, &target, &forward);
	/* (not to a point beside or behind the view: the camera pressed
	against a wall, its line hitting one at the eye) */
	if (normalize3d(&forward) < THIRD_PERSON_AIM_NEAREST * 0.5f ||
		dot_product3d(&forward, &views[local_player_index].forward) < 0.7f)
	{
		return;
	}
	result->forward = forward;
	observer_up_from_forward(&result->forward, &result->up);
}
