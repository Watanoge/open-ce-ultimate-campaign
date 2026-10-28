/*
TAG_EDITOR.C

The Tag Editor (tag_editor.h; docs/tag_injector.md, "The Tag Editor"): the
values and the HUD of the Tag Injector's weapons, changed from the overlay
menu (overlay_menu.c's TAG EDITOR) while one is held, and kept in the
weapon's tag of its profile as edits (port_tag_profiles.h), each a number
by its key:

	weapon.<field>                   the weapon's own (its zoom, its heat)
	trigger<n>.<field>               a trigger's (its rate of fire, spread)
	magazine<n>.<field>              a magazine's (its size)
	projectile<n>.<field>            the projectile trigger n fires
	projectile<n>.impact.<field>     its damage on a hit
	projectile<n>.stuck.<field>      its damage stuck to what it hit
	projectile<n>.explosion.<field>  its detonation effect's damage
	melee.<field>                    the weapon's melee damage
	hud.anchor                       the corner its HUD is drawn from
	hud.<element><n>.<x|y|sx|sy>     a HUD element's offset and scale
	hud.<element><n>.<m>.<x|y|sx|sy> a crosshair's or overlay's item's
	hud.<element><n>[.<m>].hide      1: the element (the item) not drawn
	hud.parent<l>.<any of hud.'s>    of its HUD's parent HUD l levels up
	                                 (parent1 its parent, parent2 that
	                                 one's: a visor drawn for many weapons)

each value as the tag keeps it (a projectile's speeds a tick's, angles in
radians), which the menu shows as Guerilla does (a second's, degrees).

A level adds a profile's weapons with their edits (tag_editor_weapon_loaded,
from tag_injector_tags_load): the weapon's own values are changed in it, and
a projectile, a damage effect, a detonation effect or a HUD it changes is
given to it as a copy of its own (a parent HUD changed: every HUD up to it,
each naming the next's copy) (tag_injector_tag_copy), named for the
edits (its path, # and their hash), so that the other weapons that share it
keep theirs, and those edited the same way share one copy: five weapons of
one damage effect, two of them not edited, two edited one way and one
another, play three. Its clients play the same (the host's share carries
the edits: tag_multiplayer.c).

While a weapon is held, a change is made to the tag it plays at once (a
copy shared with others changes for them too, until the level is loaded
again and they are given their own), but in a network game with other
machines, whose tags must stay as the host's: there a weapon's values
change when the next game loads, and a HUD moved is this machine's at once.
A client of a network game changes nothing: it plays the host's.
*/

/* ---------- headers */

#include "cseries.h"
#include "errors.h"
#include "effects/effect_definitions.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "interface/hud_definitions.h"
#include "interface/weapon_hud_interface_definition.h"
#include "items/projectile_definitions.h"
#include "items/weapon_definitions.h"
#include "networking/network_game_globals.h"
#include "networking/network_server_manager.h"
#include "objects/damage_effect_definitions.h"
#include "objects/objects.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "units/units.h"
#include "halo_overlay_menu.h"
#include "tag_editor.h"
#include "tag_injector.h"
#include "tag_multiplayer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- constants */

enum
{
	PROJECTILE_GROUP_TAG = 'proj',
	DAMAGE_EFFECT_GROUP_TAG = 'jpt!',
	EFFECT_GROUP_TAG = 'effe',
	WEAPON_HUD_GROUP_TAG = 'wphi',

	EDIT_KEY_LENGTH = 48,
	/* the weapons a level gives edits, the values kept as they were before
	they were changed (to put back), and the edits waiting to be written */
	MAXIMUM_EDITOR_WEAPONS = 128,
	MAXIMUM_ORIGINALS = 2048,
	MAXIMUM_PENDING = 64,
	/* the lists gone into, the rows of one, the weapons named as sharing a
	tag, and the time after the last change its edits are written */
	MAXIMUM_EDITOR_DEPTH = 6,
	MAXIMUM_EDITOR_ROWS = TAG_EDITOR_MAXIMUM_ROWS,
	MAXIMUM_SHARERS = 32,
	EDITOR_WRITE_MILLISECONDS = 600,
	/* the HUD's elements moved: a step and a held one's, the time before a
	held direction repeats and how often, the time an element chosen
	blinks, and how far it is moved to blink */
	HUD_STEP = 1,
	HUD_FAST_STEP = 4,
	HUD_REPEAT_DELAY_MILLISECONDS = 300,
	HUD_REPEAT_MILLISECONDS = 50,
	HUD_FAST_AFTER_MILLISECONDS = 1200,
	HUD_BLINK_MILLISECONDS = 1200,
	HUD_BLINK_PERIOD_MILLISECONDS = 200,
	HUD_BLINK_OFFSET = 4000,
	/* a name typed: its length at most (a profile's settings' values are
	31 characters: port_tag_profiles.c) */
	RENAME_LENGTH = 31,

	/* a weapon HUD interface's elements (tag_schema_effects.c's structures):
	of each its bytes, where its placement is in it, and where an element
	with items (a crosshair's, an overlay's) has their block */
	HUD_STATIC_BYTES = 0xB4,
	HUD_METER_BYTES = 0xB4,
	HUD_NUMBER_BYTES = 0xA0,
	HUD_ELEMENT_PLACEMENT_OFFSET = 0x24,
	HUD_ITEMS_ELEMENT_BYTES = 0x68,
	HUD_ITEMS_OFFSET = 0x34,
	HUD_CROSSHAIR_ITEM_BYTES = 0x6C,
	HUD_OVERLAY_ITEM_BYTES = 0x88,
	/* their bitmaps (a static's and a meter's, a crosshair's and an
	overlay's: a number has none) */
	HUD_ELEMENT_BITMAP_OFFSET = 0x48,
	HUD_ITEMS_BITMAP_OFFSET = 0x24,
	/* an element hidden: its map type (hud_weapon.c draws one whose bit
	of the map's types is set: this bit never is), and an item hidden: its
	runtime invalid flag (a crosshair's flags' bit 7, an overlay's bit 1) */
	HUD_ELEMENT_MAP_TYPE_OFFSET = 0x04,
	HUD_HIDDEN_MAP_TYPE = 15,
	HUD_CROSSHAIR_ITEM_FLAGS_OFFSET = 0x48,
	HUD_CROSSHAIR_ITEM_HIDDEN_FLAG = 1 << 7,
	HUD_OVERLAY_ITEM_FLAGS_OFFSET = 0x4C,
	HUD_OVERLAY_ITEM_HIDDEN_FLAG = 1 << 1,
	/* a HUD and its parents (hud_weapon.c's MAXIMUM_WEAPON_HUD_DEFINITION_DEPTH) */
	MAXIMUM_HUD_LEVELS = 16,
};

#define EDITOR_PI 3.14159265358979f
#define EDITOR_TICKS_PER_SECOND 30.0f

/* what an edit is of */
enum edit_target
{
	_target_weapon,
	_target_trigger,
	_target_magazine,
	_target_projectile,
	_target_damage,
	_target_hud,

	NUMBER_OF_EDIT_TARGETS
};

/* a damage effect's, by what it is of (the key's word) */
enum edit_damage_part
{
	_damage_impact,
	_damage_stuck,
	_damage_explosion,
	_damage_melee,

	NUMBER_OF_EDIT_DAMAGE_PARTS
};

/* how a value is kept, and shown */
enum edit_kind
{
	/* a real, as it is */
	_kind_real,
	/* a short */
	_kind_short,
	/* radians, shown in degrees */
	_kind_angle,
	/* world units a tick, shown a second's */
	_kind_speed,
	/* whether a HUD element (its map type), a crosshair's item or an
	overlay's (its flags: a short) is hidden, 1 or 0; kept as it was as
	the short it is (value_raw) */
	_kind_hidden_element,
	_kind_hidden_crosshair_item,
	_kind_hidden_overlay_item,
};

/* the HUD's elements, by what they are (the key's word) */
enum edit_hud_kind
{
	_hud_static,
	_hud_meter,
	_hud_number,
	_hud_crosshair,
	_hud_overlay,

	NUMBER_OF_EDIT_HUD_KINDS
};

/* an element's placement values (the key's last word) */
enum edit_hud_value
{
	_hud_x,
	_hud_y,
	_hud_scale_x,
	_hud_scale_y,
	NUMBER_OF_HUD_PLACEMENT_VALUES,
	/* (whether it is drawn) */
	_hud_hide = NUMBER_OF_HUD_PLACEMENT_VALUES,

	NUMBER_OF_EDIT_HUD_VALUES
};

/* a list of the menu's */
enum editor_page
{
	_page_root,
	/* the first list's: the weapon's values, its HUDs, and its tag's
	settings in the profile */
	_page_stats,
	_page_huds,
	_page_settings,
	_page_weapon,
	_page_trigger,
	_page_magazine,
	_page_projectile,
	_page_damage,
	_page_hud,
};

/* what a row of a list does */
enum editor_action
{
	_action_none,
	_action_page,
	_action_field,
	_action_scope,
	_action_hud_anchor,
	_action_hud_element,
	_action_reset_page,
	_action_reset_all,
	/* the tag's name in the profile typed, and the tag taken out of it */
	_action_rename,
	_action_remove,
};

/* ---------- structures */

/* a value the menu changes: of what, its key's last word, its name, where
it is, how it is kept, and its step and bounds (as shown) */
struct edit_field
{
	short target;
	char const *key;
	char const *label;
	unsigned short offset;
	short kind;
	real step;
	real minimum;
	real maximum;
};

/* a key read: its field (NULL for the HUD's), of what (its trigger,
magazine or projectile, or its HUD's level: 0 its own, 1 its parent; its
damage effect's part), or its HUD element (its kind, element and item: NONE
for none) and value (NONE: the anchor) */
struct edit_key
{
	short target;
	short index;
	short part;
	struct edit_field const *field;
	short hud_kind;
	short hud_element;
	short hud_item;
	short hud_value;
};

/* a row of a list: what it is and does, its words, and of what */
struct row_spec
{
	short kind;
	short action;
	short arguments[3];
	struct edit_field const *field;
	char label[TAG_EDITOR_LABEL_LENGTH];
};

/* a weapon of the level that shares the tag of a list (a projectile, a
damage effect, a HUD): the weapon, and its keys' beginning for it */
struct sharer
{
	long weapon;
	char prefix[EDIT_KEY_LENGTH];
};

/* the weapon held, and what the menu may do with it */
struct editor_context
{
	long weapon;
	/* its profile's (the one edits are written to: a network game's host
	writes the profile it shares; NONE: none, edits are not made) and its
	tag there */
	long profile;
	long tag;
	/* whether a change is made to the tag played at once */
	boolean live;
	char name[TAG_EDITOR_LABEL_LENGTH];
	/* why nothing can be changed ("" when it can) */
	char why[TAG_EDITOR_NOTE_LENGTH];
};

/* (cache_files.c's table, as tag_injector.c has it) */
struct editor_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

/* ---------- prototypes */

void *cache_files_tag_instances(long *count);
unsigned long system_milliseconds(void);
long port_tag_profiles_find(char const *folder);
long port_tag_profiles_tag_find(long profile, unsigned long group, char const *name);
long port_tag_profiles_tag_edit_count(long profile, long tag);
char const *port_tag_profiles_tag_edit_key(long profile, long tag, long edit);
double port_tag_profiles_tag_edit_value(long profile, long tag, long edit);
int port_tag_profiles_tag_edit_find(long profile, long tag, char const *key, double *value);
int port_tag_profiles_tag_set_edit(long profile, long tag, char const *key, double value);
int port_tag_profiles_tag_clear_edit(long profile, long tag, char const *key);
int port_tag_profiles_tag_clear_edits(long profile, long tag, char const *prefix);
int port_tag_profiles_shared(long profile);
int port_tag_profiles_tag_set_option(long profile, long tag, const char *key, const char *value);
int port_tag_profiles_tag_remove(long profile, long tag);
char const *port_tag_mp_profiles_option(long profile, char const *key);

/* ---------- the values */

#define WEAPON_FIELD(key, label, member, kind, step, minimum, maximum) \
	{ _target_weapon, key, label, (unsigned short)offsetof(struct weapon_definition, weapon.member), kind, step, \
		minimum, maximum }
#define TRIGGER_FIELD(key, label, member, kind, step, minimum, maximum) \
	{ _target_trigger, key, label, (unsigned short)offsetof(struct weapon_trigger_definition, member), kind, step, \
		minimum, maximum }
#define MAGAZINE_FIELD(key, label, member, kind, step, minimum, maximum) \
	{ _target_magazine, key, label, (unsigned short)offsetof(struct weapon_magazine_definition, member), kind, step, \
		minimum, maximum }
#define PROJECTILE_FIELD(key, label, member, kind, step, minimum, maximum) \
	{ _target_projectile, key, label, (unsigned short)offsetof(struct projectile_definition, projectile.member), kind, \
		step, minimum, maximum }
