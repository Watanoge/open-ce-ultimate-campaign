/* The view over the shoulder, on foot (third_person.c). */
#ifndef THIRD_PERSON_H
#define THIRD_PERSON_H

#include "math/real_math.h"

struct camera_command;
struct observer_result;

/* a player's view, as the game being played has it (game_modes_third_person) */
enum
{
	/* first person, whatever the player's setting */
	_third_person_rule_first_person = 0,
	/* the player's setting (game.third_person) */
	_third_person_rule_choice,
	/* over the shoulder, whatever the player's setting */
	_third_person_rule_forced
};

/* the local player's view is over their shoulder: on foot, alive, not
scoped, and the game and their setting have it so */
boolean third_person_active(short local_player_index);
/* following_camera_update's command for it: the shoulder, the distance
back, and what the eye's line hits, which the view turns to */
void third_person_command(short local_player_index, real_euler_angles2d const *facing,
	struct camera_command *command);
/* (observer_postcheck) the shoulder kept out of walls: focus, offset from the
eye, brought back to where the line from the eye meets one */
void third_person_clip_focus(short local_player_index, real_point3d const *eye, real_point3d *focus);
/* (observer_postcheck) the view turned to what the eye's line hits, so that
the screen's middle is where the shot goes */
void third_person_converge(short local_player_index, struct observer_result *result);

#endif
