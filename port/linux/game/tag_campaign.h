/*
TAG_CAMPAIGN.H

The Tag Injector in the campaign (tag_campaign.c, docs/tag_injector.md
"Campaign profiles"): a campaign level played alone plays the weapons and AI
of a profile as a campaign profile says (port_tag_mp_profiles.c's
campaign), in the randomizer's place: the weapons lying in the level and
those its actors carry swapped, its enemies swapped (each kind, and each
rank of a kind that has ranks, a slot of its own), and the actors of a slot
given the weapons chosen for it.
*/

#ifndef __TAG_CAMPAIGN_H
#define __TAG_CAMPAIGN_H

/* ---------- constants */

enum
{
	/* the campaign's weapons a campaign profile names (the fuel rod and
	the energy sword last: the AI alone hold them) */
	TAG_CAMPAIGN_STOCK_WEAPONS = 11,
	/* the campaign's enemies a campaign profile swaps, in randomizer.c's
	TYPE_* order (the type of a unit's name) */
	TAG_CAMPAIGN_STOCK_ENEMIES = 10,
	/* the ranks of an enemy, in randomizer.c's RANK_* order (RANK_ANY, 0:
	an enemy without ranks) */
	TAG_CAMPAIGN_RANKS = 8,
};

/* ---------- prototypes/TAG_CAMPAIGN.C */

/* the campaign's weapons and enemies: of each its key (the setting's word:
"shotgun", "elite"), and its name as the menus show it ("SHOTGUN"); a
weapon's path */
char const *tag_campaign_weapon_key(
	short weapon);
char const *tag_campaign_weapon_name(
	short weapon);
char const *tag_campaign_weapon_tag(
	short weapon);
char const *tag_campaign_enemy_key(
	short enemy);
char const *tag_campaign_enemy_name(
	short enemy);
/* an enemy's ranks: how many (0: it has none), and the nth (a RANK_*);
a rank's name as the menus show it ("SPEC OPS"); whether the enemy holds
weapons a loadout can choose (not a hunter, an engineer, a sentinel, a
carrier or infection form) */
short tag_campaign_enemy_rank_count(
	short enemy);
short tag_campaign_enemy_rank(
	short enemy,
	short index);
char const *tag_campaign_rank_name(
	short rank);
boolean tag_campaign_enemy_armed(
	short enemy);
/* a slot's key (an enemy's, and its rank's: "grunt_minor", "marine"), the
[enemies] and [loadout] tables' key, and its name ("GRUNT MINOR") */
void tag_campaign_slot_key(
	short enemy,
	short rank,
	char *key,
	long key_size);
void tag_campaign_slot_name(
	short enemy,
	short rank,
	char *name,
	long name_size);

/* the campaign profile played (game.campaign_tag_profile: the index of
port_tag_mp_profiles' campaign profiles, else NONE), and it chosen (NONE:
none), written to config.toml */
long tag_campaign_profile_chosen(
	void);
void tag_campaign_profile_choose(
	long profile);

/* randomizer.c's, as a campaign level played alone loads: the profile
whose tags the campaign profile played adds (opened: port_tag_profiles'
index, else NONE: the randomizer plays the level as it would); then the
tags it added (the profile's tags they were taken out for), and the level
let go */
long tag_campaign_level_profile(
	void);
void tag_campaign_level_loaded(
	long profile,
	long const *roots,
	long const *root_tags,
	long count);
void tag_campaign_level_unloaded(
	void);
/* whether the level loaded plays a campaign profile */
boolean tag_campaign_level_playing(
	void);

/* randomizer.c's, as the level places them: the actor variant an actor of
the level's (`original`, of the stock enemy `enemy` and its rank, NONE for
none of them; an ally of the player's or not) is placed as; the weapon a
weapon lying in the level is; the weapon an actor placed in the slot of
`enemy` and `rank` holds in place of its own (`weapon`, which the unit
`unit_index` is to hold). Each the one it was given when the profile does
not swap it; `seed` the placement's roll */
long tag_campaign_actor_variant(
	long original,
	short enemy,
	short rank,
	boolean ally,
	unsigned long seed);
long tag_campaign_pickup(
	long weapon,
	unsigned long seed);
long tag_campaign_enemy_weapon(
	short enemy,
	short rank,
	long weapon,
	long unit_index,
	unsigned long seed);

#endif
