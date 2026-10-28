/*
TAG_CAMPAIGN.C

The Tag Injector in the campaign (tag_campaign.h; docs/tag_injector.md,
"Campaign profiles"). A campaign level played alone (not a network game,
nor a challenge) can play a campaign profile (the TAG INJECTOR PROFILE
screen before the level: game.campaign_tag_profile), which names a profile
of the Tag Injector's and how the level plays its weapons and AI
(port_tag_mp_profiles.c's campaign folder). It plays them in the randomizer's
place: randomizer.c adds the profile's tags to the level as it would its own
(tag_injector_tags_load), and asks here what each placement is.

- Weapons. Each weapon lying in the level (not one a script names), and
  each an actor is given (the fuel rod and the energy sword are only
  those), is swapped as WEAPON ROTATION and WEAPON REPLACEMENT say: RANDOM,
  one of the profile's weapons at random, and with ADD the level's own
  among them; MANUAL, the weapons chosen for each of the campaign's
  (ORIGINAL among them), and with ADD the level's own too. One with none
  chosen stays as it is; an actor's, one its unit cannot hold too.
- Enemies. Each actor the level places (not one a script commands, nor one
  a conversation names) is swapped as ENEMY ROTATION and ENEMY REPLACEMENT
  say, as weapons are: RANDOM among the profile's AI (the player's allies
  are left as they are), MANUAL the AI chosen for its slot: its kind of
  the campaign's enemies (by its unit's type, randomizer.c's: a grunt, an
  elite, a marine...) and, of a kind with ranks, its rank (by its name:
  minor, major, spec ops...). randomizer.c places a squad's infection forms
  swapped for other AI only a third of the time, as its own pools do.
- Loadouts. An actor placed in a slot (the level's own, or the AI it was
  swapped for) holds one of the weapons chosen for the slot (ORIGINAL its
  own, the profile's weapons, the campaign's when the level has them) that
  its unit can hold, at random, in place of the weapons' swap; none chosen,
  or none it can hold, its own, swapped as weapons are.

Each roll is the placement's (randomizer.c's placement_seed), so a level
started again rolls again, and a checkpoint keeps what was placed.
*/

/* ---------- headers */

#include "cseries.h"
#include "cseries/errors.h"
#include "ai/actor_definitions.h"
#include "items/weapon_definitions.h"
#include "tag_files/tag_groups.h"

#include "tag_injector.h"
#include "tag_campaign.h"

#include <stdio.h>
#include <string.h>

/* port_config.c's, port_tag_profiles.c's, port_tag_mp_profiles.c's, units.c's */
int config_boolean(char const *name);
char const *config_string(char const *name);
int config_write(char const *name, char const *value);
long port_tag_profiles_scan(void);
long port_tag_profiles_find(char const *folder);
char const *port_tag_profiles_name(long profile);
unsigned long port_tag_profiles_tag_group(long profile, long tag);
char const *port_tag_profiles_tag_name(long profile, long tag);
long port_tag_campaign_profiles_scan(void);
long port_tag_campaign_profiles_find(char const *file);
char const *port_tag_campaign_profiles_name(long profile);
char const *port_tag_campaign_profiles_file(long profile);
char const *port_tag_campaign_profiles_option(long profile, char const *key);
long port_tag_campaign_profiles_list_count(long profile, char const *section, char const *key);
char const *port_tag_campaign_profiles_list(long profile, char const *section, char const *key, long entry);
extern boolean unit_can_hold_weapon_definition(long unit_index, long weapon_definition_index);

/* ---------- constants */

enum
{
	/* the profile's weapons and AI a level plays at most (randomizer.c's
	CUSTOM_ROOT_MAX), and the tags a manual choice or a loadout chooses
	among */
	MAXIMUM_PROFILE_ROOTS = 128,
	MAXIMUM_CHOICES = 32,
	/* a choice of the level's own (ORIGINAL) */
	CHOICE_ORIGINAL = -2,
};

enum
{
	_rotation_add,
	_rotation_only,
};

enum
{
	_replace_random,
	_replace_manual,
};

/* ---------- structures */

struct stock_weapon
{
	char const *key;
	char const *name;
	char const *tag;
};

struct choices
{
	long tags[MAXIMUM_CHOICES];
	short count;
};

/* ---------- globals */

