/*
INFECTION.C

Infection, a mode of team Slayer (game_modes.c). Humans are red and keep the
gametype's equipment; zombies are blue, carry the Oddball skull and nothing
else, and fight on foot, faster than humans, their skull's melee stronger.
The round waits for two living players, counts down, and then the host
chooses the first zombies (leaving at least one human). A human killed by a
zombie, by themselves or by the world becomes a zombie (one killed by a
teammate does not); a zombie respawns a zombie, and a player who joins late
joins the zombies. The zombies win when no human is left, the humans when the
round's time runs out. Each player scores a kill of the other side.

Only the host decides: it chooses the zombies, turns the dead, gives the
skulls and ends the round, and sends every client its state
(struct distributed_infection_state, with the game type's:
game_engine_write_network_state), which a client checks whole before taking.
Each machine then sets each player's team, speed and shields by their role.

Its options are the gametype's (game_engine.h's struct game_variant_options,
infection_*). Floodfection (the options' mode_variant) is Infection with the
Flood's look, which each machine of this fork draws for itself: its zombies
green instead of blue and drawn as Flood combat forms, but in their own
player's first person (floodfection_forms.c): the round's first zombies
elites, with their shields and twice as fast and as healthy as a human
(200% speed and health), the rest humans, without shields and half as fast
and as healthy again as a human (150%), whatever the options say; a
headshot not killing them outright, bleeding the Flood's green, crying out
as the Flood when hurt, dying or killing a human, and throwing its bits as
they die; its humans each in their own colour (as free for all's) instead
of red, the last tenth of them strengthened (whole, overshielded, half
their ammunition and a grenade), and the last one again (overshielded, all
their ammunition and two grenades) and shown to every zombie; every map
under a green murk that hides the sky and all past some 55 world units (16
under a roof), and a Flood carrier's burst where a human turns or a zombie
dies (its tags from watanoge_gamemodes.pack). The host tells its clients
the first zombies apart from the state
(_distributed_message_floodfection_originals). A build without it plays
the same round, its zombies blue and all alike, and its maps clear. The
rules and much of the code follow the Infection of pull
request #420 by arisson2001rojas-design.
*/

#include "cseries.h"
#include "cutscene/cinematics.h"
#include "effects/effect_definitions.h"
#include "effects/effects.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_messaging.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "physics/collisions.h"
#include "render/render_cameras.h"
#include "saved games/player_profile.h"
#include "scenario/scenario.h"
#include "sound/game_sound.h"
#include "sound/sound_definitions.h"
#include "tag_files/tag_groups.h"
#include "text/unicode.h"
#include "units/bipeds.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "halo_skulls.h"
#include "floodfection_forms.h"
#include "game_modes.h"
#include "infection.h"
#include "network_distributed.h"
#include "network_infection.h"
#include "tag_import.h"
#include "third_person.h"

void platform_log(char const *format, ...);

/* a score past this is not a client's to take */
#define INFECTION_MAXIMUM_SCORE 1000000

