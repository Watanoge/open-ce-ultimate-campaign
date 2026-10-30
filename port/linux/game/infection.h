/* infection.h: Infection, a mode of team Slayer (port/linux/game/infection.c) */
#ifndef HALO_INFECTION_H
#define HALO_INFECTION_H

struct game_engine;
struct render_fog;
union real_rgb_color;
union real_vector3d;

/* the game being played is Infection (game_modes_mode's) */
boolean infection_enabled(void);
/* Slayer's engine with Infection's rules (game_modes_engine) */
struct game_engine *infection_engine_get(struct game_engine const *slayer);
/* a player spawned: a zombie's skull (game_modes_postspawn) */
void infection_postspawn(long player_index);

/* Floodfection's look (the gametype's mode_variant): a player's armour
colour in place of its team's, a zombie's green and a human's their own
(game_engine_player_get_change_color), and the fog over the map in place of
its own, closer indoors (the camera's cluster has no sky), TRUE when it is
(render.c: the sky is then not drawn) */
void infection_change_color(long player_index, union real_rgb_color *color);
/* the game being played is Floodfection */
boolean infection_floodfection_enabled(void);

/* Floodfection's settings, in game_variant_options' floodfection bytes:
each its byte, its first bit and its bits (menu_functions.c's spinners set
them), its 0 the default; infection.c reads each through its table */
#define FLOODFECTION_SETTING(byte, shift, bits) ((byte) << 16 | (shift) << 8 | (bits))
#define FLOODFECTION_SETTING_BYTE(setting) ((setting) >> 16)
#define FLOODFECTION_SETTING_SHIFT(setting) (((setting) >> 8) & 0xFF)
#define FLOODFECTION_SETTING_BITS(setting) ((setting) & 0xFF)
enum
{
	/* the human forms': health and speed (a table's place: 0 150%), and
	shields (1 on) */
	_floodfection_setting_human_health = FLOODFECTION_SETTING(0, 0, 3),
	_floodfection_setting_human_speed = FLOODFECTION_SETTING(0, 3, 3),
	_floodfection_setting_human_shields = FLOODFECTION_SETTING(0, 6, 1),
	/* the elite forms': health and speed (0 200%), and no shields (1) */
	_floodfection_setting_elite_health = FLOODFECTION_SETTING(1, 0, 3),
	_floodfection_setting_elite_speed = FLOODFECTION_SETTING(1, 3, 3),
	_floodfection_setting_elite_no_shields = FLOODFECTION_SETTING(1, 6, 1),
	/* which zombies are elites (_floodfection_elites_*), and the chance of
	each spawn of RANDOM's (a table's place: 0 25%) */
	_floodfection_setting_elites = FLOODFECTION_SETTING(2, 0, 2),
	_floodfection_setting_elite_chance = FLOODFECTION_SETTING(2, 2, 3),
	/* whose view is over the shoulder (_floodfection_camera_*) */
	_floodfection_setting_camera = FLOODFECTION_SETTING(2, 5, 2),
	/* the share of the players the last humans strengthened are (a table's
	place: 0 10%, 1 none), and the last one not strengthened, not shown to
	the zombies, and no Flood music (1 each) */
	_floodfection_setting_last_stand = FLOODFECTION_SETTING(3, 0, 3),
	_floodfection_setting_no_last_spartan_buff = FLOODFECTION_SETTING(3, 3, 1),
	_floodfection_setting_no_last_spartan_shown = FLOODFECTION_SETTING(3, 4, 1),
	_floodfection_setting_no_music = FLOODFECTION_SETTING(3, 5, 1)
};
enum
{
	_floodfection_elites_first = 0,
	_floodfection_elites_none,
	_floodfection_elites_random
};
enum
{
	_floodfection_camera_flood = 0,
	_floodfection_camera_everyone,
	_floodfection_camera_choice,
	_floodfection_camera_first_person
};
/* a player's view in a Floodfection game (_third_person_rule_*:
third_person.h): its CAMERA setting's, by the player's role */
short infection_third_person(long player_index);
/* the object is a Floodfection zombie's unit: hit, it bleeds as the Flood
(projectiles.c), and a headshot does not kill it outright (damage.c) */
boolean infection_floodfection_zombie(long object_index);
boolean infection_fog(short local_player_index, boolean indoors, struct render_fog *fog);
/* the level loading is a Floodfection game's, and its tags of Two Betrayals
(the burst, the flood bits, the zombies' forms) added to the level's import
(randomizer_tags_loaded, tag_import.c) */
boolean infection_tag_import_wanted(void);
void infection_tag_import(void);

/* a living zombie's unit */
boolean infection_unit_is_zombie(long unit_index);
/* a player may drop the weapon (a zombie keeps its skull: players.c, units.c) */
boolean infection_allow_weapon_drop(long unit_index, long weapon_index);
/* the skull may be deleted (weapons.c's and objects.c's checks that a
game's objective is not) */
boolean infection_can_delete_skull(long object_index);
/* a unit's melee damage multiplier: a zombie's skull's */
real infection_melee_multiplier(long unit_index);
/* a zombie's melee hits a human riding in an open seat, not the vehicle
(unit_cause_player_melee_damage) */
boolean infection_melee_rider_target(long attacker_index, long target_index);
/* ... which the host checks of a client's report: its own line from the
attacker's head to the rider (network_damage.c) */
boolean infection_melee_rider_visible(long attacker_index, long target_index, union real_vector3d const *direction);
/* a player's team, speed and shields as their role has them (a unit
attached: network_player_attach_unit) */
void infection_reconcile_player(long player_index);
/* the round's time left (the score's title) */
long infection_remaining_ticks(void);
/* the state the host sends its clients (game_engine_write_network_state) */
long infection_write_network_state(byte *buffer, long size);
boolean infection_read_network_state(byte const *buffer, long size, boolean first);
/* a Floodfection game's first zombies, which the host tells its clients
apart from the state (_distributed_message_floodfection_originals: a build
without it takes the state as Infection's): to a client when it is ready
(NONE: to every client, if they changed) */
void infection_send_originals(long machine_index);
int infection_handles_message(unsigned short const *message, unsigned short size);
void infection_handle_message(long machine_index, unsigned short const *message, unsigned short size);
/* (game_engine_render_nav_points, each frame) the last human, shown to a
Floodfection game's zombies */
void infection_render_nav_points(short local_player_index);

#endif
