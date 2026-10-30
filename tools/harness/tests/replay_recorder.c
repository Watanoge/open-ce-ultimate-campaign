/*
REPLAY_RECORDER.C (test)

The replay recorder's machine (network_distributed.c's distributed_recorder_find,
distributed_client_machines, distributed_viewing_machines, the sends to a
machine and the batches' flush, distributed_machine_sees_player; players.c's
machine_get_player_list): the host recording a replay sends a tick's state to
a machine index of its own past the session's, so a game of every machine
(the host and 127 bots, or 128 players) is recorded whole.
test_replay_recorder.py takes the constants (config.inc) and the code
(under_test.inc).
*/

#include "harness.h"

typedef unsigned short word;
#define csmemset memset
#define csmemcpy memcpy

#include "config.inc"

#define MAXIMUM_NETWORK_MACHINE_COUNT HALO_PORT_MAXIMUM_NETWORK_MACHINES
#define MAXIMUM_TRACKED_PLAYERS HALO_PORT_MAXIMUM_NETWORK_PLAYERS

struct player_datum
{
	short local_player_index;
};

struct distributed_message_header
{
	word header;
	byte type;
	byte count;
	long game_time;
};

struct distributed_batch
{
	word size;
	byte data[1024];
};

enum
{
	_distributed_message_batch = 1,
	_distributed_message_test = 2,
	_distributed_send_sees_bit = 2,
	_network_replay_datagram = 1,
	_network_replay_reliable_to_machine = 3,
};

/* the game's machines' players (players.c's table), and past its end a row
that is no machine's: one read past the table reads it, not NONE */
static struct
{
	long table[MAXIMUM_NETWORK_MACHINE_COUNT][MAXIMUM_LOCAL_PLAYERS];
	long past_the_end[4][MAXIMUM_LOCAL_PLAYERS];
} players;
#define machine_to_player_table players.table

static struct player_datum player_data[MAXIMUM_TRACKED_PLAYERS];
static struct player_datum *player_try_and_get(long player_index)
{
	long index = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);

	return index < MAXIMUM_TRACKED_PLAYERS ? &player_data[index] : NULL;
}

/* the session: its machines, and whether the host records */
static long session_machines[MAXIMUM_NETWORK_MACHINE_COUNT];
static short session_machine_count;
static boolean recording;
static boolean network_replay_recording(void) { return recording; }
static short network_distributed_server_machines(long *machine_indices, short maximum)
{
	short index;

	for (index = 0; index < session_machine_count && index < maximum; index++)
		machine_indices[index] = session_machines[index];
	return index;
}

/* where each message went: to the replay (its kind), a client (its machine,
reliably or not), or the host */
static long recorded[8];
static long sent_to_machine[MAXIMUM_NETWORK_MACHINE_COUNT + 2];
static long sent_reliably_to_machine[MAXIMUM_NETWORK_MACHINE_COUNT + 2];
static long sent_to_host;
static void network_replay_record(byte kind, void const *data, long size)
{
	(void)data;
	(void)size;
	recorded[kind]++;
}
static void network_distributed_server_send_to_machine(long machine_index, void const *data, long size)
{
	(void)data;
	(void)size;
	CHECK(machine_index >= 0 && machine_index < MAXIMUM_NETWORK_MACHINE_COUNT, "sent to machine %ld, no client's",
		machine_index);
	sent_to_machine[machine_index]++;
}
static void network_distributed_server_send_to_machine_reliably(long machine_index, void const *data, long size)
{
	(void)data;
	(void)size;
	CHECK(machine_index >= 0 && machine_index < MAXIMUM_NETWORK_MACHINE_COUNT, "sent reliably to machine %ld, no client's",
		machine_index);
	sent_reliably_to_machine[machine_index]++;
}
static void network_distributed_client_send(void const *data, long size)
{
	(void)data;
	(void)size;
	sent_to_host++;
}

static long game_time_get(void) { return 100; }
static void build_message_header(word *header, word size, short a, short b)
{
	(void)a;
	(void)b;
	*header = size;
}
static void distributed_fill_header(void *message, byte type, short count, word size)
{
	struct distributed_message_header *header = (struct distributed_message_header *)message;

	(void)size;
	header->type = type;
	header->count = (byte)count;
}
static word distributed_entry_size(short count, word size)
{
	(void)count;
	(void)size;
	return 0;
}