/* (the fuel rod and the energy sword last: the AI alone hold them) */
static struct stock_weapon const stock_weapons[TAG_CAMPAIGN_STOCK_WEAPONS] =
{
	{ "assault_rifle", "ASSAULT RIFLE", "weapons\\assault rifle\\assault rifle" },
	{ "pistol", "PISTOL", "weapons\\pistol\\pistol" },
	{ "sniper_rifle", "SNIPER RIFLE", "weapons\\sniper rifle\\sniper rifle" },
	{ "rocket_launcher", "ROCKET LAUNCHER", "weapons\\rocket launcher\\rocket launcher" },
	{ "shotgun", "SHOTGUN", "weapons\\shotgun\\shotgun" },
	{ "flamethrower", "FLAMETHROWER", "weapons\\flamethrower\\flamethrower" },
	{ "plasma_rifle", "PLASMA RIFLE", "weapons\\plasma rifle\\plasma rifle" },
	{ "plasma_pistol", "PLASMA PISTOL", "weapons\\plasma pistol\\plasma pistol" },
	{ "needler", "NEEDLER", "weapons\\needler\\needler" },
	{ "fuel_rod", "FUEL ROD", "weapons\\fuel rod gun\\fuel rod" },
	{ "energy_sword", "ENERGY SWORD", "weapons\\energy sword\\energy sword" },
};

/* (randomizer.c's RANK_* order) */
static struct
{
	char const *key;
	char const *name;
} const ranks[TAG_CAMPAIGN_RANKS] =
{
	{ "any", "ANY" },
	{ "minor", "MINOR" },
	{ "major", "MAJOR" },
	{ "specops", "SPEC OPS" },
	{ "zealot", "ZEALOT" },
	{ "stealth", "STEALTH" },
	{ "human", "HUMAN" },
	{ "elite", "ELITE" },
};

#define RANK_BIT(rank) (1 << (rank))
#define RANKS_MINOR_MAJOR (RANK_BIT(1) | RANK_BIT(2))

/* (randomizer.c's TYPE_* order): each its ranks (RANK_BIT's: the
randomizer screen's), and whether it holds weapons a loadout can choose */
static struct
{
	char const *key;
	char const *name;
	short ranks;
	boolean armed;
} const stock_enemies[TAG_CAMPAIGN_STOCK_ENEMIES] =
{
	{ "grunt", "GRUNT", RANKS_MINOR_MAJOR | RANK_BIT(3), TRUE },
	{ "elite", "ELITE", RANKS_MINOR_MAJOR | RANK_BIT(3) | RANK_BIT(4) | RANK_BIT(5), TRUE },
	{ "jackal", "JACKAL", RANKS_MINOR_MAJOR, TRUE },
	{ "hunter", "HUNTER", RANKS_MINOR_MAJOR, FALSE },
	{ "flood_combat", "COMBAT FORM", RANK_BIT(6) | RANK_BIT(7), TRUE },
	{ "flood_carrier", "CARRIER FORM", 0, FALSE },
	{ "marine", "MARINE", 0, TRUE },
	{ "flood_infection", "INFECTION FORM", 0, FALSE },
	{ "engineer", "ENGINEER", 0, FALSE },
	{ "sentinel", "SENTINEL", 0, FALSE },
};

static struct
{
	/* the campaign profile the level plays (its file's name), and whether
	its tags were added */
	char file[256];
	boolean active;
	short weapon_rotation;
	short weapon_replace;
	short enemy_rotation;
	short enemy_replace;
	/* the profile's weapons and AI the level was given, and the campaign's
	weapons it has (NONE: not) */
	long weapons[MAXIMUM_PROFILE_ROOTS];
	short weapon_count;
	long ai[MAXIMUM_PROFILE_ROOTS];
	short ai_count;
	long stock[TAG_CAMPAIGN_STOCK_WEAPONS];
	/* MANUAL's choices for each of the campaign's weapons and each enemy
	slot, and each slot's loadout (by enemy and rank) */
	struct choices weapon_manual[TAG_CAMPAIGN_STOCK_WEAPONS];
	struct choices enemy_manual[TAG_CAMPAIGN_STOCK_ENEMIES][TAG_CAMPAIGN_RANKS];
	struct choices loadouts[TAG_CAMPAIGN_STOCK_ENEMIES][TAG_CAMPAIGN_RANKS];
} tag_campaign_level;

