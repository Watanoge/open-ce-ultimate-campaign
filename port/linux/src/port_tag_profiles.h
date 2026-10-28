/*
PORT_TAG_PROFILES.H

The tag injector's profiles in the data root's tag_injector folder
(port_tag_profiles.c): each a name and the tags taken out of Custom Edition
maps into it, as tag bundles, which the randomizer's Go Crazy adds to a
level (port/linux/game/tag_injector.c, which declares these for itself:
keep the two the same).
*/

#ifndef PORT_TAG_PROFILES_H
#define PORT_TAG_PROFILES_H

struct port_tag_store_region;

/* finds the profiles again: how many there are. Every profile and tag index
from before is gone */
long port_tag_profiles_scan(void);
/* how many profiles the last scan found, and how many times they have been
found (a profile made, deleted or changed is a scan) */
long port_tag_profiles_count(void);
unsigned long port_tag_profiles_scans(void);
/* a profile's name, as it was typed, and its folder's (the setting
game.tag_injection_profile names it by); "" past them */
const char *port_tag_profiles_name(long profile);
const char *port_tag_profiles_folder(long profile);
/* the profile of the folder `folder` (any case), or -1 */
long port_tag_profiles_find(const char *folder);
/* makes a profile named `name` (its folder's name made of its letters,
digits, spaces, - and _): its index, or -1 */
long port_tag_profiles_create(const char *name);
/* makes a profile named `name` (as port_tag_profiles_create) that is a copy
of another: its tags, their settings and its pack. Its index, or -1 (a copy
that could not be made whole is deleted) */
long port_tag_profiles_duplicate(long profile, const char *name);
/* deletes a profile, its folder and its bundles; 0 if it cannot */
int port_tag_profiles_delete(long profile);

/* a profile's tags, each the map it was taken out of (its file's name), its
group and its path */
long port_tag_profiles_tag_count(long profile);
const char *port_tag_profiles_tag_map(long profile, long tag);
unsigned long port_tag_profiles_tag_group(long profile, long tag);
const char *port_tag_profiles_tag_name(long profile, long tag);
/* a tag's bundle's bytes, as the last scan found it (0: missing), and the
bytes a profile's folder holds, all its tags' and its pack's (which keeps
what they share once); whether a tag is kept as a tag file and the pack's
regions (port_tag_store.c), not a bundle as it was taken out */
unsigned long port_tag_profiles_tag_size(long profile, long tag);
unsigned long port_tag_profiles_size(long profile);
int port_tag_profiles_tag_stored(long profile, long tag);
/* whether a tag is played (on), or kept in the profile and left out (off);
set_ writes the profile: 0 if it cannot. Turning a tag on clears its
"failed" setting */
int port_tag_profiles_tag_enabled(long profile, long tag);
int port_tag_profiles_tag_set_enabled(long profile, long tag, int enabled);
/* a tag's setting, by its key: "faction", "type" (an actor variant's) and
"class" (a weapon's), the randomizer's names for them, "custom_type", the
name of its own type it is chosen by, "hud", the HUD a weapon is drawn
with (tag_injector.h's stock HUDs), "replace", "true" when the tag
takes the place of the campaign's of its path (profile.toml's boolean), and
"name", the name the menus show it by (its path stays its map's), and
"failed", why it could not be played in the level last loaded, which turned
it off (tag_injector.c's tag_injector_failure_describe); "" when it is not
set (the randomizer finds it by the tag's name, its custom type is
the name it is shown by, a weapon keeps its own HUD, the campaign's tag
stays, and it is shown by its path's last part). set_ keeps the value's
letters, digits, spaces, - and _ (lower case but for custom_type's and
name's), and writes the profile: 0 if it cannot */
const char *port_tag_profiles_tag_option(long profile, long tag, const char *key);
int port_tag_profiles_tag_set_option(long profile, long tag, const char *key, const char *value);
/* A tag's edits (the Tag Editor's: port/linux/game/tag_editor.c): values
of its tags changed from its map's, each a number by its key
("trigger1.rate_of_fire"), which a level gives its tags as it adds them
([tags.edits] in profile.toml). How many, and of each its key and value;
_find says whether it has one of a key, and its value; set_ adds one or
changes it, clear_edits takes out those whose keys begin with prefix (all
of them: "") and clear_edit the one of a key, each writing the profile: 0 if
it cannot (a key of other than lower case letters, digits, . and _, or past
the most a tag has). */
long port_tag_profiles_tag_edit_count(long profile, long tag);
const char *port_tag_profiles_tag_edit_key(long profile, long tag, long edit);
double port_tag_profiles_tag_edit_value(long profile, long tag, long edit);
int port_tag_profiles_tag_edit_find(long profile, long tag, const char *key, double *value);
int port_tag_profiles_tag_set_edit(long profile, long tag, const char *key, double value);
int port_tag_profiles_tag_clear_edits(long profile, long tag, const char *prefix);
int port_tag_profiles_tag_clear_edit(long profile, long tag, const char *key);
/* a hash of a tag's settings and edits (what a share of it carries
besides its bundle), which changes when they do */
unsigned long port_tag_profiles_tag_signature(long profile, long tag);
/* the tag of a group and path (any case) of a profile, or -1 */
long port_tag_profiles_tag_find(long profile, unsigned long group, const char *name);
/* adds a tag to a profile, with its bundle (`size` bytes), or replaces the
one of its group and path: with its file data's regions (`region_count` of
them, after its first `tags_size` bytes: tag_bundle_load_tags_regions), it
is kept as a tag file and the profile's pack has its regions, else as it
is; 0 if it cannot be written */
int port_tag_profiles_tag_add(long profile, const char *map, unsigned long group, const char *name,
	const void *bundle, unsigned long size, unsigned long tags_size, const struct port_tag_store_region *regions,
	long region_count);
