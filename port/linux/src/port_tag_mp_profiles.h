/*
PORT_TAG_MP_PROFILES.H

The Tag Injector's multiplayer profiles in the data root's
tag_injector/multiplayer folder (port_tag_mp_profiles.c): each a name, the
profile whose weapons it plays (port_tag_profiles.h) and how a multiplayer
game plays them (port/linux/game/tag_multiplayer.c, which declares these
for itself: keep the two the same). And its campaign profiles, in
tag_injector/campaign, kept the same way: each a name, the profile whose
weapons and AI it plays and how a campaign level played alone plays them
(port/linux/game/tag_campaign.c, which declares these for itself too).
*/

#ifndef PORT_TAG_MP_PROFILES_H
#define PORT_TAG_MP_PROFILES_H

/* whether a folder of tag_injector is the multiplayer or the campaign
profiles' (any case), which is no profile's */
int port_tag_mp_profiles_folder_reserved(const char *folder);
/* finds the multiplayer profiles again: how many there are (every index
from before is gone); how many the last scan found, and how many times they
have been found or changed */
long port_tag_mp_profiles_scan(void);
long port_tag_mp_profiles_count(void);
unsigned long port_tag_mp_profiles_scans(void);
/* a profile's name, as it was typed, and its file's (without .toml: the
setting game.multiplayer_tag_profile names it by); "" past them */
const char *port_tag_mp_profiles_name(long profile);
const char *port_tag_mp_profiles_file(long profile);
/* the profile of the file `file` (any case), or -1 */
long port_tag_mp_profiles_find(const char *file);
/* makes a profile named `name` (its file's name made of its letters,
digits, spaces, - and _) playing the profile of the folder `profile`, its
settings the defaults (rotation "add", replace "random", loadout "map"):
its index, or -1 */
long port_tag_mp_profiles_create(const char *name, const char *profile);
/* deletes a profile's file; 0 if it cannot */
int port_tag_mp_profiles_delete(long profile);
/* the name it is shown by (its file's stays); 0 if it has no letter or
digit, or cannot be written */
int port_tag_mp_profiles_rename(long profile, const char *name);
/* a setting, by its key ("profile", "rotation", "replace", "loadout",
"primary", "secondary"): "" when it is not set. set_ writes the profile: 0
if it cannot */
const char *port_tag_mp_profiles_option(long profile, const char *key);
int port_tag_mp_profiles_set_option(long profile, const char *key, const char *value);
/* the weapons taking the place of the map's weapon of a key ("shotgun"),
by their paths ("original": the map's own): how many, each, whether one is
among them, and one added or taken out (written: 0 if it cannot be) */
long port_tag_mp_profiles_manual_count(long profile, const char *key);
const char *port_tag_mp_profiles_manual(long profile, const char *key, long entry);
int port_tag_mp_profiles_manual_has(long profile, const char *key, const char *path);
int port_tag_mp_profiles_manual_set(long profile, const char *key, const char *path, int on);
/* the path of a profile's file (shared with a network game's players:
port_tag_profiles_shared_build), "" past them */
void port_tag_mp_profiles_path(long profile, char *path, unsigned long size);
/* a copy of the profile of the file at `path` (a share's multiplayer.toml),
named as it is with `suffix` after, in a file of its own, playing the
profile of the folder `profile`: its index, or -1 */
long port_tag_mp_profiles_import(const char *path, const char *suffix, const char *profile);

/* the campaign profiles, as the multiplayer ones are: their settings'
keys "profile", "weapon_rotation", "weapon_replace", "enemy_rotation" and
"enemy_replace" (a new one's "add", "random", "add", "random"); their
tables (a section: "weapons", "enemies" or "loadout") the tags chosen for
a key of one ("shotgun" of "weapons", "elite" of "enemies", an actor
variant's path of "loadout"), by their paths ("original": the level's own;
"stock:" and a weapon's key) */
long port_tag_campaign_profiles_scan(void);
long port_tag_campaign_profiles_count(void);
unsigned long port_tag_campaign_profiles_scans(void);
const char *port_tag_campaign_profiles_name(long profile);
const char *port_tag_campaign_profiles_file(long profile);
long port_tag_campaign_profiles_find(const char *file);
long port_tag_campaign_profiles_create(const char *name, const char *profile);
int port_tag_campaign_profiles_delete(long profile);
int port_tag_campaign_profiles_rename(long profile, const char *name);
const char *port_tag_campaign_profiles_option(long profile, const char *key);
int port_tag_campaign_profiles_set_option(long profile, const char *key, const char *value);
long port_tag_campaign_profiles_list_count(long profile, const char *section, const char *key);
const char *port_tag_campaign_profiles_list(long profile, const char *section, const char *key, long entry);
int port_tag_campaign_profiles_list_has(long profile, const char *section, const char *key, const char *path);
int port_tag_campaign_profiles_list_set(long profile, const char *section, const char *key, const char *path, int on);
/* a campaign profile's file's path, and a copy of a share's campaign.toml
(a co-op game's), as the multiplayer profiles' */
void port_tag_campaign_profiles_path(long profile, char *path, unsigned long size);
long port_tag_campaign_profiles_import(const char *path, const char *suffix, const char *profile);

#endif
