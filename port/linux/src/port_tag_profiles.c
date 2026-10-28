/*
PORT_TAG_PROFILES.C

The tag injector's profiles (port_tag_profiles.h): the folders of the data
root's tag_injector folder, each a profile, which holds a profile.toml and
the tag bundles of its tags (port/linux/game/tag_bundle.h), one a file:

	tag_injector/
	  Brutes/
	    profile.toml
	    carbine.weap.tagbundle
	    brute major carbine.actv.tagbundle

	name = "Brutes"
	format = 1

	[[tags]]
	map = "firefight-fissurefall"
	group = "weap"
	tag = "cmt\\weapons\\covenant\\carbine\\carbine"
	file = "carbine.weap.tagbundle"
	enabled = true
	faction = "covenant"
	class = "automatic"
	custom_type = "Carbine"
	hud = "auto"
	replace = true
	name = "Brute Carbine"
	pickup = "text"
	pickup_text = "Brute Carbine"

A tag's enabled, and its settings (port_tag_profiles.h's
port_tag_profiles_tag_option), may be left out: it is on, and its settings
"" (the randomizer finds them by its tag's name). A build before them reads
the profile as it did, without them. A tag's name is the one the menus show
it by, which the player gave it; its path (tag) stays its map's, which its
bundle has and a level knows it by.

A tag added is kept as port_tag_store.c keeps it: its tags in a tag file
(carbine.weap.tagref), and its pixels, samples and vertices in the
profile's pack (tags.pack, tags.index), each once whichever tags have them,
compressed. A bundle as it was taken out (carbine.weap.tagbundle, as an
earlier build kept them, or ce_tags extract writes) is read as it is; the
Tag Injector's menu stores those as it opens (tag_injector_profile_store).
A profile with a tag file is of format 2, which an earlier build does not
list.

A profile holds its bundles, rather than naming the maps they come from:
it is played without those maps, and can be given to another machine
whole. The Tag Injector menu (Settings > Gameplay, menu_functions.c) makes
profiles and takes tags out of maps into them (tag_injector.c), and the
randomizer's Go Crazy adds the tags of the profile game.tag_injection_profile
names to a level.

A folder without profile.toml is not a profile, and a profile made for a
later format than this build's is not listed. A tag whose bundle is missing
is listed, and left out where it is played.

A network game's shared profile (port/linux/game/tag_multiplayer.c) is a
folder of tag_injector/.shared, named for the hash of its archive: the
host's weapons a multiplayer game plays, made from one of its profiles, and
a client's download of it. It is no scan's (the menus never list it): it is
opened alone, into a slot of its own past the profiles found, and kept
there whatever scans come. Its archive (<id>.tagshare beside it) is its
folder's files in one, as map sharing sends one file:

	"OCETAGS1", a count (little-endian, 4 bytes), and of each file its
	name's length (4 bytes), its name and its size (4 bytes); then each
	file's bytes, in that order

whose names are those of a profile's folder alone (no folder, nor one
beginning with a dot).
*/

#include "platform.h"
#include "port_config.h"
#include "port_tag_mp_profiles.h"
#include "port_tag_profiles.h"
#include "port_tag_store.h"
#include "posix.h"
#include "tomlc17.h"

#include <SDL3/SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------- constants */

/* the profiles' format this build reads, and writes: 2 has tag files (a
profile of bundles alone is written 1, which an earlier build reads) */
#define TAG_PROFILES_FORMAT 2
#define TAG_PROFILES_BUNDLES_FORMAT 1

#define MAXIMUM_TAG_PROFILES 64
#define MAXIMUM_PROFILE_TAGS 256
#define MAXIMUM_FOLDER_LENGTH 64
#define MAXIMUM_NAME_LENGTH 64
#define MAXIMUM_TAG_NAME_LENGTH 256
#define MAXIMUM_MAP_NAME_LENGTH 64
#define MAXIMUM_FILE_NAME_LENGTH 128
#define MAXIMUM_PATH_LENGTH 1024
#define MAXIMUM_OPTION_LENGTH 32
/* a tag's edits (port_tag_profiles_tag_edit_*): the most a tag has, and the
longest key */
#define MAXIMUM_TAG_EDITS 512
#define MAXIMUM_EDIT_KEY_LENGTH 48
/* the most a character takes in profile.toml (\u00XX) */
#define TOML_CHARACTER_LENGTH 6
/* the most bundles open at once (a level's), and what their reads go
through on their way into the Xbox's memory, which the renderer watches
(memory_watch.c: the kernel fails a read into memory it has protected) */
#define MAXIMUM_OPEN_BUNDLES 64
#define READ_BOUNCE_SIZE 0x10000
/* the shared profiles' folder (of tag_injector), the slot the one opened is
in, and an archive's magic, most files and largest file */
#define SHARED_FOLDER ".shared"
#define SHARED_PROFILE MAXIMUM_TAG_PROFILES
#define SHARED_ID_LENGTH 32
#define ARCHIVE_MAGIC "OCETAGS1"
#define ARCHIVE_MAGIC_LENGTH 8
#define MAXIMUM_ARCHIVE_FILES (MAXIMUM_PROFILE_TAGS + 8)
#define MAXIMUM_ARCHIVE_FILE_SIZE 0x40000000UL
/* the host's multiplayer profile in its share, and what a client's copy of
a share it downloaded is named (after the host's names) */
#define SHARED_MULTIPLAYER_FILE "multiplayer.toml"
#define SHARED_IMPORTED_EXTENSION ".imported"
#define SHARED_COPY_SUFFIX " (online)"

/* ---------- structures */

/* a tag's settings, by their keys in profile.toml (port_tag_profiles.h) */
static const char *const tag_option_keys[] = { "faction", "type", "class", "custom_type", "hud", "replace", "name",
	"failed", "pickup", "pickup_text" };
#define TAG_OPTION_COUNT (sizeof(tag_option_keys) / sizeof(tag_option_keys[0]))

/* an edit of a tag's values (the Tag Editor's: tag_editor.c), by its key */
struct tag_profile_edit
{
	char key[MAXIMUM_EDIT_KEY_LENGTH];
	double value;
};

struct tag_profile_tag
{
	char map[MAXIMUM_MAP_NAME_LENGTH];
	unsigned long group;
	char name[MAXIMUM_TAG_NAME_LENGTH];
	char file[MAXIMUM_FILE_NAME_LENGTH];
	int enabled;
	char options[TAG_OPTION_COUNT][MAXIMUM_OPTION_LENGTH];
	/* its edits, by key (allocated: NULL for none) */
	struct tag_profile_edit *edits;
	long edit_count;
	/* its bundle's bytes, as the scan found it (0: missing), its file's,
	and whether that is a tag file (port_tag_store.c) */
	unsigned long size;
	unsigned long file_size;
	int stored;
};

struct tag_profile
{
	char folder[MAXIMUM_FOLDER_LENGTH];
	char name[MAXIMUM_NAME_LENGTH];
	struct tag_profile_tag *tags;
	long tag_count;
	/* its pack's bytes, as the scan found them */
	unsigned long pack_size;
};

/* ---------- globals */

static struct
{
	/* the profiles found, then the shared profile opened (SHARED_PROFILE) */
	struct tag_profile profiles[MAXIMUM_TAG_PROFILES + 1];
	long profile_count;
	int shared_open;
	unsigned long scans;
	/* the bundles a level reads from, whatever profiles are found meanwhile:
	a bundle's file, or a tag file's handle (port_tag_store.c), plus one
	(0: none) */
	FILE *open_bundles[MAXIMUM_OPEN_BUNDLES];
	long stored_bundles[MAXIMUM_OPEN_BUNDLES];
	unsigned char bounce[READ_BOUNCE_SIZE];
} port_tag_profiles;

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

static const char *profiles_root(char *path, size_t size)
{
	snprintf(path, size, "%s/tag_injector", platform_data_root());
	return path;
}

static void profile_path(const struct tag_profile *profile, const char *file, char *path, size_t size)
{
	char root[MAXIMUM_PATH_LENGTH];

	snprintf(path, size, "%s/%s%s%s", profiles_root(root, sizeof(root)), profile->folder, file ? "/" : "",
		file ? file : "");
}

static void text_copy(char *destination, size_t size, const char *source)
{
	snprintf(destination, size, "%s", source ? source : "");
}

/* a name made a file's or folder's: its letters, digits, spaces, dots, -
and _ (others are left out), without spaces at either end; 0 if none is
left */
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
			(character >= '0' && character <= '9') || character == ' ' || character == '_' || character == '-' ||
			(character == '.' && write > name))
			*write++ = character;
	}
	while (write > name && (write[-1] == ' ' || write[-1] == '.'))
		write--;
	*write = 0;
	return write != name;
}

/* a group as text ("weap"), and back */
static void group_text(unsigned long group, char text[5])
{
	int index;

	for (index = 0; index < 4; index++)
	{
		char character = (char)(group >> (24 - 8 * index));

		text[index] = character >= 0x20 && character < 0x7F ? character : '?';
	}
	text[4] = 0;
}

