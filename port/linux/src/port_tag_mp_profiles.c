/*
PORT_TAG_MP_PROFILES.C

The Tag Injector's multiplayer and campaign profiles (port_tag_mp_profiles.h):
how a multiplayer game on one of the game's own multiplayer maps, or a
campaign level played alone, plays the tags of a profile (port_tag_profiles.c),
each a file of the data root's tag_injector/multiplayer or
tag_injector/campaign folder (folders no profile scan lists, and no profile
is made in: port_tag_mp_profiles_folder_reserved):

	tag_injector/
	  multiplayer/
	    Shotgun Madness.toml
	  campaign/
	    Brute Invasion.toml

An earlier build kept them in .multiplayer and .campaign, which a player
copying the folder took for missing: such a folder is moved to its new name
when it is read, if that one is not there yet.

	name = "Shotgun Madness"
	profile = "Brutes"
	rotation = "add"
	replace = "manual"
	loadout = "set"
	primary = "cmt\\weapons\\covenant\\carbine\\carbine"
	secondary = "stock:pistol"

	[manual]
	shotgun = ["original", "cmt\\weapons\\covenant\\carbine\\carbine"]

	name = "Brute Invasion"
	profile = "Brutes"
	weapon_rotation = "add"
	weapon_replace = "manual"
	enemy_rotation = "only"
	enemy_replace = "random"

	[weapons]
	plasma_rifle = ["original", "cmt\\weapons\\covenant\\carbine\\carbine"]

	[enemies]
	elite = ["cmt\\characters\\brute\\brute major"]

	[loadout]
	"cmt\\characters\\brute\\brute major" = ["original", "stock:needler"]

Their settings (port/linux/game/tag_multiplayer.c and tag_campaign.c say
what each does) are strings, any of them left out ""; each table (a
multiplayer profile's [manual], a campaign profile's [weapons], [enemies]
and [loadout]) has, for each of its keys (one of the stock weapons or
enemies: "shotgun", "elite"; [loadout]'s an actor variant's path), the tags
chosen for it, by their paths in the profile ("original": the level's own;
"stock:" and a stock weapon's key). A setting's value is kept as it was
given, but for its control characters.
*/

#include "platform.h"
#include "port_config.h"
#include "port_tag_mp_profiles.h"
#include "posix.h"
#include "tomlc17.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------- constants */

#define MAXIMUM_MP_PROFILES 64
#define MAXIMUM_MANUAL_ENTRIES 1024
#define MAXIMUM_FILE_LENGTH 64
#define MAXIMUM_NAME_LENGTH 64
#define MAXIMUM_OPTIONS 8
#define MAXIMUM_VALUE_LENGTH 256
#define MAXIMUM_PATH_LENGTH 1024
#define TOML_CHARACTER_LENGTH 6
#define MP_EXTENSION ".toml"

/* ---------- structures */

struct manual_entry
{
	/* its table (the kind's sections') */
	short section;
	char key[MAXIMUM_VALUE_LENGTH];
	char path[MAXIMUM_VALUE_LENGTH];
};

struct mp_profile
{
	/* its file's name, without .toml */
	char file[MAXIMUM_FILE_LENGTH];
	char name[MAXIMUM_NAME_LENGTH];
	char options[MAXIMUM_OPTIONS][MAXIMUM_VALUE_LENGTH];
	struct manual_entry *manual;
	long manual_count;
};

/* a kind of profile: its folder (and an earlier build's), what the log calls it, its settings (and
the defaults a new one is made with: key, value, ..., NULL), its tables, and
the profiles of it last found */
struct profile_kind
{
	char const *folder;
	char const *old_folder;
	char const *what;
	char const *const *option_keys;
	size_t option_count;
	char const *const *defaults;
	char const *const *sections;
	size_t section_count;
	struct mp_profile profiles[MAXIMUM_MP_PROFILES];
	long count;
	unsigned long scans;
};

/* ---------- globals */

static char const *const mp_option_keys[] = { "profile", "rotation", "replace", "loadout", "primary", "secondary" };
static char const *const mp_defaults[] = { "rotation", "add", "replace", "random", "loadout", "map", NULL };
static char const *const mp_sections[] = { "manual" };
static char const *const campaign_option_keys[] =
	{ "profile", "weapon_rotation", "weapon_replace", "enemy_rotation", "enemy_replace" };
