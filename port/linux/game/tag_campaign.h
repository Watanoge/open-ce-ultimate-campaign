/*
TAG_CAMPAIGN.H

The Tag Injector in the campaign (tag_campaign.c, docs/tag_injector.md
"Campaign profiles"): a campaign level played alone plays the weapons and AI
of a profile as a campaign profile says (port_tag_mp_profiles.c's
.campaign), in the randomizer's place: its weapons lying in the level and
its enemies swapped, and the profile's AI given the weapons chosen for them.
*/

#ifndef __TAG_CAMPAIGN_H
#define __TAG_CAMPAIGN_H

/* ---------- constants */

enum
{
	/* the campaign's weapons a campaign profile names: the first
	TAG_CAMPAIGN_PICKUP_WEAPONS lie in the levels, the rest (the fuel rod,
	the energy sword) only the AI hold */
	TAG_CAMPAIGN_STOCK_WEAPONS = 11,
	TAG_CAMPAIGN_PICKUP_WEAPONS = 9,
	/* the campaign's enemies a campaign profile swaps, in randomizer.c's
	TYPE_* order (the type of a unit's name) */
	TAG_CAMPAIGN_STOCK_ENEMIES = 10,
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
the level's (`original`, of the stock enemy `enemy`, NONE for none of
them; an ally of the player's or not) is placed as; the weapon a weapon
lying in the level is; the weapon an actor of the variant `variant` holds
in place of its own (`weapon`, which the unit `unit_index` is to hold).
Each the one it was given when the profile does not swap it; `seed` the
placement's roll */
long tag_campaign_actor_variant(
	long original,
	short enemy,
	boolean ally,
	unsigned long seed);
long tag_campaign_pickup(
	long weapon,
	unsigned long seed);
long tag_campaign_enemy_weapon(
	long variant,
	long weapon,
	long unit_index,
	unsigned long seed);

#endif