/* ---------- private code */

static char lower(char character)
{
	return character >= 'A' && character <= 'Z' ? (char)(character - 'A' + 'a') : character;
}

static boolean names_match(char const *first, char const *second)
{
	for (; *first && lower(*first) == lower(*second); first++, second++)
	{
	}
	return !*first && !*second;
}

static short choice_of(char const *value, char const *const *values, short count)
{
	short index;

	for (index = 0; value && index < count; index++)
	{
		if (!strcmp(value, values[index]))
			return index;
	}
	return 0;
}

/* a tag of a choice by its path: ORIGINAL, one of the profile's (its
weapons, or its AI), or one of the campaign's weapons the level has
("stock:" and its key); NONE for none of them */
static long choice_tag(char const *path, long profile, long const *roots, long const *root_tags, long count,
	unsigned long group)
{
	long index;

	if (!strcmp(path, "original"))
		return CHOICE_ORIGINAL;
	if (group == 'weap' && !strncmp(path, "stock:", 6))
	{
		for (index = 0; index < TAG_CAMPAIGN_STOCK_WEAPONS; index++)
		{
			if (!strcmp(path + 6, stock_weapons[index].key))
				return tag_campaign_level.stock[index];
		}
		return NONE;
	}
	for (index = 0; index < count; index++)
	{
		if (roots[index] != NONE && port_tag_profiles_tag_group(profile, root_tags[index]) == group &&
			names_match(port_tag_profiles_tag_name(profile, root_tags[index]), path))
			return roots[index];
	}
	return NONE;
}

static void choice_add(struct choices *choices, long tag)
{
	short index;

	if (tag == NONE || choices->count >= MAXIMUM_CHOICES)
		return;
	for (index = 0; index < choices->count; index++)
	{
		if (choices->tags[index] == tag)
			return;
	}
	choices->tags[choices->count++] = tag;
}

/* a table's choices for a key, read from the campaign profile; with ADD,
the level's own among them when any are */
static void choices_read(struct choices *choices, long campaign_profile, char const *section, char const *key,
	long profile, long const *roots, long const *root_tags, long count, unsigned long group, boolean add)
{
	long entries = port_tag_campaign_profiles_list_count(campaign_profile, section, key);
	long entry;

	choices->count = 0;
	for (entry = 0; entry < entries; entry++)
	{
		char const *path = port_tag_campaign_profiles_list(campaign_profile, section, key, entry);
		long tag = choice_tag(path, profile, roots, root_tags, count, group);

		if (tag == NONE)
			error(_error_silent, "tag campaign: [%s] %s's %s is not in the level", section, key, path);
		choice_add(choices, tag);
	}
	if (add && choices->count)
		choice_add(choices, CHOICE_ORIGINAL);
}

/* one of a choice's tags at random (ORIGINAL the level's own) */
static long choice_pick(struct choices const *choices, long original, unsigned long seed)
{
	long tag;

	if (!choices->count)
		return original;
	tag = choices->tags[seed % (unsigned long)choices->count];
	return tag == CHOICE_ORIGINAL ? original : tag;
}

/* an enemy's slot's rank: its rank, of a kind that has it; RANK_ANY of a
kind without ranks; NONE for none of its kind's */
static short slot_rank(short enemy, short rank)
{
	if (enemy < 0 || enemy >= TAG_CAMPAIGN_STOCK_ENEMIES)
		return NONE;
	if (!stock_enemies[enemy].ranks)
		return 0;
	return rank > 0 && rank < TAG_CAMPAIGN_RANKS && (stock_enemies[enemy].ranks & RANK_BIT(rank)) ? rank : NONE;
}

/* those of a choice's tags (ORIGINAL the actor's own) its unit can hold */
static short holdable(struct choices const *choices, long weapon, long unit_index, long *held)
{
	short count = 0;
	short index;

	for (index = 0; index < choices->count; index++)
	{
		long tag = choices->tags[index] == CHOICE_ORIGINAL ? weapon : choices->tags[index];

		if (tag == weapon || unit_can_hold_weapon_definition(unit_index, tag))
			held[count++] = tag;
	}
	return count;
}