#define DAMAGE_FIELD(key, label, member, kind, step, minimum, maximum) \
	{ _target_damage, key, label, (unsigned short)offsetof(struct damage_effect_definition, member), kind, step, \
		minimum, maximum }

static struct edit_field const edit_fields[] =
{
	WEAPON_FIELD("zoom_min", "Zoom (first)", zoom_magnification_minimum, _kind_real, 0.1f, 1.0f, 20.0f),
	WEAPON_FIELD("zoom_max", "Zoom (last)", zoom_magnification_maximum, _kind_real, 0.1f, 1.0f, 20.0f),
	WEAPON_FIELD("heat_overheat", "Overheats at", heat_overheated_threshold, _kind_real, 0.05f, 0.0f, 1.0f),
	WEAPON_FIELD("heat_recover", "Cools down to", heat_recovery_threshold, _kind_real, 0.05f, 0.0f, 1.0f),
	WEAPON_FIELD("heat_loss", "Heat lost a second", heat_loss_per_second, _kind_real, 0.05f, 0.0f, 10.0f),
	WEAPON_FIELD("autoaim_angle", "Autoaim angle (deg)", aim_assist_parameters.autoaim_angle, _kind_angle, 0.5f, 0.0f,
		45.0f),
	WEAPON_FIELD("autoaim_range", "Autoaim range", aim_assist_parameters.autoaim_distance, _kind_real, 1.0f, 0.0f,
		1000.0f),
	WEAPON_FIELD("magnetism_angle", "Magnetism angle (deg)", aim_assist_parameters.magnetism_angle, _kind_angle, 0.5f,
		0.0f, 45.0f),
	WEAPON_FIELD("magnetism_range", "Magnetism range", aim_assist_parameters.magnetism_distance, _kind_real, 1.0f,
		0.0f, 1000.0f),
	WEAPON_FIELD("move_penalty_forward", "Slows walking", forward_movement_penalty, _kind_real, 0.05f, 0.0f, 1.0f),
	WEAPON_FIELD("move_penalty_sideways", "Slows strafing", sideways_movement_penalty, _kind_real, 0.05f, 0.0f, 1.0f),

	TRIGGER_FIELD("rate_of_fire_min", "Rate of fire (first)", initial_rate_of_fire, _kind_real, 0.5f, 0.0f, 120.0f),
	TRIGGER_FIELD("rate_of_fire_max", "Rate of fire (held)", final_rate_of_fire, _kind_real, 0.5f, 0.0f, 120.0f),
	TRIGGER_FIELD("rounds_per_shot", "Rounds a shot", rounds_per_shot, _kind_short, 1.0f, 0.0f, 100.0f),
	TRIGGER_FIELD("projectiles_per_shot", "Projectiles a shot", projectiles_per_shot, _kind_short, 1.0f, 1.0f, 64.0f),
	TRIGGER_FIELD("spread_min", "Spread (first, deg)", projectile_error_angle_lower_bound, _kind_angle, 0.1f, 0.0f,
		45.0f),
	TRIGGER_FIELD("spread_max", "Spread (held, deg)", projectile_error_angle_upper_bound, _kind_angle, 0.1f, 0.0f,
		45.0f),
	TRIGGER_FIELD("distribution_angle", "Pellet spread (deg)", projectile_distribution_angle, _kind_angle, 0.5f, 0.0f,
		90.0f),
	TRIGGER_FIELD("heat_per_round", "Heat a round", heat_generated_per_round, _kind_real, 0.01f, 0.0f, 1.0f),
	TRIGGER_FIELD("charging_time", "Charging time (s)", charging_time, _kind_real, 0.1f, 0.0f, 10.0f),

	MAGAZINE_FIELD("size", "Magazine", rounds_loaded_maximum, _kind_short, 1.0f, 1.0f, 999.0f),
	MAGAZINE_FIELD("total_initial", "Ammo picked up", rounds_total_initial, _kind_short, 1.0f, 0.0f, 9999.0f),
	MAGAZINE_FIELD("total_maximum", "Ammo carried", rounds_total_maximum, _kind_short, 1.0f, 0.0f, 9999.0f),
	MAGAZINE_FIELD("rounds_reloaded", "Rounds a reload", rounds_reloaded, _kind_short, 1.0f, 1.0f, 999.0f),
	MAGAZINE_FIELD("recharge", "Recharged a second", rounds_recharged_per_second, _kind_short, 1.0f, 0.0f, 999.0f),

	PROJECTILE_FIELD("speed", "Speed", initial_velocity, _kind_speed, 5.0f, 0.0f, 3000.0f),
	PROJECTILE_FIELD("final_speed", "Speed (slowed to)", final_velocity, _kind_speed, 5.0f, 0.0f, 3000.0f),
	PROJECTILE_FIELD("gravity", "Gravity", air_gravity_scale, _kind_real, 0.05f, 0.0f, 10.0f),
	PROJECTILE_FIELD("range", "Range", maximum_range, _kind_real, 5.0f, 0.0f, 10000.0f),
	PROJECTILE_FIELD("falloff_start", "Damage falls from", air_damage_range_lower_bound, _kind_real, 1.0f, 0.0f,
		10000.0f),
	PROJECTILE_FIELD("falloff_end", "Damage gone at", air_damage_range_upper_bound, _kind_real, 1.0f, 0.0f, 10000.0f),
	PROJECTILE_FIELD("arming_time", "Arming time (s)", arming_time, _kind_real, 0.05f, 0.0f, 10.0f),
	PROJECTILE_FIELD("timer_min", "Fuse (shortest, s)", timer_lower_bound, _kind_real, 0.1f, 0.0f, 30.0f),
	PROJECTILE_FIELD("timer_max", "Fuse (longest, s)", timer_upper_bound, _kind_real, 0.1f, 0.0f, 30.0f),

	DAMAGE_FIELD("damage_min", "Damage (least)", damage.damage_lower_bound, _kind_real, 0.5f, 0.0f, 10000.0f),
	DAMAGE_FIELD("damage_max", "Damage (most)", damage.damage_upper_bound, _kind_real, 0.5f, 0.0f, 10000.0f),
	DAMAGE_FIELD("radius", "Radius", cutoff_radius, _kind_real, 0.1f, 0.0f, 100.0f),
	DAMAGE_FIELD("falloff", "Full damage within", falloff_radius, _kind_real, 0.1f, 0.0f, 100.0f),
	DAMAGE_FIELD("force", "Force", damage.instantaneous_acceleration, _kind_real, 0.5f, 0.0f, 500.0f),
	DAMAGE_FIELD("stun", "Stun", damage.stun, _kind_real, 0.05f, 0.0f, 1.0f),
};

/* the words of the keys of the damage effects' parts, and their lists'
names */
static char const *const damage_part_keys[NUMBER_OF_EDIT_DAMAGE_PARTS] = { "impact", "stuck", "explosion", "melee" };
static char const *const damage_part_names[NUMBER_OF_EDIT_DAMAGE_PARTS] =
	{ "Impact damage", "Stuck damage", "Explosion damage", "Melee damage" };
/* the HUD's elements' and values' words, and the elements' names */
static char const *const hud_kind_keys[NUMBER_OF_EDIT_HUD_KINDS] =
	{ "static", "meter", "number", "crosshair", "overlay" };
static char const *const hud_kind_names[NUMBER_OF_EDIT_HUD_KINDS] =
	{ "Static", "Meter", "Number", "Crosshair", "Overlay" };
static char const *const hud_value_keys[NUMBER_OF_EDIT_HUD_VALUES] = { "x", "y", "sx", "sy", "hide" };
static char const *const anchor_names[NUMBER_OF_HUD_ANCHORS] =
	{ "TOP LEFT", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM RIGHT", "CENTRE" };

/* ---------- globals */

static struct
{
	/* the level's: the weapons given edits as it loaded, and the values the
	edits changed, as they were (by the weapon and the key) */
	long weapons[MAXIMUM_EDITOR_WEAPONS];
	short weapon_count;
	struct
	{
		long weapon;
		char key[EDIT_KEY_LENGTH];
		real value;
	} originals[MAXIMUM_ORIGINALS];
	short original_count;

	/* the menu's: the lists gone into and the row chosen in each, whether
	a list's edits are made to every weapon that shares its tag, and the
	command pressed once (to be pressed again) */
	struct
	{
		short page;
		short index;
		short part;
		short selected;
	} pages[MAXIMUM_EDITOR_DEPTH];
	short depth;
	boolean everyone;
	short confirming;
	/* the weapon the lists are of (NONE: none) */
	long weapon;

	/* the edits made and not written yet, and when the last was */
	struct
	{
		long profile;
		long tag;
		char key[EDIT_KEY_LENGTH];
		double value;
		boolean clear;
	} pending[MAXIMUM_PENDING];
	short pending_count;
	unsigned long changed_at;

	/* the HUD element moved (its HUD's level, its kind, element and
	item), whether its scale is (not its offset), the buttons' repeat, its
	blink, and its placement as it was when it was chosen */
	boolean capturing;
	short hud_level;
	short hud_kind;
	short hud_element;
	short hud_item;
	boolean scaling;
	unsigned long repeat_at;
	unsigned long held_since;
	unsigned long blink_until;
	boolean blinked;
	point2d blink_offset;

	/* the name being typed (in place of the lists, as a HUD element is
	moved), its character chosen, and the buttons' repeat */
	boolean renaming;
	char name[RENAME_LENGTH + 1];
	short cursor;
	unsigned long rename_repeat_at;
} tag_editor;

/* the characters a name is typed of, in the order up steps through them */
static char const rename_characters[] =
	" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.()'+#";

static void pending_write(void);

/* ---------- private code: the tags */

static real field_factor(
	short kind)
{
	return kind == _kind_angle ? 180.0f / EDITOR_PI : kind == _kind_speed ? EDITOR_TICKS_PER_SECOND : 1.0f;
}

static void reference_set(
	struct tag_reference *reference,
	long datum)
{
	reference->index = datum;
	reference->name = datum == NONE ? reference->name : tag_get_name(datum);
	reference->name_length = datum == NONE || !reference->name ? 0 : (long)strlen(reference->name);
}

/* a tag's root, of its group, or NULL (NONE, or not of that group) */
static byte *tag_root(
	long group_tag,
	long datum)
{
	long count = 0;
	struct editor_tag_instance *instances = cache_files_tag_instances(&count);
	long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(datum);

	if (datum == NONE || !instances || absolute < 0 || absolute >= count || instances[absolute].tag_index != datum ||
		instances[absolute].group_tag != group_tag)
		return NULL;
	return instances[absolute].base_address;
}

static struct weapon_definition *weapon_definition(
	long weapon)
{
	return (struct weapon_definition *)tag_root(WEAPON_DEFINITION_TAG, weapon);
}

static struct weapon_trigger_definition *weapon_trigger(
	long weapon,
	short index)
{
	struct weapon_definition *definition = weapon_definition(weapon);

	return definition && index >= 0 && index < definition->weapon.triggers.count ?
		TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, index, struct weapon_trigger_definition) : NULL;
}

static struct weapon_magazine_definition *weapon_magazine(
	long weapon,
	short index)
{
	struct weapon_definition *definition = weapon_definition(weapon);

	return definition && index >= 0 && index < definition->weapon.magazines.count ?
		TAG_BLOCK_GET_ELEMENT(&definition->weapon.magazines, index, struct weapon_magazine_definition) : NULL;
}

/* a detonation effect of a projectile with a damage effect among its parts
(its own, else the first of its material responses'), and the part; NONE
for none */
static long projectile_explosion(
	long projectile,
	struct effect_part_definition **part)
{
	struct projectile_definition *definition = (struct projectile_definition *)tag_root(PROJECTILE_GROUP_TAG,
		projectile);
	long candidates[8];
	short candidate_count = 0;
	short candidate;
	long index;

	*part = NULL;
	if (!definition)
		return NONE;
	candidates[candidate_count++] = definition->projectile.effect.index;
	for (index = 0; index < definition->projectile.material_responses.count && candidate_count < 8; index++)
	{
		struct projectile_material_response_definition *response = TAG_BLOCK_GET_ELEMENT(
			&definition->projectile.material_responses, index, struct projectile_material_response_definition);

		candidates[candidate_count++] = response->detonation_effect.index;
	}
	for (candidate = 0; candidate < candidate_count; candidate++)
	{
		struct effect_definition *effect = (struct effect_definition *)tag_root(EFFECT_GROUP_TAG,
			candidates[candidate]);
		long event_index;

		for (event_index = 0; effect && event_index < effect->events.count; event_index++)
		{
			struct effect_event_definition *event = TAG_BLOCK_GET_ELEMENT(&effect->events, event_index,
				struct effect_event_definition);
			long part_index;

			for (part_index = 0; part_index < event->parts.count; part_index++)
			{
				struct effect_part_definition *found = TAG_BLOCK_GET_ELEMENT(&event->parts, part_index,
					struct effect_part_definition);

				if (tag_root(DAMAGE_EFFECT_GROUP_TAG, found->reference.index))
				{
					*part = found;
					return candidates[candidate];
				}
			}
		}
	}
	return NONE;
}

