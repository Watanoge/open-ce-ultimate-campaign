/*
TAG_EDITOR.H

The Tag Editor (tag_editor.c): the values and the HUD of the Tag Injector's
weapons changed as they are played, from the overlay menu (overlay_menu.c's
TAG EDITOR), and kept in their profile as edits (port_tag_profiles.h's
port_tag_profiles_tag_*edit*), which every level they are added to gives
them (tag_injector.c), a network game's clients too (its share carries
them: tag_multiplayer.c).
*/

#ifndef __TAG_EDITOR_H
#define __TAG_EDITOR_H
#pragma once

/* ---------- constants */

/* what a row of the overlay's TAG EDITOR is */
enum tag_editor_row_kind
{
	/* words alone: A does nothing */
	_tag_editor_row_text,
	/* a list of more rows: A or right goes into it */
	_tag_editor_row_folder,
	/* a value: left and right change it, A puts it back as its map had it */
	_tag_editor_row_value,
	/* something done when A is pressed (twice, for one that cannot be undone) */
	_tag_editor_row_command,

	NUMBER_OF_TAG_EDITOR_ROW_KINDS
};

enum
{
	TAG_EDITOR_LABEL_LENGTH = 64,
	TAG_EDITOR_VALUE_LENGTH = 24,
	TAG_EDITOR_NOTE_LENGTH = 128,
	/* the most rows a list has */
	TAG_EDITOR_MAXIMUM_ROWS = 160,
	/* the on-screen keyboard a name is typed with: its columns, and its
	rows (the last of wide keys, each TAG_EDITOR_KEYBOARD_WIDE columns) */
	TAG_EDITOR_KEYBOARD_COLUMNS = 10,
	TAG_EDITOR_KEYBOARD_ROWS = 5,
	TAG_EDITOR_KEYBOARD_WIDE = 2,
};

/* ---------- structures */

struct tag_editor_row
{
	short kind;
	char label[TAG_EDITOR_LABEL_LENGTH];
	char value[TAG_EDITOR_VALUE_LENGTH];
	/* a line about it, shown while it is chosen ("" for none) */
	char note[TAG_EDITOR_NOTE_LENGTH];
};

/* ---------- prototypes/TAG_EDITOR.C */

/* tag_injector.c's, as a level adds a profile's tags: a weapon of the
profile's bundles given the edits its tag of the profile has. A projectile,
a damage effect or a HUD it edits is given to it as a copy of its own (named
for its edits: another weapon of the same edits has the same copy), so that
the weapons that share it and are not edited, or are edited otherwise, keep
theirs. */
void tag_editor_weapon_loaded(
	long profile,
	long weapon_datum);
/* the level's tags let go (tag_injector_tags_unloaded) */
void tag_editor_level_unloaded(
	void);
/* a weapon's pickup prompt as its tag's Pickup settings have it
(source/interface/hud.c): `text_wanted` TRUE when it names the weapon in
words rather than with its HUD's icon, and the words typed (NULL for
none: its name as the map has it, or not words at all) */
unsigned short const *tag_editor_pickup_text(
	long weapon,
	unsigned char *text_wanted);

/* The overlay's TAG EDITOR: the weapon the first local player holds, its
values and its HUD, in lists. _open starts at its first list; _title is the
list's name; _rows its rows (`capacity` at most: how many); _choose is A on
a row, _adjust left (-1) or right (1) on one (`fast` when held a while),
each saying what it did in `status` ("" for nothing); _back goes up a list
(FALSE at the first, which the overlay leaves). `selected` is the row
chosen, which going into a list or back out of one sets. */
void tag_editor_menu_open(
	void);
char const *tag_editor_menu_title(
	void);
short tag_editor_menu_rows(
	struct tag_editor_row *rows,
	short capacity);
void tag_editor_menu_choose(
	short row,
	short *selected,
	char *status,
	long status_size);
void tag_editor_menu_adjust(
	short row,
	short direction,
	boolean fast,
	char *status,
	long status_size);
boolean tag_editor_menu_back(
	short *selected);
/* The HUD's elements moved over the game (A on one of the HUD list's):
while it is (_capturing), the overlay draws none of its panel, only
_capture_text's two lines at the screen's foot, and gives each frame's
buttons (halo_overlay_menu.h's bits: pressed since the last frame, and held)
to _capture. */
boolean tag_editor_menu_capturing(
	void);
void tag_editor_menu_capture(
	unsigned long pressed,
	unsigned long held);
void tag_editor_menu_capture_text(
	char *first,
	char *second,
	long size);
/* A name typed (Tag settings' Name), as a HUD element is moved: while it is
(_typing), the overlay draws an on-screen keyboard over _capture_text's
lines too. _keyboard_key is a key's label ("" for none: a wide key of the
last row is given at its first column alone), whether it is the key picked,
and whether it is lit (capitals on); _keyboard_press presses one (a click,
a tap). */
boolean tag_editor_menu_typing(
	void);
void tag_editor_menu_keyboard_key(
	short row,
	short column,
	char *label,
	long size,
	boolean *picked,
	boolean *lit);
void tag_editor_menu_keyboard_press(
	short row,
	short column);
/* each frame the overlay is open, and as it closes (`closing`): the edits
made written to the profile a moment after the last */
void tag_editor_menu_update(
	boolean closing);

#endif
