/*
TAG_CAMPAIGN.C

The Tag Injector in the campaign (tag_campaign.h; docs/tag_injector.md,
"Campaign profiles"). A campaign level played alone (not a network game,
nor a challenge) can play a campaign profile (the TAG INJECTOR PROFILE
screen before the level: game.campaign_tag_profile), which names a profile
of the Tag Injector's and how the level plays its weapons and AI
(port_tag_mp_profiles.c's .campaign). It plays them in the randomizer's
place: randomizer.c adds the profile's tags to the level as it would its own
(tag_injector_tags_load), and asks here what each placement is.

- Weapons. Each weapon lying in the level (not one a script names) is
  swapped as WEAPON ROTATION and WEAPON REPLACEMENT say: RANDOM, one of the
  profile's weapons at random, and with ADD the level's own among them;
  MANUAL, the weapons chosen for each of the campaign's (ORIGINAL among
  them), and with ADD the level's own too. One with none chosen stays as it
  is.
- Enemies. Each actor the level places (not one a script commands, nor one
  a conversation names) is swapped as ENEMY ROTATION and ENEMY REPLACEMENT
  say, as weapons are: RANDOM among the profile's AI (the player's allies
  are left as they are), MANUAL the AI chosen for each of the campaign's
  enemies (by its unit's type, randomizer.c's: a grunt, an elite, a
  marine...). randomizer.c places a squad's infection forms swapped for
  other AI only a third of the time, as its own pools do.
- Loadouts. Each of the profile's AI the level places holds one of the
  weapons chosen for it (ORIGINAL its own, the profile's weapons, the
  campaign's when the level has them) that its unit can hold, at random;
  none chosen, or none it can hold, its own.

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

/* (the pickups first: TAG_CAMPAIGN_PICKUP_WEAPONS) */
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

/* (randomizer.c's TYPE_* order) */
static struct
{
	char const *key;
	char const *name;
} const stock_enemies[TAG_CAMPAIGN_STOCK_ENEMIES] =
{
	{ "grunt", "GRUNT" },
	{ "elite", "ELITE" },
	{ "jackal", "JACKAL" },
	{ "hunter", "HUNTER" },
	{ "flood_combat", "COMBAT FORM" },
	{ "flood_carrier", "CARRIER FORM" },
	{ "marine", "MARINE" },
	{ "flood_infection", "INFECTION FORM" },
	{ "engineer", "ENGINEER" },
	{ "sentinel", "SENTINEL" },
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
	/* MANUAL's choices for each of the campaign's weapons and enemies, and
	each AI's loadout */
	struct choices weapon_manual[TAG_CAMPAIGN_PICKUP_WEAPONS];
	struct choices enemy_manual[TAG_CAMPAIGN_STOCK_ENEMIES];
	struct choices loadouts[MAXIMUM_PROFILE_ROOTS];
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
		{
			short ai = tag_campaign_level.ai_count++;

			tag_campaign_level.ai[ai] = roots[index];
			choices_read(&tag_campaign_level.loadouts[ai], campaign_profile, "loadout",
				port_tag_profiles_tag_name(profile, root_tags[index]), profile, roots, root_tags, count, 'weap',
				FALSE);
		}
	}

	/* MANUAL's */
	add = tag_campaign_level.weapon_rotation == _rotation_add;
	for (entry = 0; tag_campaign_level.weapon_replace == _replace_manual && entry < TAG_CAMPAIGN_PICKUP_WEAPONS; entry++)
	{
		choices_read(&tag_campaign_level.weapon_manual[entry], campaign_profile, "weapons", stock_weapons[entry].key,
			profile, roots, root_tags, count, 'weap', add);
	}
	add = tag_campaign_level.enemy_rotation == _rotation_add;
	for (entry = 0; tag_campaign_level.enemy_replace == _replace_manual && entry < TAG_CAMPAIGN_STOCK_ENEMIES; entry++)
	{
		choices_read(&tag_campaign_level.enemy_manual[entry], campaign_profile, "enemies", stock_enemies[entry].key,
			profile, roots, root_tags, count, 'actv', add);
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
	boolean ally,
	unsigned long seed)
{
	if (!tag_campaign_level.active || original == NONE)
		return original;
	if (tag_campaign_level.enemy_replace == _replace_manual)
	{
		return enemy >= 0 && enemy < TAG_CAMPAIGN_STOCK_ENEMIES ?
			choice_pick(&tag_campaign_level.enemy_manual[enemy], original, seed) : original;
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
	for (entry = 0; entry < TAG_CAMPAIGN_PICKUP_WEAPONS; entry++)
	{
		if (tag_campaign_level.stock[entry] == weapon)
			return choice_pick(&tag_campaign_level.weapon_manual[entry], weapon, seed);
	}
	return weapon;
}

long tag_campaign_enemy_weapon(
	long variant,
	long weapon,
	long unit_index,
	unsigned long seed)
{
	long held[MAXIMUM_CHOICES];
	short held_count = 0;
	short ai;
	short index;

	if (!tag_campaign_level.active || variant == NONE)
		return weapon;
	for (ai = 0; ai < tag_campaign_level.ai_count && tag_campaign_level.ai[ai] != variant; ai++)
	{
	}
	if (ai == tag_campaign_level.ai_count || !tag_campaign_level.loadouts[ai].count)
		return weapon;
	/* (those its unit has the animations to hold: its own it holds as the
	level has it) */
	for (index = 0; index < tag_campaign_level.loadouts[ai].count; index++)
	{
		long tag = tag_campaign_level.loadouts[ai].tags[index];

		if (tag == CHOICE_ORIGINAL)
			held[held_count++] = weapon;
		else if (unit_can_hold_weapon_definition(unit_index, tag))
			held[held_count++] = tag;
	}
	return held_count ? held[seed % (unsigned long)held_count] : weapon;
}