/* the damage effect of a part (a projectile's, else the weapon's melee),
or NONE */
static long damage_effect(
	long weapon,
	long projectile,
	short part)
{
	struct projectile_definition *definition = (struct projectile_definition *)tag_root(PROJECTILE_GROUP_TAG,
		projectile);
	struct effect_part_definition *effect_part;

	switch (part)
	{
	case _damage_impact:
		return definition ? definition->projectile.impact_damage.index : NONE;
	case _damage_stuck:
		return definition ? definition->projectile.attached_detonation_damage.index : NONE;
	case _damage_explosion:
		return projectile_explosion(projectile, &effect_part) != NONE ? effect_part->reference.index : NONE;
	case _damage_melee:
		return weapon_definition(weapon) ? weapon_definition(weapon)->weapon.melee_attack_damage.index : NONE;
	}
	return NONE;
}

/* a weapon's HUD of a level (0 its own, 1 its parent HUD, and so on), or
NONE */
static long weapon_hud(
	long weapon,
	short level)
{
	struct weapon_definition *definition = weapon_definition(weapon);
	long hud = definition ? definition->weapon.interface_definition.hud_interface.index : NONE;
	short up;

	for (up = 0; up < MAX(level, 0) && up < MAXIMUM_HUD_LEVELS; up++)
	{
		struct weapon_hud_interface_definition *hud_definition = (struct weapon_hud_interface_definition *)tag_root(
			WEAPON_HUD_GROUP_TAG, hud);

		hud = hud_definition ? hud_definition->parent_hud.index : NONE;
	}
	return level < MAXIMUM_HUD_LEVELS && tag_root(WEAPON_HUD_GROUP_TAG, hud) ? hud : NONE;
}

/* how many HUDs a weapon draws (its own and its parents) */
static short weapon_hud_levels(
	long weapon)
{
	short levels = 0;

	while (levels < MAXIMUM_HUD_LEVELS && weapon_hud(weapon, levels) != NONE)
		levels++;
	return levels;
}

/* the tag a key of a weapon's is a value of (the weapon's for its own, a
trigger's and a magazine's; a HUD's of its level): NONE for none */
static long target_tag(
	long weapon,
	short target,
	short index,
	short part)
{
	struct weapon_trigger_definition *trigger = weapon_trigger(weapon, index);
	struct weapon_definition *definition = weapon_definition(weapon);

	switch (target)
	{
	case _target_weapon:
	case _target_trigger:
	case _target_magazine:
		return definition ? weapon : NONE;
	case _target_projectile:
		return trigger && tag_root(PROJECTILE_GROUP_TAG, trigger->projectile.index) ? trigger->projectile.index : NONE;
	case _target_damage:
		return damage_effect(weapon, part == _damage_melee || !trigger ? NONE : trigger->projectile.index, part);
	case _target_hud:
		return weapon_hud(weapon, index);
	}
	return NONE;
}

/* a HUD element (its item, for a crosshair's or an overlay's: `element`,
when it is not NULL, its element), or NULL */
static byte *hud_part(
	long hud,
	short kind,
	short element,
	short item,
	byte **element_address)
{
	struct weapon_hud_interface_definition *definition = (struct weapon_hud_interface_definition *)tag_root(
		WEAPON_HUD_GROUP_TAG, hud);
	struct tag_block *block;
	long bytes;
	byte *address;

	if (!definition || element < 0)
		return NULL;
	switch (kind)
	{
	case _hud_static: block = &definition->statics; bytes = HUD_STATIC_BYTES; break;
	case _hud_meter: block = &definition->meters; bytes = HUD_METER_BYTES; break;
	case _hud_number: block = &definition->numbers; bytes = HUD_NUMBER_BYTES; break;
	case _hud_crosshair: block = &definition->crosshairs; bytes = HUD_ITEMS_ELEMENT_BYTES; break;
	case _hud_overlay: block = &definition->overlays; bytes = HUD_ITEMS_ELEMENT_BYTES; break;
	default: return NULL;
	}
	if (element >= block->count || !block->address)
		return NULL;
	address = (byte *)block->address + element * bytes;
	if (element_address)
		*element_address = address;
	if (kind == _hud_static || kind == _hud_meter || kind == _hud_number)
		return address;
	block = (struct tag_block *)(address + HUD_ITEMS_OFFSET);
	if (item < 0 || item >= block->count || !block->address)
		return NULL;
	return (byte *)block->address + item * (kind == _hud_crosshair ? HUD_CROSSHAIR_ITEM_BYTES :
		HUD_OVERLAY_ITEM_BYTES);
}

/* a HUD element's placement (its item's, for a crosshair's or an
overlay's), or NULL */
static struct hud_placement_definition *hud_placement(
	long hud,
	short kind,
	short element,
	short item)
{
	byte *address = hud_part(hud, kind, element, item, NULL);

	if (!address)
		return NULL;
	return (struct hud_placement_definition *)(kind == _hud_crosshair || kind == _hud_overlay ? address :
		address + HUD_ELEMENT_PLACEMENT_OFFSET);
}

/* where whether a HUD element (its item) is drawn is kept, and how, or
NULL */
static void *hud_hidden_address(
	long hud,
	short kind,
	short element,
	short item,
	short *value_kind)
{
	byte *address = hud_part(hud, kind, element, item, NULL);

	if (!address)
		return NULL;
	switch (kind)
	{
	case _hud_crosshair:
		*value_kind = _kind_hidden_crosshair_item;
		return address + HUD_CROSSHAIR_ITEM_FLAGS_OFFSET;
	case _hud_overlay:
		*value_kind = _kind_hidden_overlay_item;
		return address + HUD_OVERLAY_ITEM_FLAGS_OFFSET;
	}
	*value_kind = _kind_hidden_element;
	return address + HUD_ELEMENT_MAP_TYPE_OFFSET;
}

/* the last part of the path of a HUD element's bitmap ("" for none) */
static char const *hud_bitmap_name(
	long hud,
	short kind,
	short element)
{
	byte *address = NULL;
	struct tag_reference const *reference;
	char const *path;
	char const *leaf;

	hud_part(hud, kind, element, 0, &address);
	if (kind == _hud_number || !address)
		return "";
	reference = (struct tag_reference const *)(address + (kind == _hud_crosshair || kind == _hud_overlay ?
		HUD_ITEMS_BITMAP_OFFSET : HUD_ELEMENT_BITMAP_OFFSET));
	path = reference->index == NONE ? NULL : tag_get_name(reference->index);
	if (!path)
		return "";
	leaf = strrchr(path, '\\');
	return leaf ? leaf + 1 : path;
}

/* how many elements of a kind a HUD has, and items an element */
static short hud_count(
	long hud,
	short kind,
	short element)
{
	struct weapon_hud_interface_definition *definition = (struct weapon_hud_interface_definition *)tag_root(
		WEAPON_HUD_GROUP_TAG, hud);
	struct tag_block *block;

	if (!definition)
		return 0;
	block = kind == _hud_static ? &definition->statics : kind == _hud_meter ? &definition->meters :
		kind == _hud_number ? &definition->numbers : kind == _hud_crosshair ? &definition->crosshairs :
		&definition->overlays;
	if (element == NONE)
		return block->address ? (short)MIN(block->count, MAXIMUM_EDITOR_ROWS) : 0;
	if ((kind != _hud_crosshair && kind != _hud_overlay) || element < 0 || element >= block->count || !block->address)
		return 0;
	block = (struct tag_block *)((byte *)block->address + element * HUD_ITEMS_ELEMENT_BYTES + HUD_ITEMS_OFFSET);
	return block->address ? (short)MIN(block->count, MAXIMUM_EDITOR_ROWS) : 0;
}

/* ---------- private code: the keys */

static int prefix_number(
	char const *key,
	char const *word,
	short *number,
	char const **rest)
{
	size_t length = strlen(word);
	char *end;
	long value;

	if (strncmp(key, word, length) || key[length] < '0' || key[length] > '9')
		return FALSE;
	value = strtol(key + length, &end, 10);
	if (*end != '.' || value < 0 || value > 999)
		return FALSE;
	*number = (short)value;
	*rest = end + 1;
	return TRUE;
}

static struct edit_field const *field_find(
	short target,
	char const *word)
{
	short index;

	for (index = 0; index < (short)NUMBEROF(edit_fields); index++)
	{
		if (edit_fields[index].target == target && !strcmp(edit_fields[index].key, word))
			return &edit_fields[index];
	}
	return NULL;
}

/* a key read (edit_key); FALSE when it is none of the editor's */
static boolean key_parse(
	char const *key,
	struct edit_key *result)
{
	char const *rest;
	short number;
	short part;

	memset(result, 0, sizeof(*result));
	result->index = NONE;
	result->part = NONE;
	result->hud_kind = NONE;
	result->hud_element = NONE;
	result->hud_item = NONE;
	result->hud_value = NONE;
	if (!strncmp(key, "weapon.", 7))
	{
		result->target = _target_weapon;
		result->field = field_find(_target_weapon, key + 7);
		return result->field != NULL;
	}
	if (prefix_number(key, "trigger", &number, &rest))
	{
		result->target = _target_trigger;
		result->index = number;
		result->field = field_find(_target_trigger, rest);
		return result->field != NULL;
	}
	if (prefix_number(key, "magazine", &number, &rest))
	{
		result->target = _target_magazine;
		result->index = number;
		result->field = field_find(_target_magazine, rest);
		return result->field != NULL;
	}
	if (!strncmp(key, "melee.", 6))
	{
		result->target = _target_damage;
		result->part = _damage_melee;
		result->field = field_find(_target_damage, key + 6);
		return result->field != NULL;
	}
	if (prefix_number(key, "projectile", &number, &rest))
	{
		result->index = number;
		for (part = 0; part < _damage_melee; part++)
		{
			size_t length = strlen(damage_part_keys[part]);

			if (!strncmp(rest, damage_part_keys[part], length) && rest[length] == '.')
			{
				result->target = _target_damage;
				result->part = part;
				result->field = field_find(_target_damage, rest + length + 1);
				return result->field != NULL;
			}
		}
		result->target = _target_projectile;
		result->field = field_find(_target_projectile, rest);
		return result->field != NULL;
	}
	if (!strncmp(key, "hud.", 4))
	{
		char const *words = key + 4;
		short kind;

		result->target = _target_hud;
		result->index = 0;
		/* (a parent HUD's: its level) */
		if (prefix_number(words, "parent", &number, &rest))
		{
			if (number < 1 || number >= MAXIMUM_HUD_LEVELS)
				return FALSE;
			result->index = number;
			words = rest;
		}
		if (!strcmp(words, "anchor"))
			return TRUE;
		for (kind = 0; kind < NUMBER_OF_EDIT_HUD_KINDS; kind++)
		{
			short value;

			if (!prefix_number(words, hud_kind_keys[kind], &number, &rest))
				continue;
			result->hud_kind = kind;
			result->hud_element = number;
			if (kind == _hud_crosshair || kind == _hud_overlay)
			{
				char *end;
				long item = strtol(rest, &end, 10);

				if (end == rest || *end != '.' || item < 0 || item > 999)
					return FALSE;
				result->hud_item = (short)item;
				rest = end + 1;
			}
			for (value = 0; value < NUMBER_OF_EDIT_HUD_VALUES; value++)
			{
				if (!strcmp(rest, hud_value_keys[value]))
				{
					result->hud_value = value;
					return TRUE;
				}
			}
			return FALSE;
		}
	}
	return FALSE;
}

/* the beginning of the keys of a list's tag ("projectile0.impact."): of a
trigger's, a magazine's and the weapon's own, theirs */
static void key_prefix(
	short target,
	short index,
	short part,
	char *prefix,
	long size)
{
	switch (target)
	{
	case _target_weapon: snprintf(prefix, size, "weapon."); break;
	case _target_trigger: snprintf(prefix, size, "trigger%d.", index); break;
	case _target_magazine: snprintf(prefix, size, "magazine%d.", index); break;
	case _target_projectile: snprintf(prefix, size, "projectile%d.", index); break;
	case _target_damage:
		if (part == _damage_melee)
			snprintf(prefix, size, "melee.");
		else
			snprintf(prefix, size, "projectile%d.%s.", index, damage_part_keys[part]);
		break;
	case _target_hud:
		if (index > 0)
			snprintf(prefix, size, "hud.parent%d.", index);
		else
			snprintf(prefix, size, "hud.");
		break;
	default: prefix[0] = 0; break;
	}
}

static void hud_key(
	short level,
	short kind,
	short element,
	short item,
	short value,
	char *key,
	long size)
{
	char prefix[EDIT_KEY_LENGTH];

	key_prefix(_target_hud, level, NONE, prefix, sizeof(prefix));
	if (value == NONE)
		snprintf(key, size, "%sanchor", prefix);
	else if (kind == _hud_crosshair || kind == _hud_overlay)
		snprintf(key, size, "%s%s%d.%d.%s", prefix, hud_kind_keys[kind], element, item, hud_value_keys[value]);
	else
		snprintf(key, size, "%s%s%d.%s", prefix, hud_kind_keys[kind], element, hud_value_keys[value]);
}

