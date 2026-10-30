/* The view over the shoulder in a multiplayer game (network.third_person,
third_person.c): seeing round corners is an edge, so it is the host's to
allow, for every player at once, each of whom then has their own setting
(game.third_person). A game of this machine alone (split screen, bots) is
its own setting's. The host sends its choice to each client reliably when it
is ready and when it changes, as a message upstream's builds drop as a kind
they do not know: a plain game stays one they play, and their players stay
in first person; until it arrives, and in an upstream host's game, which
never sends it, a client is in first person. */
#include "cseries.h"
#include "game/game.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "network_distributed.h"
#include "network_third_person.h"
#include <string.h>

int config_boolean(char const *name);
unsigned long config_changes(void);

static boolean client_allowed = FALSE;
static short sent_allowed = NONE;

struct third_person_message
{
	struct distributed_message_header header;
	word allowed;
	word reserved;
};

/* the host's choice, read again when a setting changes */
static boolean host_allowed(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static boolean allowed = FALSE;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		allowed = config_boolean("network.third_person") != 0;
	}
	return allowed;
}

void network_third_person_reset(void)
{
	client_allowed = FALSE;
	sent_allowed = NONE;
}

/* a client the host's word, else this machine's own setting */
boolean network_third_person_allowed(void)
{
	if (game_connection() == _game_connection_network_client)
		return client_allowed;
	return host_allowed();
}

static void third_person_message_make(struct third_person_message *message, boolean allowed)
{
	memset(message, 0, sizeof(*message));
	message->allowed = allowed ? 1 : 0;
}

void network_third_person_send(long machine_index)
{
	struct third_person_message message;

	if (game_connection() != _game_connection_network_server)
		return;
	third_person_message_make(&message, host_allowed());
	distributed_send_to_machine_reliably(machine_index, &message, _distributed_message_third_person_policy, 1,
		sizeof(message));
}

void network_third_person_update(void)
{
	struct third_person_message message;
	boolean allowed;

	if (!game_in_progress() || main_menu_is_active() || game_connection() != _game_connection_network_server)
		return;
	allowed = host_allowed();
	if (sent_allowed == allowed)
		return;
	sent_allowed = allowed;
	third_person_message_make(&message, allowed);
	distributed_send(&message, _distributed_message_third_person_policy, 1, sizeof(message),
		_distributed_to_clients_reliably);
}

boolean network_third_person_handles_message(word const *message, word size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return FALSE;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_third_person_policy;
}

/* (the host's alone, over its stream, whole) */
void network_third_person_handle_message(long machine_index, word const *data, word size)
{
	struct third_person_message message;

	if (machine_index != NONE || game_connection() != _game_connection_network_client ||
		network_connection_last_read_was_unreliable() || size != sizeof(message))
		return;
	memcpy(&message, data, sizeof(message));
	if (GET_MESSAGE_TYPE(message.header.header) != 2 ||
		message.header.type != _distributed_message_third_person_policy ||
		message.header.count != 1 || message.allowed > 1 || message.reserved != 0)
		return;
	client_allowed = message.allowed != 0;
}