static int group_parse(const char *text, unsigned long *group)
{
	size_t length = strlen(text);
	size_t index;

	if (!length || length > 4)
		return 0;
	*group = 0;
	for (index = 0; index < 4; index++)
		*group = *group << 8 | (unsigned char)(index < length ? text[index] : ' ');
	return 1;
}

/* a value as a TOML basic string's characters, after what text holds (its
control characters escaped, which TOML reads back as they were) */
static void toml_string_append(char *text, size_t size, const char *value)
{
	size_t length = strlen(text);

	for (; *value && length + TOML_CHARACTER_LENGTH < size; value++)
	{
		unsigned char character = (unsigned char)*value;

		if (character < 0x20 || character == 0x7F)
		{
			snprintf(text + length, size - length, "\\u%04X", character);
			length += 6;
			continue;
		}
		if (character == '"' || character == '\\')
			text[length++] = '\\';
		text[length++] = (char)character;
	}
	text[length] = 0;
}

static const char *toml_text(toml_datum_t table, const char *key, const char *path)
{
	toml_datum_t value = toml_get(table, key);

	if (value.type == TOML_UNKNOWN)
		return NULL;
	if (value.type != TOML_STRING)
	{
		platform_log("tag injector: %s line %d: %s is not a string", path, value.lineno, key);
		return NULL;
	}
	return value.u.s;
}

/* a tag's setting's index, by its key, or -1 */
static int tag_option_index(const char *key)
{
	size_t index;

	for (index = 0; key && index < TAG_OPTION_COUNT; index++)
	{
		if (!strcmp(tag_option_keys[index], key))
			return (int)index;
	}
	return -1;
}