/* where a key's value is in the tags of a weapon as they are, or NULL; and
how it is kept */
static void *key_address(
	long weapon,
	struct edit_key const *key,
	short *kind)
{
	byte *base = NULL;

	*kind = key->field ? key->field->kind : _kind_real;
	switch (key->target)
	{
	case _target_weapon: base = (byte *)weapon_definition(weapon); break;
	case _target_trigger: base = (byte *)weapon_trigger(weapon, key->index); break;
	case _target_magazine: base = (byte *)weapon_magazine(weapon, key->index); break;
	case _target_projectile:
		base = tag_root(PROJECTILE_GROUP_TAG, target_tag(weapon, _target_projectile, key->index, NONE));
		break;
	case _target_damage:
		base = tag_root(DAMAGE_EFFECT_GROUP_TAG, target_tag(weapon, _target_damage, key->index, key->part));
		break;
	case _target_hud:
	{
		long hud = target_tag(weapon, _target_hud, key->index, NONE);
		struct hud_placement_definition *placement;

		if (key->hud_value == _hud_hide)
			return hud_hidden_address(hud, key->hud_kind, key->hud_element, key->hud_item, kind);
		if (key->hud_value == NONE)
		{
			struct weapon_hud_interface_definition *definition = (struct weapon_hud_interface_definition *)tag_root(
				WEAPON_HUD_GROUP_TAG, hud);

			*kind = _kind_short;
			return definition ? &definition->absolute_placement.corner : NULL;
		}
		placement = hud_placement(hud, key->hud_kind, key->hud_element, key->hud_item);
		if (!placement)
			return NULL;
		*kind = key->hud_value <= _hud_y ? _kind_short : _kind_real;
		return key->hud_value == _hud_x ? (void *)&placement->offset.x : key->hud_value == _hud_y ?
			(void *)&placement->offset.y : key->hud_value == _hud_scale_x ? (void *)&placement->scale.i :
			(void *)&placement->scale.j;
	}
	}
	return base && key->field ? base + key->field->offset : NULL;
}

static boolean kind_hidden(
	short kind)
{
	return kind == _kind_hidden_element || kind == _kind_hidden_crosshair_item || kind == _kind_hidden_overlay_item;
}

static real value_read(
	void const *address,
	short kind)
{
	short raw = kind_hidden(kind) ? *(short const *)address : 0;

	switch (kind)
	{
	case _kind_short: return (real)*(short const *)address;
	case _kind_hidden_element: return raw == HUD_HIDDEN_MAP_TYPE ? 1.0f : 0.0f;
	case _kind_hidden_crosshair_item: return (raw & HUD_CROSSHAIR_ITEM_HIDDEN_FLAG) ? 1.0f : 0.0f;
	case _kind_hidden_overlay_item: return (raw & HUD_OVERLAY_ITEM_HIDDEN_FLAG) ? 1.0f : 0.0f;
	}
	return *(real const *)address;
}

/* a value written (an edit's: a hidden one's 1 hides it, 0 does nothing,
its own kept puts it back: value_restore) */
static void value_write(
	void *address,
	short kind,
	real value)
{
	switch (kind)
	{
	case _kind_short:
		*(short *)address = (short)(value < 0.0f ? value - 0.5f : value + 0.5f);
		return;
	case _kind_hidden_element:
		if (value >= 0.5f)
			*(short *)address = HUD_HIDDEN_MAP_TYPE;
		return;
	case _kind_hidden_crosshair_item:
		if (value >= 0.5f)
			*(short *)address |= HUD_CROSSHAIR_ITEM_HIDDEN_FLAG;
		return;
	case _kind_hidden_overlay_item:
		if (value >= 0.5f)
			*(short *)address |= HUD_OVERLAY_ITEM_HIDDEN_FLAG;
		return;
	}
	*(real *)address = value;
}

/* a value as it is kept to be put back (a hidden one's: the short it is),
and put back */
static real value_raw(
	void const *address,
	short kind)
{
	return kind_hidden(kind) ? (real)*(short const *)address : value_read(address, kind);
}

static void value_restore(
	void *address,
	short kind,
	real raw)
{
	if (kind_hidden(kind))
		*(short *)address = (short)raw;
	else
		value_write(address, kind, raw);
}

/* the value of a key as the weapon had it before it was edited (FALSE:
not known), and it kept the first time */
static boolean original_find(
	long weapon,
	char const *key,
	real *value)
{
	short index;

	for (index = 0; index < tag_editor.original_count; index++)
	{
		if (tag_editor.originals[index].weapon == weapon && !strcmp(tag_editor.originals[index].key, key))
		{
			*value = tag_editor.originals[index].value;
			return TRUE;
		}
	}
	return FALSE;
}

static void original_keep(
	long weapon,
	char const *key,
	real value)
{
	real known;

	if (original_find(weapon, key, &known) || tag_editor.original_count >= MAXIMUM_ORIGINALS)
		return;
	tag_editor.originals[tag_editor.original_count].weapon = weapon;
	snprintf(tag_editor.originals[tag_editor.original_count].key, EDIT_KEY_LENGTH, "%s", key);
	tag_editor.originals[tag_editor.original_count].value = value;
	tag_editor.original_count++;
}

/* ---------- private code: a level's edits */

/* a hash of a tag's edits of a weapon (those of keys beginning `prefix`, by
the rest of their keys: the same of another trigger's), whatever their
order; 0 for none */
static unsigned long edits_hash(
	long profile,
	long tag,
	char const *prefix)
{
	long count = port_tag_profiles_tag_edit_count(profile, tag);
	size_t length = strlen(prefix);
	unsigned long hash = 0;
	long edit;

	for (edit = 0; edit < count; edit++)
	{
		char const *key = port_tag_profiles_tag_edit_key(profile, tag, edit);
		char text[EDIT_KEY_LENGTH + 32];
		unsigned long value = 2166136261UL;
		char const *character;

		if (strncmp(key, prefix, length))
			continue;
		snprintf(text, sizeof(text), "%s=%.9g", key + length, port_tag_profiles_tag_edit_value(profile, tag, edit));
		for (character = text; *character; character++)
			value = (value ^ (unsigned char)*character) * 16777619UL & 0xFFFFFFFFUL;
		hash = (hash + value) & 0xFFFFFFFFUL;
	}
	return hash;
}

static boolean edits_any(
	long profile,
	long tag,
	char const *prefix)
{
	long count = port_tag_profiles_tag_edit_count(profile, tag);
	size_t length = strlen(prefix);
	long edit;

	for (edit = 0; edit < count; edit++)
	{
		if (!strncmp(port_tag_profiles_tag_edit_key(profile, tag, edit), prefix, length))
			return TRUE;
	}
	return FALSE;
}

/* a copy of a tag for a weapon's edits of it (named its path, # and their
hash: whether it was made before is said, as its edits are then made), or
NONE */
static long edited_copy(
	long datum,
	long profile,
	long tag,
	char const *prefix,
	boolean blocks,
	boolean *made_before)
{
	char name[512];
	char const *path = tag_get_name(datum);
	long group_tag;
	long count = 0;
	struct editor_tag_instance *instances = cache_files_tag_instances(&count);

	*made_before = FALSE;
	if (datum == NONE || !path || !instances || DATUM_INDEX_TO_ABSOLUTE_INDEX(datum) >= count)
		return NONE;
	group_tag = instances[DATUM_INDEX_TO_ABSOLUTE_INDEX(datum)].group_tag;
	snprintf(name, sizeof(name), "%s#%08lx", path, edits_hash(profile, tag, prefix));
	*made_before = tag_loaded(group_tag, name) != NONE;
	return tag_injector_tag_copy(datum, name, blocks);
}

/* each edit of a weapon's of keys beginning `prefix` (of a target, or of
any: NONE): its value in the tag it plays now kept as it was, the first
time, and with `write` the edit's made there */
static void edits_write(
	long profile,
	long tag,
	long weapon,
	char const *prefix,
	short target,
	boolean write)
{
	long count = port_tag_profiles_tag_edit_count(profile, tag);
	size_t length = strlen(prefix);
	long edit;

	for (edit = 0; edit < count; edit++)
	{
		char const *key = port_tag_profiles_tag_edit_key(profile, tag, edit);
		struct edit_key parsed;
		void *address;
		short kind;

		if (strncmp(key, prefix, length))
			continue;
		if (!key_parse(key, &parsed))
		{
			if (write)
				error(_error_silent, "tag editor: %s: the edit %s is not one this build makes", tag_get_name(weapon),
					key);
			continue;
		}
		if (target != NONE && parsed.target != target)
			continue;
		address = key_address(weapon, &parsed, &kind);
		if (!address)
		{
			if (write)
				error(_error_silent, "tag editor: %s has nothing of the edit %s", tag_get_name(weapon), key);
			continue;
		}
		original_keep(weapon, key, value_raw(address, kind));
		if (write)
			value_write(address, kind, (real)port_tag_profiles_tag_edit_value(profile, tag, edit));
	}
}

/* the highest level of a HUD a weapon's edits are of (0: its own) */
static short hud_edits_deepest(
	long profile,
	long tag)
{
	long count = port_tag_profiles_tag_edit_count(profile, tag);
	short deepest = 0;
	long edit;

	for (edit = 0; edit < count; edit++)
	{
		struct edit_key parsed;

		if (key_parse(port_tag_profiles_tag_edit_key(profile, tag, edit), &parsed) && parsed.target == _target_hud)
			deepest = MAX(deepest, parsed.index);
	}
	return deepest;
}

/* a projectile's damage effect of a part given the weapon's copy of it */
static void projectile_damage_edit(
	long profile,
	long tag,
	long weapon,
	short trigger_index,
	long projectile,
	short part)
{
	struct projectile_definition *definition = (struct projectile_definition *)tag_root(PROJECTILE_GROUP_TAG,
		projectile);
	char prefix[EDIT_KEY_LENGTH];
	long damage = damage_effect(weapon, projectile, part);
	long copy;
	boolean made_before;

	key_prefix(_target_damage, trigger_index, part, prefix, sizeof(prefix));
	if (!definition || damage == NONE || !edits_any(profile, tag, prefix))
		return;
	copy = edited_copy(damage, profile, tag, prefix, FALSE, &made_before);
	if (copy == NONE)
	{
		error(_error_silent, "tag editor: no copy of %s for %s", tag_get_name(damage), tag_get_name(weapon));
		return;
	}
	if (part == _damage_explosion)
	{
		struct effect_part_definition *effect_part;
		long effect = projectile_explosion(projectile, &effect_part);
		char effect_prefix[EDIT_KEY_LENGTH];
		long effect_copy;
		boolean effect_made_before;
		struct effect_definition *copied;
		long index;

		/* (the detonation effect a copy too, its parts naming the copy, and
		the projectile's references to it the effect's copy) */
		snprintf(effect_prefix, sizeof(effect_prefix), "%s", prefix);
		effect_copy = edited_copy(effect, profile, tag, effect_prefix, TRUE, &effect_made_before);
		copied = (struct effect_definition *)tag_root(EFFECT_GROUP_TAG, effect_copy);
		if (!copied)
			return;
		for (index = 0; !effect_made_before && index < copied->events.count; index++)
		{
			struct effect_event_definition *event = TAG_BLOCK_GET_ELEMENT(&copied->events, index,
				struct effect_event_definition);
			long part_index;

			for (part_index = 0; part_index < event->parts.count; part_index++)
			{
				struct effect_part_definition *found = TAG_BLOCK_GET_ELEMENT(&event->parts, part_index,
					struct effect_part_definition);

				if (found->reference.index == damage)
					reference_set(&found->reference, copy);
			}
		}
		if (definition->projectile.effect.index == effect)
			reference_set(&definition->projectile.effect, effect_copy);
		for (index = 0; index < definition->projectile.material_responses.count; index++)
		{
			struct projectile_material_response_definition *response = TAG_BLOCK_GET_ELEMENT(
				&definition->projectile.material_responses, index, struct projectile_material_response_definition);

			if (response->detonation_effect.index == effect)
				reference_set(&response->detonation_effect, effect_copy);
		}
	}
	else if (part == _damage_impact)
	{
		reference_set(&definition->projectile.impact_damage, copy);
	}
	else
	{
		reference_set(&definition->projectile.attached_detonation_damage, copy);
	}
	if (!made_before)
		edits_write(profile, tag, weapon, prefix, _target_damage, TRUE);
}