static char const *const campaign_defaults[] =
	{ "weapon_rotation", "add", "weapon_replace", "random", "enemy_rotation", "add", "enemy_replace", "random", NULL };
static char const *const campaign_sections[] = { "weapons", "enemies", "loadout" };

#define NUMBEROF_KEYS(keys) (sizeof(keys) / sizeof((keys)[0]))

static struct profile_kind mp_kind =
{
	"multiplayer", ".multiplayer", "multiplayer", mp_option_keys, NUMBEROF_KEYS(mp_option_keys), mp_defaults, mp_sections,
	NUMBEROF_KEYS(mp_sections)
};
static struct profile_kind campaign_kind =
{
	"campaign", ".campaign", "campaign", campaign_option_keys, NUMBEROF_KEYS(campaign_option_keys), campaign_defaults,
	campaign_sections, NUMBEROF_KEYS(campaign_sections)
};

typedef char option_count_assert[NUMBEROF_KEYS(mp_option_keys) <= MAXIMUM_OPTIONS &&
	NUMBEROF_KEYS(campaign_option_keys) <= MAXIMUM_OPTIONS ? 1 : -1];

/* ---------- private code */

static char lower(char character)
{
	return character >= 'A' && character <= 'Z' ? (char)(character - 'A' + 'a') : character;
}

static int names_compare(const char *first, const char *second)
{
	for (; *first && lower(*first) == lower(*second); first++, second++)
	{
	}
	return (unsigned char)lower(*first) - (unsigned char)lower(*second);
}

static void folder_path(const struct profile_kind *kind, char *path, size_t size)
{
	snprintf(path, size, "%s/tag_injector/%s", platform_data_root(), kind->folder);
}

/* an earlier build's folder of the kind's profiles moved to the folder's
name, if it is there and the folder is not */
static void folder_move(const struct profile_kind *kind)
{
	char old_path[MAXIMUM_PATH_LENGTH];
	char path[MAXIMUM_PATH_LENGTH];
	struct posix_file_information information;

	snprintf(old_path, sizeof(old_path), "%s/tag_injector/%s", platform_data_root(), kind->old_folder);
	folder_path(kind, path, sizeof(path));
	if (posix_stat(old_path, &information) != 0 || !(information.flags & _posix_file_is_directory) ||
		posix_stat(path, &information) == 0)
		return;
	/* (a file at a time: the web build's storage moves no folder) */
	if (posix_make_directory(path) != 0 && posix_stat(path, &information) != 0)
	{
		platform_log("tag injector: %s cannot be made: the %s profiles of %s are not read", path, kind->what, old_path);
		return;
	}
	{
		void *directory = posix_directory_open(old_path);
		char name[256];
		long moved = 0;
		long left = 0;

		while (directory && posix_directory_next(directory, name, sizeof(name)))
		{
			char from[MAXIMUM_PATH_LENGTH];
			char to[MAXIMUM_PATH_LENGTH];

			if (!strcmp(name, ".") || !strcmp(name, ".."))
				continue;
			snprintf(from, sizeof(from), "%s/%s", old_path, name);
			snprintf(to, sizeof(to), "%s/%s", path, name);
			if (rename(from, to) == 0)
				moved++;
			else
				left++;
		}
		if (directory)
			posix_directory_close(directory);
		if (!left)
			rmdir(old_path);
		platform_log("tag injector: %ld files of the %s profiles moved from %s to %s%s", moved, kind->what, old_path,
			path, left ? " (some could not be: they stay)" : "");
	}
}

static void profile_path(const struct profile_kind *kind, const struct mp_profile *profile, char *path, size_t size)
{
	char folder[MAXIMUM_PATH_LENGTH];

	folder_path(kind, folder, sizeof(folder));
	snprintf(path, size, "%s/%s%s", folder, profile->file, MP_EXTENSION);
}

static void text_copy(char *destination, size_t size, const char *source)
{
	snprintf(destination, size, "%s", source ? source : "");
}