static struct distributed_batch distributed_batches[MAXIMUM_SENDERS];
static byte distributed_sends[DISTRIBUTED_VIEWING_MACHINES][MAXIMUM_TRACKED_PLAYERS];
static void distributed_batch_add(short sender, void const *message, word size, word entry_size)
{
	(void)message;
	(void)entry_size;
	CHECK(sender >= 0 && sender < MAXIMUM_SENDERS, "a batch for sender %d", sender);
	distributed_batches[sender].size += size;
}

static struct
{
	boolean in_tick;
	boolean valid;
	short count;
	long indices[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
	long recorder;
} distributed_machines = { FALSE, FALSE, 0, { 0 }, NONE };

#include "under_test.inc"

/* a game of `machine_count` machines, each with a player (the first the
host's own, its player local), and `recording` */
static void game(short machine_count, boolean record)
{
	long machine_index, local_player_index;

	memset(&players, 0, sizeof(players));
	for (machine_index = 0; machine_index < MAXIMUM_NETWORK_MACHINE_COUNT; machine_index++)
	{
		for (local_player_index = 0; local_player_index < MAXIMUM_LOCAL_PLAYERS; local_player_index++)
			players.table[machine_index][local_player_index] = NONE;
	}
	/* (not NONE: a read past the table finds players) */
	for (machine_index = 0; machine_index < 4; machine_index++)
	{
		for (local_player_index = 0; local_player_index < MAXIMUM_LOCAL_PLAYERS; local_player_index++)
			players.past_the_end[machine_index][local_player_index] = 5;
	}
	session_machine_count = 0;
	for (machine_index = 0; machine_index < machine_count; machine_index++)
	{
		players.table[machine_index][0] = machine_index;
		player_data[machine_index].local_player_index = machine_index == 0 ? 0 : NONE;
		session_machines[session_machine_count++] = machine_index;
	}
	recording = record;
	distributed_machines.valid = FALSE;
	distributed_machines.in_tick = TRUE;
	memset(distributed_batches, 0, sizeof(distributed_batches));
	memset(distributed_sends, 0, sizeof(distributed_sends));
	memset(recorded, 0, sizeof(recorded));
	memset(sent_to_machine, 0, sizeof(sent_to_machine));
	memset(sent_reliably_to_machine, 0, sizeof(sent_reliably_to_machine));
	sent_to_host = 0;
}

/* a tick's message to each machine the state goes to, and the batches out */
static short send_to_viewers(void)
{
	long machine_indices[DISTRIBUTED_VIEWING_MACHINES];
	short count = distributed_viewing_machines(machine_indices, DISTRIBUTED_VIEWING_MACHINES);
	short index;

	for (index = 0; index < count; index++)
	{
		struct distributed_message_header message;

		memset(&message, 0, sizeof(message));
		distributed_send_to_machine(machine_indices[index], &message, _distributed_message_test, 0, sizeof(message));
	}
	for (index = 0; index < MAXIMUM_SENDERS; index++)
		distributed_batch_flush(index);
	return count;
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";

	/* the recorder is no slot of the session's: its own, past them */
	CASE("constants")
	{
		CHECK(DISTRIBUTED_RECORDER_MACHINE >= HALO_PORT_MAXIMUM_NETWORK_MACHINES, "the recorder (%d) is a session slot",
			DISTRIBUTED_RECORDER_MACHINE);
		CHECK(DISTRIBUTED_RECORDER_MACHINE < DISTRIBUTED_VIEWING_MACHINES, "no row for the recorder");
		CHECK(HOST_SENDER != DISTRIBUTED_RECORDER_MACHINE && HOST_SENDER < MAXIMUM_SENDERS &&
			DISTRIBUTED_RECORDER_MACHINE < MAXIMUM_SENDERS, "the host's batch and the recorder's are not each their own");
		return 0;
	}
	/* a full game (the host and 127 others): every client, and the recorder,
	each tick's datagram in the replay */
	CASE("full-game")
	{
		long machine_index;
		short count;

		game(HALO_PORT_MAXIMUM_NETWORK_MACHINES, TRUE);
		CHECK(distributed_recorder_machine() == DISTRIBUTED_RECORDER_MACHINE, "the recorder is %ld",
			distributed_recorder_machine());
		count = send_to_viewers();
		CHECK(count == HALO_PORT_MAXIMUM_NETWORK_MACHINES, "%d machines viewing, not the 127 clients and the recorder",
			count);
		CHECK(recorded[_network_replay_datagram] == 1, "%ld datagrams recorded, not 1", recorded[_network_replay_datagram]);
		for (machine_index = 1; machine_index < HALO_PORT_MAXIMUM_NETWORK_MACHINES; machine_index++)
			CHECK(sent_to_machine[machine_index] == 1, "machine %ld sent %ld", machine_index, sent_to_machine[machine_index]);
		CHECK(sent_to_machine[0] == 0 && sent_to_host == 0, "the host's own machine was sent to");
		return 0;
	}
	/* a dedicated server's 128 clients, none of them its own */
	CASE("every-machine-a-client")
	{
		short count;

		game(HALO_PORT_MAXIMUM_NETWORK_MACHINES, TRUE);
		player_data[0].local_player_index = NONE;
		count = send_to_viewers();
		CHECK(count == DISTRIBUTED_VIEWING_MACHINES, "%d machines viewing, not 128 clients and the recorder", count);
		CHECK(recorded[_network_replay_datagram] == 1, "%ld datagrams recorded, not 1", recorded[_network_replay_datagram]);
		return 0;
	}
	/* not recording: no recorder, nothing recorded */
	CASE("not-recording")
	{
		short count;

		game(4, FALSE);
		CHECK(distributed_recorder_machine() == NONE, "a recorder while not recording");
		count = send_to_viewers();
		CHECK(count == 3 && recorded[_network_replay_datagram] == 0, "%d viewing, %ld recorded", count,
			recorded[_network_replay_datagram]);
		return 0;
	}
	/* a reliable message to the recorder is the replay's, not a client's */
	CASE("reliable")
	{
		struct distributed_message_header message;

		game(HALO_PORT_MAXIMUM_NETWORK_MACHINES, TRUE);
		memset(&message, 0, sizeof(message));
		distributed_send_to_machine_reliably(DISTRIBUTED_RECORDER_MACHINE, &message, _distributed_message_test, 0,
			sizeof(message));
		CHECK(recorded[_network_replay_reliable_to_machine] == 1, "not recorded");
		distributed_send_to_machine_reliably(5, &message, _distributed_message_test, 0, sizeof(message));
		CHECK(sent_reliably_to_machine[5] == 1 && recorded[_network_replay_reliable_to_machine] == 1,
			"machine 5's went elsewhere");
		return 0;
	}
	/* the recorder has no players (no row of players.c's table, nor past it),
	and what it sees is kept in a row of its own */
	CASE("no-players")
	{
		long *list;
		long local_player_index;

		game(HALO_PORT_MAXIMUM_NETWORK_MACHINES, TRUE);
		list = machine_get_player_list(DISTRIBUTED_RECORDER_MACHINE);
		for (local_player_index = 0; local_player_index < MAXIMUM_LOCAL_PLAYERS; local_player_index++)
			CHECK(list[local_player_index] == NONE, "the recorder has player %ld", list[local_player_index]);
		CHECK(!distributed_machine_is_local(DISTRIBUTED_RECORDER_MACHINE), "the recorder is the host's");
		CHECK(machine_get_player_list(3)[0] == 3, "machine 3's players are not its own");
		distributed_sends[DISTRIBUTED_RECORDER_MACHINE][7] = FLAG(_distributed_send_sees_bit);
		CHECK(distributed_machine_sees_player(DISTRIBUTED_RECORDER_MACHINE, 7), "the recorder sees nothing");
		CHECK(!distributed_machine_sees_player(DISTRIBUTED_VIEWING_MACHINES, 7), "a machine past the recorder sees");
		return 0;
	}
	fprintf(stderr, "unknown case: %s\n", case_name);
	return 2;
}