void tag_editor_weapon_loaded(
	long profile,
	long weapon_datum)
{
	struct weapon_definition *definition = weapon_definition(weapon_datum);
	long tag;
	short index;
	char prefix[EDIT_KEY_LENGTH];

	if (!definition)
		return;
	if (tag_editor.weapon_count < MAXIMUM_EDITOR_WEAPONS)
		tag_editor.weapons[tag_editor.weapon_count++] = weapon_datum;
	tag = port_tag_profiles_tag_find(profile, WEAPON_DEFINITION_TAG, tag_get_name(weapon_datum));
	if (tag == NONE || !port_tag_profiles_tag_edit_count(profile, tag))
		return;
	error(_error_silent, "tag editor: %s has %ld edits", tag_get_name(weapon_datum),
		port_tag_profiles_tag_edit_count(profile, tag));

	/* its own values, its triggers' and its magazines' */
	edits_write(profile, tag, weapon_datum, "weapon.", _target_weapon, TRUE);
	edits_write(profile, tag, weapon_datum, "trigger", _target_trigger, TRUE);
	edits_write(profile, tag, weapon_datum, "magazine", _target_magazine, TRUE);

	/* each trigger's projectile, and its damage effects, a copy */
	for (index = 0; index < definition->weapon.triggers.count; index++)
	{
		struct weapon_trigger_definition *trigger = weapon_trigger(weapon_datum, index);
		long projectile = trigger->projectile.index;
		long copy;
		boolean made_before;
		short part;

		key_prefix(_target_projectile, index, NONE, prefix, sizeof(prefix));
		if (!tag_root(PROJECTILE_GROUP_TAG, projectile) || !edits_any(profile, tag, prefix))
			continue;
		/* (its values and its damage effects' kept as they were, before it
		plays a copy) */
		edits_write(profile, tag, weapon_datum, prefix, NONE, FALSE);
		copy = edited_copy(projectile, profile, tag, prefix, TRUE, &made_before);
		if (copy == NONE)
		{
			error(_error_silent, "tag editor: no copy of %s for %s", tag_get_name(projectile),
				tag_get_name(weapon_datum));
			continue;
		}
		reference_set(&trigger->projectile, copy);
		/* (a copy made before has its edits, and its damage effects'
		copies) */
		if (made_before)
			continue;
		edits_write(profile, tag, weapon_datum, prefix, _target_projectile, TRUE);
		for (part = 0; part < _damage_melee; part++)
			projectile_damage_edit(profile, tag, weapon_datum, index, copy, part);
	}

	/* its melee damage, and its HUD */
	if (edits_any(profile, tag, "melee.") &&
		tag_root(DAMAGE_EFFECT_GROUP_TAG, definition->weapon.melee_attack_damage.index))
	{
		boolean made_before;
		long copy;

		edits_write(profile, tag, weapon_datum, "melee.", _target_damage, FALSE);
		copy = edited_copy(definition->weapon.melee_attack_damage.index, profile, tag, "melee.", FALSE, &made_before);
		if (copy != NONE)
		{
			reference_set(&definition->weapon.melee_attack_damage, copy);
			if (!made_before)
				edits_write(profile, tag, weapon_datum, "melee.", _target_damage, TRUE);
		}
	}
	if (edits_any(profile, tag, "hud.") && weapon_hud(weapon_datum, 0) != NONE)
	{
		struct tag_reference *reference = &definition->weapon.interface_definition.hud_interface;
		boolean made_before = FALSE;
		boolean first_made_before = FALSE;
		short deepest = hud_edits_deepest(profile, tag);
		short level;

		edits_write(profile, tag, weapon_datum, "hud.", _target_hud, FALSE);
		/* (its HUD a copy, and each parent up to the last edited, each copy
		naming the next's: one made before names its own already) */
		for (level = 0; level <= deepest && !made_before && reference && reference->index != NONE; level++)
		{
			long copy = edited_copy(reference->index, profile, tag, "hud.", TRUE, &made_before);
			struct weapon_hud_interface_definition *copied;

			if (copy == NONE)
			{
				error(_error_silent, "tag editor: no copy of %s for %s", tag_get_name(reference->index),
					tag_get_name(weapon_datum));
				break;
			}
			reference_set(reference, copy);
			if (level == 0)
				first_made_before = made_before;
			copied = (struct weapon_hud_interface_definition *)tag_root(WEAPON_HUD_GROUP_TAG, copy);
			reference = copied ? &copied->parent_hud : NULL;
		}
		if (level > 0 && !first_made_before)
			edits_write(profile, tag, weapon_datum, "hud.", _target_hud, TRUE);
	}
}

void tag_editor_level_unloaded(
	void)
{
	/* (the edits made written; the HUD element moved is let go with the
	tags, not put back) */
	pending_write();
	tag_editor.blinked = FALSE;
	tag_editor.weapon_count = 0;
	tag_editor.original_count = 0;
	tag_editor.capturing = FALSE;
	tag_editor.weapon = NONE;
}

/* ---------- private code: the weapon held */

/* whether a change is made to the tags played at once: not in a network
game with other machines, whose tags are the host's */
static boolean editor_live(
	void)
{
	if (!network_game_is_active() || network_game_is_splitscreen_local())
		return TRUE;
	return tag_multiplayer_hosted() &&
		network_game_server_port_remote_machine_count(global_network_game_server_get(), FALSE) == 0;
}

static void editor_context(
	struct editor_context *context)
{
	long level_profile = tag_injector_level_profile();
	long player_index = local_player_get_player_index(0);
	long unit_index = player_index == NONE ? NONE : player_get(player_index)->unit_index;
	struct unit_datum *unit = unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	struct object_datum *weapon = NULL;

	memset(context, 0, sizeof(*context));
	context->weapon = NONE;
	context->profile = NONE;
	context->tag = NONE;
	if (unit && unit->unit.current_weapon_index >= 0 && unit->unit.current_weapon_index < MAXIMUM_WEAPONS_PER_UNIT)
	{
		weapon = (struct object_datum *)object_try_and_get_and_verify_type(
			unit->unit.weapon_object_indices[unit->unit.current_weapon_index], _object_mask_weapon);
	}
	if (!weapon)
	{
		snprintf(context->why, sizeof(context->why), "Hold a weapon of the Tag Injector's profile to edit it.");
		return;
	}
	context->weapon = weapon->definition_index;
	snprintf(context->name, sizeof(context->name), "%s", tag_get_name(context->weapon) ?
		tag_get_name(context->weapon) : "?");
	if (level_profile == NONE)
	{
		snprintf(context->why, sizeof(context->why), "This level plays no Tag Injector profile.");
		context->weapon = NONE;
		return;
	}
	/* (a network game's shared profile: its host's own, which it shares;
	nothing a client's) */
	context->profile = level_profile;
	if (port_tag_profiles_shared(level_profile))
	{
		long mp_profile = tag_multiplayer_game_profile();

		context->profile = tag_multiplayer_hosted() && mp_profile != NONE ?
			port_tag_profiles_find(port_tag_mp_profiles_option(mp_profile, "profile")) : NONE;
		if (context->profile == NONE)
		{
			snprintf(context->why, sizeof(context->why), "Only the host edits the game's weapons: its edits reach "
				"every player.");
			context->weapon = NONE;
			return;
		}
	}
	context->tag = port_tag_profiles_tag_find(context->profile, WEAPON_DEFINITION_TAG, tag_get_name(context->weapon));
	if (context->tag == NONE)
	{
		snprintf(context->why, sizeof(context->why), "This weapon is not one of the profile's.");
		context->weapon = NONE;
		return;
	}
	tag_injector_tag_display_name(context->profile, context->tag, context->name, sizeof(context->name));
	context->live = editor_live();
}

/* the weapons of the level that share the tag of a list with the weapon (a
projectile, a damage effect, a HUD), each with its keys' beginning: how
many */
static short sharers_find(
	long weapon,
	short target,
	short index,
	short part,
	struct sharer *sharers,
	short capacity)
{
	long datum = target_tag(weapon, target, index, part);
	long count = 0;
	struct editor_tag_instance *instances = cache_files_tag_instances(&count);
	short found = 0;
	long absolute;

	if (datum == NONE || !instances || target == _target_weapon || target == _target_trigger ||
		target == _target_magazine)
		return 0;
	for (absolute = 0; absolute < count && found < capacity; absolute++)
	{
		long other = instances[absolute].tag_index;
		struct weapon_definition *definition;
		short trigger_index;

		if (instances[absolute].group_tag != WEAPON_DEFINITION_TAG || !(definition = weapon_definition(other)))
			continue;
		if (target == _target_damage && part == _damage_melee)
		{
			if (other != weapon && target_tag(other, target, NONE, part) == datum)
			{
				sharers[found].weapon = other;
				key_prefix(target, NONE, part, sharers[found++].prefix, EDIT_KEY_LENGTH);
			}
			continue;
		}
		/* (a HUD: of any of its levels, a parent HUD drawn for many) */
		if (target == _target_hud)
		{
			short level;

			for (level = 0; level < MAXIMUM_HUD_LEVELS && found < capacity; level++)
			{
				long hud = weapon_hud(other, level);

				if (hud == NONE)
					break;
				if (hud != datum || (other == weapon && level == index))
					continue;
				sharers[found].weapon = other;
				key_prefix(target, level, NONE, sharers[found++].prefix, EDIT_KEY_LENGTH);
			}
			continue;
		}
		for (trigger_index = 0; trigger_index < definition->weapon.triggers.count && found < capacity; trigger_index++)
		{
			if ((other == weapon && trigger_index == index) || target_tag(other, target, trigger_index, part) != datum)
				continue;
			sharers[found].weapon = other;
			key_prefix(target, trigger_index, part, sharers[found++].prefix, EDIT_KEY_LENGTH);
		}
	}
	return found;
}

/* the name a weapon is shown by: its profile's tag's, else its path's last
part */
static void weapon_name(
	long profile,
	long weapon,
	char *name,
	long size)
{
	char const *path = tag_get_name(weapon);
	long tag = path ? port_tag_profiles_tag_find(profile, WEAPON_DEFINITION_TAG, path) : NONE;
	char const *leaf;

	if (tag != NONE)
	{
		tag_injector_tag_display_name(profile, tag, name, size);
		return;
	}
	leaf = path ? strrchr(path, '\\') : NULL;
	snprintf(name, size, "%s", leaf ? leaf + 1 : path ? path : "?");
}

/* the edits made written to their profiles */
static void pending_write(
	void)
{
	short index;

	for (index = 0; index < tag_editor.pending_count; index++)
	{
		boolean written = tag_editor.pending[index].clear ?
			port_tag_profiles_tag_clear_edit(tag_editor.pending[index].profile, tag_editor.pending[index].tag,
				tag_editor.pending[index].key) :
			port_tag_profiles_tag_set_edit(tag_editor.pending[index].profile, tag_editor.pending[index].tag,
				tag_editor.pending[index].key, tag_editor.pending[index].value);

		if (!written)
			error(_error_silent, "tag editor: the edit %s could not be written", tag_editor.pending[index].key);
	}
	if (tag_editor.pending_count)
		error(_error_silent, "tag editor: %d edits written", tag_editor.pending_count);
	tag_editor.pending_count = 0;
}

/* an edit to be written (clear: taken out) */
static void pending_add(
	long profile,
	long tag,
	char const *key,
	double value,
	boolean clear)
{
	short index;

	for (index = 0; index < tag_editor.pending_count; index++)
	{
		if (tag_editor.pending[index].profile == profile && tag_editor.pending[index].tag == tag &&
			!strcmp(tag_editor.pending[index].key, key))
			break;
	}
	if (index == tag_editor.pending_count)
	{
		if (tag_editor.pending_count == MAXIMUM_PENDING)
			pending_write();
		index = tag_editor.pending_count++;
	}
	tag_editor.pending[index].profile = profile;
	tag_editor.pending[index].tag = tag;
	snprintf(tag_editor.pending[index].key, EDIT_KEY_LENGTH, "%s", key);
	tag_editor.pending[index].value = value;
	tag_editor.pending[index].clear = clear;
	tag_editor.changed_at = system_milliseconds();
}

/* the value of a key the menu shows: the edit not written yet, else the
profile's (in a network game, which the tags have not), else the tag's */
static boolean shown_value(
	struct editor_context const *context,
	char const *key,
	real *value)
{
	struct edit_key parsed;
	double edited;
	short index;
	short kind;
	void *address;

	for (index = 0; index < tag_editor.pending_count; index++)
	{
		if (tag_editor.pending[index].profile == context->profile && tag_editor.pending[index].tag == context->tag &&
			!strcmp(tag_editor.pending[index].key, key) && !tag_editor.pending[index].clear)
		{
			*value = (real)tag_editor.pending[index].value;
			return TRUE;
		}
	}
	if (!context->live && port_tag_profiles_tag_edit_find(context->profile, context->tag, key, &edited))
	{
		*value = (real)edited;
		return TRUE;
	}
	if (!key_parse(key, &parsed) || !(address = key_address(context->weapon, &parsed, &kind)))
		return FALSE;
	*value = value_read(address, kind);
	return TRUE;
}