/* a value as it is kept: without its control characters */
static void value_copy(char *destination, size_t size, const char *source)
{
	size_t length = 0;

	for (; source && *source && length + 1 < size; source++)
	{
		if ((unsigned char)*source >= 0x20 && *source != 0x7F)
			destination[length++] = *source;
	}
	destination[length] = 0;
}

/* a name made a file's: its letters, digits, spaces, - and _, without
spaces at either end; 0 if none is left */
static int file_name_clean(char *name)
{
	char *read = name;
	char *write = name;

	while (*read == ' ')
		read++;
	for (; *read; read++)
	{
		char character = *read;

		if ((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
			(character >= '0' && character <= '9') || character == ' ' || character == '_' || character == '-')
			*write++ = character;
	}
	while (write > name && write[-1] == ' ')
		write--;
	*write = 0;
	return write != name;
}

static void toml_string_append(char *text, size_t size, const char *value)
{
	size_t length = strlen(text);

	for (; *value && length + TOML_CHARACTER_LENGTH < size; value++)
	{
		unsigned char character = (unsigned char)*value;

		if (character < 0x20 || character == 0x7F)
			continue;
		if (character == '"' || character == '\\')
			text[length++] = '\\';
		text[length++] = (char)character;
	}
	text[length] = 0;
}

static int option_index(const struct profile_kind *kind, const char *key)
{
	size_t index;

	for (index = 0; key && index < kind->option_count; index++)
	{
		if (!strcmp(kind->option_keys[index], key))
			return (int)index;
	}
	return -1;
}

static int section_index(const struct profile_kind *kind, const char *section)
{
	size_t index;

	for (index = 0; section && index < kind->section_count; index++)
	{
		if (!strcmp(kind->sections[index], section))
			return (int)index;
	}
	return -1;
}

/* whether a key is written bare: lowercase letters, digits and _ (else
quoted, as an actor variant's path is) */
static int key_bare(const char *key)
{
	for (; *key; key++)
	{
		if (!((*key >= 'a' && *key <= 'z') || (*key >= '0' && *key <= '9') || *key == '_'))
			return 0;
	}
	return 1;
}

/* whether a table's key is one: something, without control characters */
static int key_valid(const char *key)
{
	size_t length = key ? strlen(key) : 0;
	size_t index;

	if (!length || length >= MAXIMUM_VALUE_LENGTH)
		return 0;
	for (index = 0; index < length; index++)
	{
		if ((unsigned char)key[index] < 0x20 || key[index] == 0x7F)
			return 0;
	}
	return 1;
}

static void profile_dispose(struct mp_profile *profile)
{
	free(profile->manual);
	memset(profile, 0, sizeof(*profile));
}

static int manual_add(struct mp_profile *profile, short section, const char *key, const char *path)
{
	struct manual_entry *grown;

	if (profile->manual_count >= MAXIMUM_MANUAL_ENTRIES || !key_valid(key) || !path || !path[0] ||
		strlen(path) >= MAXIMUM_VALUE_LENGTH)
		return 0;
	grown = realloc(profile->manual, (size_t)(profile->manual_count + 1) * sizeof(*grown));
	if (!grown)
		return 0;
	profile->manual = grown;
	grown[profile->manual_count].section = section;
	value_copy(grown[profile->manual_count].key, sizeof(grown[0].key), key);
	value_copy(grown[profile->manual_count].path, sizeof(grown[0].path), path);
	profile->manual_count++;
	return 1;
}

static int entry_is(const struct manual_entry *entry, short section, const char *key)
{
	return entry->section == section && !names_compare(entry->key, key);
}

/* reads a profile's file (its name without .toml); 0 when it is not one */
static int profile_read(const struct profile_kind *kind, struct mp_profile *profile, const char *folder,
	const char *file)
{
	char path[MAXIMUM_PATH_LENGTH];
	size_t size = 0;
	char *text;
	toml_result_t result;
	toml_datum_t value;
	size_t option;
	size_t section;

	memset(profile, 0, sizeof(*profile));
	text_copy(profile->file, sizeof(profile->file), file);
	text_copy(profile->name, sizeof(profile->name), file);
	snprintf(path, sizeof(path), "%s/%s%s", folder, file, MP_EXTENSION);
	text = config_file_read(path, &size);
	if (!text)
		return 0;
	result = toml_parse(text, (int)size);
	free(text);
	if (!result.ok)
	{
		platform_log("tag injector: %s: %s; it is not a %s profile", path, result.errmsg, kind->what);
		toml_free(result);
		return 0;
	}
	value = toml_get(result.toptab, "name");
	if (value.type == TOML_STRING && value.u.s[0])
		value_copy(profile->name, sizeof(profile->name), value.u.s);
	for (option = 0; option < kind->option_count; option++)
	{
		value = toml_get(result.toptab, kind->option_keys[option]);
		if (value.type == TOML_STRING)
			value_copy(profile->options[option], sizeof(profile->options[option]), value.u.s);
	}
	for (section = 0; section < kind->section_count; section++)
	{
		toml_datum_t table = toml_get(result.toptab, kind->sections[section]);
		int key;

		if (table.type != TOML_TABLE)
			continue;
		for (key = 0; key < table.u.tab.size; key++)
		{
			toml_datum_t entries = table.u.tab.value[key];
			int entry;

			if (entries.type != TOML_ARRAY)
				continue;
			for (entry = 0; entry < entries.u.arr.size; entry++)
			{
				if (entries.u.arr.elem[entry].type == TOML_STRING &&
					!manual_add(profile, (short)section, table.u.tab.key[key], entries.u.arr.elem[entry].u.s))
					platform_log("tag injector: %s: a tag of [%s] is left out", path, kind->sections[section]);
			}
		}
	}
	toml_free(result);
	return 1;
}

static int profile_write(const struct profile_kind *kind, const struct mp_profile *profile)
{
	size_t capacity = 1024 + (kind->option_count + (size_t)profile->manual_count * 2) *
		(MAXIMUM_VALUE_LENGTH * TOML_CHARACTER_LENGTH + 16);
	char folder[MAXIMUM_PATH_LENGTH];
	char path[MAXIMUM_PATH_LENGTH];
	char *text = malloc(capacity);
	size_t option;
	size_t section;
	long index;
	int written;
	FILE *stream;

	if (!text)
		return 0;
	snprintf(text, capacity, "# a Tag Injector %s profile (Settings > Gameplay > Tag Injector)\nname = \"", kind->what);
	toml_string_append(text, capacity, profile->name);
	snprintf(text + strlen(text), capacity - strlen(text), "\"\n");
	for (option = 0; option < kind->option_count; option++)
	{
		snprintf(text + strlen(text), capacity - strlen(text), "%s = \"", kind->option_keys[option]);
		toml_string_append(text, capacity, profile->options[option]);
		snprintf(text + strlen(text), capacity - strlen(text), "\"\n");
	}
	for (section = 0; section < kind->section_count; section++)
	{
		int header = 0;

		/* (each key once, its tags in the order they were added) */
		for (index = 0; index < profile->manual_count; index++)
		{
			const struct manual_entry *entry = &profile->manual[index];
			long other;

			if (entry->section != (short)section)
				continue;
			for (other = 0; other < index && !entry_is(&profile->manual[other], entry->section, entry->key); other++)
			{
			}
			if (other < index)
				continue;
			if (!header)
			{
				snprintf(text + strlen(text), capacity - strlen(text), "\n[%s]\n", kind->sections[section]);
				header = 1;
			}
			if (key_bare(entry->key))
				snprintf(text + strlen(text), capacity - strlen(text), "%s = [", entry->key);
			else
			{
				snprintf(text + strlen(text), capacity - strlen(text), "\"");
				toml_string_append(text, capacity, entry->key);
				snprintf(text + strlen(text), capacity - strlen(text), "\" = [");
			}
			for (other = index; other < profile->manual_count; other++)
			{
				if (!entry_is(&profile->manual[other], entry->section, entry->key))
					continue;
				snprintf(text + strlen(text), capacity - strlen(text), "%s\"", other > index ? ", " : "");
				toml_string_append(text, capacity, profile->manual[other].path);
				snprintf(text + strlen(text), capacity - strlen(text), "\"");
			}
			snprintf(text + strlen(text), capacity - strlen(text), "]\n");
		}
	}
	/* (the tag_injector folder first, which a profile may not have made yet) */
	snprintf(folder, sizeof(folder), "%s/tag_injector", platform_data_root());
	posix_make_directory(folder);
	folder_move(kind);
	folder_path(kind, folder, sizeof(folder));
	posix_make_directory(folder);
	profile_path(kind, profile, path, sizeof(path));
	{
		char temporary[MAXIMUM_PATH_LENGTH + 8];

		snprintf(temporary, sizeof(temporary), "%s.new", path);
		stream = fopen(temporary, "wb");
		written = stream && fwrite(text, 1, strlen(text), stream) == strlen(text);
		if (stream)
			written = fclose(stream) == 0 && written;
		written = written && rename(temporary, path) == 0;
		if (!written)
			remove(temporary);
	}
	if (!written)
		platform_log("tag injector: %s cannot be written", path);
	free(text);
	return written;
}

static struct mp_profile *profile_get(struct profile_kind *kind, long profile)
{
	return profile >= 0 && profile < kind->count ? &kind->profiles[profile] : NULL;
}

static long kind_scan(struct profile_kind *kind)
{
	char folder[MAXIMUM_PATH_LENGTH];
	char name[256];
	void *directory;
	long index;

	for (index = 0; index < kind->count; index++)
		profile_dispose(&kind->profiles[index]);
	kind->count = 0;
	kind->scans++;
	folder_move(kind);
	folder_path(kind, folder, sizeof(folder));
	directory = posix_directory_open(folder);
	if (!directory)
		return 0;
	while (posix_directory_next(directory, name, sizeof(name)))
	{
		size_t length = strlen(name);
		size_t extension = strlen(MP_EXTENSION);
		struct mp_profile profile;

		if (name[0] == '.' || length <= extension || strcmp(name + length - extension, MP_EXTENSION) ||
			length - extension >= MAXIMUM_FILE_LENGTH)
			continue;
		name[length - extension] = 0;
		if (kind->count == MAXIMUM_MP_PROFILES)
		{
			platform_log("tag injector: more than %d %s profiles in %s: %s and those after it are left out",
				MAXIMUM_MP_PROFILES, kind->what, folder, name);
			break;
		}
		if (!profile_read(kind, &profile, folder, name))
			continue;
		for (index = kind->count; index > 0 && names_compare(kind->profiles[index - 1].name, profile.name) > 0; index--)
			kind->profiles[index] = kind->profiles[index - 1];
		kind->profiles[index] = profile;
		kind->count++;
	}
	posix_directory_close(directory);
	return kind->count;
}

static const char *kind_name(struct profile_kind *kind, long profile)
{
	return profile_get(kind, profile) ? profile_get(kind, profile)->name : "";
}

static const char *kind_file(struct profile_kind *kind, long profile)
{
	return profile_get(kind, profile) ? profile_get(kind, profile)->file : "";
}

static long kind_find(struct profile_kind *kind, const char *file)
{
	long index;

	for (index = 0; file && file[0] && index < kind->count; index++)
	{
		if (!names_compare(kind->profiles[index].file, file))
			return index;
	}
	return -1;
}

/* a file's name for a profile of the name `base` that no file has yet
(base, base 2, ...): 0 if there is none */
static int file_choose(struct profile_kind *kind, struct mp_profile *made, const char *base)
{
	int suffix;

	for (suffix = 1; suffix < 1000; suffix++)
	{
		char path[MAXIMUM_PATH_LENGTH];
		struct posix_file_information information;

		if (suffix == 1)
			snprintf(made->file, sizeof(made->file), "%s", base);
		else
			snprintf(made->file, sizeof(made->file), "%s %d", base, suffix);
		profile_path(kind, made, path, sizeof(path));
		if (posix_stat(path, &information) != 0)
			return 1;
	}
	return 0;
}

static long kind_create(struct profile_kind *kind, const char *name, const char *profile)
{
	char base[MAXIMUM_FILE_LENGTH - 8];
	struct mp_profile made;
	const char *const *defaults;

	snprintf(base, sizeof(base), "%s", name ? name : "");
	if (!file_name_clean(base))
	{
		platform_log("tag injector: '%s' has no letter or digit to name a %s profile by", name ? name : "", kind->what);
		return -1;
	}
	memset(&made, 0, sizeof(made));
	value_copy(made.name, sizeof(made.name), name);
	value_copy(made.options[option_index(kind, "profile")], MAXIMUM_VALUE_LENGTH, profile);
	for (defaults = kind->defaults; defaults[0] && defaults[1]; defaults += 2)
		text_copy(made.options[option_index(kind, defaults[0])], MAXIMUM_VALUE_LENGTH, defaults[1]);
	if (!file_choose(kind, &made, base) || !profile_write(kind, &made))
		return -1;
	kind_scan(kind);
	platform_log("tag injector: made the %s profile %s (%s), of %s", kind->what, made.name, made.file,
		profile ? profile : "");
	return kind_find(kind, made.file);
}

static int kind_delete(struct profile_kind *kind, long profile)
{
	struct mp_profile *found = profile_get(kind, profile);
	char path[MAXIMUM_PATH_LENGTH];

	if (!found)
		return 0;
	profile_path(kind, found, path, sizeof(path));
	if (remove(path) != 0)
	{
		platform_log("tag injector: %s cannot be deleted", path);
		return 0;
	}
	platform_log("tag injector: deleted the %s profile %s", kind->what, found->name);
	kind_scan(kind);
	return 1;
}

static int kind_rename(struct profile_kind *kind, long profile, const char *name)
{
	struct mp_profile *found = profile_get(kind, profile);
	char clean[MAXIMUM_NAME_LENGTH];

	if (!found || !name)
		return 0;
	text_copy(clean, sizeof(clean), name);
	if (!file_name_clean(clean))
		return 0;
	value_copy(found->name, sizeof(found->name), name);
	kind->scans++;
	return profile_write(kind, found);
}

static const char *kind_option(struct profile_kind *kind, long profile, const char *key)
{
	int option = option_index(kind, key);

	return profile_get(kind, profile) && option >= 0 ? profile_get(kind, profile)->options[option] : "";
}

static int kind_set_option(struct profile_kind *kind, long profile, const char *key, const char *value)
{
	struct mp_profile *found = profile_get(kind, profile);
	int option = option_index(kind, key);
	char kept[MAXIMUM_VALUE_LENGTH];

	if (!found || option < 0 || (value && strlen(value) >= MAXIMUM_VALUE_LENGTH))
		return 0;
	value_copy(kept, sizeof(kept), value);
	if (!strcmp(found->options[option], kept))
		return 1;
	text_copy(found->options[option], sizeof(found->options[option]), kept);
	kind->scans++;
	platform_log("tag injector: the %s profile %s: %s = \"%s\"", kind->what, found->name, key, kept);
	return profile_write(kind, found);
}

static long kind_list_count(struct profile_kind *kind, long profile, const char *section_name, const char *key)
{
	struct mp_profile *found = profile_get(kind, profile);
	int section = section_index(kind, section_name);
	long count = 0;
	long index;

	for (index = 0; found && section >= 0 && key && index < found->manual_count; index++)
		count += entry_is(&found->manual[index], (short)section, key);
	return count;
}

static const char *kind_list(struct profile_kind *kind, long profile, const char *section_name, const char *key,
	long entry)
{
	struct mp_profile *found = profile_get(kind, profile);
	int section = section_index(kind, section_name);
	long index;

	for (index = 0; found && section >= 0 && key && index < found->manual_count; index++)
	{
		if (!entry_is(&found->manual[index], (short)section, key))
			continue;
		if (!entry--)
			return found->manual[index].path;
	}
	return "";
}

static int kind_list_has(struct profile_kind *kind, long profile, const char *section_name, const char *key,
	const char *path)
{
	struct mp_profile *found = profile_get(kind, profile);
	int section = section_index(kind, section_name);
	long index;

	for (index = 0; found && section >= 0 && key && path && index < found->manual_count; index++)
	{
		if (entry_is(&found->manual[index], (short)section, key) && !names_compare(found->manual[index].path, path))
			return 1;
	}
	return 0;
}

static int kind_list_set(struct profile_kind *kind, long profile, const char *section_name, const char *key,
	const char *path, int on)
{
	struct mp_profile *found = profile_get(kind, profile);
	int section = section_index(kind, section_name);
	long index;

	if (!found || section < 0 || !key_valid(key) || !path || !path[0])
		return 0;
	if ((kind_list_has(kind, profile, section_name, key, path) != 0) == (on != 0))
		return 1;
	if (on)
	{
		if (!manual_add(found, (short)section, key, path))
			return 0;
	}
	else
	{
		for (index = 0; index < found->manual_count; index++)
		{
			if (entry_is(&found->manual[index], (short)section, key) && !names_compare(found->manual[index].path, path))
				break;
		}
		memmove(&found->manual[index], &found->manual[index + 1],
			(size_t)(found->manual_count - index - 1) * sizeof(found->manual[0]));
		found->manual_count--;
	}
	kind->scans++;
	return profile_write(kind, found);
}

/* ---------- public code */

int port_tag_mp_profiles_folder_reserved(const char *folder)
{
	return folder && (!names_compare(folder, mp_kind.folder) || !names_compare(folder, campaign_kind.folder));
}

long port_tag_mp_profiles_scan(void)
{
	return kind_scan(&mp_kind);
}

long port_tag_mp_profiles_count(void)
{
	return mp_kind.count;
}

unsigned long port_tag_mp_profiles_scans(void)
{
	return mp_kind.scans;
}

const char *port_tag_mp_profiles_name(long profile)
{
	return kind_name(&mp_kind, profile);
}

const char *port_tag_mp_profiles_file(long profile)
{
	return kind_file(&mp_kind, profile);
}

long port_tag_mp_profiles_find(const char *file)
{
	return kind_find(&mp_kind, file);
}

long port_tag_mp_profiles_create(const char *name, const char *profile)
{
	return kind_create(&mp_kind, name, profile);
}

int port_tag_mp_profiles_delete(long profile)
{
	return kind_delete(&mp_kind, profile);
}

int port_tag_mp_profiles_rename(long profile, const char *name)
{
	return kind_rename(&mp_kind, profile, name);
}

const char *port_tag_mp_profiles_option(long profile, const char *key)
{
	return kind_option(&mp_kind, profile, key);
}

int port_tag_mp_profiles_set_option(long profile, const char *key, const char *value)
{
	return kind_set_option(&mp_kind, profile, key, value);
}

long port_tag_mp_profiles_manual_count(long profile, const char *key)
{
	return kind_list_count(&mp_kind, profile, "manual", key);
}

const char *port_tag_mp_profiles_manual(long profile, const char *key, long entry)
{
	return kind_list(&mp_kind, profile, "manual", key, entry);
}

int port_tag_mp_profiles_manual_has(long profile, const char *key, const char *path)
{
	return kind_list_has(&mp_kind, profile, "manual", key, path);
}

int port_tag_mp_profiles_manual_set(long profile, const char *key, const char *path, int on)
{
	return kind_list_set(&mp_kind, profile, "manual", key, path, on);
}

void port_tag_mp_profiles_path(long profile, char *path, unsigned long size)
{
	if (profile_get(&mp_kind, profile))
		profile_path(&mp_kind, profile_get(&mp_kind, profile), path, (size_t)size);
	else if (size)
		path[0] = 0;
}

/* a copy of a profile's file of a share (port_tag_mp_profiles_import's,
port_tag_campaign_profiles_import's) */
static long kind_import(struct profile_kind *kind, const char *path, const char *suffix, const char *profile)
{
	char folder[MAXIMUM_PATH_LENGTH];
	char file[MAXIMUM_FILE_LENGTH];
	char base[MAXIMUM_FILE_LENGTH - 8];
	const char *slash = path ? strrchr(path, '/') : NULL;
	size_t length;
	struct mp_profile read;
	char name[MAXIMUM_NAME_LENGTH];

	/* (read as a profile of its folder is: its name without .toml) */
	if (!slash || (size_t)(slash - path) >= sizeof(folder))
		return -1;
	snprintf(folder, sizeof(folder), "%.*s", (int)(slash - path), path);
	length = strlen(slash + 1);
	if (length <= strlen(MP_EXTENSION) || length - strlen(MP_EXTENSION) >= sizeof(file) ||
		strcmp(slash + 1 + length - strlen(MP_EXTENSION), MP_EXTENSION))
		return -1;
	snprintf(file, sizeof(file), "%.*s", (int)(length - strlen(MP_EXTENSION)), slash + 1);
	if (!profile_read(kind, &read, folder, file))
		return -1;
	/* (its name the host's and the suffix, its file a new one, playing the
	profile given) */
	snprintf(name, sizeof(name), "%.*s%s", (int)(sizeof(name) - strlen(suffix ? suffix : "") - 1), read.name,
		suffix ? suffix : "");
	value_copy(read.name, sizeof(read.name), name);
	value_copy(read.options[option_index(kind, "profile")], MAXIMUM_VALUE_LENGTH, profile);
	snprintf(base, sizeof(base), "%s", name);
	if (!file_name_clean(base))
		snprintf(base, sizeof(base), "online");
	if (!file_choose(kind, &read, base) || !profile_write(kind, &read))
	{
		profile_dispose(&read);
		return -1;
	}
	snprintf(file, sizeof(file), "%s", read.file);
	profile_dispose(&read);
	kind_scan(kind);
	platform_log("tag injector: copied the %s profile %s, playing %s", kind->what, name, profile);
	return kind_find(kind, file);
}

long port_tag_mp_profiles_import(const char *path, const char *suffix, const char *profile)
{
	return kind_import(&mp_kind, path, suffix, profile);
}

long port_tag_campaign_profiles_scan(void)
{
	return kind_scan(&campaign_kind);
}

long port_tag_campaign_profiles_count(void)
{
	return campaign_kind.count;
}

unsigned long port_tag_campaign_profiles_scans(void)
{
	return campaign_kind.scans;
}

const char *port_tag_campaign_profiles_name(long profile)
{
	return kind_name(&campaign_kind, profile);
}

const char *port_tag_campaign_profiles_file(long profile)
{
	return kind_file(&campaign_kind, profile);
}

long port_tag_campaign_profiles_find(const char *file)
{
	return kind_find(&campaign_kind, file);
}

long port_tag_campaign_profiles_create(const char *name, const char *profile)
{
	return kind_create(&campaign_kind, name, profile);
}

int port_tag_campaign_profiles_delete(long profile)
{
	return kind_delete(&campaign_kind, profile);
}

int port_tag_campaign_profiles_rename(long profile, const char *name)
{
	return kind_rename(&campaign_kind, profile, name);
}

const char *port_tag_campaign_profiles_option(long profile, const char *key)
{
	return kind_option(&campaign_kind, profile, key);
}

int port_tag_campaign_profiles_set_option(long profile, const char *key, const char *value)
{
	return kind_set_option(&campaign_kind, profile, key, value);
}

long port_tag_campaign_profiles_list_count(long profile, const char *section, const char *key)
{
	return kind_list_count(&campaign_kind, profile, section, key);
}

const char *port_tag_campaign_profiles_list(long profile, const char *section, const char *key, long entry)
{
	return kind_list(&campaign_kind, profile, section, key, entry);
}

int port_tag_campaign_profiles_list_has(long profile, const char *section, const char *key, const char *path)
{
	return kind_list_has(&campaign_kind, profile, section, key, path);
}

int port_tag_campaign_profiles_list_set(long profile, const char *section, const char *key, const char *path, int on)
{
	return kind_list_set(&campaign_kind, profile, section, key, path, on);
}

void port_tag_campaign_profiles_path(long profile, char *path, unsigned long size)
{
	if (profile_get(&campaign_kind, profile))
		profile_path(&campaign_kind, profile_get(&campaign_kind, profile), path, (size_t)size);
	else if (size)
		path[0] = 0;
}

long port_tag_campaign_profiles_import(const char *path, const char *suffix, const char *profile)
{
	return kind_import(&campaign_kind, path, suffix, profile);
}