static struct
{
	struct game_engine engine;
	struct game_engine slayer;
	struct distributed_infection_state state;
	/* the host's, by absolute player index: the player of the slot, the unit
	given its role's equipment, the unit counted dead, the skull given, and
	the unit whose shields a zombie's role took (and their maximum) */
	long participant[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long equipped_unit[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long killed_unit[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long skull[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long shield_unit[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	real shield_maximum[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	/* the unit whose body a Floodfection zombie's role strengthened, and
	its maximum before (by absolute player index; every machine's) */
	long health_unit[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	real health_maximum[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	/* the host's, of a Floodfection game: the unit last strengthened as one
	of the last humans (by absolute player index), and as the last */
	long last_stand_unit[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long last_human_unit;
	/* a skull being given (the pickup checks allow it) */
	boolean equipping;
	/* the map has no skull, or one could not be given: the round ends */
	boolean missing_skull;
} infection;

/* ---- the players' roles */

boolean infection_enabled(
	void)
{
	return game_modes_mode() == _game_mode_infection;
}

/* a player's identity on every machine: their name and place in the game */
static unsigned long infection_player_identity(
	long player_index)
{
	struct network_player const *network_player = &player_get(player_index)->network_player_data;
	unsigned long hash = 2166136261UL;
	short index;

	for (index = 0; index < NUMBEROF(network_player->name); index++)
		hash = (hash ^ (word)network_player->name[index]) * 16777619UL;
	hash = (hash ^ (byte)network_player->machine_index) * 16777619UL;
	hash = (hash ^ (byte)network_player->controller_index) * 16777619UL;
	hash = (hash ^ (byte)network_player->player_list_index) * 16777619UL;
	return hash ? hash : 1;
}

/* a player's role, if the state has it for them (not one who left, whose
slot another took) */
static struct distributed_infection_player *infection_player(
	long player_index)
{
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);

	if (player_index == NONE || !VALID_INDEX(slot, HALO_PORT_MAXIMUM_NETWORK_PLAYERS) ||
		!player_try_and_get(player_index) ||
		infection.state.players[slot].identity != infection_player_identity(player_index))
	{
		return NULL;
	}
	return &infection.state.players[slot];
}

static boolean infection_weapon_is_skull(
	long weapon_index)
{
	struct object_datum *object = object_try_and_get_and_verify_type(weapon_index, _object_mask_weapon);

	return object && object->definition_index == get_ball_definition_index();
}

boolean infection_can_delete_skull(
	long object_index)
{
	return infection_enabled() && infection_weapon_is_skull(object_index);
}

boolean infection_unit_is_zombie(
	long unit_index)
{
	struct unit_datum *unit;
	struct distributed_infection_player *role;

	if (unit_index == NONE || !infection_enabled() || (unit = unit_try_and_get(unit_index)) == NULL)
		return FALSE;
	role = infection_player(unit->unit.player_index);
	return role && role->role == _distributed_infection_role_zombie;
}

/* asked of a player's own drops only: a death, and the inventories the host
sends, still take a skull away */
boolean infection_allow_weapon_drop(
	long unit_index,
	long weapon_index)
{
	struct unit_datum *unit;
	struct player_datum *player;

	if (!infection_enabled() || infection.state.phase != _distributed_infection_active || infection.equipping ||
		!infection_unit_is_zombie(unit_index))
	{
		return TRUE;
	}
	unit = unit_get(unit_index);
	player = player_try_and_get(unit->unit.player_index);
	if (!player || player->quit_out_of_game || player->unit_index != unit_index ||
		TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
	{
		return TRUE;
	}
	return !infection_weapon_is_skull(weapon_index);
}

/* a zombie's skull in the hand */
static boolean infection_unit_holds_skull(
	long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);

	return VALID_INDEX(unit->unit.current_weapon_index, MAXIMUM_WEAPONS_PER_UNIT) &&
		infection_weapon_is_skull(unit->unit.weapon_object_indices[unit->unit.current_weapon_index]);
}

real infection_melee_multiplier(
	long unit_index)
{
	if (!infection_unit_is_zombie(unit_index) || infection.state.phase != _distributed_infection_active ||
		!infection_unit_holds_skull(unit_index))
	{
		return 1.0f;
	}
	return game_variant_options_get()->infection_skull_damage / 100.0f;
}

/* a living human in a seat of a vehicle that is not hidden, whom a zombie
holding its skull struck itself (not the vehicle: Slayer's melee hits the
vehicle a rider is in) */
boolean infection_melee_rider_target(
	long attacker_index,
	long target_index)
{
	struct unit_datum *target;
	struct unit_datum *vehicle;
	struct distributed_infection_player *role;
	struct unit_definition *definition;
	short seat;

	if (!infection_unit_is_zombie(attacker_index) || infection.state.phase != _distributed_infection_active ||
		!infection_unit_holds_skull(attacker_index))
	{
		return FALSE;
	}
	target = unit_try_and_get(target_index);
	if (!target || target->object.type != _object_type_biped || TEST_FLAG(target->object.damage_flags, _object_dead_bit))
		return FALSE;
	role = infection_player(target->unit.player_index);
	if (!role || role->role != _distributed_infection_role_human)
		return FALSE;
	vehicle = (struct unit_datum *)object_try_and_get_and_verify_type(target->object.parent_object_index,
		_object_mask_vehicle);
	if (!vehicle)
		return FALSE;
	definition = unit_definition_get(vehicle->definition_index);
	seat = target->unit.parent_seat_index;
	return VALID_INDEX(seat, definition->unit.seats.count) &&
		!TEST_FLAG(TAG_BLOCK_GET_ELEMENT(&definition->unit.seats, seat, struct unit_seat)->flags,
			_unit_seat_invisible_bit);
}

/* the host's check of a client's melee report on a rider: one of its own
lines through the melee's cone (0.8 world units from the attacker's head,
as the game's melee) reaches the rider, through the host's own world, not
from a place the client says */
boolean infection_melee_rider_visible(
	long attacker_index,
	long target_index,
	real_vector3d const *direction)
{
	struct object_marker head;
	real_vector3d facing = *direction;
	real_vector3d perpendicular;
	real_vector3d cross;
	real_vector3d ray;
	struct collision_result hit;
	long outer;
	long inner;

	if (!infection_melee_rider_target(attacker_index, target_index) || normalize3d(&facing) <= 0.0f)
		return FALSE;
	object_get_marker_by_name(attacker_index, "head", &head, 1);
	normalize3d(perpendicular3d(&facing, &perpendicular));
	cross_product3d(&facing, &perpendicular, &cross);
	for (outer = -2; outer <= 2; outer++)
	{
		for (inner = -2; inner <= 2; inner++)
		{
			ray.i = facing.i * 0.8f + (cross.i * inner + perpendicular.i * outer) * 0.1f;
			ray.j = facing.j * 0.8f + (cross.j * inner + perpendicular.j * outer) * 0.1f;
			ray.k = facing.k * 0.8f + (cross.k * inner + perpendicular.k * outer) * 0.1f;
			if (collision_test_vector(_collision_test_for_projectiles_flags, &head.matrix.position, &ray,
				attacker_index, &hit) && hit.type == _collision_result_object && hit.object_index == target_index)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

/* ---- Floodfection's look */

/* the Flood's fog (Floodfection's, and a campaign's with Outbreak on): from a few world units off, opaque at its end, which the
window clears to (the sky not drawn). Outdoors as far as a field needs;
under a roof (a cluster with no sky: Prisoner's rooms, a base's halls) much
closer, as rooms a few units across would hardly be tinted by the
outdoors' */
#define FLOODFECTION_FOG_START_DISTANCE 3.0f
#define FLOODFECTION_FOG_OPAQUE_DISTANCE 55.0f
#define FLOODFECTION_INDOOR_FOG_START_DISTANCE 0.5f
#define FLOODFECTION_INDOOR_FOG_OPAQUE_DISTANCE 16.0f
/* the share of the way from one to the other each frame drawn, walking in
or out a door */
#define FLOODFECTION_FOG_BLEND 0.08f
/* the seconds the fog takes to thin out as a cutscene starts, and to close
in again after it */
#define FLOODFECTION_FOG_CUTSCENE_FADE_SECONDS 2.0f

static real_rgb_color const floodfection_zombie_color = { { 0.30f, 0.48f, 0.12f } };
static real_rgb_color const floodfection_fog_color = { { 0.16f, 0.20f, 0.09f } };

/* each local player's fog distances as last drawn, 0 before the first */
static real floodfection_fog_start[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
static real floodfection_fog_opaque[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
/* how much of the fog is over the view, 1 out of a cutscene and 0 in one,
eased between them from floodfection_fog_fade_time's (real time); below 0
before the first frame drawn, which takes the one it wants at once */
static real floodfection_fog_fade = -1.0f;
static unsigned long floodfection_fog_fade_time;

static boolean infection_floodfection(
	void)
{
	return infection_enabled() && game_modes_mode_variant() == _game_mode_variant_floodfection;
}

boolean infection_floodfection_enabled(
	void)
{
	return infection_floodfection();
}

/* ---- Floodfection's settings (the gametype's: infection.h's
_floodfection_setting_*), a table's place each, its 0 the default */

static short const floodfection_human_healths[] = { 150, 100, 125, 175, 200, 250, 300 };
static short const floodfection_human_speeds[] = { 150, 100, 110, 125, 175, 200, 250 };
static short const floodfection_elite_healths[] = { 200, 100, 125, 150, 175, 250, 300 };
static short const floodfection_elite_speeds[] = { 200, 100, 125, 150, 175, 225, 250 };
static short const floodfection_elite_chances[] = { 25, 10, 15, 20, 35, 50, 75, 100 };
/* (0: no last stand) */
static short const floodfection_last_stand_percents[] = { 10, 0, 5, 15, 20, 25, 33, 50 };

static short floodfection_setting(
	long setting)
{
	byte const *bytes = game_variant_options_get()->floodfection;

	return (short)((bytes[FLOODFECTION_SETTING_BYTE(setting)] >> FLOODFECTION_SETTING_SHIFT(setting)) &
		((1 << FLOODFECTION_SETTING_BITS(setting)) - 1));
}

/* the setting's value of its table (out of it: the default) */
static short floodfection_setting_value(
	long setting,
	short const *table,
	short count)
{
	short index = floodfection_setting(setting);

	return table[index < count ? index : 0];
}

#define FLOODFECTION_SETTING_VALUE(setting, table) floodfection_setting_value(setting, table, NUMBEROF(table))

short infection_third_person(
	long player_index)
{
	struct player_datum *player = player_try_and_get(player_index);

	switch (floodfection_setting(_floodfection_setting_camera))
	{
	case _floodfection_camera_flood:
		return player && infection_unit_is_zombie(player->unit_index) ?
			_third_person_rule_forced : _third_person_rule_first_person;
	case _floodfection_camera_everyone:
		return _third_person_rule_forced;
	case _floodfection_camera_choice:
		return _third_person_rule_choice;
	default:
		return _third_person_rule_first_person;
	}
}

boolean infection_floodfection_zombie(
	long object_index)
{
	return object_index != NONE && infection_floodfection() &&
		object_try_and_get_and_verify_type(object_index, _object_mask_unit) &&
		infection_unit_is_zombie(object_index);
}

/* a zombie green; anyone else the colour of their profile, as free for
all's (the humans are one team, but not one colour) */
void infection_change_color(
	long player_index,
	real_rgb_color *color)
{
	struct distributed_infection_player *role;

	if (!infection_floodfection())
		return;
	role = infection_player(player_index);
	if (role && role->role == _distributed_infection_role_zombie)
		*color = floodfection_zombie_color;
	else
		*color = player_profile_get_rgb_color(player_get(player_index)->network_player_data.primary_color_index);
}

void infection_fog_reset(
	void)
{
	csmemset(floodfection_fog_start, 0, sizeof(floodfection_fog_start));
	csmemset(floodfection_fog_opaque, 0, sizeof(floodfection_fog_opaque));
	floodfection_fog_fade = -1.0f;
}

/* the fog's share eased a frame on toward a cutscene's 0 or the game's 1;
TRUE, in a cutscene: the sky shown through it (and hidden again after) at
once, as the sky draws unfogged, under the fade to black that starts and ends
most cutscenes */
static boolean floodfection_fog_fade_update(
	void)
{
	boolean cutscene = cinematic_globals && game_in_progress() && cinematic_in_progress();
	real wanted = cutscene ? 0.0f : 1.0f;
	unsigned long now = system_milliseconds();

	if (floodfection_fog_fade < 0.0f)
		floodfection_fog_fade = wanted;
	else
	{
		real step = (real)(now - floodfection_fog_fade_time) / (1000.0f * FLOODFECTION_FOG_CUTSCENE_FADE_SECONDS);

		if (floodfection_fog_fade < wanted)
			floodfection_fog_fade = MIN(wanted, floodfection_fog_fade + step);
		else
			floodfection_fog_fade = MAX(wanted, floodfection_fog_fade - step);
	}
	floodfection_fog_fade_time = now;

	return cutscene;
}

boolean infection_fog(
	short local_player_index,
	boolean indoors,
	struct render_fog *fog)
{
	real start = indoors ? FLOODFECTION_INDOOR_FOG_START_DISTANCE : FLOODFECTION_FOG_START_DISTANCE;
	real opaque = indoors ? FLOODFECTION_INDOOR_FOG_OPAQUE_DISTANCE : FLOODFECTION_FOG_OPAQUE_DISTANCE;
	boolean cutscene;

	/* (skull_active: a campaign's level alone) */
	if (!infection_floodfection() && !skull_active(_skull_outbreak))
		return FALSE;
	if (VALID_INDEX(local_player_index, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS))
	{
		real *last_start = &floodfection_fog_start[local_player_index];
		real *last_opaque = &floodfection_fog_opaque[local_player_index];

		if (*last_opaque > 0.0f)
		{
			start = *last_start + (start - *last_start) * FLOODFECTION_FOG_BLEND;
			opaque = *last_opaque + (opaque - *last_opaque) * FLOODFECTION_FOG_BLEND;
		}
		*last_start = start;
		*last_opaque = opaque;
	}
	cutscene = floodfection_fog_fade_update();
	if (floodfection_fog_fade < 1.0f)
	{
		/* thinning to the map's own fog (scenario_get_atmospheric_fog's), or
		none on a map without, and back */
		real fade = floodfection_fog_fade;
		real map_density = fog->atmospheric_maximum_distance != 0.0f ? fog->atmospheric_maximum_density : 0.0f;

		if (fade <= 0.0f)
			return !cutscene;
		if (map_density > 0.0f)
		{
			start = fog->atmospheric_minimum_distance + (start - fog->atmospheric_minimum_distance) * fade;
			opaque = fog->atmospheric_maximum_distance + (opaque - fog->atmospheric_maximum_distance) * fade;
		}
		fog->atmospheric_color.red += (floodfection_fog_color.red - fog->atmospheric_color.red) * fade;
		fog->atmospheric_color.green += (floodfection_fog_color.green - fog->atmospheric_color.green) * fade;
		fog->atmospheric_color.blue += (floodfection_fog_color.blue - fog->atmospheric_color.blue) * fade;
		fog->atmospheric_maximum_density = map_density + (1.0f - map_density) * fade;
		fog->atmospheric_minimum_distance = start;
		fog->atmospheric_maximum_distance = MAX(opaque, start + 0.0001f);
		return !cutscene;
	}
	fog->atmospheric_color = floodfection_fog_color;
	fog->atmospheric_maximum_density = 1.0f;
	fog->atmospheric_minimum_distance = start;
	fog->atmospheric_maximum_distance = MAX(opaque, start + 0.0001f);
	return TRUE;
}

/* ---- Floodfection's elites */

/* the zombies that are elites, by absolute player index (a bit each): the
host's, and as it last told every client (a client's, the host's word).
The host chooses by the gametype's ELITES: the round's first zombies (until
they leave), none, or each zombie's spawn at its chance (floodfection_choose_elite) */
#define FLOODFECTION_ORIGINAL_WORDS (HALO_PORT_MAXIMUM_NETWORK_PLAYERS / 32)

static unsigned long floodfection_originals[FLOODFECTION_ORIGINAL_WORDS];
static unsigned long floodfection_originals_sent[FLOODFECTION_ORIGINAL_WORDS];
static boolean floodfection_originals_sent_valid;
/* the host's: the round's first zombies, and the unit each player's elite
was last chosen for */
static unsigned long floodfection_first[FLOODFECTION_ORIGINAL_WORDS];
static long floodfection_elite_unit[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];

struct floodfection_originals_message
{
	struct distributed_message_header header;
	/* the game they are of (network_game_get_number_of_games_played) */
	long game_number;
	unsigned long originals[FLOODFECTION_ORIGINAL_WORDS];
};

static void floodfection_set_bit(
	unsigned long *bits,
	long slot,
	boolean set)
{
	unsigned long bit = 1UL << (slot & 31);

	if (!VALID_INDEX(slot, HALO_PORT_MAXIMUM_NETWORK_PLAYERS))
		return;
	if (set)
		bits[slot >> 5] |= bit;
	else
		bits[slot >> 5] &= ~bit;
}

static boolean floodfection_bit(
	unsigned long const *bits,
	long slot)
{
	return VALID_INDEX(slot, HALO_PORT_MAXIMUM_NETWORK_PLAYERS) && (bits[slot >> 5] & (1UL << (slot & 31))) != 0;
}

/* (the host) a player gone, or new: neither a first zombie nor an elite */
static void floodfection_forget_player(
	long slot)
{
	floodfection_set_bit(floodfection_originals, slot, FALSE);
	floodfection_set_bit(floodfection_first, slot, FALSE);
	if (VALID_INDEX(slot, HALO_PORT_MAXIMUM_NETWORK_PLAYERS))
		floodfection_elite_unit[slot] = NONE;
}

/* (the host) a zombie's unit new to it: an elite or not, by ELITES */
static void floodfection_choose_elite(
	long slot,
	long unit_index)
{
	boolean elite = FALSE;

	if (!VALID_INDEX(slot, HALO_PORT_MAXIMUM_NETWORK_PLAYERS) || floodfection_elite_unit[slot] == unit_index)
		return;
	floodfection_elite_unit[slot] = unit_index;
	switch (floodfection_setting(_floodfection_setting_elites))
	{
	case _floodfection_elites_none:
		break;
	case _floodfection_elites_random:
		elite = local_random_range(0, 100) < FLOODFECTION_SETTING_VALUE(_floodfection_setting_elite_chance,
			floodfection_elite_chances);
		break;
	default:
		elite = floodfection_bit(floodfection_first, slot);
		break;
	}
	floodfection_set_bit(floodfection_originals, slot, elite);
}

/* a zombie that is an elite, in Floodfection */
static boolean infection_original(
	long player_index)
{
	struct distributed_infection_player *role = infection_player(player_index);
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);

	return role && role->role == _distributed_infection_role_zombie && infection_floodfection() &&
		floodfection_bit(floodfection_originals, slot);
}

void infection_send_originals(
	long machine_index)
{
	struct floodfection_originals_message message;

	if (game_connection() != _game_connection_network_server || !infection_floodfection())
		return;
	if (machine_index == NONE && floodfection_originals_sent_valid &&
		!csmemcmp(floodfection_originals_sent, floodfection_originals, sizeof(floodfection_originals)))
	{
		return;
	}
	csmemset(&message, 0, sizeof(message));
	message.game_number = infection.state.game_number;
	csmemcpy(message.originals, floodfection_originals, sizeof(message.originals));
	if (machine_index != NONE)
	{
		distributed_send_to_machine_reliably(machine_index, &message, _distributed_message_floodfection_originals, 1,
			sizeof(message));
		return;
	}
	distributed_send(&message, _distributed_message_floodfection_originals, 1, sizeof(message),
		_distributed_to_clients_reliably);
	csmemcpy(floodfection_originals_sent, floodfection_originals, sizeof(floodfection_originals_sent));
	floodfection_originals_sent_valid = TRUE;
}

int infection_handles_message(
	unsigned short const *message,
	unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return FALSE;
	csmemcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_floodfection_originals;
}

/* a client: the host's, of this game, taken whole */
void infection_handle_message(
	long machine_index,
	unsigned short const *data,
	unsigned short size)
{
	struct floodfection_originals_message message;
	struct data_iterator iterator;

	if (machine_index != NONE || !network_game_distributed_client() || size != sizeof(message))
		return;
	csmemcpy(&message, data, sizeof(message));
	if (GET_MESSAGE_TYPE(message.header.header) != 2 ||
		message.header.type != _distributed_message_floodfection_originals || message.header.count != 1 ||
		message.game_number != network_game_get_number_of_games_played())
	{
		return;
	}
	csmemcpy(floodfection_originals, message.originals, sizeof(floodfection_originals));
	if (!infection_enabled() || !player_data)
		return;
	data_iterator_new(&iterator, player_data);
	while (data_iterator_next(&iterator))
		infection_reconcile_player(iterator.datum_index);
}

/* ---- Floodfection's voice */

/* the Flood combat forms' cries (Two Betrayals', copied into the level with
the burst: infection_tag_import), each machine's own, from a zombie's unit
(its Spartan's own voice silenced: unit_dialogue.c's unit_speak): hurt (its
shields' and body's share fallen, at most so often), dying, killing a
human, leaping and striking */
enum
{
	_floodfection_sound_pain = 0,
	_floodfection_sound_death,
	_floodfection_sound_kill,
	_floodfection_sound_leap,
	_floodfection_sound_melee,
	NUMBER_OF_FLOODFECTION_SOUNDS
};

#define FLOODFECTION_PAIN_TICKS (TICKS_PER_SECOND * 3 / 4)
/* a jump's speed up from the ground (world units a tick), less a step off a
ledge */
#define FLOODFECTION_LEAP_SPEED 0.02f

static char const *const floodfection_sound_tags[NUMBER_OF_FLOODFECTION_SOUNDS] =
{
	"sound\\dialog\\flood\\pain",
	"sound\\dialog\\flood\\death",
	"sound\\dialog\\flood\\flood_idle_combat",
	"sound\\dialog\\flood\\leap",
	"sound\\dialog\\flood\\melee",
};
static long floodfection_sound_definitions[NUMBER_OF_FLOODFECTION_SOUNDS];
/* by absolute player index: the zombie's unit last heard, its shields' and
body's share then, when it last cried out in pain, and whether it was in
the air and striking */
static long floodfection_voice_unit[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static real floodfection_voice_vitality[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static long floodfection_voice_time[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static boolean floodfection_voice_airborne[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static boolean floodfection_voice_melee[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static short floodfection_voice_melee_ticks[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
/* the Flood's music (Two Betrayals', each machine's own, but with NO MUSIC):
through the round, and from the last stand (the last humans strengthened,
or the last one) on; which plays (NONE: none), the humans last counted, and
when the last one's horde began (floodfection_round_update) */
#define FLOODFECTION_MUSIC_ROUND "levels\\c40\\music\\c40_08"
#define FLOODFECTION_MUSIC_LAST_STAND "levels\\c40\\music\\c40_11"
static long floodfection_music_definitions[2];
static long floodfection_music_playing = NONE;
static long floodfection_humans_seen = NONE;
static long floodfection_horde_start = NONE;
/* the HUD's nav point the last human is shown with to the zombies (Slayer's
target's), looked for once a map */
static short floodfection_nav_index;
static boolean floodfection_nav_index_found;

static void floodfection_sound(
	short sound,
	long unit_index)
{
	long definition_index = floodfection_sound_definitions[sound];

	if (definition_index != NONE && unit_index != NONE && object_try_and_get(unit_index))
		object_impulse_sound_new(unit_index, definition_index, NONE, global_origin3d, global_forward3d, 1.0f);
}

/* each tick, each player: a living zombie's unit hurt since the last, off
the ground (a jump: going up), or begun a strike */
static void floodfection_voice(
	long player_index,
	boolean zombie)
{
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	struct player_datum *player = player_get(player_index);
	struct unit_datum *unit = player->unit_index != NONE && !player->quit_out_of_game ?
		unit_try_and_get(player->unit_index) : NULL;
	struct biped_datum *biped;
	long now = game_time_get();
	real vitality;
	boolean airborne;
	boolean melee;
	short melee_ticks;
	boolean same;

	if (!VALID_INDEX(slot, HALO_PORT_MAXIMUM_NETWORK_PLAYERS))
		return;
	if (!zombie || !unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
	{
		floodfection_voice_unit[slot] = NONE;
		return;
	}
	same = floodfection_voice_unit[slot] == player->unit_index;
	vitality = unit->object.body_vitality + unit->object.shield_vitality;
	if (same && vitality < floodfection_voice_vitality[slot] - 0.005f &&
		(floodfection_voice_time[slot] == NONE || now < floodfection_voice_time[slot] ||
			now - floodfection_voice_time[slot] >= FLOODFECTION_PAIN_TICKS))
	{
		floodfection_voice_time[slot] = now;
		floodfection_sound(_floodfection_sound_pain, player->unit_index);
	}
	biped = (struct biped_datum *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_biped);
	airborne = biped && TEST_FLAG(biped->biped.flags, _biped_airborne_bit);
	/* (a player's strike is its biped's melee ticks, not an animation
	state: bipeds.c; a new one's are more than the last tick's) */
	melee_ticks = biped ? biped->biped.player_melee_ticks : 0;
	melee = unit->unit.animation.state == _unit_state_melee_attack ||
		unit->unit.animation.state == _unit_state_melee_airborne;
	if (same && ((melee && !floodfection_voice_melee[slot]) || melee_ticks > floodfection_voice_melee_ticks[slot]))
		floodfection_sound(_floodfection_sound_melee, player->unit_index);
	else if (same && airborne && !floodfection_voice_airborne[slot] &&
		unit->object.translational_velocity.k > FLOODFECTION_LEAP_SPEED && unit->object.parent_object_index == NONE)
	{
		floodfection_sound(_floodfection_sound_leap, player->unit_index);
	}
	floodfection_voice_unit[slot] = player->unit_index;
	floodfection_voice_vitality[slot] = vitality;
	floodfection_voice_airborne[slot] = airborne;
	floodfection_voice_melee[slot] = melee;
	floodfection_voice_melee_ticks[slot] = melee_ticks;
}

/* how high above a dead zombie's feet its bits are thrown from (world units) */
#define FLOODFECTION_BITS_HEIGHT 0.5f

/* the Flood's burst (Two Betrayals' carrier's, copied into the level from
watanoge_gamemodes.pack: infection_tag_import) where a player turned or a zombie died, and
where a zombie died the Flood's bits too (its limbs, head, skin and
innards thrown). Each machine plays its own, as it sees the turn (the
host's state) or the death (the unit's): no message, and nothing of the
game's but particles, decals, the sound and the bits, garbage, which each
machine keeps for itself (network_objects.c sends none): no damage, and no
objects a client would not have */
static long floodfection_burst_definition = NONE;
static char const *const floodfection_bits_tags[] =
{
	FLOODFECTION_BITS_TAGS
};
static long floodfection_bits_definitions[NUMBEROF(floodfection_bits_tags)];
/* by absolute player index: the role last seen, and the unit last burst */
static short floodfection_seen_role[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static long floodfection_burst_unit[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];

/* Floodfection's tags of Two Betrayals, copied into a multiplayer level as
it loads (randomizer_tags_loaded: tag_import.c), from its pack (its
manifest: port/assets/tag_packs/watanoge_gamemodes.toml). The game's mode,
as every machine of the game has it, says whether, so each copies alike */
boolean infection_tag_import_wanted(
	void)
{
	struct network_game *game = network_game_get_game();

	return game && game_modes_variant_mode_variant(&game->variant, &game->variant_options) ==
		_game_mode_variant_floodfection;
}

void infection_tag_import(
	void)
{
	static struct tag_import_root const roots[] =
	{
		{ 'effe', SKULLS_OUTBREAK_BURST_TAG },
		/* (its bits where a zombie dies) */
		{ 'effe', "effects\\flood bit limb explode" },
		{ 'effe', "effects\\flood bit h-head explode" },
		{ 'effe', "effects\\flood bit skin explode" },
		/* (its zombies' forms: floodfection_forms.c) */
		{ 'mode', "characters\\floodcombat_human\\floodcombat_human" },
		{ 'antr', "characters\\floodcombat_human\\floodcombat_human" },
		{ 'mode', "characters\\floodcombat elite\\floodcombat elite" },
		{ 'antr', "characters\\floodcombat elite\\floodcombat elite" },
		/* (their cries: floodfection_sound_tags) */
		{ 'snd!', "sound\\dialog\\flood\\pain" },
		{ 'snd!', "sound\\dialog\\flood\\death" },
		{ 'snd!', "sound\\dialog\\flood\\flood_idle_combat" },
		{ 'snd!', "sound\\dialog\\flood\\leap" },
		{ 'snd!', "sound\\dialog\\flood\\melee" },
		/* (the level's music of the Flood: floodfection_music_tags) */
		{ 'lsnd', FLOODFECTION_MUSIC_ROUND },
		{ 'lsnd', FLOODFECTION_MUSIC_LAST_STAND },
	};

	tag_import_roots(GAME_MODES_PACK, SKULLS_FLOOD_DONOR, roots, (int)NUMBEROF(roots));
}

static void floodfection_burst_reset(
	void)
{
	long slot;

	floodfection_burst_definition = tag_loaded(EFFECT_DEFINITION_TAG, SKULLS_OUTBREAK_BURST_TAG);
	for (slot = 0; slot < (long)NUMBEROF(floodfection_bits_tags); slot++)
		floodfection_bits_definitions[slot] = tag_loaded(EFFECT_DEFINITION_TAG, floodfection_bits_tags[slot]);
	for (slot = 0; slot < NUMBER_OF_FLOODFECTION_SOUNDS; slot++)
		floodfection_sound_definitions[slot] = tag_loaded(SOUND_DEFINITION_TAG, floodfection_sound_tags[slot]);
	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		floodfection_seen_role[slot] = _distributed_infection_role_none;
		floodfection_burst_unit[slot] = NONE;
		floodfection_voice_unit[slot] = NONE;
		floodfection_voice_time[slot] = NONE;
	}
	csmemset(floodfection_originals, 0, sizeof(floodfection_originals));
	csmemset(floodfection_first, 0, sizeof(floodfection_first));
	csmemset(floodfection_elite_unit, 0xFF, sizeof(floodfection_elite_unit));
	floodfection_originals_sent_valid = FALSE;
	floodfection_music_definitions[0] = tag_loaded(LOOPING_SOUND_DEFINITION_TAG, FLOODFECTION_MUSIC_ROUND);
	floodfection_music_definitions[1] = tag_loaded(LOOPING_SOUND_DEFINITION_TAG, FLOODFECTION_MUSIC_LAST_STAND);
	floodfection_music_playing = NONE;
	floodfection_humans_seen = NONE;
	floodfection_horde_start = NONE;
	floodfection_nav_index_found = FALSE;
	infection_fog_reset();
	floodfection_forms_reset();
}

static void floodfection_effect_at(
	long definition_index,
	real_point3d const *point)
{
	char const *marker_names[1];
	real_point3d marker_points[1];
	real_vector3d marker_forwards[1];

	if (definition_index == NONE)
		return;
	marker_names[0] = "";
	marker_points[0] = *point;
	marker_forwards[0] = *global_up3d;
	skulls_effect_harmless = TRUE;
	skulls_effect_no_objects = TRUE;
	effect_new_unattached_from_markers(definition_index, NONE, NULL, 1, marker_names, marker_points,
		marker_forwards, 0.0f, 0.0f, NULL, NULL, TRUE);
	skulls_effect_harmless = FALSE;
	skulls_effect_no_objects = FALSE;
}

/* the burst at the unit, and a dead zombie's bits (from its middle, about
where its form's chest was) */
static void floodfection_burst_at(
	long unit_index,
	boolean bits)
{
	real_point3d point;
	short index;

	object_get_origin(unit_index, &point);
	floodfection_effect_at(floodfection_burst_definition, &point);
	if (!bits)
		return;
	point.z += FLOODFECTION_BITS_HEIGHT;
	for (index = 0; index < (short)NUMBEROF(floodfection_bits_definitions); index++)
		floodfection_effect_at(floodfection_bits_definitions[index], &point);
}

/* a human turned (killed or chosen: the body, or the living unit), or a
zombie's unit dead: a burst there, once for each unit */
static void floodfection_bursts(
	long player_index)
{
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	struct distributed_infection_player *role = infection_player(player_index);
	struct player_datum *player = player_get(player_index);
	long unit_index = player->unit_index != NONE ? player->unit_index : player->dead_unit_index;
	struct unit_datum *unit = unit_index != NONE ? unit_try_and_get(unit_index) : NULL;
	short seen;

	if (!infection_floodfection() || !VALID_INDEX(slot, HALO_PORT_MAXIMUM_NETWORK_PLAYERS))
		return;
	seen = floodfection_seen_role[slot];
	floodfection_seen_role[slot] = role ? role->role : _distributed_infection_role_none;
	if (floodfection_burst_definition == NONE || !role || role->role != _distributed_infection_role_zombie ||
		!unit || player->quit_out_of_game || floodfection_burst_unit[slot] == unit_index)
	{
		return;
	}
	if (seen == _distributed_infection_role_human || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
	{
		floodfection_burst_unit[slot] = unit_index;
		floodfection_burst_at(unit_index, seen != _distributed_infection_role_human);
		if (seen != _distributed_infection_role_human)
			floodfection_sound(_floodfection_sound_death, unit_index);
	}
}

/* ---- the host's round */

static void infection_finish(
	long winner)
{
	if (infection.state.phase == _distributed_infection_finished)
		return;
	infection.state.winner = winner;
	infection.state.phase = _distributed_infection_finished;
	infection.state.revision++;
	platform_log("infection: finished, winner %ld", winner);
	game_engine_end_game();
}

/* a unit's armour in its team's colour (a human turned while alive), as a
spawn colours it; the colour channels a function drives (the shield's glow)
are left to it */
static void infection_color_unit(
	struct unit_datum *unit,
	long player_index)
{
	struct object_definition *definition = object_definition_get(unit->definition_index);
	real_rgb_color color;
	short channel;

	game_engine_player_get_change_color(&color, player_index);
	for (channel = 0; channel < NUMBER_OF_OBJECT_CHANGE_COLORS; channel++)
	{
		unit->object.base_change_colors[channel] = color;
		if (channel < definition->object.change_colors.count)
		{
			struct object_change_color_definition *change_color = TAG_BLOCK_GET_ELEMENT(
				&definition->object.change_colors, channel, struct object_change_color_definition);

			if (change_color->scaled_by || change_color->darken_by)
				continue;
		}
		unit->object.outgoing_change_colors[channel] = color;
	}
}

/* a zombie without shields (the gametype's infection_zombie_shields): a
shield capacity of 0, the game's own unshielded unit, which neither takes
shield damage nor recharges; a human again (or the option on) gets the
unit's capacity back, without healing its body */
static void infection_apply_shields(
	long slot,
	long unit_index,
	struct unit_datum *unit,
	boolean disabled)
{
	if (infection.shield_unit[slot] != unit_index)
		infection.shield_unit[slot] = NONE;
	if (disabled)
	{
		if (infection.shield_unit[slot] == NONE)
		{
			infection.shield_unit[slot] = unit_index;
			infection.shield_maximum[slot] = unit->object.maximum_shield_vitality;
		}
		unit->object.maximum_shield_vitality = 0.0f;
		unit->object.shield_vitality = 0.0f;
		SET_FLAG(unit->object.damage_flags, _object_shield_charging_bit, FALSE);
		SET_FLAG(unit->object.damage_flags, _object_shield_over_charging_bit, FALSE);
		SET_FLAG(unit->object.damage_flags, _object_shield_depleted_bit, TRUE);
	}
	else if (infection.shield_unit[slot] == unit_index)
	{
		unit->object.maximum_shield_vitality = infection.shield_maximum[slot];
		infection.shield_unit[slot] = NONE;
	}
}

/* a Floodfection zombie's body: its form's share of its unit's (the
gametype's: half as much again for a human form, twice for an elite; the
maximum raised, its share of it kept, so turning heals no one); a human
again gets the unit's back. (The scale set again each time: a client may
be told a zombie is an elite after its turn) */
static void infection_apply_health(
	long slot,
	long unit_index,
	struct unit_datum *unit,
	real scale)
{
	if (infection.health_unit[slot] != unit_index)
		infection.health_unit[slot] = NONE;
	if (scale != 1.0f)
	{
		if (infection.health_unit[slot] == NONE && unit->object.maximum_body_vitality > 0.0f)
		{
			infection.health_unit[slot] = unit_index;
			infection.health_maximum[slot] = unit->object.maximum_body_vitality;
		}
		if (infection.health_unit[slot] == unit_index)
			unit->object.maximum_body_vitality = infection.health_maximum[slot] * scale;
	}
	else if (infection.health_unit[slot] == unit_index)
	{
		unit->object.maximum_body_vitality = infection.health_maximum[slot];
		infection.health_unit[slot] = NONE;
	}
}

void infection_reconcile_player(
	long player_index)
{
	struct game_variant_options const *options = game_variant_options_get();
	struct distributed_infection_player *role;
	struct player_datum *player;
	struct unit_datum *unit;
	long team;
	boolean floodfection = infection_floodfection();
	boolean original;

	if (!infection_enabled() || (role = infection_player(player_index)) == NULL ||
		role->role == _distributed_infection_role_none)
	{
		return;
	}
	player = player_get(player_index);
	team = role->role == _distributed_infection_role_zombie ? 1 : 0;
	original = infection_original(player_index);
	player->team_index = team;
	player->network_player_data.team_index = (char)team;
	player->speed_multiplier = !team ? 1.0f : !floodfection ? options->infection_zombie_speed / 100.0f :
		(original ? FLOODFECTION_SETTING_VALUE(_floodfection_setting_elite_speed, floodfection_elite_speeds) :
			FLOODFECTION_SETTING_VALUE(_floodfection_setting_human_speed, floodfection_human_speeds)) / 100.0f;
	if (player->unit_index != NONE && (unit = unit_try_and_get(player->unit_index)) != NULL)
	{
		/* (Floodfection's zombies' by their form, whatever ZOMBIE SHIELDS
		says) */
		infection_apply_shields(DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), player->unit_index, unit,
			team && (!floodfection ? !options->infection_zombie_shields : original ?
				floodfection_setting(_floodfection_setting_elite_no_shields) != 0 :
				!floodfection_setting(_floodfection_setting_human_shields)));
		infection_apply_health(DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), player->unit_index, unit,
			!team || !floodfection ? 1.0f : (original ?
				FLOODFECTION_SETTING_VALUE(_floodfection_setting_elite_health, floodfection_elite_healths) :
				FLOODFECTION_SETTING_VALUE(_floodfection_setting_human_health, floodfection_human_healths)) / 100.0f);
		unit->object.owner_team_index = (short)team;
		infection_color_unit(unit, player_index);
		/* (a living zombie's grenades and powerups gone; a dead one's are its
		body's, which drops them) */
		if (team && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		{
			/* (a grenade being wound up when its thrower turned is not thrown:
			the throw would find no grenade left) */
			if (unit->unit.grenade_throw_state == _unit_grenade_throw_wind_up)
				unit->unit.grenade_throw_state = _unit_grenade_throw_ending;
			csmemset(unit->unit.grenade_counts, 0, sizeof(unit->unit.grenade_counts));
			csmemset(player->powerup_durations, 0, sizeof(player->powerup_durations));
		}
	}
	/* (and the host's game record, which the lobby and late joins read) */
	if (!network_game_distributed_client())
	{
		struct network_game_server *server = global_network_game_server_get();
		struct network_game *game = server ? network_game_server_get_game(server) : NULL;
		long list_index = player->network_player_data.player_list_index;

		if (game && VALID_INDEX(list_index, HALO_PORT_MAXIMUM_NETWORK_PLAYERS))
			game->players[list_index].team_index = (char)team;
	}
}

static void infection_delete_weapon(
	long weapon_index)
{
	long slot;

	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		if (infection.skull[slot] == weapon_index)
			infection.skull[slot] = NONE;
	}
	if (weapon_index != NONE && object_try_and_get(weapon_index))
		object_delete(weapon_index);
}

static void infection_delete_skull(
	long slot)
{
	infection_delete_weapon(infection.skull[slot]);
	infection.skull[slot] = NONE;
}

/* a unit's weapons gone but the one kept (skulls only: a human's skull
picked up), each deleted once */
static void infection_strip_weapons(
	long unit_index,
	long keep,
	boolean skulls_only)
{
	struct unit_datum *unit = unit_get(unit_index);
	long removed[MAXIMUM_WEAPONS_PER_UNIT];
	long count = 0;
	long slot;
	long previous;
	long kept = NONE;

	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long weapon_index = unit->unit.weapon_object_indices[slot];

		if (weapon_index == NONE)
			continue;
		if (weapon_index == keep && kept == NONE)
		{
			kept = slot;
			continue;
		}
		if (skulls_only && !infection_weapon_is_skull(weapon_index))
			continue;
		unit->unit.weapon_object_indices[slot] = NONE;
		if (unit->unit.current_weapon_index == slot)
			unit->unit.current_weapon_index = NONE;
		if (unit->unit.desired_weapon_index == slot)
			unit->unit.desired_weapon_index = NONE;
		if (weapon_index == keep)
			continue;
		for (previous = 0; previous < count && removed[previous] != weapon_index; previous++)
		{
		}
		if (previous == count)
			removed[count++] = weapon_index;
	}
	for (slot = 0; slot < count; slot++)
		infection_delete_weapon(removed[slot]);
	if (kept != NONE)
		unit->unit.desired_weapon_index = (short)kept;
	else if (unit->unit.desired_weapon_index == NONE)
		unit->unit.desired_weapon_index = unit_inventory_next_weapon(unit_index, NONE, 0);
}

/* the host gives a living player their role's equipment: a zombie its skull
alone (out of any vehicle, without grenades or powerups), a human what they
spawned with, without a skull */
static void infection_equip(
	long player_index)
{
	struct player_datum *player = player_get(player_index);
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	struct distributed_infection_player *role = infection_player(player_index);
	struct unit_datum *unit;
	long weapon_index;
	short inventory_slot;

	if (network_game_distributed_client() || !role || player->quit_out_of_game || player->unit_index == NONE ||
		infection.missing_skull)
	{
		return;
	}
	unit = unit_get(player->unit_index);
	if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		return;
	/* (a Floodfection zombie's new unit an elite or not, its form's speed,
	health and shields at once) */
	if (role->role == _distributed_infection_role_zombie && infection_floodfection() &&
		VALID_INDEX(slot, HALO_PORT_MAXIMUM_NETWORK_PLAYERS) && floodfection_elite_unit[slot] != player->unit_index)
	{
		floodfection_choose_elite(slot, player->unit_index);
		infection_reconcile_player(player_index);
	}
	if (infection.equipped_unit[slot] == player->unit_index)
	{
		if (role->role == _distributed_infection_role_human)
		{
			infection_strip_weapons(player->unit_index, NONE, TRUE);
			return;
		}
		/* (a zombie still holding its skull keeps it; any other weapon goes) */
		for (inventory_slot = 0; inventory_slot < MAXIMUM_WEAPONS_PER_UNIT; inventory_slot++)
		{
			weapon_index = unit->unit.weapon_object_indices[inventory_slot];
			if (infection_weapon_is_skull(weapon_index))
			{
				if (infection.skull[slot] != weapon_index)
					infection_delete_skull(slot);
				infection.skull[slot] = weapon_index;
				infection_strip_weapons(player->unit_index, weapon_index, FALSE);
				return;
			}
		}
	}
	infection.equipping = TRUE;
	infection_strip_weapons(player->unit_index, NONE, role->role == _distributed_infection_role_human);
	infection_delete_skull(slot);
	if (role->role == _distributed_infection_role_zombie)
	{
		if (unit->object.parent_object_index != NONE)
			unit_exit_seat_end(player->unit_index);
		csmemset(unit->unit.grenade_counts, 0, sizeof(unit->unit.grenade_counts));
		if (unit->unit.equipment_object_index != NONE)
		{
			object_delete(unit->unit.equipment_object_index);
			unit->unit.equipment_object_index = NONE;
		}
		weapon_index = NONE;
		if (get_ball_definition_index() != NONE)
		{
			struct object_placement_data placement;

			object_placement_data_new(&placement, get_ball_definition_index(), NONE);
			placement.owner_team_index = 1;
			weapon_index = object_new(&placement);
		}
		if (weapon_index == NONE || !unit_add_weapon_to_inventory(player->unit_index, weapon_index, 2))
		{
			if (weapon_index != NONE)
				object_delete(weapon_index);
			infection.missing_skull = TRUE;
		}
		else
		{
			infection.skull[slot] = weapon_index;
		}
	}
	infection.equipping = FALSE;
	infection.equipped_unit[slot] = player->unit_index;
	infection.killed_unit[slot] = NONE;
}

/* a player new to the round: a human before the zombies are chosen, a
zombie after */
static void infection_player_added(
	long player_index)
{
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	struct distributed_infection_player *role;

	if (infection.slayer.player_added)
		infection.slayer.player_added(player_index);
	if (!VALID_INDEX(slot, HALO_PORT_MAXIMUM_NETWORK_PLAYERS))
		return;
	role = &infection.state.players[slot];
	if (!network_game_distributed_client())
	{
		infection_delete_skull(slot);
		csmemset(role, 0, sizeof(*role));
		floodfection_forget_player(slot);
		infection.last_stand_unit[slot] = NONE;
		role->identity = infection_player_identity(player_index);
		role->role = infection.state.phase >= _distributed_infection_active ? _distributed_infection_role_zombie :
			_distributed_infection_role_human;
		infection.participant[slot] = player_index;
		infection.equipped_unit[slot] = NONE;
		infection.killed_unit[slot] = NONE;
		infection.state.revision++;
	}
	infection_reconcile_player(player_index);
}

void infection_postspawn(
	long player_index)
{
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);

	if (!VALID_INDEX(slot, HALO_PORT_MAXIMUM_NETWORK_PLAYERS))
		return;
	if (!network_game_distributed_client() && infection.participant[slot] != player_index)
		infection_player_added(player_index);
	infection_reconcile_player(player_index);
	infection.equipped_unit[slot] = NONE;
	infection_equip(player_index);
}

static void infection_player_killed_player(
	long killing_player_index,
	long killing_object_index,
	long dead_player_index,
	boolean friendly_fire)
{
	struct distributed_infection_player *dead = infection_player(dead_player_index);
	struct distributed_infection_player *killer = infection_player(killing_player_index);
	struct player_datum *player = player_get(dead_player_index);
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(dead_player_index);
	long unit_index = player->unit_index != NONE ? player->unit_index : player->dead_unit_index;

	(void)killing_object_index;
	/* (every machine: a zombie's cry, killing a human) */
	if (infection_floodfection() && killer && killer != dead && killer->role == _distributed_infection_role_zombie &&
		!friendly_fire)
	{
		floodfection_sound(_floodfection_sound_kill, player_get(killing_player_index)->unit_index);
	}
	if (network_game_distributed_client() || infection.state.phase != _distributed_infection_active || !dead ||
		player->quit_out_of_game || infection.killed_unit[slot] != NONE)
	{
		return;
	}
	infection.killed_unit[slot] = unit_index == NONE ? 0 : unit_index;
	if (killer && killer != dead && killer->role != dead->role && killer->score < INFECTION_MAXIMUM_SCORE)
		killer->score++;
	/* (a human killed by a zombie, themselves or the world turns; one killed
	by a human teammate does not) */
	if (dead->role == _distributed_infection_role_human &&
		!(friendly_fire && killer && killer != dead && killer->role == dead->role))
	{
		dead->role = _distributed_infection_role_zombie;
		platform_log("infection: player %ld infected", slot);
	}
	infection_reconcile_player(dead_player_index);
	/* (the body's skull is deleted on the player's next tick, once the
	unit's death has dropped what it held) */
	infection.equipped_unit[slot] = NONE;
	infection.state.revision++;
}

/* the round's result: 0 the humans won, 1 the zombies, 2 no one (everyone
left), NONE still on. Turning the last human wins over the time running
out on the same tick. */
static long infection_round_result(
	long humans,
	long zombies,
	long elapsed,
	long limit)
{
	if (!humans && !zombies)
		return 2;
	if (!humans)
		return 1;
	if (!zombies || (limit > 0 && elapsed >= limit))
		return 0;
	return NONE;
}

/* how many players the round starts as zombies, of the players in the game
and the living drawn from: the gametype's count, or in Floodfection a tenth
of the players if more (rounded up: 11 players, 2), leaving a human */
#define FLOODFECTION_INITIAL_ZOMBIES_PERCENT 10

static long infection_initial_zombies(
	long players,
	long living)
{
	long zombies = game_variant_options_get()->infection_initial_zombies;

	if (infection_floodfection())
		zombies = MAX(zombies, (players * FLOODFECTION_INITIAL_ZOMBIES_PERCENT + 99) / 100);
	return MAX(0, MIN(living - 1, zombies));
}

static long infection_time_limit_ticks(
	void)
{
	return game_variant_options_get()->time_limit * 60L * TICKS_PER_SECOND;
}

/* ---- Floodfection's last stand */

/* the humans strengthened once this few are left (the gametype's LAST
STAND of the players, rounded up: 10% of 11 players, 2), each living unit of
theirs once: whole, overshielded, at least half of each weapon's most
ammunition and a grenade more; and the last human again (but with LAST
SPARTAN BUFF off): overshielded, each weapon full and two grenades more.
The host's, which its clients see in the units' shields, health and
inventories it sends */

/* the humans left that the last stand begins at, of the players; 0 none */
static long floodfection_last_stand_threshold(
	long players)
{
	long percent = FLOODFECTION_SETTING_VALUE(_floodfection_setting_last_stand, floodfection_last_stand_percents);

	return percent ? MAX(1, (players * percent + 99) / 100) : 0;
}

/* a share of each weapon's most ammunition at least (all: loaded too), a
battery's charge as much, and grenades more of the first kind, as many as a
unit carries */
static void floodfection_arm(
	struct unit_datum *unit,
	real share,
	short grenades)
{
	struct game_globals *globals = scenario_get_game_globals();
	short slot;

	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long weapon_index = unit->unit.weapon_object_indices[slot];
		struct weapon_datum *weapon = weapon_index != NONE ? weapon_try_and_get(weapon_index) : NULL;
		struct weapon_definition *definition;
		short magazine;

		if (!weapon || infection_weapon_is_skull(weapon_index))
			continue;
		definition = weapon_definition_get(weapon->definition_index);
		if (!definition->weapon.magazines.count)
		{
			weapon->weapon.age = MIN(weapon->weapon.age, 1.0f - share);
			continue;
		}
		for (magazine = 0; magazine < 2 && magazine < definition->weapon.magazines.count; magazine++)
		{
			struct weapon_magazine *state = &weapon->weapon.magazines[magazine];
			struct weapon_magazine_definition *magazine_definition = TAG_BLOCK_GET_ELEMENT(
				&definition->weapon.magazines, magazine, struct weapon_magazine_definition);
			short rounds = (short)(magazine_definition->rounds_total_maximum * share + 0.5f);

			state->rounds_total = MAX(state->rounds_total, rounds);
			if (share >= 1.0f)
				state->rounds_loaded = MAX(state->rounds_loaded, magazine_definition->rounds_loaded_maximum);
		}
	}
	if (globals && globals->grenades.count > 0)
	{
		short maximum = TAG_BLOCK_GET_ELEMENT(&globals->grenades, 0, struct game_globals_grenade)->maximum_count;

		if (unit->unit.grenade_counts[0] < maximum)
			unit->unit.grenade_counts[0] = (char)MIN(maximum, unit->unit.grenade_counts[0] + grenades);
	}
}

/* the shields charged past full again, as an overshield's pickup does (an
overshield worn down charges back up) */
static void floodfection_overshield(
	struct unit_datum *unit)
{
	if (unit->object.maximum_shield_vitality <= 0.0f)
		return;
	SET_FLAG(unit->object.damage_flags, _object_shield_over_charging_bit, TRUE);
	SET_FLAG(unit->object.damage_flags, _object_shield_depleted_bit, FALSE);
	if (unit->object.shield_vitality <= 0.0f)
		unit->object.shield_vitality = 0.01f;
	unit->object.shield_stun_ticks = 0;
}

static void floodfection_last_stand(
	long humans,
	long players)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long threshold;

	if (!infection_floodfection())
		return;
	threshold = floodfection_last_stand_threshold(players);
	if (humans > threshold && humans != 1)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		struct distributed_infection_player *role = infection_player(iterator.datum_index);
		struct unit_datum *unit;

		if (player->quit_out_of_game || !role || role->role != _distributed_infection_role_human ||
			player->unit_index == NONE || (unit = unit_try_and_get(player->unit_index)) == NULL ||
			TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		{
			continue;
		}
		if (humans <= threshold && infection.last_stand_unit[slot] != player->unit_index)
		{
			infection.last_stand_unit[slot] = player->unit_index;
			unit->object.body_vitality = 1.0f;
			floodfection_overshield(unit);
			floodfection_arm(unit, 0.5f, 1);
			platform_log("infection: player %ld among the last %ld humans", slot, threshold);
		}
		if (humans == 1 && infection.last_human_unit != player->unit_index &&
			!floodfection_setting(_floodfection_setting_no_last_spartan_buff))
		{
			infection.last_human_unit = player->unit_index;
			floodfection_overshield(unit);
			floodfection_arm(unit, 1.0f, 2);
			platform_log("infection: player %ld the last human", slot);
		}
	}
}

/* the one human left of a Floodfection round, alive, as every machine has
the host's state; else NONE */
static long floodfection_last_human_unit(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long unit_index = NONE;
	long humans = 0;

	if (!infection_floodfection() || infection.state.phase != _distributed_infection_active)
		return NONE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct distributed_infection_player *role = infection_player(iterator.datum_index);

		if (player->quit_out_of_game || !role || role->role != _distributed_infection_role_human)
			continue;
		if (++humans > 1)
			return NONE;
		unit_index = player->unit_index;
	}
	if (unit_index == NONE || !unit_try_and_get(unit_index) ||
		TEST_FLAG(unit_get(unit_index)->object.damage_flags, _object_dead_bit))
	{
		return NONE;
	}
	return unit_index;
}

/* the horde the last human hears (every machine: the Flood's cries all
about, none from anywhere), its cries' ticks from its start */
static struct
{
	short tick;
	short sound;
} const floodfection_horde[] =
{
	{ 0, _floodfection_sound_kill }, { 4, _floodfection_sound_leap }, { 9, _floodfection_sound_pain },
	{ 13, _floodfection_sound_melee }, { 18, _floodfection_sound_kill }, { 24, _floodfection_sound_death },
	{ 29, _floodfection_sound_leap }, { 36, _floodfection_sound_kill }, { 42, _floodfection_sound_melee },
	{ 50, _floodfection_sound_pain }, { 57, _floodfection_sound_kill }, { 66, _floodfection_sound_leap },
	{ 75, _floodfection_sound_kill }, { 88, _floodfection_sound_death },
};

static void floodfection_print(
	wchar_t const *text,
	long only_player_index)
{
	short local_player_index;

	for (local_player_index = 0; local_player_index < local_player_count(); local_player_index++)
	{
		long player_index = local_player_get_player_index(local_player_index);

		if (player_index != NONE && (only_player_index == NONE || player_index == only_player_index))
			hud_print_message(local_player_index, text);
	}
}

/* each tick, every machine (the host's state): the humans left told when
the last stand's few or fewer are left, and the last one ("Last Spartan
standing"), whom the horde greets; and the Flood's music, the last
stand's once it began */
static void floodfection_round_update(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long humans = 0;
	long players = 0;
	long last_player_index = NONE;
	long threshold;
	long music = NONE;
	long now = game_time_get();
	short index;

	if (!infection_floodfection())
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct distributed_infection_player *role = infection_player(iterator.datum_index);

		if (player->quit_out_of_game || !role || role->role == _distributed_infection_role_none)
			continue;
		players++;
		if (role->role == _distributed_infection_role_human)
		{
			humans++;
			last_player_index = iterator.datum_index;
		}
	}
	threshold = floodfection_last_stand_threshold(players);
	if (infection.state.phase != _distributed_infection_active)
		floodfection_humans_seen = NONE;
	else if (humans != floodfection_humans_seen && humans > 0)
	{
		if (humans == 1)
		{
			floodfection_print(L"Last Spartan standing", NONE);
			floodfection_print(L"You are the last Spartan: the Flood is coming", last_player_index);
			floodfection_horde_start = now;
		}
		else if (humans <= threshold && (floodfection_humans_seen == NONE || humans < floodfection_humans_seen))
		{
			wchar_t text[64];

			usnprintf(text, NUMBEROF(text) - 1, L"Only %ld Spartans remain", humans);
			text[NUMBEROF(text) - 1] = 0;
			floodfection_print(text, NONE);
		}
		floodfection_humans_seen = humans;
	}
	/* (the horde's cries due this tick) */
	if (floodfection_horde_start != NONE)
	{
		long elapsed = now - floodfection_horde_start;

		for (index = 0; index < (short)NUMBEROF(floodfection_horde); index++)
		{
			long definition_index = floodfection_sound_definitions[floodfection_horde[index].sound];

			if (floodfection_horde[index].tick == elapsed && definition_index != NONE)
				unspatialized_impulse_sound_new(definition_index, 1.0f);
		}
		if (elapsed < 0 || elapsed > floodfection_horde[NUMBEROF(floodfection_horde) - 1].tick)
			floodfection_horde_start = NONE;
	}
	if (infection.state.phase == _distributed_infection_active && !floodfection_setting(_floodfection_setting_no_music))
		music = floodfection_music_definitions[humans == 1 || humans <= threshold ? 1 : 0];
	if (music != floodfection_music_playing)
	{
		if (floodfection_music_playing != NONE)
			scripted_looping_sound_stop(floodfection_music_playing);
		floodfection_music_playing = music;
		if (music != NONE)
			scripted_looping_sound_start(music, NONE, 1.0f);
	}
}

void infection_render_nav_points(
	short local_player_index)
{
	long player_index = local_player_index != NONE ? local_player_get_player_index(local_player_index) : NONE;
	struct distributed_infection_player *role = player_index != NONE ? infection_player(player_index) : NULL;
	long unit_index;
	long own_unit_index;
	real_point3d head;
	real_point3d position;

	if (!role || role->role != _distributed_infection_role_zombie ||
		floodfection_setting(_floodfection_setting_no_last_spartan_shown) ||
		(own_unit_index = player_get(player_index)->unit_index) == NONE ||
		(unit_index = floodfection_last_human_unit()) == NONE)
	{
		return;
	}
	if (!floodfection_nav_index_found)
	{
		floodfection_nav_index_found = TRUE;
		floodfection_nav_index = find_nav_point("target_blue");
	}
	if (floodfection_nav_index == NONE)
		return;
	/* (above the middle, as a goal's: game_engine_set_goal_position) */
	position = unit_get(unit_index)->object.bounding_sphere_center;
	position.z += 0.63f;
	unit_get_head_position(own_unit_index, &head);
	custom_render_nav_point(local_player_index, &position, floodfection_nav_index,
		hud_get_nav_point_render_type(local_player_index, &head, &position, NONE));
}

static void infection_update(
	void)
{
	struct game_variant_options const *options = game_variant_options_get();
	struct data_iterator iterator;
	struct player_datum *player;
	boolean present[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long candidates[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long humans = 0;
	long zombies = 0;
	long count = 0;
	long index;
	long initial_zombies;
	long now = game_time_get();

	if (infection.slayer.update)
		infection.slayer.update();
	floodfection_round_update();
	if (network_game_distributed_client() || infection.state.phase == _distributed_infection_finished)
		return;
	infection_send_originals(NONE);
	csmemset(present, 0, sizeof(present));
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		struct unit_datum *unit;

		if (player->quit_out_of_game || !VALID_INDEX(slot, HALO_PORT_MAXIMUM_NETWORK_PLAYERS))
			continue;
		present[slot] = TRUE;
		if (infection.participant[slot] != iterator.datum_index)
			infection_player_added(iterator.datum_index);
		infection_reconcile_player(iterator.datum_index);
		if (infection.state.players[slot].role == _distributed_infection_role_human)
			humans++;
		else
			zombies++;
		/* (the first zombies are chosen of the living: a full map can leave
		players waiting to spawn) */
		unit = player->unit_index != NONE ? unit_try_and_get(player->unit_index) : NULL;
		if (unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			candidates[count++] = iterator.datum_index;
	}
	for (index = 0; index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
	{
		if (!present[index] && infection.participant[index] != NONE)
		{
			infection_delete_skull(index);
			infection.participant[index] = NONE;
			infection.equipped_unit[index] = NONE;
			infection.killed_unit[index] = NONE;
			infection.last_stand_unit[index] = NONE;
			floodfection_forget_player(index);
			csmemset(&infection.state.players[index], 0, sizeof(infection.state.players[index]));
			infection.state.revision++;
		}
	}
	if (infection.missing_skull)
	{
		platform_log("infection: no skull to give (the map has no Oddball ball), the round ends");
		infection_finish(NONE);
		return;
	}
	if (infection.state.phase == _distributed_infection_active)
	{
		long result = infection_round_result(humans, zombies, now - infection.state.phase_tick,
			infection_time_limit_ticks());

		if (result != NONE)
		{
			platform_log("infection: result %ld, %ld humans, %ld zombies", result, humans, zombies);
			infection_finish(result == 2 ? NONE : result);
			return;
		}
		floodfection_last_stand(humans, humans + zombies);
		return;
	}
	if (count < 2)
	{
		if (infection.state.phase != _distributed_infection_waiting)
			infection.state.revision++;
		infection.state.phase = _distributed_infection_waiting;
		infection.state.phase_tick = now;
		return;
	}
	if (infection.state.phase == _distributed_infection_waiting)
	{
		infection.state.phase = _distributed_infection_countdown;
		infection.state.phase_tick = now;
		infection.state.revision++;
		platform_log("infection: countdown, %ld players", count);
	}
	if (now - infection.state.phase_tick < options->infection_countdown * TICKS_PER_SECOND)
		return;
	infection.state.phase = _distributed_infection_active;
	infection.state.phase_tick = now;
	initial_zombies = infection_initial_zombies(humans + zombies, count);
	/* the first zombies, drawn without repeats, leaving a human */
	for (index = 0; index < initial_zombies; index++)
	{
		long pick = index + local_random_range(0, (short)(count - index));
		long chosen = candidates[pick];
		long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(chosen);

		candidates[pick] = candidates[index];
		candidates[index] = chosen;
		infection.state.players[slot].role = _distributed_infection_role_zombie;
		infection.equipped_unit[slot] = NONE;
		floodfection_set_bit(floodfection_first, slot, infection_floodfection());
		infection_reconcile_player(chosen);
		infection_equip(chosen);
	}
	infection.state.revision++;
	platform_log("infection: active, %ld players, %ld zombies", count, initial_zombies);
}

long infection_remaining_ticks(
	void)
{
	long limit = infection_time_limit_ticks();

	return infection.state.phase == _distributed_infection_active ?
		MAX(0, limit - (game_time_get() - infection.state.phase_tick)) : limit;
}

static boolean infection_initialize_for_new_map(
	void)
{
	struct game_variant *variant = game_engine_get_variant();

	csmemset(&infection.state, 0, sizeof(infection.state));
	csmemset(infection.participant, 0xFF, sizeof(infection.participant));
	csmemset(infection.equipped_unit, 0xFF, sizeof(infection.equipped_unit));
	csmemset(infection.killed_unit, 0xFF, sizeof(infection.killed_unit));
	csmemset(infection.skull, 0xFF, sizeof(infection.skull));
	csmemset(infection.shield_unit, 0xFF, sizeof(infection.shield_unit));
	csmemset(infection.health_unit, 0xFF, sizeof(infection.health_unit));
	csmemset(infection.last_stand_unit, 0xFF, sizeof(infection.last_stand_unit));
	infection.last_human_unit = NONE;
	infection.state.winner = NONE;
	infection.state.game_number = network_game_get_game() ? network_game_get_number_of_games_played() : 0;
	infection.missing_skull = get_ball_definition_index() == NONE;
	infection.equipping = FALSE;
	floodfection_burst_reset();
	/* (everyone respawns; the variant's traits give zombies no infinite
	grenades nor camouflage) */
	variant->universal_variant.lives = 0;
	variant->universal_variant.odd_man_out = FALSE;
	SET_FLAG(variant->universal_variant.flags, _game_variant_infinite_grenades_bit, FALSE);
	SET_FLAG(variant->universal_variant.flags, _game_variant_always_invisible_bit, FALSE);
	return infection.slayer.initialize_for_new_map ? infection.slayer.initialize_for_new_map() : TRUE;
}

/* a skull is picked up only as it is given; a zombie picks up nothing else
(and readying the skull it holds asks this too) */
static boolean infection_allow_pick_up(
	long unit_index,
	long weapon_index)
{
	short slot;

	if (infection.equipping)
		return TRUE;
	if (infection_weapon_is_skull(weapon_index))
	{
		if (infection_unit_is_zombie(unit_index))
		{
			for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
			{
				if (unit_get(unit_index)->unit.weapon_object_indices[slot] == weapon_index)
					return TRUE;
			}
		}
		return FALSE;
	}
	return !infection_unit_is_zombie(unit_index);
}

static boolean infection_picking_up(
	long weapon_index,
	long player_index)
{
	(void)weapon_index;
	(void)player_index;
	return infection.equipping;
}

/* a skull dropped (its zombie killed, or forced out of a hand): given again
on the next tick, if its zombie lives */
static void infection_weapon_dropped(
	long weapon_index)
{
	long slot;

	if (network_game_distributed_client() || infection.equipping)
		return;
	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		if (infection.skull[slot] == weapon_index)
		{
			infection.equipped_unit[slot] = NONE;
			break;
		}
	}
}

static void infection_player_update_each_tick(
	long player_index)
{
	struct player_datum *player = player_get(player_index);

	infection_reconcile_player(player_index);
	floodfection_bursts(player_index);
	if (infection_floodfection())
	{
		struct distributed_infection_player *role = infection_player(player_index);
		boolean zombie = role && role->role == _distributed_infection_role_zombie;

		floodfection_forms_update(player_index, zombie, infection_original(player_index));
		floodfection_voice(player_index, zombie);
	}
	if (network_game_distributed_client())
		return;
	/* (a dead zombie's skull deleted once its unit's death has dropped it) */
	if (player->quit_out_of_game || player->unit_index == NONE ||
		TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
	{
		infection_delete_skull(DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index));
	}
	else
	{
		infection_equip(player_index);
	}
}

static long infection_get_player_score(
	long player_index,
	enum get_score_type score_type)
{
	struct distributed_infection_player *role = infection_player(player_index);
	long slot;
	long score = 0;

	if (!role)
		return 0;
	if (score_type != _get_score_team)
		return role->score;
	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		if (infection.state.players[slot].role == role->role)
			score += infection.state.players[slot].score;
	}
	return score;
}

static wchar_t *infection_format_player_score(
	long player_index,
	wchar_t *text)
{
	usprintf(text, L"%ld", infection_get_player_score(player_index, _get_score_individual));
	return text;
}

/* (a team's score, whatever its name says) */
static wchar_t *infection_format_team_score(
	long team_index,
	wchar_t *text)
{
	usprintf(text, L"%ld", game_engine_get_team_score(team_index));
	return text;
}

static long infection_did_player_win(
	long player_index)
{
	struct distributed_infection_player *role = infection_player(player_index);

	if (infection.state.winner == NONE || !role)
		return NONE;
	return (role->role == _distributed_infection_role_zombie ? 1 : 0) == infection.state.winner;
}

struct game_engine *infection_engine_get(
	struct game_engine const *slayer)
{
	infection.slayer = *slayer;
	infection.engine = *slayer;
	infection.engine.initialize_for_new_map = infection_initialize_for_new_map;
	infection.engine.player_added = infection_player_added;
	infection.engine.player_update_each_tick = infection_player_update_each_tick;
	infection.engine.prespawn_player_update = infection_reconcile_player;
	infection.engine.update = infection_update;
	infection.engine.allow_pick_up = infection_allow_pick_up;
	infection.engine.picking_up = infection_picking_up;
	infection.engine.weapon_dropped = infection_weapon_dropped;
	infection.engine.player_killed_player = infection_player_killed_player;
	infection.engine.get_player_score = infection_get_player_score;
	infection.engine.format_player_score = infection_format_player_score;
	infection.engine.format_team_name = infection_format_team_score;
	infection.engine.format_message = NULL;
	infection.engine.did_player_win = infection_did_player_win;
	return &infection.engine;
}

/* ---- the state the host sends */

/* a state a client may take: of this game, no older than its own, every
value in range */
static boolean infection_state_valid(
	struct distributed_infection_state const *state,
	long game_number,
	long revision)
{
	short index;

	if (state->game_number != game_number || state->revision < revision || state->revision < 0 ||
		state->phase < _distributed_infection_waiting || state->phase > _distributed_infection_finished ||
		state->phase_tick < 0 || state->winner < NONE || state->winner > 1 ||
		(state->phase != _distributed_infection_finished && state->winner != NONE))
	{
		return FALSE;
	}
	for (index = 0; index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
	{
		struct distributed_infection_player const *player = &state->players[index];

		if (player->role > _distributed_infection_role_zombie || player->score < 0 ||
			player->score > INFECTION_MAXIMUM_SCORE || (player->role && !player->identity) ||
			player->reserved[0] || player->reserved[1] || player->reserved[2])
		{
			return FALSE;
		}
	}
	return TRUE;
}

long infection_write_network_state(
	byte *buffer,
	long size)
{
	if (size < (long)sizeof(infection.state))
		return 0;
	csmemcpy(buffer, &infection.state, sizeof(infection.state));
	return sizeof(infection.state);
}

boolean infection_read_network_state(
	byte const *buffer,
	long size,
	boolean first)
{
	struct distributed_infection_state state;
	struct data_iterator iterator;

	if (!network_game_distributed_client() || size != (long)sizeof(state))
		return FALSE;
	csmemcpy(&state, buffer, sizeof(state));
	if (!infection_state_valid(&state, network_game_get_number_of_games_played(), first ? 0 : infection.state.revision))
		return FALSE;
	infection.state = state;
	data_iterator_new(&iterator, player_data);
	while (data_iterator_next(&iterator))
		infection_reconcile_player(iterator.datum_index);
	return TRUE;
}