/* RANDOM's: one of the profile's tags, and with ADD the level's own */
static long random_pick(long const *tags, short count, boolean add, long original, unsigned long seed)
{
	unsigned long choices = (unsigned long)count + (add ? 1 : 0);
	unsigned long choice;

	if (!count)
		return original;
	choice = seed % choices;
	return choice == (unsigned long)count ? original : tags[choice];
}

/* ---------- public code */

char const *tag_campaign_weapon_key(
	short weapon)
{
	return weapon >= 0 && weapon < TAG_CAMPAIGN_STOCK_WEAPONS ? stock_weapons[weapon].key : "";
}

char const *tag_campaign_weapon_name(
	short weapon)
{
	return weapon >= 0 && weapon < TAG_CAMPAIGN_STOCK_WEAPONS ? stock_weapons[weapon].name : "";
}

char const *tag_campaign_weapon_tag(
	short weapon)
{
	return weapon >= 0 && weapon < TAG_CAMPAIGN_STOCK_WEAPONS ? stock_weapons[weapon].tag : "";
}

char const *tag_campaign_enemy_key(
	short enemy)
{
	return enemy >= 0 && enemy < TAG_CAMPAIGN_STOCK_ENEMIES ? stock_enemies[enemy].key : "";
}

char const *tag_campaign_enemy_name(
	short enemy)
{
	return enemy >= 0 && enemy < TAG_CAMPAIGN_STOCK_ENEMIES ? stock_enemies[enemy].name : "";
}

short tag_campaign_enemy_rank_count(
	short enemy)
{
	short count = 0;
	short rank;

	for (rank = 1; enemy >= 0 && enemy < TAG_CAMPAIGN_STOCK_ENEMIES && rank < TAG_CAMPAIGN_RANKS; rank++)
		count += (stock_enemies[enemy].ranks & RANK_BIT(rank)) != 0;
	return count;
}

short tag_campaign_enemy_rank(
	short enemy,
	short index)
{
	short rank;

	for (rank = 1; enemy >= 0 && enemy < TAG_CAMPAIGN_STOCK_ENEMIES && rank < TAG_CAMPAIGN_RANKS; rank++)
	{
		if ((stock_enemies[enemy].ranks & RANK_BIT(rank)) && !index--)
			return rank;
	}
	return 0;
}

char const *tag_campaign_rank_name(
	short rank)
{
	return rank >= 0 && rank < TAG_CAMPAIGN_RANKS ? ranks[rank].name : "";
}

boolean tag_campaign_enemy_armed(
	short enemy)
{
	return enemy >= 0 && enemy < TAG_CAMPAIGN_STOCK_ENEMIES && stock_enemies[enemy].armed;
}

void tag_campaign_slot_key(
	short enemy,
	short rank,
	char *key,
	long key_size)
{
	if (slot_rank(enemy, rank) > 0)
		snprintf(key, (size_t)key_size, "%s_%s", stock_enemies[enemy].key, ranks[rank].key);
	else
		snprintf(key, (size_t)key_size, "%s", tag_campaign_enemy_key(enemy));
}

void tag_campaign_slot_name(
	short enemy,
	short rank,
	char *name,
	long name_size)
{
	if (slot_rank(enemy, rank) > 0)
		snprintf(name, (size_t)name_size, "%s %s", stock_enemies[enemy].name, ranks[rank].name);
	else
		snprintf(name, (size_t)name_size, "%s", tag_campaign_enemy_name(enemy));
}

long tag_campaign_profile_chosen(
	void)
{
	char const *file = config_string("game.campaign_tag_profile");

	if (!config_boolean("game.tag_injection") || !file || !file[0])
		return NONE;
	port_tag_campaign_profiles_scan();
	return port_tag_campaign_profiles_find(file);
}

void tag_campaign_profile_choose(
	long profile)
{
	char const *file = profile == NONE ? "" : port_tag_campaign_profiles_file(profile);

	char const *played = config_string("game.campaign_tag_profile");

	if (played && !strcmp(played, file))
		return;
	if (!config_write("game.campaign_tag_profile", file))
		error(_error_silent, "tag campaign: could not write game.campaign_tag_profile to config.toml");
	error(_error_silent, "tag campaign: the campaign profile played: %s",
		profile == NONE ? "none" : port_tag_campaign_profiles_name(profile));
}