/* a key's value set (by the menu), for the weapon and, as the list says,
those that share its tag: in the tag played (when it can be: the value as
it was kept first) and the profile's edits */
static boolean value_set(
	struct editor_context const *context,
	char const *key,
	real value,
	boolean clear)
{
	struct edit_key parsed;
	struct sharer sharers[MAXIMUM_SHARERS];
	short sharer_count = 0;
	short index;
	short kind;
	void *address;
	char prefix[EDIT_KEY_LENGTH];

	if (!key_parse(key, &parsed))
		return FALSE;
	address = key_address(context->weapon, &parsed, &kind);
	/* (a HUD is drawn by this machine alone: moved at once, whatever the
	game) */
	if (address && (context->live || parsed.target == _target_hud))
	{
		real original;

		original_keep(context->weapon, key, value_raw(address, kind));
		if (clear && original_find(context->weapon, key, &original))
			value_restore(address, kind, original);
		else if (!clear)
			value_write(address, kind, value);
	}
	pending_add(context->profile, context->tag, key, value, clear);
	if (!tag_editor.everyone)
		return TRUE;
	key_prefix(parsed.target, parsed.index, parsed.part, prefix, sizeof(prefix));
	sharer_count = sharers_find(context->weapon, parsed.target, parsed.index, parsed.part, sharers, MAXIMUM_SHARERS);
	for (index = 0; index < sharer_count; index++)
	{
		char other_key[EDIT_KEY_LENGTH];
		long tag = port_tag_profiles_tag_find(context->profile, WEAPON_DEFINITION_TAG,
			tag_get_name(sharers[index].weapon));

		if (tag == NONE)
			continue;
		snprintf(other_key, sizeof(other_key), "%s%s", sharers[index].prefix, key + strlen(prefix));
		pending_add(context->profile, tag, other_key, value, clear);
	}
	return TRUE;
}

/* ---------- private code: the menu's lists */

static short page_top(
	void)
{
	return tag_editor.pages[tag_editor.depth].page;
}

static short page_target(
	short page)
{
	switch (page)
	{
	case _page_weapon: return _target_weapon;
	case _page_trigger: return _target_trigger;
	case _page_magazine: return _target_magazine;
	case _page_projectile: return _target_projectile;
	case _page_damage: return _target_damage;
	case _page_hud: return _target_hud;
	}
	return NONE;
}

static void row_add(
	struct row_spec *rows,
	short *count,
	short kind,
	short action,
	short argument0,
	short argument1,
	short argument2,
	struct edit_field const *field,
	char const *label)
{
	struct row_spec *row;

	if (*count >= MAXIMUM_EDITOR_ROWS)
		return;
	row = &rows[(*count)++];
	row->kind = kind;
	row->action = action;
	row->arguments[0] = argument0;
	row->arguments[1] = argument1;
	row->arguments[2] = argument2;
	row->field = field;
	snprintf(row->label, sizeof(row->label), "%s", label);
}

/* the rows of the list gone into, for the weapon held */
static short page_rows(
	struct editor_context const *context,
	struct row_spec *rows)
{
	short count = 0;
	short page = page_top();
	short index = tag_editor.pages[tag_editor.depth].index;
	short part = tag_editor.pages[tag_editor.depth].part;
	struct weapon_definition *definition = weapon_definition(context->weapon);
	char label[TAG_EDITOR_LABEL_LENGTH];
	short field;

	if (context->weapon == NONE || !definition)
	{
		row_add(rows, &count, _tag_editor_row_text, _action_none, 0, 0, 0, NULL, context->why);
		return count;
	}
	if (page == _page_root)
	{
		row_add(rows, &count, _tag_editor_row_text, _action_none, 0, 0, 0, NULL, context->name);
		row_add(rows, &count, _tag_editor_row_folder, _action_page, _page_stats, 0, 0, NULL, "Weapon stats");
		if (weapon_hud_levels(context->weapon))
			row_add(rows, &count, _tag_editor_row_folder, _action_page, _page_huds, 0, 0, NULL, "HUD");
		row_add(rows, &count, _tag_editor_row_folder, _action_page, _page_settings, 0, 0, NULL, "Tag settings");
		row_add(rows, &count, _tag_editor_row_command, _action_reset_all, 0, 0, 0, NULL, "Reset all edits");
		return count;
	}
	if (page == _page_settings)
	{
		row_add(rows, &count, _tag_editor_row_command, _action_rename, 0, 0, 0, NULL, "Name");
		row_add(rows, &count, _tag_editor_row_command, _action_remove, 0, 0, 0, NULL, "Remove from profile");
		return count;
	}
	if (page == _page_stats)
	{
		short trigger_index;

		row_add(rows, &count, _tag_editor_row_folder, _action_page, _page_weapon, 0, 0, NULL, "Weapon");
		for (trigger_index = 0; trigger_index < definition->weapon.triggers.count && trigger_index < 2; trigger_index++)
		{
			snprintf(label, sizeof(label), "%s trigger", trigger_index ? "Secondary" : "Primary");
			row_add(rows, &count, _tag_editor_row_folder, _action_page, _page_trigger, trigger_index, 0, NULL, label);
		}
		for (trigger_index = 0; trigger_index < definition->weapon.magazines.count && trigger_index < 2; trigger_index++)
		{
			snprintf(label, sizeof(label), trigger_index ? "Second magazine" : "Magazine");
			row_add(rows, &count, _tag_editor_row_folder, _action_page, _page_magazine, trigger_index, 0, NULL, label);
		}
		for (trigger_index = 0; trigger_index < definition->weapon.triggers.count && trigger_index < 2; trigger_index++)
		{
			if (target_tag(context->weapon, _target_projectile, trigger_index, NONE) == NONE)
				continue;
			snprintf(label, sizeof(label), "%s projectile", trigger_index ? "Secondary" : "Primary");
			row_add(rows, &count, _tag_editor_row_folder, _action_page, _page_projectile, trigger_index, 0, NULL,
				label);
		}
		if (target_tag(context->weapon, _target_damage, NONE, _damage_melee) != NONE)
		{
			row_add(rows, &count, _tag_editor_row_folder, _action_page, _page_damage, NONE, _damage_melee, NULL,
				damage_part_names[_damage_melee]);
		}
		return count;
	}
	if (page == _page_huds)
	{
		short trigger_index;

		/* (its HUD, and each parent HUD it draws too) */
		for (trigger_index = 0; trigger_index < weapon_hud_levels(context->weapon); trigger_index++)
		{
			char const *path = tag_get_name(weapon_hud(context->weapon, trigger_index));
			char const *leaf = path ? strrchr(path, '\\') : NULL;

			if (trigger_index)
				snprintf(label, sizeof(label), "Parent HUD %d (%s)", trigger_index, leaf ? leaf + 1 :
					path ? path : "?");
			else
				snprintf(label, sizeof(label), "HUD");
			row_add(rows, &count, _tag_editor_row_folder, _action_page, _page_hud, trigger_index, 0, NULL, label);
		}
		return count;
	}
	/* (a list of a shared tag: whether its edits are every sharer's) */
	if (page == _page_projectile || page == _page_damage || page == _page_hud)
	{
		struct sharer sharers[MAXIMUM_SHARERS];

		if (sharers_find(context->weapon, page_target(page), index, part, sharers, MAXIMUM_SHARERS))
			row_add(rows, &count, _tag_editor_row_value, _action_scope, 0, 0, 0, NULL, "Edits apply to");
	}
	if (page == _page_hud)
	{
		short kind;

		row_add(rows, &count, _tag_editor_row_value, _action_hud_anchor, 0, 0, 0, NULL, "Anchor");
		for (kind = 0; kind < NUMBER_OF_EDIT_HUD_KINDS; kind++)
		{
			long hud = target_tag(context->weapon, _target_hud, index, NONE);
			short element;

			for (element = 0; element < hud_count(hud, kind, NONE); element++)
			{
				char const *bitmap = hud_bitmap_name(hud, kind, element);
				short item;

				if (kind != _hud_crosshair && kind != _hud_overlay)
				{
					snprintf(label, sizeof(label), bitmap[0] ? "%s %d (%s)" : "%s %d", hud_kind_names[kind],
						element + 1, bitmap);
					row_add(rows, &count, _tag_editor_row_value, _action_hud_element, kind, element, NONE, NULL, label);
					continue;
				}
				for (item = 0; item < hud_count(hud, kind, element); item++)
				{
					snprintf(label, sizeof(label), bitmap[0] ? "%s %d.%d (%s)" : "%s %d.%d", hud_kind_names[kind],
						element + 1, item + 1, bitmap);
					row_add(rows, &count, _tag_editor_row_value, _action_hud_element, kind, element, item, NULL, label);
				}
			}
		}
		row_add(rows, &count, _tag_editor_row_command, _action_reset_page, 0, 0, 0, NULL, "Reset HUD");
		return count;
	}
	for (field = 0; field < (short)NUMBEROF(edit_fields); field++)
	{
		if (edit_fields[field].target == page_target(page))
			row_add(rows, &count, _tag_editor_row_value, _action_field, 0, 0, 0, &edit_fields[field],
				edit_fields[field].label);
	}
	if (page == _page_projectile)
	{
		long projectile = target_tag(context->weapon, _target_projectile, index, NONE);

		for (part = 0; part < _damage_melee; part++)
		{
			if (damage_effect(context->weapon, projectile, part) != NONE)
				row_add(rows, &count, _tag_editor_row_folder, _action_page, _page_damage, index, part, NULL,
					damage_part_names[part]);
		}
	}
	row_add(rows, &count, _tag_editor_row_command, _action_reset_page, 0, 0, 0, NULL, "Reset these");
	return count;
}

/* a row's key (its field's, or its HUD value's) */
static void row_key(
	struct row_spec const *row,
	short hud_value,
	char *key,
	long size)
{
	char prefix[EDIT_KEY_LENGTH];
	short page = page_top();

	if (row->action == _action_hud_anchor)
	{
		hud_key(tag_editor.pages[tag_editor.depth].index, NONE, NONE, NONE, NONE, key, size);
		return;
	}
	if (row->action == _action_hud_element)
	{
		hud_key(tag_editor.pages[tag_editor.depth].index, row->arguments[0], row->arguments[1], row->arguments[2],
			hud_value, key, size);
		return;
	}
	key_prefix(page_target(page), tag_editor.pages[tag_editor.depth].index, tag_editor.pages[tag_editor.depth].part,
		prefix, sizeof(prefix));
	snprintf(key, size, "%s%s", prefix, row->field ? row->field->key : "");
}

static void number_text(
	real value,
	short kind,
	char *text,
	long size)
{
	if (kind == _kind_short)
		snprintf(text, size, "%d", (int)(value < 0.0f ? value - 0.5f : value + 0.5f));
	else if (value >= 100.0f || value <= -100.0f)
		snprintf(text, size, "%.0f", value);
	else
		snprintf(text, size, "%.2f", value);
}

/* whether a key is of a list's (of its keys' beginning; of a HUD's level,
unless it is NONE: "hud." begins its parents' too) */
static boolean key_of_list(
	char const *key,
	char const *prefix,
	short hud_level)
{
	struct edit_key parsed;

	if (strncmp(key, prefix, strlen(prefix)))
		return FALSE;
	return hud_level == NONE || (key_parse(key, &parsed) && parsed.target == _target_hud && parsed.index == hud_level);
}

/* the list's rows' edits taken out (all of the weapon's: the whole
profile's tag's), and their values put back where they were kept */
static void edits_reset(
	struct editor_context const *context,
	char const *prefix,
	short hud_level)
{
	long count = port_tag_profiles_tag_edit_count(context->profile, context->tag);
	long edit;
	short index;

	for (edit = 0; edit < count; edit++)
	{
		char key[EDIT_KEY_LENGTH];

		snprintf(key, sizeof(key), "%s", port_tag_profiles_tag_edit_key(context->profile, context->tag, edit));
		if (key_of_list(key, prefix, hud_level))
			value_set(context, key, 0.0f, TRUE);
	}
	for (index = 0; index < tag_editor.pending_count; index++)
	{
		if (tag_editor.pending[index].profile == context->profile && tag_editor.pending[index].tag == context->tag &&
			key_of_list(tag_editor.pending[index].key, prefix, hud_level))
			tag_editor.pending[index].clear = TRUE;
	}
	/* (the values kept are put back, though the edits were written) */
	for (index = 0; index < tag_editor.original_count; index++)
	{
		struct edit_key parsed;
		void *address;
		short kind;

		if (tag_editor.originals[index].weapon != context->weapon ||
			!key_of_list(tag_editor.originals[index].key, prefix, hud_level) ||
			(!context->live && strncmp(tag_editor.originals[index].key, "hud.", 4)) ||
			!key_parse(tag_editor.originals[index].key, &parsed) ||
			!(address = key_address(context->weapon, &parsed, &kind)))
			continue;
		value_restore(address, kind, tag_editor.originals[index].value);
	}
}

/* ---------- private code: the HUD moved */

static struct hud_placement_definition *captured_placement(
	struct editor_context const *context)
{
	return hud_placement(target_tag(context->weapon, _target_hud, tag_editor.hud_level, NONE), tag_editor.hud_kind,
		tag_editor.hud_element, tag_editor.hud_item);
}

