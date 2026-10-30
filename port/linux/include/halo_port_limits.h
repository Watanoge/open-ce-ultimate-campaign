/*
HALO_PORT_LIMITS.H

Multiplayer session limits of the native builds (Windows, Linux, Android),
force-included by halo_linux_prefix.h and halo_windows_prefix.h.

The Xbox game allows 16 players on at most 4 machines (up to 4 players each
on split screen). The native builds allow 128 players on up to 128
machines; split screen stays at 4 players per machine.

128 is the largest session that fits the game's existing records: player,
machine and team indices are stored in signed chars (0..127 with NONE), and
a finishing place in 7 bits.
*/

#ifndef __HALO_PORT_LIMITS_H
#define __HALO_PORT_LIMITS_H

/* ---------- session limits */

#define HALO_PORT_MAXIMUM_NETWORK_PLAYERS 128
#define HALO_PORT_MAXIMUM_NETWORK_MACHINES 128

/* a host polls its listening socket and one socket per machine; the Xbox's
Winsock headers default to 64 (the prefix headers define FD_SETSIZE from
this before any of them is read) */
#define HALO_PORT_FD_SETSIZE 256

/* ---------- memory capacity for these limits (game state and pools) */

#include "halo_port_capacity.h"

/* ---------- struct network_game layout

The game settings record (struct network_game) is declared separately in
several networking and interface units; its layout follows from the limits.
Every copy checks its offsets against these values. The Xbox values (4
machines, 16 players) are 0x226 and 0x434. */

#define HALO_PORT_NETWORK_MACHINE_SIZE 0x44
#define HALO_PORT_NETWORK_PLAYER_SIZE 0x20
#define HALO_PORT_NETWORK_GAME_MACHINES_OFFSET 0x114
#define HALO_PORT_NETWORK_GAME_PLAYER_COUNT_OFFSET \
	(HALO_PORT_NETWORK_GAME_MACHINES_OFFSET + HALO_PORT_MAXIMUM_NETWORK_MACHINES * HALO_PORT_NETWORK_MACHINE_SIZE)
#define HALO_PORT_NETWORK_GAME_PLAYERS_OFFSET (HALO_PORT_NETWORK_GAME_PLAYER_COUNT_OFFSET + 2)
#define HALO_PORT_NETWORK_GAME_PLAYERS_END \
	(HALO_PORT_NETWORK_GAME_PLAYERS_OFFSET + HALO_PORT_MAXIMUM_NETWORK_PLAYERS * HALO_PORT_NETWORK_PLAYER_SIZE)
#define HALO_PORT_NETWORK_GAME_RANDOM_SEED_OFFSET (HALO_PORT_NETWORK_GAME_PLAYERS_END + 2)
/* (then the gametype's PC options, struct game_variant_options: 0x2C bytes) */
#define HALO_PORT_NETWORK_GAME_VARIANT_OPTIONS_OFFSET (HALO_PORT_NETWORK_GAME_PLAYERS_END + 0xA)
#define HALO_PORT_NETWORK_GAME_LOCAL_DATA_OFFSET (HALO_PORT_NETWORK_GAME_PLAYERS_END + 0x36)
#define HALO_PORT_NETWORK_GAME_SIZE (HALO_PORT_NETWORK_GAME_PLAYERS_END + 0x3A)

/* ---------- system link protocol

The native builds' messages differ from the Xbox game's (longer arrays, the
game settings record in fragments), so they search for games with their own
protocol version and never see the Xbox game's, or it theirs. */

#define HALO_PORT_NETWORK_GAME_MESSAGE_VERSION 2