long tag_campaign_level_profile(
	void)
{
	long campaign_profile = tag_campaign_profile_chosen();
	long profile;

	tag_campaign_level.file[0] = 0;
	if (campaign_profile == NONE)
		return NONE;
	port_tag_profiles_scan();
	profile = port_tag_profiles_find(port_tag_campaign_profiles_option(campaign_profile, "profile"));
	if (profile == NONE || tag_injector_profile_enabled_count(profile, 0) <= 0)
	{
		error(_error_silent, "tag campaign: %s's profile (%s) is not there, or has no tags on: not played",
			port_tag_campaign_profiles_name(campaign_profile),
			port_tag_campaign_profiles_option(campaign_profile, "profile"));
		return NONE;
	}
	snprintf(tag_campaign_level.file, sizeof(tag_campaign_level.file), "%s",
		port_tag_campaign_profiles_file(campaign_profile));
	return profile;
}

void tag_campaign_level_loaded(
	long profile,
	long const *roots,
	long const *root_tags,
	long count)
{
	static char const *const rotations[] = { "add", "only" };
	static char const *const replaces[] = { "random", "manual" };
	long campaign_profile = port_tag_campaign_profiles_find(tag_campaign_level.file);
	char file[sizeof(tag_campaign_level.file)];
	boolean add;
	long index;
	short entry;

	snprintf(file, sizeof(file), "%s", tag_campaign_level.file);
	tag_campaign_level_unloaded();
	if (profile == NONE || campaign_profile == NONE)
		return;
	snprintf(tag_campaign_level.file, sizeof(tag_campaign_level.file), "%s", file);
	tag_campaign_level.active = TRUE;
	tag_campaign_level.weapon_rotation = choice_of(port_tag_campaign_profiles_option(campaign_profile,
		"weapon_rotation"), rotations, NUMBEROF(rotations));
	tag_campaign_level.weapon_replace = choice_of(port_tag_campaign_profiles_option(campaign_profile,
		"weapon_replace"), replaces, NUMBEROF(replaces));
	tag_campaign_level.enemy_rotation = choice_of(port_tag_campaign_profiles_option(campaign_profile,
		"enemy_rotation"), rotations, NUMBEROF(rotations));
	tag_campaign_level.enemy_replace = choice_of(port_tag_campaign_profiles_option(campaign_profile,
		"enemy_replace"), replaces, NUMBEROF(replaces));

	/* the campaign's weapons the level has, and the profile's tags added */
	for (entry = 0; entry < TAG_CAMPAIGN_STOCK_WEAPONS; entry++)
		tag_campaign_level.stock[entry] = tag_loaded(WEAPON_DEFINITION_TAG, stock_weapons[entry].tag);
	for (index = 0; index < count; index++)
	{
		unsigned long group;

		if (roots[index] == NONE)
			continue;
		group = port_tag_profiles_tag_group(profile, root_tags[index]);
		if (group == 'weap' && tag_campaign_level.weapon_count < MAXIMUM_PROFILE_ROOTS)
			tag_campaign_level.weapons[tag_campaign_level.weapon_count++] = roots[index];
		else if (group == 'actv' && tag_campaign_level.ai_count < MAXIMUM_PROFILE_ROOTS)
			tag_campaign_level.ai[tag_campaign_level.ai_count++] = roots[index];
	}

	/* MANUAL's, and each slot's loadout */
	add = tag_campaign_level.weapon_rotation == _rotation_add;
	for (entry = 0; tag_campaign_level.weapon_replace == _replace_manual && entry < TAG_CAMPAIGN_STOCK_WEAPONS; entry++)
	{
		choices_read(&tag_campaign_level.weapon_manual[entry], campaign_profile, "weapons", stock_weapons[entry].key,
			profile, roots, root_tags, count, 'weap', add);
	}
	add = tag_campaign_level.enemy_rotation == _rotation_add;
	for (entry = 0; entry < TAG_CAMPAIGN_STOCK_ENEMIES; entry++)
	{
		short rank;

		for (rank = 0; rank < TAG_CAMPAIGN_RANKS; rank++)
		{
			char key[64];

			if (slot_rank(entry, rank) != rank)
				continue;
			tag_campaign_slot_key(entry, rank, key, sizeof(key));
			if (tag_campaign_level.enemy_replace == _replace_manual)
				choices_read(&tag_campaign_level.enemy_manual[entry][rank], campaign_profile, "enemies", key,
					profile, roots, root_tags, count, 'actv', add);
			if (stock_enemies[entry].armed)
				choices_read(&tag_campaign_level.loadouts[entry][rank], campaign_profile, "loadout", key, profile,
					roots, root_tags, count, 'weap', FALSE);
		}
	}
	error(_error_silent, "tag campaign: %s plays %s: %d weapons (rotation %s, replace %s), %d AI (rotation %s, "
		"replace %s)", port_tag_campaign_profiles_name(campaign_profile), port_tag_profiles_name(profile),
		tag_campaign_level.weapon_count, rotations[tag_campaign_level.weapon_rotation],
		replaces[tag_campaign_level.weapon_replace], tag_campaign_level.ai_count,
		rotations[tag_campaign_level.enemy_rotation], replaces[tag_campaign_level.enemy_replace]);
}