/* a value's key of the element moved */
static void captured_key(
	short value,
	char *key,
	long size)
{
	hud_key(tag_editor.hud_level, tag_editor.hud_kind, tag_editor.hud_element, tag_editor.hud_item, value, key, size);
}

/* the element blinking put back where it is */
static void blink_end(
	struct hud_placement_definition *placement)
{
	if (tag_editor.blinked && placement)
		placement->offset = tag_editor.blink_offset;
	tag_editor.blinked = FALSE;
}

static void capture_end(
	void)
{
	struct editor_context context;

	editor_context(&context);
	if (context.weapon != NONE)
		blink_end(captured_placement(&context));
	tag_editor.blinked = FALSE;
	tag_editor.capturing = FALSE;
}

/* ---------- public code */

void tag_editor_menu_open(
	void)
{
	struct editor_context context;

	editor_context(&context);
	if (tag_editor.capturing)
		capture_end();
	/* (the lists of the weapon it was left at, else the first list) */
	if (context.weapon != tag_editor.weapon)
	{
		tag_editor.depth = 0;
		memset(tag_editor.pages, 0, sizeof(tag_editor.pages));
		tag_editor.weapon = context.weapon;
	}
	tag_editor.everyone = FALSE;
	tag_editor.confirming = NONE;
}

char const *tag_editor_menu_title(
	void)
{
	static char title[TAG_EDITOR_LABEL_LENGTH];
	short page = page_top();
	short index = tag_editor.pages[tag_editor.depth].index;

	switch (page)
	{
	case _page_stats: return "Weapon stats";
	case _page_huds: return "HUD";
	case _page_settings: return "Tag settings";
	case _page_weapon: return "Weapon";
	case _page_trigger: return index ? "Secondary trigger" : "Primary trigger";
	case _page_magazine: return index ? "Second magazine" : "Magazine";
	case _page_projectile: return index ? "Secondary projectile" : "Primary projectile";
	case _page_damage:
		snprintf(title, sizeof(title), "%s", damage_part_names[PIN(tag_editor.pages[tag_editor.depth].part, 0,
			NUMBER_OF_EDIT_DAMAGE_PARTS - 1)]);
		return title;
	case _page_hud:
		if (index <= 0)
			return "HUD";
		snprintf(title, sizeof(title), "Parent HUD %d", index);
		return title;
	}
	return "Tag Editor";
}

/* a row as the overlay shows it */
static void row_fill(
	struct editor_context const *context_pointer,
	struct row_spec const *spec,
	short row,
	struct tag_editor_row *result)
{
	struct editor_context context = *context_pointer;
	real value;
	char key[EDIT_KEY_LENGTH];

	memset(result, 0, sizeof(*result));
	result->kind = spec->kind;
	snprintf(result->label, sizeof(result->label), "%s", spec->label);
	switch (spec->action)
	{
	case _action_none:
		if (context.weapon != NONE && page_top() == _page_root)
		{
			snprintf(result->value, sizeof(result->value), "%s", context.live ? "LIVE" : "NEXT GAME");
			snprintf(result->note, sizeof(result->note), "%s", context.live ?
				"Changes are made at once, and kept in the profile." :
				"In a network game: values change at the next game, the HUD at once.");
		}
		break;
	case _action_scope:
	{
		struct sharer sharers[MAXIMUM_SHARERS];
		short sharer_count = sharers_find(context.weapon, page_target(page_top()),
			tag_editor.pages[tag_editor.depth].index, tag_editor.pages[tag_editor.depth].part, sharers,
			MAXIMUM_SHARERS);
		short index;

		snprintf(result->value, sizeof(result->value), "%s", tag_editor.everyone ? "ALL" : "THIS ONE");
		snprintf(result->note, sizeof(result->note), "Shared with ");
		for (index = 0; index < sharer_count && index < 3; index++)
		{
			char name[TAG_EDITOR_LABEL_LENGTH];

			weapon_name(context.profile, sharers[index].weapon, name, sizeof(name));
			snprintf(result->note + strlen(result->note), sizeof(result->note) - strlen(result->note), "%s%s",
				index ? ", " : "", name);
		}
		if (sharer_count > 3)
			snprintf(result->note + strlen(result->note), sizeof(result->note) - strlen(result->note), " and %d more",
				sharer_count - 3);
		snprintf(result->note + strlen(result->note), sizeof(result->note) - strlen(result->note), "%s",
			tag_editor.everyone ? "." : ": they change too until the level loads again.");
		break;
	}
	case _action_field:
	{
		real original;

		row_key(spec, NONE, key, sizeof(key));
		if (shown_value(&context, key, &value))
			number_text(value * field_factor(spec->field->kind), spec->field->kind, result->value,
				sizeof(result->value));
		if (original_find(context.weapon, key, &original) && original != value)
		{
			char text[TAG_EDITOR_VALUE_LENGTH];

			number_text(original * field_factor(spec->field->kind), spec->field->kind, text, sizeof(text));
			snprintf(result->note, sizeof(result->note), "Its map's: %s. A puts it back.", text);
		}
		break;
	}
	case _action_hud_anchor:
		row_key(spec, NONE, key, sizeof(key));
		if (shown_value(&context, key, &value))
			snprintf(result->value, sizeof(result->value), "%s",
				anchor_names[PIN((short)value, 0, NUMBER_OF_HUD_ANCHORS - 1)]);
		snprintf(result->note, sizeof(result->note), "The corner of the screen the HUD's offsets are from.");
		break;
	case _action_hud_element:
	{
		real x = 0.0f;
		real y = 0.0f;

		real hidden = 0.0f;

		row_key(spec, _hud_x, key, sizeof(key));
		shown_value(&context, key, &x);
		row_key(spec, _hud_y, key, sizeof(key));
		shown_value(&context, key, &y);
		row_key(spec, _hud_hide, key, sizeof(key));
		shown_value(&context, key, &hidden);
		if (hidden >= 0.5f)
			snprintf(result->value, sizeof(result->value), "HIDDEN");
		else
			snprintf(result->value, sizeof(result->value), "%d, %d", (int)x, (int)y);
		snprintf(result->note, sizeof(result->note), "%s", hidden >= 0.5f ?
			"Left or right shows it again." : "A moves it over the game. Left or right hides it.");
		break;
	}
	case _action_reset_page:
	case _action_reset_all:
		if (tag_editor.confirming == row)
			snprintf(result->note, sizeof(result->note), "Press A again to put them back as the map had them.");
		break;
	case _action_rename:
		snprintf(result->value, sizeof(result->value), "%s", context.name);
		snprintf(result->note, sizeof(result->note), "A types its name in the profile (the menus' and the HUD "
			"notices'; its path stays).");
		break;
	case _action_remove:
		snprintf(result->note, sizeof(result->note), "%s", tag_editor.confirming == row ?
			"Press A again to take it out of the profile, and delete its bundle." :
			"Takes it out of the profile (this level keeps it until it loads again).");
		break;
	}
}

short tag_editor_menu_rows(
	struct tag_editor_row *rows,
	short capacity)
{
	static struct row_spec specs[MAXIMUM_EDITOR_ROWS];
	struct editor_context context;
	short count;
	short row;

	editor_context(&context);
	/* (another weapon in hand: its first list) */
	if (context.weapon != tag_editor.weapon)
		tag_editor_menu_open();
	count = page_rows(&context, specs);
	for (row = 0; row < count && row < capacity; row++)
		row_fill(&context, &specs[row], row, &rows[row]);
	return row;
}

void tag_editor_menu_choose(
	short row,
	short *selected,
	char *status,
	long status_size)
{
	static struct row_spec rows[MAXIMUM_EDITOR_ROWS];
	struct editor_context context;
	short count;
	struct row_spec const *spec;
	char key[EDIT_KEY_LENGTH];
	char prefix[EDIT_KEY_LENGTH];

	status[0] = 0;
	editor_context(&context);
	count = page_rows(&context, rows);
	if (row < 0 || row >= count)
		return;
	spec = &rows[row];
	if (spec->action != _action_reset_page && spec->action != _action_reset_all && spec->action != _action_remove)
		tag_editor.confirming = NONE;
	switch (spec->action)
	{
	case _action_page:
		if (tag_editor.depth + 1 >= MAXIMUM_EDITOR_DEPTH)
			return;
		tag_editor.pages[tag_editor.depth].selected = row;
		tag_editor.depth++;
		tag_editor.pages[tag_editor.depth].page = spec->arguments[0];
		tag_editor.pages[tag_editor.depth].index = spec->arguments[1];
		tag_editor.pages[tag_editor.depth].part = spec->arguments[2];
		tag_editor.pages[tag_editor.depth].selected = 0;
		tag_editor.everyone = FALSE;
		*selected = 0;
		return;
	case _action_field:
	{
		real original;

		row_key(spec, NONE, key, sizeof(key));
		if (original_find(context.weapon, key, &original) || port_tag_profiles_tag_edit_find(context.profile,
			context.tag, key, NULL))
		{
			value_set(&context, key, 0.0f, TRUE);
			snprintf(status, status_size, "%s: as its map had it", spec->label);
		}
		return;
	}
	case _action_scope:
		tag_editor.everyone = !tag_editor.everyone;
		return;
	case _action_hud_element:
	{
		struct hud_placement_definition *placement;

		tag_editor.hud_level = tag_editor.pages[tag_editor.depth].index;
		tag_editor.hud_kind = spec->arguments[0];
		tag_editor.hud_element = spec->arguments[1];
		tag_editor.hud_item = spec->arguments[2];
		placement = captured_placement(&context);
		if (!placement)
			return;
		tag_editor.capturing = TRUE;
		tag_editor.scaling = FALSE;
		tag_editor.repeat_at = 0;
		tag_editor.held_since = 0;
		tag_editor.blink_until = system_milliseconds() + HUD_BLINK_MILLISECONDS;
		tag_editor.blinked = FALSE;
		/* (its place and scale kept as they were, to be put back) */
		row_key(spec, _hud_x, key, sizeof(key));
		original_keep(context.weapon, key, (real)placement->offset.x);
		row_key(spec, _hud_y, key, sizeof(key));
		original_keep(context.weapon, key, (real)placement->offset.y);
		row_key(spec, _hud_scale_x, key, sizeof(key));
		original_keep(context.weapon, key, placement->scale.i);
		row_key(spec, _hud_scale_y, key, sizeof(key));
		original_keep(context.weapon, key, placement->scale.j);
		return;
	}
	case _action_rename:
		/* (typed over the game, as a HUD element is moved) */
		snprintf(tag_editor.name, sizeof(tag_editor.name), "%s", context.name);
		tag_editor.cursor = (short)strlen(tag_editor.name);
		tag_editor.rename_repeat_at = 0;
		tag_editor.held_since = 0;
		tag_editor.renaming = TRUE;
		return;
	case _action_remove:
	{
		char name[TAG_EDITOR_LABEL_LENGTH];

		if (tag_editor.confirming != row)
		{
			tag_editor.confirming = row;
			return;
		}
		tag_editor.confirming = NONE;
		/* (the edits made written first: the profile's tags are numbered
		again once one is taken out) */
		pending_write();
		snprintf(name, sizeof(name), "%s", context.name);
		if (port_tag_profiles_tag_remove(context.profile, context.tag))
		{
			error(_error_silent, "tag editor: %s taken out of its profile", name);
			snprintf(status, status_size, "%s: taken out of the profile", name);
			tag_editor.depth = 0;
			*selected = 0;
		}
		else
		{
			snprintf(status, status_size, "%s could not be taken out (the log says why)", name);
		}
		return;
	}
	case _action_reset_page:
	case _action_reset_all:
		if (tag_editor.confirming != row)
		{
			tag_editor.confirming = row;
			return;
		}
		tag_editor.confirming = NONE;
		if (spec->action == _action_reset_all)
			prefix[0] = 0;
		else
			key_prefix(page_target(page_top()), tag_editor.pages[tag_editor.depth].index,
				tag_editor.pages[tag_editor.depth].part, prefix, sizeof(prefix));
		edits_reset(&context, prefix, spec->action == _action_reset_page && page_top() == _page_hud ?
			tag_editor.pages[tag_editor.depth].index : NONE);
		snprintf(status, status_size, "Put back as the map had them");
		return;
	}
}