/* a setting's value as it is kept: its letters, digits, spaces, - and _
(a custom type's name and a tag's are typed), lower case but for
custom_type's, name's and pickup_text's; 0 if it is too long */
static int tag_option_clean(int option, const char *value, char *clean)
{
	int typed = !strcmp(tag_option_keys[option], "custom_type") || !strcmp(tag_option_keys[option], "name") ||
		!strcmp(tag_option_keys[option], "pickup_text");
	size_t length = 0;

	while (value && *value == ' ')
		value++;
	for (; value && *value; value++)
	{
		char character = *value;

		if (!((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
			(character >= '0' && character <= '9') || character == ' ' || character == '_' || character == '-'))
			continue;
		if (length + 1 >= MAXIMUM_OPTION_LENGTH)
			return 0;
		clean[length++] = typed ? character : lower(character);
	}
	while (length > 0 && clean[length - 1] == ' ')
		length--;
	clean[length] = 0;
	return 1;
}

/* whether an edit's key is one: lower case letters, digits, . and _, as
long as one is kept */
static int edit_key_valid(const char *key)
{
	size_t length;

	if (!key || !key[0])
		return 0;
	for (length = 0; key[length]; length++)
	{
		char character = key[length];

		if (length + 1 >= MAXIMUM_EDIT_KEY_LENGTH ||
			!((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '.' ||
			character == '_'))
			return 0;
	}
	return 1;
}

/* a tag's edit of a key, or NULL */
static struct tag_profile_edit *edit_find(struct tag_profile_tag *tag, const char *key)
{
	long index;

	for (index = 0; key && index < tag->edit_count; index++)
	{
		if (!strcmp(tag->edits[index].key, key))
			return &tag->edits[index];
	}
	return NULL;
}

/* a tag's edit of a key set to value, added if it has none; 0 if there is
no room */
static int edit_set(struct tag_profile_tag *tag, const char *key, double value)
{
	struct tag_profile_edit *found = edit_find(tag, key);

	if (!found)
	{
		struct tag_profile_edit *grown;

		if (tag->edit_count >= MAXIMUM_TAG_EDITS)
			return 0;
		grown = realloc(tag->edits, (size_t)(tag->edit_count + 1) * sizeof(*grown));
		if (!grown)
			return 0;
		tag->edits = grown;
		found = &tag->edits[tag->edit_count++];
		text_copy(found->key, sizeof(found->key), key);
	}
	found->value = value;
	return 1;
}

/* writes `size` bytes to the file `path`, whole: to a file beside it first,
which then takes its place (so that a write cut short leaves the file as it
was) */
static int file_write(const char *path, const void *bytes, unsigned long size)
{
	char temporary[MAXIMUM_PATH_LENGTH];
	int written;
#ifdef HALO_GUEST
	FILE *file;
#endif

	if (snprintf(temporary, sizeof(temporary), "%s.new", path) >= (int)sizeof(temporary))
		return 0;
#ifdef HALO_GUEST
	/* (the guest's own C library: SDL's are the host's) */
	file = fopen(temporary, "wb");
	if (!file)
		return 0;
	written = fwrite(bytes, 1, size, file) == size;
	written = fclose(file) == 0 && written;
	if (written)
		written = rename(temporary, path) == 0;
	if (!written)
		remove(temporary);
#else
	written = SDL_SaveFile(temporary, bytes, size);
	if (written)
		written = SDL_RenamePath(temporary, path);
	if (!written)
		SDL_RemovePath(temporary);
#endif
	return written;
}

/* a file put in another's place (port_tag_store.c's pack written again) */
static int file_replace(const char *from, const char *to)
{
#ifdef HALO_GUEST
	return rename(from, to) == 0;
#else
	return SDL_RenamePath(from, to);
#endif
}

/* whether a file's name is a tag file's (port_tag_store.c) */
static int file_is_stored(const char *file)
{
	size_t length = strlen(file);
	size_t extension = strlen(PORT_TAG_STORE_EXTENSION);

	return length > extension && !strcmp(file + length - extension, PORT_TAG_STORE_EXTENSION);
}

static void profile_dispose(struct tag_profile *profile)
{
	long index;

	for (index = 0; profile->tags && index < profile->tag_count; index++)
		free(profile->tags[index].edits);
	free(profile->tags);
	memset(profile, 0, sizeof(*profile));
}

/* reads the profile.toml of the profile in `folder`; 0 when it is not one */
static int profile_read(struct tag_profile *profile, const char *root, const char *folder)
{
	char path[MAXIMUM_PATH_LENGTH];
	size_t size = 0;
	char *text;
	toml_result_t result;
	toml_datum_t value;
	toml_datum_t tags;
	int index;

	memset(profile, 0, sizeof(*profile));
	text_copy(profile->folder, sizeof(profile->folder), folder);
	text_copy(profile->name, sizeof(profile->name), folder);
	snprintf(path, sizeof(path), "%s/%s/profile.toml", root, folder);
	text = config_file_read(path, &size);
	if (!text)
		return 0;
	result = toml_parse(text, (int)size);
	free(text);
	if (!result.ok)
	{
		platform_log("tag injector: %s: %s; it is not a profile", path, result.errmsg);
		toml_free(result);
		return 0;
	}
	value = toml_get(result.toptab, "format");
	if (value.type == TOML_INT64 && value.u.int64 > TAG_PROFILES_FORMAT)
	{
		platform_log("tag injector: %s is made for a later build (format %lld): it is not listed", path,
			(long long)value.u.int64);
		toml_free(result);
		return 0;
	}
	if (toml_text(result.toptab, "name", path) && toml_text(result.toptab, "name", path)[0])
		text_copy(profile->name, sizeof(profile->name), toml_text(result.toptab, "name", path));
	tags = toml_get(result.toptab, "tags");
	if (tags.type == TOML_ARRAY && tags.u.arr.size > 0)
	{
		long count = tags.u.arr.size < MAXIMUM_PROFILE_TAGS ? tags.u.arr.size : MAXIMUM_PROFILE_TAGS;

		if (tags.u.arr.size > MAXIMUM_PROFILE_TAGS)
			platform_log("tag injector: %s: more than %d tags: those after them are left out", path,
				MAXIMUM_PROFILE_TAGS);
		profile->tags = calloc((size_t)count, sizeof(*profile->tags));
		for (index = 0; profile->tags && index < count; index++)
		{
			toml_datum_t entry = tags.u.arr.elem[index];
			struct tag_profile_tag *tag = &profile->tags[profile->tag_count];
			const char *map;
			const char *group;
			const char *name;
			const char *file;
			toml_datum_t enabled;
			char bundle_path[MAXIMUM_PATH_LENGTH];
			struct posix_file_information information;
			size_t option;

			if (entry.type != TOML_TABLE)
			{
				platform_log("tag injector: %s line %d: a tag is not a table", path, entry.lineno);
				continue;
			}
			map = toml_text(entry, "map", path);
			group = toml_text(entry, "group", path);
			name = toml_text(entry, "tag", path);
			file = toml_text(entry, "file", path);
			if (!group || !name || !file || !name[0] || !group_parse(group, &tag->group) ||
				strlen(name) >= sizeof(tag->name) || strlen(file) >= sizeof(tag->file))
			{
				platform_log("tag injector: %s line %d: a tag without its group, path or file", path, entry.lineno);
				continue;
			}
			text_copy(tag->map, sizeof(tag->map), map);
			text_copy(tag->name, sizeof(tag->name), name);
			text_copy(tag->file, sizeof(tag->file), file);
			/* (a bundle's file is beside profile.toml, never elsewhere) */
			if (strchr(tag->file, '/') || strchr(tag->file, '\\') || !strcmp(tag->file, "..") ||
				!file_name_clean(tag->file) || strcmp(tag->file, file))
			{
				platform_log("tag injector: %s line %d: %s is not a file of the profile's folder", path,
					entry.lineno, file);
				continue;
			}
			enabled = toml_get(entry, "enabled");
			tag->enabled = enabled.type != TOML_BOOLEAN || enabled.u.boolean;
			for (option = 0; option < TAG_OPTION_COUNT; option++)
			{
				const char *value;

				/* (replace a boolean: kept as "true", or "") */
				if (!strcmp(tag_option_keys[option], "replace"))
				{
					toml_datum_t replace = toml_get(entry, "replace");

					text_copy(tag->options[option], sizeof(tag->options[option]),
						replace.type == TOML_BOOLEAN && replace.u.boolean ? "true" : "");
					continue;
				}
				value = toml_text(entry, tag_option_keys[option], path);

				if (value && !tag_option_clean((int)option, value, tag->options[option]))
				{
					platform_log("tag injector: %s line %d: %s is too long: it is left out", path, entry.lineno,
						tag_option_keys[option]);
					tag->options[option][0] = 0;
				}
			}
			/* (its edits: a table of numbers by key) */
			{
				toml_datum_t edits = toml_get(entry, "edits");
				int edit;

				if (edits.type != TOML_UNKNOWN && edits.type != TOML_TABLE)
					platform_log("tag injector: %s line %d: edits is not a table", path, edits.lineno);
				for (edit = 0; edits.type == TOML_TABLE && edit < edits.u.tab.size; edit++)
				{
					toml_datum_t number = edits.u.tab.value[edit];
					const char *key = edits.u.tab.key[edit];
					double value = number.type == TOML_FP64 ? number.u.fp64 :
						number.type == TOML_INT64 ? (double)number.u.int64 : 0.0;

					if (!edit_key_valid(key) || (number.type != TOML_FP64 && number.type != TOML_INT64) ||
						value != value || value > 1e9 || value < -1e9 || !edit_set(tag, key, value))
					{
						platform_log("tag injector: %s line %d: the edit %s is left out", path, number.lineno,
							key ? key : "?");
					}
				}
			}
			snprintf(bundle_path, sizeof(bundle_path), "%s/%s/%s", root, folder, tag->file);
			tag->stored = file_is_stored(tag->file);
			if (posix_stat(bundle_path, &information) == 0 && !(information.flags & _posix_file_is_directory))
			{
				tag->file_size = information.size_high ? 0xFFFFFFFFUL : information.size_low;
				tag->size = tag->stored ? port_tag_store_bundle_size(bundle_path) : tag->file_size;
				if (!tag->size)
					platform_log("tag injector: %s is not a tag file this build reads", bundle_path);
			}
			else
				platform_log("tag injector: %s line %d: %s cannot be found (%s)", path, entry.lineno, bundle_path,
					strerror(errno));
			profile->tag_count++;
		}
	}
	{
		char folder_path[MAXIMUM_PATH_LENGTH];

		snprintf(folder_path, sizeof(folder_path), "%s/%s", root, folder);
		profile->pack_size = port_tag_store_pack_size(folder_path);
	}
	if (tags.type != TOML_UNKNOWN && tags.type != TOML_ARRAY)
	{
		platform_log("tag injector: %s line %d: tags is not a list of tags", path, tags.lineno);
	}
	toml_free(result);
	return 1;
}

/* a profile's profile.toml, as text (the caller's to free); NULL if there is
no room */
static char *profile_text(const struct tag_profile *profile)
{
	size_t capacity = 256 + (size_t)profile->tag_count * (256 + (MAXIMUM_TAG_NAME_LENGTH + MAXIMUM_MAP_NAME_LENGTH +
		MAXIMUM_FILE_NAME_LENGTH + TAG_OPTION_COUNT * MAXIMUM_OPTION_LENGTH) * TOML_CHARACTER_LENGTH);
	char *text;
	long index;

	/* (and its tags' edits, each a line) */
	for (index = 0; index < profile->tag_count; index++)
		capacity += 32 + (size_t)profile->tags[index].edit_count * (MAXIMUM_EDIT_KEY_LENGTH + 40);
	text = malloc(capacity);
	int format = TAG_PROFILES_BUNDLES_FORMAT;

	if (!text)
		return NULL;
	for (index = 0; index < profile->tag_count; index++)
	{
		if (profile->tags[index].stored)
			format = TAG_PROFILES_FORMAT;
	}
	snprintf(text, capacity, "# the Tag Injector's profile (Settings > Gameplay > Tag Injector)\nname = \"");
	toml_string_append(text, capacity, profile->name);
	snprintf(text + strlen(text), capacity - strlen(text), "\"\nformat = %d\n", format);
	for (index = 0; index < profile->tag_count; index++)
	{
		const struct tag_profile_tag *tag = &profile->tags[index];
		char group[5];
		size_t option;

		group_text(tag->group, group);
		snprintf(text + strlen(text), capacity - strlen(text), "\n[[tags]]\nmap = \"");
		toml_string_append(text, capacity, tag->map);
		snprintf(text + strlen(text), capacity - strlen(text), "\"\ngroup = \"");
		toml_string_append(text, capacity, group);
		snprintf(text + strlen(text), capacity - strlen(text), "\"\ntag = \"");
		toml_string_append(text, capacity, tag->name);
		snprintf(text + strlen(text), capacity - strlen(text), "\"\nfile = \"");
		toml_string_append(text, capacity, tag->file);
		snprintf(text + strlen(text), capacity - strlen(text), "\"\nenabled = %s\n", tag->enabled ? "true" : "false");
		for (option = 0; option < TAG_OPTION_COUNT; option++)
		{
			if (!tag->options[option][0])
				continue;
			if (!strcmp(tag_option_keys[option], "replace"))
			{
				snprintf(text + strlen(text), capacity - strlen(text), "replace = true\n");
				continue;
			}
			snprintf(text + strlen(text), capacity - strlen(text), "%s = \"", tag_option_keys[option]);
			toml_string_append(text, capacity, tag->options[option]);
			snprintf(text + strlen(text), capacity - strlen(text), "\"\n");
		}
		/* (its edits last: a table of the tag's, after its own keys) */
		if (tag->edit_count)
		{
			long edit;

			snprintf(text + strlen(text), capacity - strlen(text), "\n[tags.edits]\n");
			for (edit = 0; edit < tag->edit_count; edit++)
			{
				snprintf(text + strlen(text), capacity - strlen(text), "\"%s\" = %.9g%s\n", tag->edits[edit].key,
					tag->edits[edit].value,
					/* (a whole number written as a float, as it was read) */
					tag->edits[edit].value == (double)(long long)tag->edits[edit].value ? ".0" : "");
			}
		}
	}
	return text;
}

/* writes a profile's profile.toml; 0 if it cannot be */
static int profile_write(const struct tag_profile *profile)
{
	char path[MAXIMUM_PATH_LENGTH];
	char *text = profile_text(profile);
	int written;

	if (!text)
		return 0;
	profile_path(profile, "profile.toml", path, sizeof(path));
	written = file_write(path, text, (unsigned long)strlen(text));
	if (!written)
		platform_log("tag injector: %s cannot be written", path);
	free(text);
	return written;
}

static struct tag_profile *profile_get(long profile)
{
	if (profile == SHARED_PROFILE)
		return port_tag_profiles.shared_open ? &port_tag_profiles.profiles[SHARED_PROFILE] : NULL;
	return profile >= 0 && profile < port_tag_profiles.profile_count ? &port_tag_profiles.profiles[profile] : NULL;
}

static struct tag_profile_tag *tag_get(long profile, long tag)
{
	struct tag_profile *found = profile_get(profile);

	return found && tag >= 0 && tag < found->tag_count ? &found->tags[tag] : NULL;
}

/* whether a file of that name is in the profile's folder */
static int profile_has_file(const struct tag_profile *profile, const char *file)
{
	char path[MAXIMUM_PATH_LENGTH];
	struct posix_file_information information;

	profile_path(profile, file, path, sizeof(path));
	return posix_stat(path, &information) == 0;
}

/* the bytes of a file of the profile's folder (0: none) */
static unsigned long profile_file_size(const struct tag_profile *profile, const char *file)
{
	char path[MAXIMUM_PATH_LENGTH];
	struct posix_file_information information;

	profile_path(profile, file, path, sizeof(path));
	return posix_stat(path, &information) == 0 && !information.size_high ? information.size_low : 0;
}

/* ---------- public code */

long port_tag_profiles_scan(void)
{
	char root[MAXIMUM_PATH_LENGTH];
	char name[256];
	void *directory;
	long index;

	for (index = 0; index < port_tag_profiles.profile_count; index++)
		profile_dispose(&port_tag_profiles.profiles[index]);
	port_tag_profiles.profile_count = 0;
	port_tag_profiles.scans++;
	directory = posix_directory_open(profiles_root(root, sizeof(root)));
	if (!directory)
		return 0;
	while (posix_directory_next(directory, name, sizeof(name)))
	{
		char path[MAXIMUM_PATH_LENGTH];
		struct posix_file_information information;
		struct tag_profile profile;

		snprintf(path, sizeof(path), "%s/%s", root, name);
		if (name[0] == '.' || strlen(name) >= MAXIMUM_FOLDER_LENGTH || port_tag_mp_profiles_folder_reserved(name) ||
			posix_stat(path, &information) != 0 ||
			!(information.flags & _posix_file_is_directory))
			continue;
		if (port_tag_profiles.profile_count == MAXIMUM_TAG_PROFILES)
		{
			platform_log("tag injector: more than %d profiles in %s: %s and those after it are left out",
				MAXIMUM_TAG_PROFILES, root, name);
			break;
		}
		if (!profile_read(&profile, root, name))
			continue;
		/* (by name, alphabetically) */
		for (index = port_tag_profiles.profile_count;
			index > 0 && names_compare(port_tag_profiles.profiles[index - 1].name, profile.name) > 0; index--)
			port_tag_profiles.profiles[index] = port_tag_profiles.profiles[index - 1];
		port_tag_profiles.profiles[index] = profile;
		port_tag_profiles.profile_count++;
	}
	posix_directory_close(directory);
	return port_tag_profiles.profile_count;
}

long port_tag_profiles_count(void)
{
	return port_tag_profiles.profile_count;
}

unsigned long port_tag_profiles_scans(void)
{
	return port_tag_profiles.scans;
}

const char *port_tag_profiles_name(long profile)
{
	return profile_get(profile) ? profile_get(profile)->name : "";
}

const char *port_tag_profiles_folder(long profile)
{
	return profile_get(profile) ? profile_get(profile)->folder : "";
}

long port_tag_profiles_find(const char *folder)
{
	long index;

	for (index = 0; folder && folder[0] && index < port_tag_profiles.profile_count; index++)
	{
		if (!names_compare(port_tag_profiles.profiles[index].folder, folder))
			return index;
	}
	return -1;
}

long port_tag_profiles_create(const char *name)
{
	char root[MAXIMUM_PATH_LENGTH];
	char base[MAXIMUM_FOLDER_LENGTH - 8];
	struct tag_profile profile;
	long index;
	int suffix;

	snprintf(base, sizeof(base), "%s", name ? name : "");
	if (!file_name_clean(base))
	{
		platform_log("tag injector: '%s' has no letter or digit to name a profile by", name ? name : "");
		return -1;
	}
	memset(&profile, 0, sizeof(profile));
	text_copy(profile.name, sizeof(profile.name), name);
	/* (a folder of its own: a name another profile's folder has is
	numbered, Brutes 2) */
	posix_make_directory(profiles_root(root, sizeof(root)));
	for (suffix = 1; suffix < 1000; suffix++)
	{
		char path[MAXIMUM_PATH_LENGTH];
		struct posix_file_information information;

		if (suffix == 1)
			snprintf(profile.folder, sizeof(profile.folder), "%s", base);
		else
			snprintf(profile.folder, sizeof(profile.folder), "%s %d", base, suffix);
		profile_path(&profile, NULL, path, sizeof(path));
		/* (not the multiplayer or campaign profiles' folder either) */
		if (posix_stat(path, &information) == 0 || port_tag_mp_profiles_folder_reserved(profile.folder))
			continue;
		if (posix_make_directory(path) != 0)
		{
			platform_log("tag injector: the folder %s cannot be made", path);
			return -1;
		}
		break;
	}
	if (suffix == 1000 || !profile_write(&profile))
		return -1;
	port_tag_profiles_scan();
	index = port_tag_profiles_find(profile.folder);
	platform_log("tag injector: made the profile %s (%s)", profile.name, profile.folder);
	return index;
}

/* copies a file, a chunk at a time (a profile's pack is large), beside its
copy first; 0 if it cannot be */
static int file_copy(const char *from, const char *to)
{
	char temporary[MAXIMUM_PATH_LENGTH];
	FILE *source;
	FILE *destination;
	int copied = 1;

	if (snprintf(temporary, sizeof(temporary), "%s.new", to) >= (int)sizeof(temporary))
		return 0;
	source = fopen(from, "rb");
	if (!source)
		return 0;
	destination = fopen(temporary, "wb");
	if (!destination)
	{
		fclose(source);
		return 0;
	}
	while (copied)
	{
		size_t count = fread(port_tag_profiles.bounce, 1, sizeof(port_tag_profiles.bounce), source);

		if (!count)
		{
			copied = !ferror(source);
			break;
		}
		copied = fwrite(port_tag_profiles.bounce, 1, count, destination) == count;
	}
	fclose(source);
	copied = fclose(destination) == 0 && copied && file_replace(temporary, to);
	if (!copied)
		remove(temporary);
	return copied;
}

long port_tag_profiles_duplicate(long profile, const char *name)
{
	struct tag_profile *found = profile_get(profile);
	struct tag_profile copy;
	char folder[MAXIMUM_FOLDER_LENGTH];
	char from_folder[MAXIMUM_PATH_LENGTH];
	char to_folder[MAXIMUM_PATH_LENGTH];
	char file[256];
	struct tag_profile_tag *tags = NULL;
	void *directory;
	long tag_count;
	long index;
	int copied = 1;

	if (!found)
		return -1;
	/* (the original's, kept while the scans below forget it) */
	tag_count = found->tag_count;
	if (tag_count)
	{
		tags = malloc((size_t)tag_count * sizeof(*tags));
		if (!tags)
			return -1;
		memcpy(tags, found->tags, (size_t)tag_count * sizeof(*tags));
		/* (and their edits, which the scans below let go with the original) */
		for (index = 0; index < tag_count; index++)
		{
			struct tag_profile_edit *edits = tags[index].edit_count ?
				malloc((size_t)tags[index].edit_count * sizeof(*edits)) : NULL;

			if (edits)
				memcpy(edits, tags[index].edits, (size_t)tags[index].edit_count * sizeof(*edits));
			tags[index].edits = edits;
			tags[index].edit_count = edits ? tags[index].edit_count : 0;
		}
	}
	text_copy(folder, sizeof(folder), found->folder);
	profile_path(found, NULL, from_folder, sizeof(from_folder));
	index = port_tag_profiles_create(name);
	if (index < 0)
	{
		free(tags);
		return -1;
	}
	/* (every file of its folder: its tag files, bundles and pack; its
	profile.toml is written with the copy's name) */
	profile_path(profile_get(index), NULL, to_folder, sizeof(to_folder));
	directory = posix_directory_open(from_folder);
	while (copied && directory && posix_directory_next(directory, file, sizeof(file)))
	{
		char from[MAXIMUM_PATH_LENGTH];
		char to[MAXIMUM_PATH_LENGTH];
		struct posix_file_information information;

		snprintf(from, sizeof(from), "%s/%s", from_folder, file);
		snprintf(to, sizeof(to), "%s/%s", to_folder, file);
		if (file[0] == '.' || !strcmp(file, "profile.toml") || !strcmp(file, SHARED_MULTIPLAYER_FILE) ||
			posix_stat(from, &information) != 0 ||
			(information.flags & _posix_file_is_directory))
			continue;
		copied = file_copy(from, to);
		if (!copied)
			platform_log("tag injector: %s cannot be copied to %s", from, to);
	}
	if (directory)
		posix_directory_close(directory);
	copy = *profile_get(index);
	copy.tags = tags;
	copy.tag_count = tag_count;
	copied = copied && profile_write(&copy);
	for (index = 0; index < tag_count; index++)
		free(tags[index].edits);
	free(tags);
	text_copy(file, sizeof(file), copy.folder);
	port_tag_profiles_scan();
	index = port_tag_profiles_find(file);
	if (!copied)
	{
		platform_log("tag injector: the profile %s could not be copied whole: its copy is deleted", folder);
		if (index >= 0)
			port_tag_profiles_delete(index);
		return -1;
	}
	platform_log("tag injector: copied the profile %s to %s", folder, file);
	return index;
}

/* deletes a folder's files, then the folder: only what is in it, and
nothing below; 0 if the folder is still there */
static int folder_delete(const char *path)
{
	char name[256];
	void *directory = posix_directory_open(path);

	while (directory && posix_directory_next(directory, name, sizeof(name)))
	{
		char file[MAXIMUM_PATH_LENGTH];
		struct posix_file_information information;

		snprintf(file, sizeof(file), "%s/%s", path, name);
		if (posix_stat(file, &information) == 0 && !(information.flags & _posix_file_is_directory))
			remove(file);
	}
	if (directory)
		posix_directory_close(directory);
	return rmdir(path) == 0;
}

int port_tag_profiles_delete(long profile)
{
	struct tag_profile *found = profile_get(profile);
	char path[MAXIMUM_PATH_LENGTH];
	int deleted;

	if (!found || profile == SHARED_PROFILE)
		return 0;
	profile_path(found, NULL, path, sizeof(path));
	deleted = folder_delete(path);
	if (!deleted)
		platform_log("tag injector: the folder %s cannot be deleted", path);
	else
		platform_log("tag injector: deleted the profile %s", found->name);
	port_tag_profiles_scan();
	return deleted;
}

long port_tag_profiles_tag_count(long profile)
{
	return profile_get(profile) ? profile_get(profile)->tag_count : 0;
}

const char *port_tag_profiles_tag_map(long profile, long tag)
{
	return tag_get(profile, tag) ? tag_get(profile, tag)->map : "";
}

unsigned long port_tag_profiles_tag_group(long profile, long tag)
{
	return tag_get(profile, tag) ? tag_get(profile, tag)->group : 0;
}

const char *port_tag_profiles_tag_name(long profile, long tag)
{
	return tag_get(profile, tag) ? tag_get(profile, tag)->name : "";
}

unsigned long port_tag_profiles_tag_size(long profile, long tag)
{
	return tag_get(profile, tag) ? tag_get(profile, tag)->size : 0;
}

unsigned long port_tag_profiles_size(long profile)
{
	struct tag_profile *found = profile_get(profile);
	unsigned long total = found ? found->pack_size : 0;
	long index;

	for (index = 0; found && index < found->tag_count; index++)
		total += found->tags[index].file_size;
	return total;
}

int port_tag_profiles_tag_stored(long profile, long tag)
{
	return tag_get(profile, tag) ? tag_get(profile, tag)->stored : 0;
}

int port_tag_profiles_file_write(const char *path, const void *bytes, unsigned long size)
{
	return file_write(path, bytes, size);
}

int port_tag_profiles_file_replace(const char *from, const char *to)
{
	return file_replace(from, to);
}

int port_tag_profiles_tag_enabled(long profile, long tag)
{
	return tag_get(profile, tag) ? tag_get(profile, tag)->enabled : 0;
}

int port_tag_profiles_tag_set_enabled(long profile, long tag, int enabled)
{
	struct tag_profile_tag *found = tag_get(profile, tag);

	if (!found)
		return 0;
	/* (turned on: tried again, whatever it failed by before) */
	if (enabled && found->options[tag_option_index("failed")][0])
	{
		found->options[tag_option_index("failed")][0] = 0;
		if (found->enabled)
		{
			port_tag_profiles.scans++;
			return profile_write(profile_get(profile));
		}
	}
	if (found->enabled == (enabled != 0))
		return 1;
	found->enabled = enabled != 0;
	port_tag_profiles.scans++;
	platform_log("tag injector: %s of the profile %s is %s", found->name, profile_get(profile)->name,
		found->enabled ? "on" : "off");
	return profile_write(profile_get(profile));
}

const char *port_tag_profiles_tag_option(long profile, long tag, const char *key)
{
	int option = tag_option_index(key);

	return tag_get(profile, tag) && option >= 0 ? tag_get(profile, tag)->options[option] : "";
}

int port_tag_profiles_tag_set_option(long profile, long tag, const char *key, const char *value)
{
	struct tag_profile_tag *found = tag_get(profile, tag);
	int option = tag_option_index(key);
	char clean[MAXIMUM_OPTION_LENGTH];

	if (!found || option < 0 || !tag_option_clean(option, value, clean))
		return 0;
	if (!strcmp(found->options[option], clean))
		return 1;
	text_copy(found->options[option], sizeof(found->options[option]), clean);
	port_tag_profiles.scans++;
	platform_log("tag injector: %s of the profile %s: %s = \"%s\"", found->name, profile_get(profile)->name, key,
		clean);
	return profile_write(profile_get(profile));
}

long port_tag_profiles_tag_edit_count(long profile, long tag)
{
	return tag_get(profile, tag) ? tag_get(profile, tag)->edit_count : 0;
}

const char *port_tag_profiles_tag_edit_key(long profile, long tag, long edit)
{
	struct tag_profile_tag *found = tag_get(profile, tag);

	return found && edit >= 0 && edit < found->edit_count ? found->edits[edit].key : "";
}

double port_tag_profiles_tag_edit_value(long profile, long tag, long edit)
{
	struct tag_profile_tag *found = tag_get(profile, tag);

	return found && edit >= 0 && edit < found->edit_count ? found->edits[edit].value : 0.0;
}

int port_tag_profiles_tag_edit_find(long profile, long tag, const char *key, double *value)
{
	struct tag_profile_tag *found = tag_get(profile, tag);
	struct tag_profile_edit *edit = found ? edit_find(found, key) : NULL;

	if (edit && value)
		*value = edit->value;
	return edit != NULL;
}

int port_tag_profiles_tag_set_edit(long profile, long tag, const char *key, double value)
{
	struct tag_profile_tag *found = tag_get(profile, tag);
	struct tag_profile_edit *edit;

	if (!found || !edit_key_valid(key) || value != value || value > 1e9 || value < -1e9)
		return 0;
	edit = edit_find(found, key);
	if (edit && edit->value == value)
		return 1;
	if (!edit_set(found, key, value))
	{
		platform_log("tag injector: %s of the profile %s has %d edits already", found->name,
			profile_get(profile)->name, MAXIMUM_TAG_EDITS);
		return 0;
	}
	port_tag_profiles.scans++;
	return profile_write(profile_get(profile));
}

/* takes out a tag's edits whose keys begin with `prefix` (exact: that are
it), writing the profile */
static int edits_clear(long profile, long tag, const char *prefix, int exact)
{
	struct tag_profile_tag *found = tag_get(profile, tag);
	size_t length = prefix ? strlen(prefix) : 0;
	long kept = 0;
	long index;

	if (!found)
		return 0;
	for (index = 0; index < found->edit_count; index++)
	{
		if (exact ? !strcmp(found->edits[index].key, prefix ? prefix : "") :
			!strncmp(found->edits[index].key, prefix ? prefix : "", length))
			continue;
		found->edits[kept++] = found->edits[index];
	}
	if (kept == found->edit_count)
		return 1;
	found->edit_count = kept;
	port_tag_profiles.scans++;
	return profile_write(profile_get(profile));
}

int port_tag_profiles_tag_clear_edits(long profile, long tag, const char *prefix)
{
	return edits_clear(profile, tag, prefix, 0);
}

int port_tag_profiles_tag_clear_edit(long profile, long tag, const char *key)
{
	return edits_clear(profile, tag, key, 1);
}

unsigned long port_tag_profiles_tag_signature(long profile, long tag)
{
	struct tag_profile_tag *found = tag_get(profile, tag);
	unsigned long value = 2166136261UL;
	const unsigned char *byte;
	size_t index;
	long edit;

	if (!found)
		return 0;
	for (index = 0; index < TAG_OPTION_COUNT; index++)
	{
		for (byte = (const unsigned char *)found->options[index]; *byte; byte++)
			value = (value ^ *byte) * 16777619UL & 0xFFFFFFFFUL;
		value = (value ^ 0xFF) * 16777619UL & 0xFFFFFFFFUL;
	}
	for (edit = 0; edit < found->edit_count; edit++)
	{
		char number[32];

		snprintf(number, sizeof(number), "=%.9g;", found->edits[edit].value);
		for (byte = (const unsigned char *)found->edits[edit].key; *byte; byte++)
			value = (value ^ *byte) * 16777619UL & 0xFFFFFFFFUL;
		for (byte = (const unsigned char *)number; *byte; byte++)
			value = (value ^ *byte) * 16777619UL & 0xFFFFFFFFUL;
	}
	return value;
}

long port_tag_profiles_tag_find(long profile, unsigned long group, const char *name)
{
	struct tag_profile *found = profile_get(profile);
	long index;

	for (index = 0; found && name && index < found->tag_count; index++)
	{
		if (found->tags[index].group == group && !names_compare(found->tags[index].name, name))
			return index;
	}
	return -1;
}

/* lets go of the regions of a profile's pack that none of its tags has any
more (port_tag_store_collect) */
static void profile_collect(struct tag_profile *profile)
{
	char folder[MAXIMUM_PATH_LENGTH];
	const char **files = malloc((size_t)(profile->tag_count ? profile->tag_count : 1) * sizeof(*files));
	long index;

	if (!files)
		return;
	for (index = 0; index < profile->tag_count; index++)
		files[index] = profile->tags[index].file;
	profile_path(profile, NULL, folder, sizeof(folder));
	if (!port_tag_store_collect(folder, files, profile->tag_count))
		platform_log("tag injector: the pack of the profile %s is kept as it was", profile->name);
	profile->pack_size = port_tag_store_pack_size(folder);
	free(files);
}

int port_tag_profiles_tag_add(long profile, const char *map, unsigned long group, const char *name,
	const void *bundle, unsigned long size, unsigned long tags_size, const struct port_tag_store_region *regions,
	long region_count)
{
	struct tag_profile *found = profile_get(profile);
	struct tag_profile_tag tag;
	char path[MAXIMUM_PATH_LENGTH];
	char replaced[MAXIMUM_FILE_NAME_LENGTH];
	char group_name[5];
	char base[MAXIMUM_FILE_NAME_LENGTH - 24];
	const char *extension = regions ? PORT_TAG_STORE_EXTENSION : ".tagbundle";
	const char *leaf;
	unsigned long stored = size;
	long existing;
	int suffix;

	if (!found || !name || !name[0] || strlen(name) >= sizeof(tag.name) || !bundle)
		return 0;
	if (found->tag_count >= MAXIMUM_PROFILE_TAGS && port_tag_profiles_tag_find(profile, group, name) < 0)
	{
		platform_log("tag injector: the profile %s has %d tags already", found->name, MAXIMUM_PROFILE_TAGS);
		return 0;
	}
	memset(&tag, 0, sizeof(tag));
	text_copy(tag.map, sizeof(tag.map), map);
	tag.group = group;
	text_copy(tag.name, sizeof(tag.name), name);
	tag.enabled = 1;
	tag.size = size;
	tag.stored = regions != NULL;
	replaced[0] = 0;
	/* (its file: the last part of its path and its group, numbered when
	another tag's has that name: carbine.weap.tagref) */
	group_text(group, group_name);
	file_name_clean(group_name);
	leaf = strrchr(name, '\\');
	snprintf(base, sizeof(base), "%s", leaf ? leaf + 1 : name);
	if (!file_name_clean(base))
		snprintf(base, sizeof(base), "tag");
	existing = port_tag_profiles_tag_find(profile, group, name);
	/* (the tag replaced keeps its file, unless it is kept otherwise now,
	whether it is on, and its settings) */
	if (existing >= 0)
	{
		text_copy(tag.file, sizeof(tag.file), found->tags[existing].file);
		tag.enabled = found->tags[existing].enabled;
		memcpy(tag.options, found->tags[existing].options, sizeof(tag.options));
		tag.edits = found->tags[existing].edits;
		tag.edit_count = found->tags[existing].edit_count;
		if (found->tags[existing].stored != tag.stored)
			text_copy(replaced, sizeof(replaced), found->tags[existing].file);
	}
	for (suffix = 1; (existing < 0 || replaced[0]) && suffix < 1000; suffix++)
	{
		if (suffix == 1)
			snprintf(tag.file, sizeof(tag.file), "%s.%s%s", base, group_name, extension);
		else
			snprintf(tag.file, sizeof(tag.file), "%s %d.%s%s", base, suffix, group_name, extension);
		if (!profile_has_file(found, tag.file))
			break;
	}
	if (suffix == 1000)
		return 0;
	profile_path(found, tag.file, path, sizeof(path));
	if (regions)
	{
		char folder[MAXIMUM_PATH_LENGTH];

		profile_path(found, NULL, folder, sizeof(folder));
		if (!port_tag_store_write(folder, tag.file, bundle, size, tags_size, regions, region_count, &stored))
			return 0;
	}
	else if (!file_write(path, bundle, size))
	{
		platform_log("tag injector: %s cannot be written", path);
		return 0;
	}
	tag.file_size = profile_file_size(found, tag.file);
	if (existing >= 0)
	{
		found->tags[existing] = tag;
	}
	else
	{
		struct tag_profile_tag *grown = realloc(found->tags, (size_t)(found->tag_count + 1) * sizeof(*grown));

		if (!grown)
		{
			remove(path);
			return 0;
		}
		found->tags = grown;
		found->tags[found->tag_count++] = tag;
	}
	if (!profile_write(found))
		return 0;
	/* (a bundle the tag file replaced goes; and the regions of the one a tag
	replaced that no other tag has) */
	if (replaced[0])
	{
		profile_path(found, replaced, path, sizeof(path));
		remove(path);
	}
	if (regions && existing >= 0)
		profile_collect(found);
	else if (regions)
	{
		char folder[MAXIMUM_PATH_LENGTH];

		profile_path(found, NULL, folder, sizeof(folder));
		found->pack_size = port_tag_store_pack_size(folder);
	}
	port_tag_profiles.scans++;
	platform_log("tag injector: added %s (%s) of %s to the profile %s, %lu bytes (%lu kept)", tag.name, group_name,
		tag.map, found->name, size, stored);
	return 1;
}

int port_tag_profiles_tag_remove(long profile, long tag)
{
	struct tag_profile *found = profile_get(profile);
	char path[MAXIMUM_PATH_LENGTH];

	if (!tag_get(profile, tag))
		return 0;
	profile_path(found, found->tags[tag].file, path, sizeof(path));
	platform_log("tag injector: took %s out of the profile %s", found->tags[tag].name, found->name);
	free(found->tags[tag].edits);
	memmove(&found->tags[tag], &found->tags[tag + 1], (size_t)(found->tag_count - tag - 1) * sizeof(found->tags[0]));
	found->tag_count--;
	port_tag_profiles.scans++;
	if (!profile_write(found))
		return 0;
	remove(path);
	profile_collect(found);
	return 1;
}

long port_tag_profiles_bundle_open(long profile, long tag, unsigned long *size)
{
	struct tag_profile_tag *found = tag_get(profile, tag);
	char path[MAXIMUM_PATH_LENGTH];
	struct posix_file_information information;
	long handle;

	*size = 0;
	if (!found)
		return -1;
	profile_path(profile_get(profile), found->file, path, sizeof(path));
	if (posix_stat(path, &information) != 0)
	{
		platform_log("tag injector: %s cannot be found (%s)", path, strerror(errno));
		return -1;
	}
	if ((information.flags & _posix_file_is_directory) || information.size_high || information.size_low > 0x7FFFFFFFUL)
	{
		platform_log("tag injector: %s is a folder, or larger than 2 GB", path);
		return -1;
	}
	for (handle = 0; handle < MAXIMUM_OPEN_BUNDLES &&
		(port_tag_profiles.open_bundles[handle] || port_tag_profiles.stored_bundles[handle]); handle++)
	{
	}
	if (handle == MAXIMUM_OPEN_BUNDLES)
	{
		platform_log("tag injector: more than %d bundles open: %s is left out", MAXIMUM_OPEN_BUNDLES, path);
		return -1;
	}
	if (found->stored)
	{
		char folder[MAXIMUM_PATH_LENGTH];
		long stored;

		profile_path(profile_get(profile), NULL, folder, sizeof(folder));
		stored = port_tag_store_open(folder, found->file, size);
		if (stored < 0)
		{
			platform_log("tag injector: %s cannot be opened", path);
			return -1;
		}
		port_tag_profiles.stored_bundles[handle] = stored + 1;
		return handle;
	}
	port_tag_profiles.open_bundles[handle] = fopen(path, "rb");
	if (!port_tag_profiles.open_bundles[handle])
	{
		platform_log("tag injector: %s cannot be opened (%s)", path, strerror(errno));
		return -1;
	}
	*size = information.size_low;
	return handle;
}

int port_tag_profiles_bundle_read_at(long handle, unsigned long offset, void *buffer, unsigned long size)
{
	FILE *file = handle >= 0 && handle < MAXIMUM_OPEN_BUNDLES ? port_tag_profiles.open_bundles[handle] : NULL;
	unsigned long done = 0;

	if (handle >= 0 && handle < MAXIMUM_OPEN_BUNDLES && port_tag_profiles.stored_bundles[handle])
		return port_tag_store_read(port_tag_profiles.stored_bundles[handle] - 1, offset, buffer, size);
	if (!file || offset > 0x7FFFFFFFUL || fseek(file, (long)offset, SEEK_SET) != 0)
		return 0;
	/* (the game reads from its main thread alone: one bounce buffer) */
	while (done < size)
	{
		unsigned long count = size - done < READ_BOUNCE_SIZE ? size - done : READ_BOUNCE_SIZE;

		if (fread(port_tag_profiles.bounce, 1, count, file) != count)
			return 0;
		memcpy((unsigned char *)buffer + done, port_tag_profiles.bounce, count);
		done += count;
	}
	return 1;
}

void port_tag_profiles_bundle_close(long handle)
{
	if (handle >= 0 && handle < MAXIMUM_OPEN_BUNDLES && port_tag_profiles.stored_bundles[handle])
	{
		port_tag_store_close(port_tag_profiles.stored_bundles[handle] - 1);
		port_tag_profiles.stored_bundles[handle] = 0;
		return;
	}
	if (handle < 0 || handle >= MAXIMUM_OPEN_BUNDLES || !port_tag_profiles.open_bundles[handle])
		return;
	fclose(port_tag_profiles.open_bundles[handle]);
	port_tag_profiles.open_bundles[handle] = NULL;
}

/* ---------- shared profiles (a network game's) */

/* whether an id names a shared profile: lowercase hexadecimal digits, or a
word of them (the host's while it builds one) */
static int shared_id_valid(const char *id)
{
	size_t length = id ? strlen(id) : 0;
	size_t index;

	if (!length || length > SHARED_ID_LENGTH)
		return 0;
	for (index = 0; index < length; index++)
	{
		if (!((id[index] >= '0' && id[index] <= '9') || (id[index] >= 'a' && id[index] <= 'z')))
			return 0;
	}
	return 1;
}

static void shared_root(char *path, size_t size)
{
	char root[MAXIMUM_PATH_LENGTH];

	snprintf(path, size, "%s/%s", profiles_root(root, sizeof(root)), SHARED_FOLDER);
}

void port_tag_profiles_shared_path(const char *name, char *path, unsigned long size)
{
	char root[MAXIMUM_PATH_LENGTH];

	shared_root(root, sizeof(root));
	snprintf(path, (size_t)size, "%s/%s", root, name ? name : "");
}

int port_tag_profiles_shared_folder_make(void)
{
	char root[MAXIMUM_PATH_LENGTH];
	char path[MAXIMUM_PATH_LENGTH];
	struct posix_file_information information;

	posix_make_directory(profiles_root(root, sizeof(root)));
	shared_root(path, sizeof(path));
	posix_make_directory(path);
	return posix_stat(path, &information) == 0 && (information.flags & _posix_file_is_directory);
}

int port_tag_profiles_shared(long profile)
{
	return profile == SHARED_PROFILE;
}

void port_tag_profiles_shared_close(void)
{
	if (port_tag_profiles.shared_open)
		profile_dispose(&port_tag_profiles.profiles[SHARED_PROFILE]);
	port_tag_profiles.shared_open = 0;
}

int port_tag_profiles_shared_present(const char *id)
{
	char path[MAXIMUM_PATH_LENGTH];
	struct posix_file_information information;

	if (!shared_id_valid(id))
		return 0;
	port_tag_profiles_shared_path(id, path, sizeof(path));
	snprintf(path + strlen(path), sizeof(path) - strlen(path), "/profile.toml");
	return posix_stat(path, &information) == 0 && !(information.flags & _posix_file_is_directory);
}

long port_tag_profiles_shared_open(const char *id)
{
	char root[MAXIMUM_PATH_LENGTH];
	char folder[MAXIMUM_FOLDER_LENGTH];
	struct tag_profile profile;

	port_tag_profiles_shared_close();
	if (!shared_id_valid(id))
		return -1;
	profiles_root(root, sizeof(root));
	snprintf(folder, sizeof(folder), "%s/%s", SHARED_FOLDER, id);
	if (!profile_read(&profile, root, folder))
		return -1;
	port_tag_profiles.profiles[SHARED_PROFILE] = profile;
	port_tag_profiles.shared_open = 1;
	return SHARED_PROFILE;
}

int port_tag_profiles_shared_remove(const char *name)
{
	char path[MAXIMUM_PATH_LENGTH];

	if (!name || !name[0] || name[0] == '.' || strchr(name, '/') || strchr(name, '\\'))
		return 0;
	port_tag_profiles_shared_path(name, path, sizeof(path));
	return remove(path) == 0;
}

int port_tag_profiles_shared_delete(const char *id)
{
	char path[MAXIMUM_PATH_LENGTH];

	if (!shared_id_valid(id))
		return 0;
	if (port_tag_profiles.shared_open &&
		!strcmp(port_tag_profiles.profiles[SHARED_PROFILE].folder + strlen(SHARED_FOLDER) + 1, id))
		port_tag_profiles_shared_close();
	port_tag_profiles_shared_path(id, path, sizeof(path));
	return folder_delete(path);
}

static void archive_long(unsigned char *bytes, unsigned long value)
{
	bytes[0] = (unsigned char)value;
	bytes[1] = (unsigned char)(value >> 8);
	bytes[2] = (unsigned char)(value >> 16);
	bytes[3] = (unsigned char)(value >> 24);
}

static unsigned long archive_get_long(const unsigned char *bytes)
{
	return (unsigned long)bytes[0] | (unsigned long)bytes[1] << 8 | (unsigned long)bytes[2] << 16 |
		(unsigned long)bytes[3] << 24;
}

/* whether a name is one an archive may hold: a file of a profile's folder */
static int archive_name_valid(const char *name)
{
	char clean[MAXIMUM_FILE_NAME_LENGTH];

	if (!name[0] || name[0] == '.' || strlen(name) >= sizeof(clean) || strchr(name, '/') || strchr(name, '\\'))
		return 0;
	snprintf(clean, sizeof(clean), "%s", name);
	return file_name_clean(clean) && !strcmp(clean, name);
}

/* a file an archive is written with: its name, and its bytes in memory
(`bytes`) or in a file (`path`) */
struct archive_entry
{
	char name[MAXIMUM_FILE_NAME_LENGTH];
	char path[MAXIMUM_PATH_LENGTH];
	const char *bytes;
	unsigned long size;
};

static int archive_entries_compare(const void *first, const void *second)
{
	return strcmp(((const struct archive_entry *)first)->name, ((const struct archive_entry *)second)->name);
}

/* writes an archive of the entries (by name: the same files are the same
archive) to a file beside `archive`, then in its place; 0 if it cannot be */
static int archive_write(struct archive_entry *entries, long count, const char *archive)
{
	char temporary[MAXIMUM_PATH_LENGTH];
	unsigned char header[8];
	long index;
	FILE *stream;
	int written;

	if (snprintf(temporary, sizeof(temporary), "%s.new", archive) >= (int)sizeof(temporary))
		return 0;
	qsort(entries, (size_t)count, sizeof(entries[0]), archive_entries_compare);
	stream = fopen(temporary, "wb");
	written = stream != NULL;
	if (written)
	{
		archive_long(header, (unsigned long)count);
		written = fwrite(ARCHIVE_MAGIC, 1, ARCHIVE_MAGIC_LENGTH, stream) == ARCHIVE_MAGIC_LENGTH &&
			fwrite(header, 1, 4, stream) == 4;
	}
	for (index = 0; written && index < count; index++)
	{
		size_t length = strlen(entries[index].name);

		archive_long(header, (unsigned long)length);
		archive_long(header + 4, entries[index].size);
		written = fwrite(header, 1, 4, stream) == 4 && fwrite(entries[index].name, 1, length, stream) == length &&
			fwrite(header + 4, 1, 4, stream) == 4;
	}
	for (index = 0; written && index < count; index++)
	{
		FILE *source;
		unsigned long left = entries[index].size;

		if (entries[index].bytes)
		{
			written = fwrite(entries[index].bytes, 1, entries[index].size, stream) == entries[index].size;
			continue;
		}
		source = fopen(entries[index].path, "rb");
		written = source != NULL;
		while (written && left)
		{
			size_t chunk = left < sizeof(port_tag_profiles.bounce) ? (size_t)left : sizeof(port_tag_profiles.bounce);

			written = fread(port_tag_profiles.bounce, 1, chunk, source) == chunk &&
				fwrite(port_tag_profiles.bounce, 1, chunk, stream) == chunk;
			left -= (unsigned long)chunk;
		}
		if (source)
			fclose(source);
	}
	if (stream)
		written = fclose(stream) == 0 && written;
	written = written && file_replace(temporary, archive);
	if (!written)
	{
		remove(temporary);
		platform_log("tag injector: the archive %s cannot be written", archive);
	}
	return written;
}

/* an entry for a file, named `name` in the archive, if it is there */
static int archive_entry_path(struct archive_entry *entry, const char *name, const char *path)
{
	struct posix_file_information information;

	memset(entry, 0, sizeof(*entry));
	text_copy(entry->name, sizeof(entry->name), name);
	text_copy(entry->path, sizeof(entry->path), path);
	if (!archive_name_valid(name) || posix_stat(entry->path, &information) != 0 ||
		(information.flags & _posix_file_is_directory) || information.size_high ||
		information.size_low > MAXIMUM_ARCHIVE_FILE_SIZE)
		return 0;
	entry->size = information.size_low;
	return 1;
}

/* ... a file of a profile's folder */
static int archive_entry_file(struct archive_entry *entry, const struct tag_profile *profile, const char *file)
{
	char path[MAXIMUM_PATH_LENGTH];

	profile_path(profile, file, path, sizeof(path));
	return archive_entry_path(entry, file, path);
}

long port_tag_profiles_shared_build(long profile, unsigned long group, const char *multiplayer, const char *archive)
{
	struct tag_profile *found = profile_get(profile);
	struct tag_profile shared;
	struct archive_entry *entries;
	char *text = NULL;
	long count = 0;
	long index;
	int written;

	if (!found || profile == SHARED_PROFILE)
		return 0;
	/* (its tags on of the group, their files there, each as it is: on, and
	not failed) */
	memset(&shared, 0, sizeof(shared));
	text_copy(shared.name, sizeof(shared.name), found->name);
	shared.tags = calloc((size_t)(found->tag_count ? found->tag_count : 1), sizeof(*shared.tags));
	entries = calloc((size_t)found->tag_count + 4, sizeof(*entries));
	if (!shared.tags || !entries)
	{
		free(shared.tags);
		free(entries);
		return 0;
	}
	for (index = 0; index < found->tag_count; index++)
	{
		struct tag_profile_tag *tag = &found->tags[index];
		long other;

		if (!tag->enabled || (group && tag->group != group) || !tag->size ||
			!archive_entry_file(&entries[count], found, tag->file))
			continue;
		/* (a file two tags name, once) */
		for (other = 0; other < count && strcmp(entries[other].name, tag->file); other++)
		{
		}
		if (other == count)
			count++;
		shared.tags[shared.tag_count] = *tag;
		shared.tags[shared.tag_count].enabled = 1;
		shared.tags[shared.tag_count].options[tag_option_index("failed")][0] = 0;
		shared.tag_count++;
	}
	/* (and the pack they share, with its index, as they are: none of it
	compressed again) */
	if (shared.tag_count)
	{
		count += archive_entry_file(&entries[count], found, "tags.pack");
		count += archive_entry_file(&entries[count], found, "tags.index");
		/* (and the multiplayer profile played, which a client takes a copy of) */
		if (multiplayer && multiplayer[0])
			count += archive_entry_path(&entries[count], SHARED_MULTIPLAYER_FILE, multiplayer);
		text = profile_text(&shared);
	}
	written = text != NULL;
	if (written)
	{
		memset(&entries[count], 0, sizeof(entries[count]));
		text_copy(entries[count].name, sizeof(entries[count].name), "profile.toml");
		entries[count].bytes = text;
		entries[count].size = (unsigned long)strlen(text);
		count++;
		written = archive_write(entries, count, archive);
	}
	free(text);
	free(entries);
	free(shared.tags);
	return written ? shared.tag_count : 0;
}

int port_tag_profiles_shared_unpack(const char *archive, const char *id)
{
	char root[MAXIMUM_PATH_LENGTH];
	char folder[MAXIMUM_PATH_LENGTH];
	unsigned char header[ARCHIVE_MAGIC_LENGTH + 4];
	char *names[MAXIMUM_ARCHIVE_FILES];
	unsigned long sizes[MAXIMUM_ARCHIVE_FILES];
	unsigned long total = 0;
	long count = 0;
	long index;
	FILE *stream;
	int unpacked;

	if (!shared_id_valid(id))
		return 0;
	stream = fopen(archive, "rb");
	if (!stream)
		return 0;
	unpacked = fread(header, 1, sizeof(header), stream) == sizeof(header) &&
		!memcmp(header, ARCHIVE_MAGIC, ARCHIVE_MAGIC_LENGTH);
	if (unpacked)
	{
		count = (long)archive_get_long(header + ARCHIVE_MAGIC_LENGTH);
		unpacked = count > 0 && count <= MAXIMUM_ARCHIVE_FILES;
		if (!unpacked)
			count = 0;
	}
	for (index = 0; index < count; index++)
		names[index] = NULL;
	for (index = 0; unpacked && index < count; index++)
	{
		unsigned char field[4];
		unsigned long length;

		unpacked = fread(field, 1, 4, stream) == 4;
		length = unpacked ? archive_get_long(field) : 0;
		unpacked = unpacked && length > 0 && length < MAXIMUM_FILE_NAME_LENGTH;
		names[index] = unpacked ? calloc(length + 1, 1) : NULL;
		unpacked = names[index] && fread(names[index], 1, length, stream) == length && fread(field, 1, 4, stream) == 4 &&
			archive_name_valid(names[index]);
		sizes[index] = unpacked ? archive_get_long(field) : 0;
		unpacked = unpacked && sizes[index] <= MAXIMUM_ARCHIVE_FILE_SIZE && total + sizes[index] >= total;
		total += sizes[index];
	}
	/* (each name once, and a profile among them) */
	for (index = 0; unpacked && index < count; index++)
	{
		long other;

		for (other = 0; unpacked && other < index; other++)
			unpacked = strcmp(names[index], names[other]) != 0;
	}
	for (index = 0; unpacked && index < count && strcmp(names[index], "profile.toml"); index++)
	{
	}
	unpacked = unpacked && index < count;
	/* (into the id's folder, made anew: deleted again unless it is whole) */
	if (port_tag_profiles.shared_open &&
		!strcmp(port_tag_profiles.profiles[SHARED_PROFILE].folder + strlen(SHARED_FOLDER) + 1, id))
		port_tag_profiles_shared_close();
	posix_make_directory(profiles_root(root, sizeof(root)));
	shared_root(folder, sizeof(folder));
	posix_make_directory(folder);
	port_tag_profiles_shared_path(id, folder, sizeof(folder));
	folder_delete(folder);
	unpacked = unpacked && posix_make_directory(folder) == 0;
	for (index = 0; unpacked && index < count; index++)
	{
		char path[MAXIMUM_PATH_LENGTH];
		FILE *destination;
		unsigned long left = sizes[index];

		snprintf(path, sizeof(path), "%s/%s", folder, names[index]);
		destination = fopen(path, "wb");
		unpacked = destination != NULL;
		while (unpacked && left)
		{
			size_t chunk = left < sizeof(port_tag_profiles.bounce) ? (size_t)left : sizeof(port_tag_profiles.bounce);

			unpacked = fread(port_tag_profiles.bounce, 1, chunk, stream) == chunk &&
				fwrite(port_tag_profiles.bounce, 1, chunk, destination) == chunk;
			left -= (unsigned long)chunk;
		}
		if (destination)
			unpacked = fclose(destination) == 0 && unpacked;
	}
	/* (nothing after the last file) */
	unpacked = unpacked && fgetc(stream) == EOF;
	fclose(stream);
	for (index = 0; index < count; index++)
		free(names[index]);
	if (!unpacked)
	{
		folder_delete(folder);
		platform_log("tag injector: the archive %s is not one", archive);
	}
	return unpacked;
}

/* the marker beside a share a client took a copy of (the copy's folder in
it), so that the copy is made once, however often it is played or deleted */
static void shared_imported_path(const char *id, char *path, size_t size)
{
	char name[SHARED_ID_LENGTH + sizeof(SHARED_IMPORTED_EXTENSION)];

	snprintf(name, sizeof(name), "%s%s", id, SHARED_IMPORTED_EXTENSION);
	port_tag_profiles_shared_path(name, path, (unsigned long)size);
}

long port_tag_profiles_shared_import(const char *id)
{
	char path[MAXIMUM_PATH_LENGTH];
	char name[MAXIMUM_NAME_LENGTH];
	char folder[MAXIMUM_FOLDER_LENGTH];
	struct posix_file_information information;
	long shared;
	long copy;

	if (!shared_id_valid(id) || !strcmp(id, "host") || !port_tag_profiles_shared_present(id))
		return -1;
	shared_imported_path(id, path, sizeof(path));
	if (posix_stat(path, &information) == 0)
		return -1;
	shared = port_tag_profiles_shared_open(id);
	if (shared < 0)
		return -1;
	snprintf(name, sizeof(name), "%.*s%s", (int)(sizeof(name) - sizeof(SHARED_COPY_SUFFIX)),
		port_tag_profiles.profiles[SHARED_PROFILE].name, SHARED_COPY_SUFFIX);
	copy = port_tag_profiles_duplicate(shared, name);
	port_tag_profiles_shared_close();
	if (copy < 0)
	{
		platform_log("tag injector: the shared profile %s cannot be copied into the profiles", id);
		return -1;
	}
	text_copy(folder, sizeof(folder), port_tag_profiles_folder(copy));
	/* (the host's multiplayer profile, playing the copy) */
	{
		char multiplayer[MAXIMUM_PATH_LENGTH];

		port_tag_profiles_shared_path(id, multiplayer, sizeof(multiplayer));
		snprintf(multiplayer + strlen(multiplayer), sizeof(multiplayer) - strlen(multiplayer), "/%s",
			SHARED_MULTIPLAYER_FILE);
		if (posix_stat(multiplayer, &information) == 0)
			port_tag_mp_profiles_import(multiplayer, SHARED_COPY_SUFFIX, folder);
	}
	if (!file_write(path, folder, (unsigned long)strlen(folder)))
		platform_log("tag injector: %s cannot be written: the share may be copied again", path);
	platform_log("tag injector: copied the shared profile %s into the profiles as %s", id, folder);
	return port_tag_profiles_find(folder);
}

long port_tag_profiles_shared_import_all(void)
{
	char root[MAXIMUM_PATH_LENGTH];
	char name[256];
	void *directory;
	long imported = 0;

	shared_root(root, sizeof(root));
	directory = posix_directory_open(root);
	while (directory && posix_directory_next(directory, name, sizeof(name)))
	{
		char path[MAXIMUM_PATH_LENGTH];
		struct posix_file_information information;

		/* (the folders of the downloads: their hashes' digits) */
		snprintf(path, sizeof(path), "%s/%s", root, name);
		if (strlen(name) != 16 || strspn(name, "0123456789abcdef") != 16 || posix_stat(path, &information) != 0 ||
			!(information.flags & _posix_file_is_directory))
			continue;
		imported += port_tag_profiles_shared_import(name) >= 0;
	}
	if (directory)
		posix_directory_close(directory);
	return imported;
}