void tag_campaign_level_unloaded(
	void)
{
	csmemset(&tag_campaign_level, 0, sizeof(tag_campaign_level));
}

boolean tag_campaign_level_playing(
	void)
{
	return tag_campaign_level.active;
}

long tag_campaign_actor_variant(
	long original,
	short enemy,
	short rank,
	boolean ally,
	unsigned long seed)
{
	if (!tag_campaign_level.active || original == NONE)
		return original;
	if (tag_campaign_level.enemy_replace == _replace_manual)
	{
		rank = slot_rank(enemy, rank);
		return rank != NONE ? choice_pick(&tag_campaign_level.enemy_manual[enemy][rank], original, seed) : original;
	}
	/* (RANDOM leaves the player's allies as they are: MANUAL's MARINE
	swaps them) */
	if (ally)
		return original;
	return random_pick(tag_campaign_level.ai, tag_campaign_level.ai_count,
		tag_campaign_level.enemy_rotation == _rotation_add, original, seed);
}

long tag_campaign_pickup(
	long weapon,
	unsigned long seed)
{
	short entry;

	if (!tag_campaign_level.active || weapon == NONE)
		return weapon;
	if (tag_campaign_level.weapon_replace == _replace_random)
	{
		return random_pick(tag_campaign_level.weapons, tag_campaign_level.weapon_count,
			tag_campaign_level.weapon_rotation == _rotation_add, weapon, seed);
	}
	for (entry = 0; entry < TAG_CAMPAIGN_STOCK_WEAPONS; entry++)
	{
		if (tag_campaign_level.stock[entry] == weapon)
			return choice_pick(&tag_campaign_level.weapon_manual[entry], weapon, seed);
	}
	return weapon;
}

long tag_campaign_enemy_weapon(
	short enemy,
	short rank,
	long weapon,
	long unit_index,
	unsigned long seed)
{
	struct choices swaps;
	long held[MAXIMUM_PROFILE_ROOTS + 1];
	short held_count;
	short entry;

	if (!tag_campaign_level.active)
		return weapon;
	/* (its slot's loadout: an actor without a weapon of its own too, as an
	AI whose weapon could not be added is) */
	rank = slot_rank(enemy, rank);
	if (rank != NONE && tag_campaign_level.loadouts[enemy][rank].count)
	{
		held_count = holdable(&tag_campaign_level.loadouts[enemy][rank], weapon, unit_index, held);
		return held_count ? held[seed % (unsigned long)held_count] : weapon;
	}
	/* (else its weapon swapped as one lying in the level is, among those its
	unit can hold) */
	if (weapon == NONE)
		return NONE;
	swaps.count = 0;
	if (tag_campaign_level.weapon_replace == _replace_random)
	{
		for (entry = 0; entry < tag_campaign_level.weapon_count && swaps.count < MAXIMUM_CHOICES; entry++)
			choice_add(&swaps, tag_campaign_level.weapons[entry]);
		if (swaps.count && tag_campaign_level.weapon_rotation == _rotation_add)
			choice_add(&swaps, CHOICE_ORIGINAL);
	}
	else
	{
		for (entry = 0; entry < TAG_CAMPAIGN_STOCK_WEAPONS; entry++)
		{
			if (tag_campaign_level.stock[entry] == weapon)
				swaps = tag_campaign_level.weapon_manual[entry];
		}
	}
	held_count = holdable(&swaps, weapon, unit_index, held);
	return held_count ? held[seed % (unsigned long)held_count] : weapon;
}