void tag_editor_menu_adjust(
	short row,
	short direction,
	boolean fast,
	char *status,
	long status_size)
{
	static struct row_spec rows[MAXIMUM_EDITOR_ROWS];
	struct editor_context context;
	short count;
	struct row_spec const *spec;
	char key[EDIT_KEY_LENGTH];
	real value;

	status[0] = 0;
	editor_context(&context);
	count = page_rows(&context, rows);
	if (row < 0 || row >= count)
		return;
	spec = &rows[row];
	tag_editor.confirming = NONE;
	if (spec->action == _action_scope)
	{
		tag_editor.everyone = !tag_editor.everyone;
		return;
	}
	if (spec->action == _action_hud_anchor)
	{
		row_key(spec, NONE, key, sizeof(key));
		if (shown_value(&context, key, &value))
			value_set(&context, key, (real)(((short)value + direction + NUMBER_OF_HUD_ANCHORS) % NUMBER_OF_HUD_ANCHORS),
				FALSE);
		return;
	}
	/* (an element hidden, or shown as its map has it) */
	if (spec->action == _action_hud_element)
	{
		row_key(spec, _hud_hide, key, sizeof(key));
		if (!shown_value(&context, key, &value))
			return;
		if (value >= 0.5f)
		{
			value_set(&context, key, 0.0f, TRUE);
			snprintf(status, status_size, "%s: shown", spec->label);
		}
		else
		{
			value_set(&context, key, 1.0f, FALSE);
			snprintf(status, status_size, "%s: hidden", spec->label);
		}
		return;
	}
	if (spec->action != _action_field)
		return;
	row_key(spec, NONE, key, sizeof(key));
	if (shown_value(&context, key, &value))
	{
		real factor = field_factor(spec->field->kind);
		real shown = value * factor + (real)direction * spec->field->step * (fast ? 10.0f : 1.0f);

		shown = PIN(shown, spec->field->minimum, spec->field->maximum);
		/* (a step from a value between steps lands on one) */
		if (spec->field->kind != _kind_short && spec->field->step > 0.0f)
		{
			real steps = shown / spec->field->step;

			shown = (real)(long)(steps + (steps < 0.0f ? -0.5f : 0.5f)) * spec->field->step;
		}
		value_set(&context, key, shown / factor, FALSE);
		if (!context.live)
			snprintf(status, status_size, "%s: at the next game", spec->label);
	}
}

boolean tag_editor_menu_back(
	short *selected)
{
	tag_editor.confirming = NONE;
	if (tag_editor.renaming)
	{
		tag_editor.renaming = FALSE;
		return TRUE;
	}
	if (tag_editor.capturing)
	{
		capture_end();
		return TRUE;
	}
	if (tag_editor.depth == 0)
		return FALSE;
	tag_editor.depth--;
	tag_editor.everyone = FALSE;
	*selected = tag_editor.pages[tag_editor.depth].selected;
	return TRUE;
}

boolean tag_editor_menu_capturing(
	void)
{
	return tag_editor.capturing || tag_editor.renaming;
}

/* the name typed: left and right the character chosen (past the end, one
more), up and down it changed (held, again and again), Back the one before
it taken out, A the name kept, B none */
static void rename_capture(
	unsigned long pressed,
	unsigned long held)
{
	enum
	{
		CHANGES = (1UL << HALO_OVERLAY_PAD_UP) | (1UL << HALO_OVERLAY_PAD_DOWN),
	};
	unsigned long now = system_milliseconds();
	unsigned long changes = pressed & CHANGES;
	short length = (short)strlen(tag_editor.name);

	if (pressed & (1UL << HALO_OVERLAY_PAD_B))
	{
		tag_editor.renaming = FALSE;
		return;
	}
	if (pressed & (1UL << HALO_OVERLAY_PAD_A))
	{
		struct editor_context context;
		short end = length;

		/* (no spaces at its end; none at all: its path's last part again) */
		while (end > 0 && tag_editor.name[end - 1] == ' ')
			tag_editor.name[--end] = 0;
		editor_context(&context);
		tag_editor.renaming = FALSE;
		if (context.weapon == NONE || context.profile == NONE ||
			!port_tag_profiles_tag_set_option(context.profile, context.tag, "name", tag_editor.name))
		{
			error(_error_silent, "tag editor: the name %s could not be written", tag_editor.name);
			return;
		}
		error(_error_silent, "tag editor: %s named %s", tag_get_name(context.weapon), tag_editor.name);
		return;
	}
	if (pressed & (1UL << HALO_OVERLAY_PAD_BACK))
	{
		if (tag_editor.cursor > 0)
		{
			memmove(tag_editor.name + tag_editor.cursor - 1, tag_editor.name + tag_editor.cursor,
				(size_t)(length - tag_editor.cursor + 1));
			tag_editor.cursor--;
		}
		return;
	}
	if (pressed & (1UL << HALO_OVERLAY_PAD_LEFT))
		tag_editor.cursor = (short)MAX(tag_editor.cursor - 1, 0);
	if (pressed & (1UL << HALO_OVERLAY_PAD_RIGHT))
		tag_editor.cursor = (short)MIN(tag_editor.cursor + 1, MIN(length, RENAME_LENGTH - 1));
	/* (up or down held changes it again and again) */
	if (changes)
	{
		tag_editor.rename_repeat_at = now + HUD_REPEAT_DELAY_MILLISECONDS;
	}
	else if ((held & CHANGES) && tag_editor.rename_repeat_at && (long)(now - tag_editor.rename_repeat_at) >= 0)
	{
		changes = held & CHANGES;
		tag_editor.rename_repeat_at = now + HUD_REPEAT_MILLISECONDS * 2;
	}
	if (changes)
	{
		char current = tag_editor.cursor < length ? tag_editor.name[tag_editor.cursor] : ' ';
		char const *found = strchr(rename_characters, current);
		short count = (short)(sizeof(rename_characters) - 1);
		short index = found && current ? (short)(found - rename_characters) : 0;

		index = (short)((index + ((changes & (1UL << HALO_OVERLAY_PAD_UP)) ? 1 : count - 1)) % count);
		if (tag_editor.cursor >= length)
		{
			if (length >= RENAME_LENGTH)
				return;
			tag_editor.name[length] = 0;
			tag_editor.name[length + 1] = 0;
		}
		tag_editor.name[tag_editor.cursor] = rename_characters[index];
	}
}

void tag_editor_menu_capture(
	unsigned long pressed,
	unsigned long held)
{
	enum
	{
		DIRECTIONS = (1UL << HALO_OVERLAY_PAD_UP) | (1UL << HALO_OVERLAY_PAD_DOWN) | (1UL << HALO_OVERLAY_PAD_LEFT) |
			(1UL << HALO_OVERLAY_PAD_RIGHT),
	};
	struct editor_context context;
	struct hud_placement_definition *placement;
	unsigned long now = system_milliseconds();
	unsigned long steps = pressed & DIRECTIONS;
	short step;
	char key[EDIT_KEY_LENGTH];

	if (tag_editor.renaming)
	{
		rename_capture(pressed, held);
		return;
	}
	editor_context(&context);
	placement = context.weapon == NONE ? NULL : captured_placement(&context);
	if (!placement)
	{
		tag_editor.capturing = FALSE;
		return;
	}
	/* (blinking a moment once chosen: moved off the screen and back) */
	blink_end(placement);
	if ((long)(now - tag_editor.blink_until) < 0 && (now / HUD_BLINK_PERIOD_MILLISECONDS) % 2 == 0 &&
		!(held & DIRECTIONS))
	{
		tag_editor.blink_offset = placement->offset;
		tag_editor.blinked = TRUE;
		placement->offset.x = (short)(placement->offset.x + HUD_BLINK_OFFSET);
	}
	if (pressed & (1UL << HALO_OVERLAY_PAD_B))
	{
		capture_end();
		return;
	}
	if (pressed & (1UL << HALO_OVERLAY_PAD_A))
		tag_editor.scaling = !tag_editor.scaling;
	/* (Back puts it where its map had it) */
	if (pressed & (1UL << HALO_OVERLAY_PAD_BACK))
	{
		short value;

		blink_end(placement);
		for (value = 0; value < NUMBER_OF_HUD_PLACEMENT_VALUES; value++)
		{
			captured_key(value, key, sizeof(key));
			value_set(&context, key, 0.0f, TRUE);
		}
		return;
	}
	/* (a direction held steps again and again, further once held a while) */
	if (steps)
	{
		tag_editor.repeat_at = now + HUD_REPEAT_DELAY_MILLISECONDS;
		tag_editor.held_since = now;
	}
	else if ((held & DIRECTIONS) && tag_editor.repeat_at && (long)(now - tag_editor.repeat_at) >= 0)
	{
		steps = held & DIRECTIONS;
		tag_editor.repeat_at = now + HUD_REPEAT_MILLISECONDS;
	}
	if (!steps)
		return;
	blink_end(placement);
	tag_editor.blink_until = now;
	step = (short)((long)(now - tag_editor.held_since) >= HUD_FAST_AFTER_MILLISECONDS ? HUD_FAST_STEP : HUD_STEP);
	if (tag_editor.scaling)
	{
		/* (up and down scale it, left and right its width alone) */
		real height = (steps & (1UL << HALO_OVERLAY_PAD_UP)) ? 0.02f : (steps & (1UL << HALO_OVERLAY_PAD_DOWN)) ?
			-0.02f : 0.0f;
		real width = height + ((steps & (1UL << HALO_OVERLAY_PAD_RIGHT)) ? 0.02f :
			(steps & (1UL << HALO_OVERLAY_PAD_LEFT)) ? -0.02f : 0.0f);

		captured_key(_hud_scale_x, key, sizeof(key));
		value_set(&context, key, PIN(placement->scale.i + width * step, 0.1f, 8.0f), FALSE);
		captured_key(_hud_scale_y, key, sizeof(key));
		value_set(&context, key, PIN(placement->scale.j + height * step, 0.1f, 8.0f), FALSE);
		return;
	}
	captured_key(_hud_x, key, sizeof(key));
	value_set(&context, key, (real)PIN(placement->offset.x + ((steps & (1UL << HALO_OVERLAY_PAD_RIGHT)) ? step :
		(steps & (1UL << HALO_OVERLAY_PAD_LEFT)) ? -step : 0), -640, 640), FALSE);
	captured_key(_hud_y, key, sizeof(key));
	value_set(&context, key, (real)PIN(placement->offset.y + ((steps & (1UL << HALO_OVERLAY_PAD_DOWN)) ? step :
		(steps & (1UL << HALO_OVERLAY_PAD_UP)) ? -step : 0), -480, 480), FALSE);
}

void tag_editor_menu_capture_text(
	char *first,
	char *second,
	long size)
{
	struct editor_context context;
	struct hud_placement_definition *placement;
	struct hud_placement_definition shown;
	char name[TAG_EDITOR_LABEL_LENGTH] = "";
	char key[EDIT_KEY_LENGTH];
	real hidden = 0.0f;

	first[0] = 0;
	second[0] = 0;
	/* (the name typed, the character chosen in brackets) */
	if (tag_editor.renaming)
	{
		short length = (short)strlen(tag_editor.name);

		snprintf(first, size, "NAME: %.*s[%c]%s", tag_editor.cursor, tag_editor.name,
			tag_editor.cursor < length ? tag_editor.name[tag_editor.cursor] : ' ',
			tag_editor.cursor < length ? tag_editor.name + tag_editor.cursor + 1 : "");
		snprintf(second, size, "D-pad: up/down letter, left/right move   %%back-button Delete   %%a-button Save   "
			"%%b-button Cancel");
		return;
	}
	editor_context(&context);
	placement = context.weapon == NONE ? NULL : captured_placement(&context);
	if (!placement)
		return;
	shown = *placement;
	if (tag_editor.blinked)
		shown.offset = tag_editor.blink_offset;
	/* (of a parent HUD: which; hidden: said) */
	if (tag_editor.hud_level > 0)
		snprintf(name, sizeof(name), "PARENT %d: ", tag_editor.hud_level);
	if (tag_editor.hud_kind == _hud_crosshair || tag_editor.hud_kind == _hud_overlay)
		snprintf(name + strlen(name), sizeof(name) - strlen(name), "%s %d.%d", hud_kind_names[tag_editor.hud_kind],
			tag_editor.hud_element + 1, tag_editor.hud_item + 1);
	else
		snprintf(name + strlen(name), sizeof(name) - strlen(name), "%s %d", hud_kind_names[tag_editor.hud_kind],
			tag_editor.hud_element + 1);
	captured_key(_hud_hide, key, sizeof(key));
	if (shown_value(&context, key, &hidden) && hidden >= 0.5f)
		snprintf(name + strlen(name), sizeof(name) - strlen(name), " (HIDDEN)");
	snprintf(first, size, "%s   X %d   Y %d   SCALE %.2f x %.2f", name, shown.offset.x, shown.offset.y, shown.scale.i,
		shown.scale.j);
	snprintf(second, size, "%s   %%a-button %s   %%back-button Reset   %%b-button Done",
		tag_editor.scaling ? "D-pad: scale (up/down), width (left/right)" : "D-pad: move", tag_editor.scaling ?
		"Move" : "Scale");
}

void tag_editor_menu_update(
	boolean closing)
{
	if (closing && tag_editor.capturing)
		capture_end();
	if (closing)
		tag_editor.renaming = FALSE;
	if (!closing && (long)(system_milliseconds() - tag_editor.changed_at) < EDITOR_WRITE_MILLISECONDS)
		return;
	pending_write();
}