/* takes a tag out of a profile, and deletes its bundle; 0 if it cannot */
int port_tag_profiles_tag_remove(long profile, long tag);
/* opens a tag's bundle for a level to read from, whatever profiles are
found meanwhile: a handle, and its size; -1 if it cannot be */
long port_tag_profiles_bundle_open(long profile, long tag, unsigned long *size);
/* reads `size` bytes at `offset` of an open bundle into buffer (the Xbox's
memory too); 0 unless all of them were read */
int port_tag_profiles_bundle_read_at(long handle, unsigned long offset, void *buffer, unsigned long size);
void port_tag_profiles_bundle_close(long handle);

/* writes a file whole, beside it first (a write cut short leaves it as it
was), and puts a file in another's place; 0 if it cannot */
int port_tag_profiles_file_write(const char *path, const void *bytes, unsigned long size);
int port_tag_profiles_file_replace(const char *from, const char *to);

/* A network game's shared profile (port/linux/game/tag_multiplayer.c): a
folder of tag_injector/.shared named for its id (lowercase letters and
digits: the host's own "host", a download's its hash's first digits),
opened alone into a slot of its own that no scan lists or forgets: the
index the functions above take it by, or -1. _close lets go of the slot;
_present says whether a folder of that id has a profile.toml; _delete
deletes the folder; _remove deletes a file of the shared folder (an
archive); _folder_make makes the shared folder (a download is written into
it). The path of a file of the shared folder (<id>.tagshare) into `path`. */
long port_tag_profiles_shared_open(const char *id);
void port_tag_profiles_shared_close(void);
int port_tag_profiles_shared(long profile);
int port_tag_profiles_shared_present(const char *id);
int port_tag_profiles_shared_delete(const char *id);
int port_tag_profiles_shared_remove(const char *name);
int port_tag_profiles_shared_folder_make(void);
void port_tag_profiles_shared_path(const char *name, char *path, unsigned long size);
/* A profile's tags on (of a group, or all: 0) in one file, `archive`: their
files, the profile's pack and its index as they are, a profile.toml of
them alone (each on, none failed), and the file `multiplayer` (a
multiplayer profile's: NULL for none) as multiplayer.toml; how many tags, 0
if none or it cannot be written. An archive made a shared profile's folder
(whole, or not at all: a folder of that id before is replaced); 0 if it
cannot be. */
long port_tag_profiles_shared_build(long profile, unsigned long group, const char *multiplayer, const char *archive);
int port_tag_profiles_shared_unpack(const char *archive, const char *id);
/* A client's copy of a share it downloaded, its own to play and change: a
profile named as the host's with " (online)" (its tags, settings and pack),
and the host's multiplayer profile playing it, if the share has one; made
once a share (a marker, <id>.imported, beside it: deleted, it is not made
again). The copy's index, else -1 (made before, or it cannot be). _all
copies every download not copied yet: how many. */
long port_tag_profiles_shared_import(const char *id);
long port_tag_profiles_shared_import_all(void);

#endif
