/*
RANDOMIZER.H

The campaign randomizer (randomizer.c). A level swaps actors and weapons
in memory; tags it does not have are read from the other campaign maps.
*/

#ifndef __RANDOMIZER_H
#define __RANDOMIZER_H

void randomizer_tags_loaded(char const *scenario_name);
/* the level's tag table: how many tags, and a hash of each one's group and
name in order, which a co-op game's machines must have alike (the
randomizer's and the skulls' copies at the same indices on each) */
void randomizer_tag_table_fingerprint(long *count, unsigned long *hash);
void randomizer_tags_unloaded(void);

/* a bitmap's or sound's bytes of an imported tag, from the map they were
copied from. 0: this tag is the open map's, and the caller reads that */
int randomizer_donor_read(long tag_index, long offset, long size, void *buffer);

/* the actor, weapon or carried weapon a placement uses. each returns the
index it was given when the randomizer is not swapping that */
long randomizer_actor_variant(long original, long encounter_index, long squad_index, void *starting_location);
/* the last randomizer_actor_variant left its infection form out: NONE, as
one of a squad's to be swapped past the setup's share (game.randomizer_crazy_pods) */
int randomizer_actor_left_out(void);
/* actor_place's actor placed, of the variant asked for: kept when swapped */
void randomizer_actor_placed(long actor_index, long requested);

/* a swapped actor's unit, with no animations for the seats a script loads
it into (vehicle_load_magic): its actor placed back as the one the level
gave it, its new unit, else NONE. Once out of the seat, landed and standing,
walking or fighting, it is swapped again (randomizer_tick) */
long randomizer_rider_restore(long unit_index);

/* the roll the level was laid out with (its own, its host's, or a daily
challenge's seed: community.c sends it with a challenge's run) */
unsigned long randomizer_roll(void);

/* the riders' and the infection forms' state, in the game state (game.c) */
void randomizer_initialize(void);
void randomizer_initialize_for_new_map(void);
void randomizer_tick(void);
long randomizer_weapon_definition(long definition_index, void *scenario_object);
/* the weapon an actor of the variant `variant_index` holds (its own:
`weapon_definition`), each attempt after one its unit refused */
long randomizer_enemy_weapon(long weapon_definition, long unit_index, long attempt, long variant_index);

/* by a tag's name: a Covenant weapon of the campaign's, and a grunt's unit
(the skulls': skulls.c) */
int randomizer_weapon_is_covenant(char const *tag_name);
int randomizer_unit_is_grunt(char const *tag_name);
int randomizer_unit_is_hunter(char const *tag_name);
/* whether a unit holds things, to carry a skull: a grunt, an elite, a
jackal or a combat form (not a carrier or infection form, a hunter, a
sentinel or an engineer) */
int randomizer_unit_can_carry(char const *tag_name);
/* whether a campaign map has a weapon, actor variant or equipment of this
group and path (the Tag Injector's REPLACE ORIGINAL). The maps are read the
first time it is asked */
int randomizer_stock_tag_exists(long group_tag, char const *name);
/* how the campaign's player (a10's characters\cyborg\cyborg, standing) is
animated holding a weapon of a label (a weapon's: "ar", matched as the game
matches it, by its weapon types'): 1 with the weapon class's animations
(bits by model_animation_definitions.c's weapon class list: idle,
move-front, throw-grenade...) and the weapon type's (reload-1, fire-1,
melee...), 0 when it has none of that label (it is held as another), -1
when a10 cannot be read. Read the first time it is asked */
int randomizer_stock_player_class(char const *label, unsigned long long *animations, unsigned long long *overlays);
/* the enemies the level's encounters place on this difficulty (and bring
back) that can carry a skull, by the squads' own actors (the randomizer's
swaps aside): the skulls' odds (skulls.c) */
long randomizer_level_carriers(void);

/* the advertisement's reserved bytes: the randomizer flag and the setup,
when this game is a randomized campaign */
void randomizer_advertisement(unsigned char *reserved);

/* the setup of the game this machine is joining, or none, and its Go
Crazy's parts (HALO_PORT_ADVERTISED_RANDOMIZER_CRAZY_OFFSET's byte) */
void randomizer_set_remote(int on, unsigned char const *setup, unsigned char crazy);
/* ... and a client's, as it was set (a replay a client records keeps it:
network_replay.c): whether the host's game is randomized, and its setup
(HALO_PORT_ADVERTISED_RANDOMIZER_SETUP_SIZE bytes) and crazy byte */
int randomizer_remote_get(unsigned char *setup, unsigned char *crazy);
int randomizer_remote_on(void);

/* the lobby's map line, prefixed while the game is randomized */
void randomizer_note_map_widget(void *widget);
wchar_t const *randomizer_level_title_text(void *widget, wchar_t const *string);

/* whether the game this machine is in, or is hosting, is randomized */
int randomizer_show_prefix(void);

/* maps/randomizer.pack, written from the campaign maps (randomizer_pack.c),
behind the loading screen's bar. 1 when it was */
int randomizer_pack_build(void);

/* the pack is there and matches the maps */
int randomizer_pack_ready(void);

/* the pack is wanted (game.randomizer turned on): a notice, then, at the
main menu, the pack written. Nothing when it is ready */
void randomizer_pack_request(void);
/* maps/skulls.pack, the skull the enemies carry (skulls.c), the same way:
wanted once skulls are to be found (game.skulls on, unlocking by playing) */
void skulls_pack_request(void);
/* skulls.pack is there and matches its map */
int skulls_pack_ready(void);
/* maps/gamemodes.pack, the multiplayer modes' tags from other maps
(Floodfection's burst, Two Betrayals'), the same way: written at start-up
once, when Two Betrayals is there; and it is there and matches its map */
int game_modes_pack_ready(void);

/* a campaign map's header, as the pack records it. 0 when the map is not
there or is not an Xbox map */
int randomizer_pack_map_header(char const *leaf, long *file_length, long *tag_offset, long *tag_size,
	int *compressed);

#endif /* __RANDOMIZER_H */