/* The native builds' network code has a version of its own (an unsigned
16-bit number): machines of different versions cannot play together, and a
client does not join a host of another version, but tells the player which
is newer (network_client_manager.c). A host advertises it, with its netcode,
in its game's advertisement's reserved bytes (network_server_message_handler.c),
which hosts built before there was a version send as zeros: version 0.
Raise it with any change to what the machines send each other. */
#define HALO_PORT_NETWORK_VERSION 41
/* ... the advertisement's reserved bytes: the version (a little-endian word),
then flags, then the game's kind (an OpenCE gametype's mode, and Hardcore:
port/linux/game/game_modes.h's game_modes_kind), for the server browser;
a randomized campaign's setup (below) takes its byte, a campaign having no
kind */
#define HALO_PORT_ADVERTISED_VERSION_OFFSET 0
#define HALO_PORT_ADVERTISED_FLAGS_OFFSET 2
#define HALO_PORT_ADVERTISED_KIND_OFFSET 3
/* ... the host plays the distributed netcode (always, since the lockstep
netcode was removed; hosts of version 4 built before then may not) */
#define HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG 0x01
/* ... the game is under way (loading, playing or over), not in its lobby:
the menus show it before joining it (hosts built before then never set it) */
#define HALO_PORT_ADVERTISED_IN_PROGRESS_FLAG 0x02
/* ... the host is a dedicated server, with no player of its own (halo
--server: port/linux/src/server_mode.c): the menus say so (from version 39) */
#define HALO_PORT_ADVERTISED_DEDICATED_FLAG 0x04
/* ... the game randomizes a campaign level (0x20: 0x04 is the dedicated flag,
above). The next three reserved bytes are which swaps are on (bit 0 enemies, bit 1 weapon pickups, bit 2 the
weapons actors carry, bit 3 the player's allies with the enemies, bit 4
go crazy; bits 5 to 7 the enemies', the pickups' and the actors' weapons'
one type a custom type of the Tag Injector profile a co-op host shares for
its pools: tag_multiplayer.c), the pool (0 any, 1 covenant, 2 flood, 3
human, 4 one type; 5 the shared profile's, with such a share) and
that type (0 grunt, 1 elite, 2 jackal, 3 hunter, 4 flood combat, 5 flood
carrier, 6 marine, 8 engineer, 9 sentinel). The next four bytes are the placement roll, a
little-endian unsigned long, so a client lays out the level the host did.
A build that cannot run it leaves its join's capability clear and is refused
only for this game */
#define HALO_PORT_ADVERTISED_RANDOMIZER_FLAG 0x20
#define HALO_PORT_ADVERTISED_RANDOMIZER_SETUP_OFFSET 3
#define HALO_PORT_ADVERTISED_RANDOMIZER_ROLL_OFFSET 6
/* ... then five bytes: the one type's rank (randomizer.c's RANK_), the
weapon pickups' pool (0 any, 1 covenant, 2 human, 3 precision, 4 automatic,
5 power, 6 one type) and that type (randomizer.c's WEAPON_), and the same
two for the weapons the actors are given. A client copies the same tags as
the host only with all of them */
#define HALO_PORT_ADVERTISED_RANDOMIZER_WEAPONS_OFFSET 10
/* the bytes from the setup's to the last of these */
#define HALO_PORT_ADVERTISED_RANDOMIZER_SETUP_SIZE 12
/* ... the campaign game is played with skulls (port/linux/game/skulls.c).
The eight reserved bytes after the randomizer's are which (halo_skulls.h's
skull_set, little-endian: skull n bit n); a client plays with these, not
its own */
#define HALO_PORT_ADVERTISED_SKULLS_FLAG 0x08
#define HALO_PORT_ADVERTISED_SKULLS_OFFSET 15
#define HALO_PORT_ADVERTISED_SKULLS_SIZE 8
/* ... and after the skulls' bytes, the randomizer's Go Crazy's parts: bit 0
any NPC holds any weapon, bit 1 the engineers and sentinels in the pools,
bit 2 the flamethrower in the mixed pools, bit 3 set (a host built before
sends 0: all of them with go crazy), and the high four bits the infection
forms swapped, in tenths (0, one in three, without go crazy) */
#define HALO_PORT_ADVERTISED_RANDOMIZER_CRAZY_OFFSET 23
#define HALO_PORT_ADVERTISED_RANDOMIZER_CRAZY_SET 0x08
/* ... the campaign game's enemies drop the host's locked skulls to be found
(skulls.c): every machine copies in the skull they carry, the host tells
its clients of each dropped and taken, and every player keeps those taken
by the host's rules (a modded game) */
#define HALO_PORT_ADVERTISED_SKULL_DROPS_FLAG 0x10
/* ... the multiplayer game plays the weapons of the host's Tag Injector
multiplayer profile (port/linux/game/tag_multiplayer.c): every machine adds
the host's shared weapons to the level, a client downloading them first
(map sharing), and the host swaps the map's weapons for them (a modded
game); or a co-op game plays the AI and weapons of its campaign profile,
shared the same way, which the host swaps the level's for */
#define HALO_PORT_ADVERTISED_TAG_SHARE_FLAG 0x40
/* ... a modded game (randomized, a campaign with skulls, or a game playing
a Tag Injector multiplayer or campaign profile), which builds
without the mod cannot play: its host advertises in the version's place
HALO_PORT_ADVERTISED_MODDED_VERSION, which no build without the mod is on
(it tells its player the host is on a newer version), and its own version
in the last two reserved bytes (little-endian). A game without either keeps
the plain version, and any build of it joins. A modded build joins either
kind: a modded game of its version and mod version, with the packs it
copies tags from written (watanoge_randomizer.pack; watanoge_skulls.pack
for Outbreak, Scarab and the skulls dropped: port/linux/game/tag_pack.c)
(port/linux/NETCODE.md, "Modded games") */
#define HALO_PORT_ADVERTISED_BASE_VERSION_OFFSET 24
/* the mod's own version: raised with any change to what modded games'
machines send each other, which plain games never carry. Builds before the
modded signature sent 1 as their capability (network_client_manager.c); 3
drops skulls in co-op (_distributed_message_skulls_state) and plays Outbreak
there (_distributed_message_skulls_outbreak_burst); 4 plays the Tag Injector's
multiplayer profiles (_distributed_message_tag_share, and map sharing's
shared weapons); 5 its campaign profiles in co-op (the same share, of a
campaign level); 6 copies Floodfection's burst into a level before a
multiplayer profile's tags, which moves theirs (randomizer.c); 7 its
zombies' cries too, which move them again, and tells its first zombies
(_distributed_message_floodfection_originals) */
#define HALO_PORT_MOD_NETWORK_VERSION 7
#define HALO_PORT_MODDED_VERSION_MARK 0xFF00
#define HALO_PORT_ADVERTISED_MODDED_VERSION (HALO_PORT_MODDED_VERSION_MARK | HALO_PORT_MOD_NETWORK_VERSION)

/* a message header's 12-bit length allows messages of up to 0xFFF bytes,
header included; the per-tick update of 128 players is 3,857 */
#define HALO_PORT_MAXIMUM_NETWORK_MESSAGE_SIZE 0x1000

/* the packet codec's limit on a decoded or encoded packet; the per-tick
update of 128 players decodes to 0x1010 bytes */
#define HALO_PORT_NETWORK_PACKET_SIZE 0x1100

/* the game settings record (HALO_PORT_NETWORK_GAME_SIZE, 13,136 bytes at 128
machines and players) does not fit one message; it is sent in pieces of
this many bytes (4 pieces), each 3,594 bytes on the wire */
#define HALO_PORT_NETWORK_GAME_SETTINGS_FRAGMENT_SIZE 0xE00

#endif /* __HALO_PORT_LIMITS_H */
