/* bots.h: computer players for local and system link multiplayer games (port/linux/game/bots.c)

A bot is a player of a local (split screen) or system link game whose
controls come from code on the host instead of a controller (the clients
drive it with the input the host relays them, as they drive its players): it is in the lobby, the scores and the game's
rules as any player is, spawns, respawns and scores. Each bot is the one
player of a machine of its own at the top of the machine indices
(BOTS_MACHINE: 127 down), which no real machine holds: the host gives the
machines that join the lowest free slots, none a bot is of
(network_game_server_add_new_client). The bots have no view and no controller: nothing is
drawn for them. */
#ifndef HALO_BOTS_H
#define HALO_BOTS_H

struct player_action;
struct network_player;

/* the most bots in one game: as many players as a game holds, but the one
hosting it (127) */
#define BOTS_MAXIMUM (HALO_PORT_MAXIMUM_NETWORK_PLAYERS - 1)
/* a bot's machine index, and the bot of one: the session's last machine
indices, the first bot's the last (127) and each next bot's the one below.
The machines that join take the lowest free ones (network_game_server_add_new_client),
so the two meet only in a game with no room left */
#define BOTS_MACHINE(bot_index) (HALO_PORT_MAXIMUM_NETWORK_MACHINES - 1 - (bot_index))
#define BOTS_INDEX(machine_index) (HALO_PORT_MAXIMUM_NETWORK_MACHINES - 1 - (machine_index))

enum
{
	/* bots.teams: in a team game, the bots on both teams (evening them
	with the players), or all on the team the players are not on */
	_bots_teams_even = 0,
	_bots_teams_against,
};

/* the settings (bots.count, bots.skill, bots.teams) */
long bots_wanted_count(void);
/* the count wanted in place of bots.count's (the benchmark's games: benchmark.c), or bots.count's again (-1) */
void bots_set_count_override(long count);
short bots_skill(void);
short bots_teams(void);

/* a machine is a bot's: one of the bots' indices that no machine that joined
holds (the host's alone knows: network_game_server_port_machine_joined) */
int bots_machine_is_bot(long machine_index);
/* (network_server_manager.c's) whether a machine that joined holds the index */
unsigned char network_game_server_port_machine_joined(long machine_index);
/* whether this machine hosts a game bots may play in: local (split screen), system link, or a private internet game
(not listed in the server browser) */
int bots_game_allowed(void);
/* the network player for bot number `bot_index` (0 up): its machine,
controller, name and no team or colour yet */
void bots_network_player(long bot_index, struct network_player *player);

/* the game tick (players_update_before_game): every bot's controls this
tick, into the actions of the players by their index; nothing outside a
local game */
void bots_update_actions(struct player_action *actions);

/* a map's start and end (players.c) */
void bots_initialize_for_new_map(void);
void bots_dispose_from_old_map(void);

#endif
