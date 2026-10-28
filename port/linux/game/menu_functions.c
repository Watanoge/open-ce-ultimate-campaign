/*
MENU_FUNCTIONS.C

The event handler functions the menus written in XML can run besides the
game's (menu_tags.c names them; ui_widget_event_handler_function_invoke
calls them from PC_MENU_FUNCTION_BASE on):

- "port quit game" (and the PC version's "main menu quit game") quits, as
  closing the window does;
- the PC version's "profile set edit begin" begins editing a profile, as
  its settings screens need, and fails with none: the main menu's is the
  first player's, and then asks for a new one's name, as the menu's other
  items do, and its handlers open the screens that make the profile ("new
  campaign decision", after which Settings' is the one being edited); a
  game's pause menu edits the profile of the player who opened it;
- "gamespy screen init" hides the server browser's error and filter panels,
  and the title of the mode "mp type set mode" did not choose (Internet or
  LAN);
- its buttons' events: "mouse emit back event" and "mouse emit x event" push
  B and X (as the mouse does), "emit custom activation event" (an OK
  button) runs the screen's custom activation handler, "single prev cl item
  activated" (an item chosen) its list's, and its back handlers go back;
- "port setting load", on a spinner's created event, shows the value its
  setting has (or the nearest of its values): config.toml's, or for
  "profile.<field>" the controller's in the profile being edited;
- "port setting save", on its deleted event, writes the value shown back,
  if the player changed it;
- the settings screens' (tools/port_settings.py): "port settings save"
  (OK) writes those changed and applies the window's (the rest follow
  config.toml themselves), "port settings defaults" shows the defaults,
  "port settings help" the help of the row chosen (and Video Setup's
  Resolution or Window Size, for the display mode shown);
- the campaign randomizer's screen (tools/port_settings.py), after a
  campaign level's difficulty when game.randomizer is on: "randomizer
  profile save" asks for a name and writes that setup beside config.toml,
  "randomizer profile load" copies a saved one into the setup shown (with
  "randomizer profiles delete mode", DELETE, on: marks it, then deletes it,
  as "randomizer profile delete", X, does; "randomizer profiles init" turns
  it off as Load's screen opens),
  "randomizer start" writes the setup and starts the level, and
  "randomizer profiles update" shows the names saved; "randomizer custom
  left" and "right" choose the Tag Injector profile's custom type of a
  TAG INJECTOR row; before it, with the Tag Injector on and a profile,
  "randomizer tag profiles update" lists them and "randomizer tag
  profile choose" plays one (or none) and opens it: the campaign profiles
  too (tag_campaign.c), the randomizer on or not, one of which plays the
  level in the randomizer's place, its screen passed over;
- the skulls screen (tools/port_settings.py), after a campaign level's
  difficulty (and the randomizer's screen) when game.skulls is on: "skull
  toggle" turns the skull of the item chosen on or off, "skulls toggle all"
  turns those had all off when any is on, else all on, "skulls random
  roll" (RANDOM's screen's ROLL) turns on a number of those had between
  the fewest and the most chosen, at random, and the rest off, "skulls start"
  writes game.skulls_active and starts the level,
  "skulls up", "down", "left" and "right" move through its grid, and
  "skulls screen update" ticks them and shows the chosen one's help;
- Settings > Gameplay > Skulls: "skulls reset progress" (A twice) forgets
  the skulls found and the question of how they are had;
- Settings > Gameplay > Tag Injector (tools/port_settings.py), laid out as
  the Mods screen is: "port tags update" shows its rows and the details of
  the one chosen (under a map's tag's or a profile's, its model, turning:
  tag_preview.c), "port tags choose" (A) goes down a list (the profiles, a
  profile's kinds of tags, a kind's tags, a tag's settings; the maps'
  kinds, folders and maps, a map's kinds of tags, a kind's tags), changes a
  setting, or adds the tag
  chosen to the profile or takes it out, "port tags left" and "right" change
  the tag injection, a tag's on or off and its settings, and the profile
  tags are added to, "port tags back" (B) goes up a list or leaves,
  "port tags new profile" makes a profile, and "port tags profile button"
  deletes the one chosen in TAG PROFILES or, elsewhere, shows the one tags
  are added to (tag_injector.c);
- Controls Setup's: the keyboard and mouse's controls, two bindings each,
  shown a group at a time ("controls update menu"); "controls begin
  binding" takes the next key or button pressed for the binding left and
  right choose ("controls binding slot"); OK ("controls screen change set")
  writes them, "controls screen defaults" shows the defaults.

A spinner's values are its strings' (setting= and values= in its <widget>,
menu_tags.c's pc_menu_setting).

The PC version's campaign menus, on player 1's profile (the active one, else
the one last used, else the first) and the game's saved game in it:
- "campaign menu continue" goes on with the saved game;
- New Game's list of levels ("initialize sp level list solo", "solo map
  list update", "solo level set map") has those the profile has reached, and
  marks those it has finished on Normal, Heroic and Legendary, as the
  Xbox's list does; a level chosen there starts anew, whatever saved game
  is in it (the PC version's New Game: Continue and Load Game go on with
  one). Its first row's chooser shows MULTIPLAYER's maps
  instead, one chosen played alone at once;
- the difficulty menu: "difficulty item select" (a difficulty chosen) and
  "set difficulty" (its OK button: the difficulty shown) start the game;
- Load Game's list ("load game menu init", "load game list update", "load
  game menu activated") has each profile's saved game, and "load game menu
  delete request" and "delete finish" delete one.
The Xbox's functions of those names take the Xbox's widgets (a spinner of
levels, the difficulty list itself), so ours run instead (menu_tags.c).

Co-op (the Xbox's Cooperative Play, Multiplayer's CO-OP CAMPAIGN): "port coop
begin" makes two players, player 1 on its profile; "port coop player 2"
gives player 2 the profile chosen and the controller that chose it; then the
campaign's New Game and difficulty start the game for both.

The rest are the PC version's, which its menus (port/assets/menus/ce)
name and the Xbox's has not: they do nothing yet, and succeed, so that what
their handlers open opens.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "input/input.h"
#include "interface/event_manager.h"
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "rasterizer/xbox/rasterizer_xbox_hardware_bitmaps.h"
#include "interface/ui_widget_event_handler_functions.h"
#include "main/main.h"
#include "networking/network_game_manager.h"
#include "saved games/player_profile.h"
#include "tag_files/tag_groups.h"
#include "text/text_group.h"
#include "text/unicode.h"

#include "halo_menus.h"
#include "custom_edition_cache.h"
#include "custom_edition_maps.h"
#include "tag_injector.h"
#include "firing_range.h"
#include "tag_multiplayer.h"
#include "tag_campaign.h"
#include "tag_preview.h"
#include "campaign_score.h"
#include "community.h"
#include "../src/community_net.h"
#include "bots.h"
#include "game_modes.h"
#include "network_voice.h"
#include "text/draw_string.h"
/* (internet play's server browser: the platform layer's) */
#include "../src/p2p.h"
/* (the server browser's master servers, and Server Setup's publisher key) */
#include "../src/master_list.h"
#include "../src/server_master.h"
#include "text_filter.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtl.h>

/* the platform layer's (port/linux/src) */
void platform_log(char const *format, ...);
void platform_request_quit(void);
char const *pc_menu_function_name(long function_index);
char const *pc_menu_game_data_input_name(long function_index);
void event_manager_post_button(short controller_index, short button_index);
int config_text(char const *name, char *text, unsigned int size);
int config_write(char const *name, char const *value);
int config_boolean(char const *name);
void config_directory(char *path, unsigned int size);
int config_default(char const *name, char *text, unsigned int size);
char const *config_string(char const *name);
long config_integer(const char *name);
void platform_display_apply(void);
void platform_binding_capture_begin(void);
int platform_binding_capture_poll(int *input);
void halo_input_name(int input, char *name, unsigned int size);
void halo_input_shown_name(char const *binding, char *shown, unsigned int size);
short pc_menu_string_index(long definition_index);
/* cseries_windows.c's */
unsigned long system_milliseconds(void);
/* the link page (port/linux/src/link_page.c), the Switch's and the PS5's:
Direct Link's way in for an invite link, with its QR code */
#if defined(HALO_SWITCH) || defined(HALO_PS5)
#define LINK_PAGE 1
#endif
void link_page_wanted(void);
char const *link_page_address(void);
int link_page_qr_size(void);
int link_page_qr_dark(int x, int y);
int link_page_text_qr(char const *text);
int link_page_text_qr_dark(int x, int y);

/* the game's (port) */
boolean ui_widget_port_dispatch_event(struct widget_instance *widget, short event_type, short controller_index,
	boolean *deleted);
void ui_widget_port_go_back(struct widget_instance *widget);
short ui_widget_port_list_index(struct widget_instance *list_widget);
boolean ui_widget_port_saved_game(char const **map_name, short *level, short *difficulty);
char const *ui_widget_event_handler_function_name(long function_index);
short main_get_solo_level_from_name(char const *name);
boolean player_name_clean(wchar_t *name, long count);
short players_port_local_player_count(void);

boolean pc_menu_event_function_invoke(struct widget_instance *widget, struct event_record *event,
	long function_index, boolean *widget_deleted);
void pc_menu_game_data_function_invoke(struct widget_instance *widget, long function);

/* ---------- constants */

#define MAXIMUM_STRINGS 64
/* the PC version's custom activation event (this engine never sends it) */
#define EVENT_CUSTOM_ACTIVATION 32
#define BUTTON_A 0
#define BUTTON_B 1
#define BUTTON_X 2
#define BUTTON_START 12

enum
{
	_pc_menu_function_quit_game,
	_pc_menu_function_setting_load,
	_pc_menu_function_setting_save,
	NUMBER_OF_PC_MENU_FUNCTIONS
};

/* ---------- structures */

/* a widget, as ui_widget.c has it */
struct widget_instance
{
	long definition_tag_index;
	char const *name;
	short local_player_index;
	short horizontal_offset;
	short vertical_offset;
	short type;
	boolean visible;
	boolean render_regardless_of_controller_index;
	boolean disabled;
	boolean pause_game_time;
	boolean delete_recursion_lock;
	boolean widget_is_error_dialog;
	boolean close_if_local_player_controller_present;
	byte pad17;
	long creation_time;
	unsigned long milliseconds_to_auto_close;
	unsigned long auto_close_fade_time;
	real alpha_modifier;
	struct widget_instance *previous;
	struct widget_instance *next;
	struct widget_instance *parent;
	struct widget_instance *child;
	struct widget_instance *focused_child;
	union
	{
		struct
		{
			wchar_t *text;
			short string_list_index;
		} text_box;
		struct
		{
			short selected_index;
			short last_list_tab_direction;
			void *list_items;
			word number_of_items;
			struct widget_instance *extended_description;
			wchar_t *item_text;
		} list;
	} parameters;
	struct
	{
		short current_frame_index;
		short first_frame_index;
		short last_frame_index;
		short number_of_sprite_frames;
	} animation;
};

typedef char verify_widget_instance_size[
	sizeof(struct widget_instance) == 0x58 ? 1 : -1];
typedef char verify_widget_instance_child_offset[
	offsetof(struct widget_instance, child) == 0x34 ? 1 : -1];
typedef char verify_widget_instance_parent_offset[
	offsetof(struct widget_instance, parent) == 0x30 ? 1 : -1];
typedef char verify_widget_instance_selected_index_offset[
	offsetof(struct widget_instance, parameters.list.selected_index) == 0x3C ? 1 : -1];
typedef char verify_widget_instance_number_of_items_offset[
	offsetof(struct widget_instance, parameters.list.number_of_items) == 0x44 ? 1 : -1];
typedef char verify_widget_instance_animation_offset[
	offsetof(struct widget_instance, animation) == 0x50 ? 1 : -1];

/* menu_tags.c's */
struct pc_menu_setting
{
	long definition_index;
	char const *setting;
	long value_count;
	char const *values[MAXIMUM_STRINGS];
	short loaded_index;
};

struct pc_menu_setting *pc_menu_setting_get(long definition_index);

/* ---------- private code */

static boolean campaign_profile(short controller, struct player_profile *profile);

static boolean text_is_number(char const *text, double *number)
{
	char *end;

	*number = strtod(text, &end);
	return *text && !*end;
}

/* whether the widget is one of a spinner's items, which the game makes of
its strings from its own definition, handlers and all (ui_widget.c,
ui_widget_load_children_recursive) */
static boolean spinner_item(struct widget_instance const *widget)
{
	return widget->parent && widget->parent->definition_tag_index == widget->definition_tag_index;
}

/* the value shown for the setting's: the same one, else the nearest number;
a word with "-" before it (network.browser_sort's order reversed: "-players")
the word's */
static short setting_value_index(struct pc_menu_setting const *setting, char const *current)
{
	double current_number, value_number, distance = 0.0;
	short index, nearest = 0;

	for (index = 0; index < setting->value_count; index++)
	{
		if (!_stricmp(setting->values[index], current))
			return index;
	}
	if (current[0] == '-' && !text_is_number(current, &current_number))
	{
		for (index = 0; index < setting->value_count; index++)
		{
			if (!_stricmp(setting->values[index], current + 1))
				return index;
		}
	}
	if (!text_is_number(current, &current_number))
		return 0;
	for (index = 0; index < setting->value_count; index++)
	{
		if (text_is_number(setting->values[index], &value_number))
		{
			double gap = value_number > current_number ? value_number - current_number : current_number - value_number;

			if (index == 0 || gap < distance)
			{
				distance = gap;
				nearest = index;
			}
		}
	}
	return nearest;
}

/* the profile's settings ("profile.<field>"): those of the controller in
the profile being edited */
static struct
{
	char const *name;
	short field;
	/* its default (player_profile.c's new profile's) */
	byte default_value;
} const profile_settings[] =
{
	{ "profile.look_sensitivity", 0, 3 },
	{ "profile.invert_look", 1, FALSE },
	{ "profile.flight_inversion", 2, FALSE },
	{ "profile.autocenter", 3, FALSE },
	{ "profile.button_preset", 4, 0 },
	{ "profile.joystick_preset", 5, 0 },
	{ "profile.vibration", 6, TRUE },
	{ "profile.ingame_help", 7, TRUE },
};

static byte *profile_setting_field(struct player_profile *profile, short field)
{
	struct player_profile_controller_settings *controls = &profile->controller_settings;

	switch (field)
	{
	case 0: return &controls->look_sensitivity;
	case 1: return (byte *)&controls->invert_look;
	case 2: return (byte *)&controls->flight_stick_aircraft_controls;
	case 3: return (byte *)&controls->autocenter;
	case 4: return &controls->button_preset;
	case 5: return &controls->joystick_preset;
	case 6: return (byte *)&controls->vibration_disabled;
	case 7: return (byte *)&controls->ingame_help_disabled;
	}
	return NULL;
}

static boolean profile_setting_boolean(short field)
{
	return field != 0 && field != 4 && field != 5;
}

/* a setting's value as text, or (default_value) its default: config.toml's,
or the profile's */
static boolean setting_text(char const *name, char *text, unsigned int size, boolean default_value)
{
	short index;

	if (strncmp(name, "profile.", 8))
	{
		if (!(default_value ? config_default(name, text, size) : config_text(name, text, size)))
			return FALSE;
		/* (display.mode empty: display.fullscreen's, as the window has it:
		sdl_platform.c) */
		if (!strcmp(name, "display.mode") && !text[0])
			snprintf(text, size, "%s", default_value || config_boolean("display.fullscreen") ? "borderless" : "windowed");
		/* (display.window_size empty: 640x480 times display.window_scale, as
		older versions set it) */
		if (!strcmp(name, "display.window_size") && !text[0])
		{
			char scale_text[32] = "";
			long scale;

			if (default_value)
				config_default("display.window_scale", scale_text, sizeof(scale_text));
			else
				config_text("display.window_scale", scale_text, sizeof(scale_text));
			scale = atol(scale_text);
			if (scale < 1)
				scale = 1;
			snprintf(text, size, "%ldx%ld", 640 * scale, 480 * scale);
		}
		return TRUE;
	}
	for (index = 0; index < NUMBEROF(profile_settings); index++)
	{
		struct player_profile *profile = player_ui_get_edit_player_profile();
		short field = profile_settings[index].field;
		long value;

		if (strcmp(name, profile_settings[index].name))
			continue;
		if (default_value)
			value = profile_settings[index].default_value;
		else if (profile && profile_setting_field(profile, field))
			value = *profile_setting_field(profile, field);
		else
			return FALSE;
		/* (vibration and help are kept as their opposites) */
		if (!default_value && (field == 6 || field == 7))
			value = !value;
		if (profile_setting_boolean(field))
			snprintf(text, size, "%s", value ? "true" : "false");
		else
			snprintf(text, size, "%ld", value);
		return TRUE;
	}
	return FALSE;
}

static boolean setting_write(char const *name, char const *value)
{
	short index;

	if (strncmp(name, "profile.", 8))
		return config_write(name, value);
	for (index = 0; index < NUMBEROF(profile_settings); index++)
	{
		struct player_profile *profile = player_ui_get_edit_player_profile();
		short field = profile_settings[index].field;
		byte *place;
		long number;

		if (strcmp(name, profile_settings[index].name))
			continue;
		if (!profile || !(place = profile_setting_field(profile, field)))
			return FALSE;
		number = profile_setting_boolean(field) ? !strcmp(value, "true") : atol(value);
		if (field == 6 || field == 7)
			number = !number;
		*place = (byte)number;
		return TRUE;
	}
	return FALSE;
}

static void setting_written(char const *name, char const *value);
extern void randomizer_pack_request(void);
extern void skulls_pack_request(void);

static boolean setting_load(struct widget_instance *widget)
{
	struct pc_menu_setting *setting = pc_menu_setting_get(widget->definition_tag_index);
	char current[300];

	if (spinner_item(widget))
		return TRUE;
	if (!setting || !setting_text(setting->setting, current, sizeof(current), FALSE))
		return FALSE;
	setting->loaded_index = setting_value_index(setting, current);
	if (setting->loaded_index < (short)widget->parameters.list.number_of_items)
		widget->parameters.list.selected_index = setting->loaded_index;
	return TRUE;
}

static boolean setting_save(struct widget_instance *widget)
{
	struct pc_menu_setting *setting = pc_menu_setting_get(widget->definition_tag_index);

	if (spinner_item(widget))
		return TRUE;
	if (!setting || setting->loaded_index == NONE || widget->parameters.list.selected_index < 0 ||
		widget->parameters.list.selected_index >= setting->value_count)
		return FALSE;
	if (widget->parameters.list.selected_index == setting->loaded_index)
		return TRUE;
	if (!config_write(setting->setting, setting->values[widget->parameters.list.selected_index]))
	{
		platform_log("menus: could not write %s to config.toml", setting->setting);
		return FALSE;
	}
	platform_log("menus: %s = %s (from the next start)", setting->setting, setting->values[widget->parameters.list.selected_index]);
	setting->loaded_index = widget->parameters.list.selected_index;
	setting_written(setting->setting, setting->values[setting->loaded_index]);
	return TRUE;
}

static short controller_of(struct widget_instance const *widget)
{
	return widget->local_player_index >= 0 && widget->local_player_index < 4 ? widget->local_player_index : 0;
}

/* the controller the event came from, else the widget's */
static short event_controller(struct widget_instance const *widget, struct event_record const *event)
{
	return event && event->controller_index >= 0 && event->controller_index < 4 ?
		event->controller_index : controller_of(widget);
}

/* runs the custom activation handler of the widget's list, or the nearest
of its parents' that has one (widget_deleted: the widget went with its
screen) */
static boolean item_activated(struct widget_instance *widget, short controller, boolean *widget_deleted)
{
	struct widget_instance *parent;

	for (parent = widget->parent; parent; parent = parent->parent)
	{
		boolean deleted;

		if (ui_widget_port_dispatch_event(parent, EVENT_CUSTOM_ACTIVATION, controller, &deleted))
		{
			if (deleted)
				*widget_deleted = TRUE;
			return TRUE;
		}
	}
	return FALSE;
}

/* whether the multiplayer menu's choice was LAN, not Internet (mp type set
mode) */
static boolean lan_mode;

/* hides the descendants of the widget with the name */
static void hide_named(struct widget_instance *widget, char const *name)
{
	struct widget_instance *child;

	for (child = widget->child; child; child = child->next)
	{
		if (!strcmp(child->name, name))
			child->visible = FALSE;
		hide_named(child, name);
	}
}

/* the server browser's panels for errors and filters are hidden until they
are wanted, and its title is Internet's or LAN's (gamespy screen init) */
static void server_browser_initialize(struct widget_instance *screen)
{
	struct widget_instance *child;

	hide_named(screen, lan_mode ? "header_internet" : "header_lan");
	/* (a widget's name is its definition's: the last part of ours) */
	for (child = screen->child; child; child = child->next)
	{
		if (!strcmp(child->name, "gamespy_error_fullscreen") || !strcmp(child->name, "filters_screen"))
		{
			child->visible = FALSE;
			if (screen->focused_child == child)
				screen->focused_child = NULL;
		}
	}
	for (child = screen->child; child && !screen->focused_child; child = child->next)
	{
		if (child->visible && child->type == 3)
			screen->focused_child = child;
	}
}

/* begins editing player 1's profile (as the campaign has it); FALSE if
there is none */
boolean pc_menu_profile_edit_begin(void)
{
	struct player_profile profile;

	if (!campaign_profile(0, &profile))
		return FALSE;
	player_ui_begin_editing_profile(player_ui_get_active_player_profile_index(0));
	return TRUE;
}

/* the game's own function of the name (as its tags name it), run for the
widget: for ours of a name of the game's, which do more than it */
static boolean game_function(char const *name, struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted)
{
	char const *function;
	long index;

	for (index = 0; (function = ui_widget_event_handler_function_name(index)) != NULL; index++)
	{
		if (!strcmp(function, name))
			return ui_widget_event_handler_function_invoke(widget, event, (word)index, widget_deleted);
	}
	return FALSE;
}

/* "profile set edit begin": Settings, the main menu's and a game's pause
menu's. With no profile, the main menu's asks for a new one's name, as the
menu's other items do ("new game if no plyr profiles"), and fails: the
screens its handler then opens make the profile and edit it
(new_profile_decision). Without the name asked, they made none, and showed
"new campaign decision" as failed. The pause menu's has no such screens,
and only fails. The main menu's is player 1's profile. The pause menu's is
the profile of the player who opened it (split screen's second player has
their own). */
static boolean profile_edit_begin(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted)
{
	if (!main_menu_is_active())
	{
		short controller = controller_of(widget);
		long index = player_ui_get_active_player_profile_index(controller);
		struct player_profile profile;

		if (index != NONE && player_profile_get(index, &profile))
		{
			player_ui_begin_editing_profile(index);
			return TRUE;
		}
		return FALSE;
	}
	if (pc_menu_profile_edit_begin())
		return TRUE;
	/* (a press: the profile is made on its controller) */
	if (main_menu_is_active() && event && event->controller_index >= 0 && event->controller_index < 4)
		game_function("new campaign chosen", widget, event, widget_deleted);
	return FALSE;
}

/* "new campaign decision" (the screen after a new profile's name, each of
the main menu's items having its own): the game's makes the profile, if the
name was entered. Settings' then begins editing it: the PC version's screens
open Settings with nothing more run, and its screen closes at once with no
profile being edited ("close if not editing profile"). */
static boolean new_profile_decision(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted)
{
	if (!game_function("new campaign decision", widget, event, widget_deleted))
		return FALSE;
	return strcmp(widget->name, "settings_creating_profile") || pc_menu_profile_edit_begin();
}

/* ---------- the campaign */

/* the rows of its lists (main_menu/new_select/list_item_0 to 10) */
#define MAXIMUM_ROWS 11
/* its strings' and sp_levels' entries past the levels': a level not
reached, and (map_data) a saved game on the level */
#define LEVEL_UNAVAILABLE 10
#define LEVEL_IN_PROGRESS 11
#define ROW_TEXT_LENGTH 64
#define MAXIMUM_PROFILES 32
#define SOUND_ERROR 4
#define SOUND_CURSOR 1
#define SOUND_FORWARD 2
#define SOUND_BACK 3

/* (an index whose profile reads: saved_game_files.c's) */
#define PROFILE_VALID_BIT 0x80000000UL

struct campaign_level
{
	boolean available;
	/* finished on Normal, Heroic, Legendary (difficulty_options_small's
	frames 1 to 3) */
	boolean finished[3];
};

struct campaign_saved_game
{
	long profile_index;
	wchar_t profile_name[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH + 1];
	short level;
	short difficulty;
};

static struct
{
	struct campaign_level levels[NUMBER_OF_SINGLE_PLAYER_LEVELS];
	/* player 1's saved game's level, else NONE */
	short saved_level;
	/* the level and saved game the lists' descriptions last showed */
	short shown_level;
	short shown_saved_game;
	/* Load Game's */
	struct campaign_saved_game saved_games[MAXIMUM_ROWS];
	short saved_game_count;
	short saved_game_to_delete;
} campaign;

/* the descendant of the widget with the name (a widget's name is its
definition's: the last part of ours), the nth of them */
static struct widget_instance *descendant(struct widget_instance *widget, char const *name, long *nth)
{
	struct widget_instance *child;

	for (child = widget->child; child; child = child->next)
	{
		struct widget_instance *found;

		if (!strcmp(child->name, name) && (*nth)-- == 0)
			return child;
		found = descendant(child, name, nth);
		if (found)
			return found;
	}
	return NULL;
}

static struct widget_instance *named(struct widget_instance *widget, char const *name, long nth)
{
	return widget ? descendant(widget, name, &nth) : NULL;
}

/* the text box's own text (as the server list sets its items'), in a
buffer of length characters (made the first time: a text box is always
given the same length) */
static void text_set_length(struct widget_instance *text_box, wchar_t const *text, short length)
{
	if (!text_box)
		return;
	if (!text_box->parameters.text_box.text)
	{
		text_box->parameters.text_box.text = ui_widget_realloc(NULL, length * sizeof(wchar_t),
			__FILE__, __LINE__);
	}
	if (text_box->parameters.text_box.text)
	{
		ustrncpy(text_box->parameters.text_box.text, text, length - 1);
		text_box->parameters.text_box.text[length - 1] = 0;
	}
}

static void text_set(struct widget_instance *text_box, wchar_t const *text)
{
	text_set_length(text_box, text, ROW_TEXT_LENGTH);
}

/* a text of several lines (the leaderboards' notes, the daily challenge's
words): its widget's room is made once, as long as this, so it is always set
so */
#define LONG_TEXT_LENGTH 640

static void long_text_set(struct widget_instance *text_box, wchar_t const *text)
{
	text_set_length(text_box, text, LONG_TEXT_LENGTH);
}

/* a button there but not to be pressed (the leaderboards with scoring off):
dimmed, and passed over by the focus */
#define DISABLED_ALPHA 0.35f

static void enabled_set(struct widget_instance *widget, boolean enabled)
{
	if (!widget)
		return;
	widget->disabled = !enabled;
	widget->alpha_modifier = enabled ? 1.0f : DISABLED_ALPHA;
}

/* one of our string lists' strings, on one line (its line breaks spaces),
of up to size characters */
static void words_get(char const *list, short index, wchar_t *text, short size)
{
	long tag_index = tag_loaded('ustr', list);
	wchar_t const *string = tag_index != NONE ? unicode_string_list_get_string(tag_index, index) : NULL;
	long length = 0;

	for (; string && *string && length < size - 1; string++)
	{
		wchar_t character = *string == '\r' || *string == '\n' ? ' ' : *string;

		if (character != ' ' || (length && text[length - 1] != ' '))
			text[length++] = character;
	}
	while (length && text[length - 1] == ' ')
		length--;
	text[length] = 0;
}

static void string_get(char const *list, short index, wchar_t *text)
{
	words_get(list, index, text, ROW_TEXT_LENGTH);
}

/* the list's row that has the focus, else NONE (its buttons) */
static short focused_row(struct widget_instance *list)
{
	struct widget_instance *child;
	short index = 0;

	for (child = list->child; child && index < MAXIMUM_ROWS; child = child->next, index++)
	{
		if (child == list->focused_child)
			return strncmp(child->name, "list_item_", 10) ? NONE : index;
	}
	return NONE;
}

static void focus_row(struct widget_instance *list, short row)
{
	struct widget_instance *child = list->child;
	short index;

	for (index = 0; child && child->next && index < row; index++)
		child = child->next;
	if (row == NONE)
	{
		/* (its buttons: the last child) */
		while (child && child->next)
			child = child->next;
	}
	if (child)
	{
		list->focused_child = child;
		list->parameters.list.selected_index = row == NONE ? 0 : row;
	}
}

/* shows the first count rows, with their texts, the arrows on the one that
has the focus, and no scroll buttons (the rows are enough) */
static void rows_update(struct widget_instance *list, short count, void (*row_text)(short row, wchar_t *text))
{
	struct widget_instance *row;
	short index = 0;

	for (row = list->child; row && index < MAXIMUM_ROWS; row = row->next, index++)
	{
		wchar_t text[ROW_TEXT_LENGTH];
		struct widget_instance *arrows = named(row, "list_item_arrows", 0);

		if (strncmp(row->name, "list_item_", 10))
			break;
		row->visible = index < count;
		if (index < count)
		{
			row_text(index, text);
			text_set(named(row, "list_item_text", 0), text);
		}
		if (arrows)
			arrows->visible = row == list->focused_child;
		if (named(row, "scroll_up_button", 0))
			named(row, "scroll_up_button", 0)->visible = FALSE;
		if (named(row, "scroll_down_button", 0))
			named(row, "scroll_down_button", 0)->visible = FALSE;
	}
}

/* player 1's profile, read again (the active one, else the one last used,
else the first), on the controller (in co-op, on the one that chose co-op:
coop_begin); FALSE if there is none */
static boolean campaign_profile(short controller, struct player_profile *profile)
{
	long profile_index = player_ui_get_active_player_profile_index(0);

	if (profile_index == NONE || !player_profile_get(profile_index, profile))
	{
		profile_index = player_ui_get_player1_last_used_profile_index();
		if (profile_index == NONE || !player_profile_get(profile_index, profile))
		{
			word count = 1;

			profile_index = NONE;
			player_profiles_enumerate_available_to_local_player_index(NONE, &count, &profile_index, FALSE);
			if ((short)count <= 0 || profile_index == NONE || !player_profile_get(profile_index, profile))
				return FALSE;
		}
	}
	player_ui_set_active_player_profile(0, profile_index, profile);
	if (player_spawn_count < 2)
		player_ui_set_single_player_local_player_controller(0, controller);
	return TRUE;
}

/* game.campaign_progression "all_unlocked": every campaign level can be
chosen, whatever the profiles have reached (the Xbox's menus too:
ui_widget_event_handler_functions.c) */
int campaign_levels_all_unlocked(void)
{
	char const *progression = config_string("game.campaign_progression");

	return progression && !_stricmp(progression, "all_unlocked");
}

/* the levels the profile has reached (as the Xbox's list has them: those
it has played, the one after the last it finished, the first; every one,
with game.campaign_progression "all_unlocked") and finished,
and its saved game's; in co-op, those either player's has reached and
finished, and no saved game (a game of one player's does not go on with
two: game_state.c's game_state_header_valid) */
static void campaign_levels_read(struct player_profile const *profile)
{
	char const *map_name;
	struct player_profile player2;
	short highest_level, highest_difficulty, difficulty, level, player;

	for (level = 0; level < NUMBER_OF_SINGLE_PLAYER_LEVELS; level++)
		memset(&campaign.levels[level], 0, sizeof(campaign.levels[level]));
	if (player_spawn_count >= 2)
		player_ui_get_active_player_profile(1, &player2);
	for (player = 0; player < (player_spawn_count >= 2 ? 2 : 1); player++)
	{
		struct player_profile const *reader = player == 0 ? profile : &player2;

		player_profile_get_highest_completed_solo_level((struct player_profile *)reader, &highest_level,
			&highest_difficulty);
		for (level = 0; level < NUMBER_OF_SINGLE_PLAYER_LEVELS; level++)
		{
			byte flags = reader->single_player_map_flags[level];
			short marker;

			campaign.levels[level].available |= flags || level == highest_level + 1 || level == 0 ||
				campaign_levels_all_unlocked();
			for (marker = 0; marker < 3; marker++)
				campaign.levels[level].finished[marker] |= (flags >> (marker + 1)) & 1;
		}
	}
	if (player_spawn_count >= 2 || !ui_widget_port_saved_game(&map_name, &campaign.saved_level, &difficulty))
		campaign.saved_level = NONE;
}

/* plays the map, at the difficulty: with saved_game, the saved game in it
goes on (main.c's main_new_map, if its difficulty is this one); without,
the map starts anew, as the game does going on to the next level
(ui_set_next_level), whatever saved game the profile has in it */
static void campaign_start(char const *map_name, short difficulty, short controller, boolean saved_game)
{
	if (player_spawn_count < 2)
		player_ui_set_single_player_local_player_controller(0, controller);
	main_set_map_name(map_name);
	if (!saved_game)
		main_disallow_persistent_storage();
	main_set_difficulty(difficulty);
	game_connection_set(0);
	main_menu_switch_to_single_player();
	player_ui_remember_player1_profile(TRUE);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
}

static boolean campaign_fail(void)
{
	ui_play_audio_feedback_sound(SOUND_ERROR);
	return FALSE;
}

/* the description's level: its name, picture and words, and the
difficulties it has been finished on (finished: New Game's, NULL for a level
not reached), or (saved_difficulty, not NONE: Load Game's) the saved game's
difficulty */
static void level_description(struct widget_instance *description, char const *prefix, short level, boolean in_progress,
	boolean const *finished, short saved_difficulty)
{
	char name[64];
	struct widget_instance *widget;
	short marker;

	snprintf(name, sizeof(name), "%s_right_name", prefix);
	if ((widget = named(description, name, 0)) != NULL)
		widget->parameters.text_box.string_list_index = level;
	snprintf(name, sizeof(name), "%s_right_pic", prefix);
	if ((widget = named(description, name, 0)) != NULL)
		widget->animation.current_frame_index = level;
	snprintf(name, sizeof(name), "%s_right_data", prefix);
	if ((widget = named(description, name, 0)) != NULL && saved_difficulty != NONE)
	{
		wchar_t text[ROW_TEXT_LENGTH];

		string_get("pc\\main_menu\\player_profiles_select\\difficulty_names", saved_difficulty, text);
		text_set(widget, text);
	}
	else if (widget)
		widget->parameters.text_box.string_list_index = in_progress ? LEVEL_IN_PROGRESS : level;
	/* (the level's records, this machine's best score and time, with the
	campaign's score: campaign_score.c's; none for a level not reached) */
	snprintf(name, sizeof(name), "%s_right_record", prefix);
	if ((widget = named(description, name, 0)) != NULL)
	{
		wchar_t text[ROW_TEXT_LENGTH];
		wchar_t score_text[24];
		wchar_t time_text[24];
		wchar_t medals_text[24];
		struct campaign_score_record record;
		long profile_index = player_ui_get_active_player_profile_index(0);

		/* (player 1's profile's: the active one, else the one used last) */
		if (profile_index == NONE)
			profile_index = player_ui_get_player1_last_used_profile_index();
		text[0] = 0;
		/* (none with scoring off: no scores are kept) */
		if (level >= 0 && level < NUMBER_OF_SINGLE_PLAYER_LEVELS && finished && community_scoring())
		{
			usprintf(score_text, L"PENDING");
			usprintf(time_text, L"PENDING");
			usprintf(medals_text, L"PENDING");
			if (campaign_score_level_record(main_get_solo_level_name(level), profile_index, &record))
			{
				usnprintf(score_text, NUMBEROF(score_text), L"%ld", record.score);
				usnprintf(time_text, NUMBEROF(time_text), L"%ld:%02ld:%02ld", record.seconds / 3600,
					record.seconds / 60 % 60, record.seconds % 60);
				if (record.medals != NONE)
					usnprintf(medals_text, NUMBEROF(medals_text), L"%ld", record.medals);
			}
			/* (two lines: draw_string.c's line ends at '\r') */
			usnprintf(text, NUMBEROF(text), L"SCORE: %s\rTIME: %s\rMEDALS: %s", score_text, time_text, medals_text);
		}
		text_set(widget, text);
	}
	for (marker = 0; marker < 3; marker++)
	{
		if ((widget = named(description, "difficulty_indicator", marker)) != NULL)
		{
			widget->visible = saved_difficulty != NONE ? marker == 0 : finished && finished[marker];
			widget->animation.current_frame_index = saved_difficulty != NONE ? saved_difficulty : marker + 1;
		}
		/* (New Game's, small badges over the picture's corner, where its
		records have the icons' place: port_settings.py's LEVEL_BADGE) */
		if ((widget = named(description, "difficulty_badge", marker)) != NULL)
		{
			widget->visible = finished && finished[marker];
			widget->animation.current_frame_index = marker + 1;
		}
	}
	/* (the badges' plate, with any of them) */
	if ((widget = named(description, "difficulty_badges_plate", 0)) != NULL)
		widget->visible = finished && (finished[0] || finished[1] || finished[2]);
}

/* player 1's profile's name (the active one, else the one used last: the
main menu clears the active ones), on the descriptions'
current_profile_name */
static void profile_name_show(struct widget_instance *description)
{
	/* (the profile read from its file at most once a second) */
	static struct player_profile profile;
	static long read_index = NONE;
	static unsigned long read_time;
	static boolean read_good;
	long index = player_ui_get_active_player_profile_index(0);

	if (!description)
		return;
	if (index != NONE)
	{
		player_ui_get_active_player_profile(0, &profile);
		read_index = NONE;
	}
	else if ((index = player_ui_get_player1_last_used_profile_index()) == NONE)
		return;
	else
	{
		/* (one that cannot be read too: tried again a second later) */
		if (index != read_index || system_milliseconds() - read_time > 1000)
		{
			read_index = index;
			read_good = player_profile_get(index, &profile);
			read_time = system_milliseconds();
		}
		if (!read_good)
			return;
	}
	text_set(named(description, "current_profile_name", 0), profile.player_name);
}

/* (Campaign's menu) "campaign menu init": player 1's profile and saved
game, for its items; DAILY CHALLENGE only with game.daily_challenge on (off,
Back takes its place) */
static boolean campaign_menu_initialize(struct widget_instance *list, short controller)
{
	struct player_profile profile;
	struct widget_instance *daily_item = named(list, "menu_item_daily", 0);
	struct widget_instance *back_item = named(list, "menu_item_back", 0);

	if (daily_item && back_item && !config_boolean("game.daily_challenge"))
	{
		daily_item->visible = FALSE;
		back_item->vertical_offset = daily_item->vertical_offset;
		if (list->focused_child == daily_item)
			list->focused_child = back_item;
	}
	if (campaign_profile(controller, &profile))
		campaign_levels_read(&profile);
	return TRUE;
}

/* "campaign menu continue": the saved game goes on */
static boolean campaign_continue(short controller)
{
	struct player_profile profile;
	char const *map_name;
	short level, difficulty;

	if (!campaign_profile(controller, &profile) || !ui_widget_port_saved_game(&map_name, &level, &difficulty))
		return campaign_fail();
	campaign_start(map_name, difficulty, controller, TRUE);
	return TRUE;
}

/* ---- the map lists (New Game's and the Map screen's): their first row's
chooser of SINGLEPLAYER (the campaign's levels), MULTIPLAYER maps (the
Xbox's), CUSTOM SINGLEPLAYER or CUSTOM MULTIPLAYER maps (the Custom Edition
maps of the custom_maps folder, by the scenario type their files give:
custom_edition_maps.c) (port_settings.MAP_KIND_CHOOSER), then the kind's
rows, scrolling as list_scroll scrolls. A singleplayer kind's map is played
as the campaign is (alone, or hosted as network co-op), a multiplayer kind's
as multiplayer maps are. DAILY CHALLENGES are the daily challenges (with
game.daily_challenge on: community.c's past challenges, today's first), each
played as the campaign's levels are, at its own difficulty with its own
randomizer and skulls, alone, in split screen or hosted. */

enum
{
	MAP_KIND_SINGLEPLAYER,
	MAP_KIND_MULTIPLAYER,
	MAP_KIND_CUSTOM_SINGLEPLAYER,
	MAP_KIND_CUSTOM_MULTIPLAYER,
	MAP_KIND_DAILY,
	NUMBER_OF_MAP_KINDS
};

/* the rows after the chooser */
#define MAP_KIND_ROWS 10

static void visible_set(struct widget_instance *widget, boolean visible);
static struct widget_instance *screen_of(struct widget_instance *widget);
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);

static boolean map_kind_singleplayer(short kind)
{
	return kind == MAP_KIND_SINGLEPLAYER || kind == MAP_KIND_CUSTOM_SINGLEPLAYER || kind == MAP_KIND_DAILY;
}

/* whether the chooser may show the kind: a singleplayer one only with
`singleplayer`, the daily challenges only with game.daily_challenge on */
static boolean map_kind_allowed(short kind, boolean singleplayer)
{
	if (!singleplayer && map_kind_singleplayer(kind))
		return FALSE;
	return kind != MAP_KIND_DAILY || config_boolean("game.daily_challenge");
}

/* The kind the chooser shows, kept to those allowed (map_kind_allowed): one
it was turned to that is not becomes the next that is, the way it turned
from `previous`. */
static short map_kind_shown(struct widget_instance *list, boolean singleplayer, short previous)
{
	struct widget_instance *spinner = named(list, "list_item_0_map_kind_spinner", 0);
	short kind;

	if (!spinner)
		return singleplayer ? MAP_KIND_SINGLEPLAYER : MAP_KIND_MULTIPLAYER;
	kind = (short)PIN(spinner->parameters.list.selected_index, 0, NUMBER_OF_MAP_KINDS - 1);
	if (!map_kind_allowed(kind, singleplayer))
	{
		short step = kind == (previous + NUMBER_OF_MAP_KINDS - 1) % NUMBER_OF_MAP_KINDS ? -1 : 1;

		do
			kind = (short)((kind + step + NUMBER_OF_MAP_KINDS) % NUMBER_OF_MAP_KINDS);
		while (!map_kind_allowed(kind, singleplayer));
		spinner->parameters.list.selected_index = kind;
	}
	return kind;
}

static void map_kind_set(struct widget_instance *list, short kind)
{
	struct widget_instance *spinner = named(list, "list_item_0_map_kind_spinner", 0);

	if (spinner)
		spinner->parameters.list.selected_index = kind;
}

/* the first of count entries shown, for the entry chosen to be in the middle */
static short map_kind_first(short chosen, short count)
{
	return (short)PIN(chosen - MAP_KIND_ROWS / 2, 0, MAX(count - MAP_KIND_ROWS, 0));
}

static boolean list_scrolls_at_end(void);

static void map_kind_focus(struct widget_instance *list, short first, short chosen)
{
	focus_row(list, (short)(1 + chosen - first));
}

/* the rows after the chooser: count entries from *first's, scrolled on at
their ends; the entry focused, NONE when the chooser or the buttons have the
focus */
static short map_kind_rows_update(struct widget_instance *list, short *first, short count,
	void (*entry_text)(short entry, wchar_t *text))
{
	short shown = (short)MIN(count, MAP_KIND_ROWS);
	short row = focused_row(list), index = 0;
	struct widget_instance *child;

	if (row != NONE && row >= 1 && row == shown && *first + shown < count && list_scrolls_at_end())
	{
		(*first)++;
		focus_row(list, --row);
	}
	else if (row == 1 && *first > 0 && list_scrolls_at_end())
	{
		(*first)--;
		focus_row(list, ++row);
	}
	for (child = list->child; child; child = child->next, index++)
	{
		wchar_t text[ROW_TEXT_LENGTH];

		if (index < 1)
			continue;
		if (strncmp(child->name, "list_item_", 10))
			break;
		child->visible = index - 1 < shown;
		if (index - 1 < shown)
		{
			entry_text((short)(*first + index - 1), text);
			text_set(named(child, "list_item_text", 0), text);
		}
		visible_set(named(child, "list_item_arrows", 0), child == list->focused_child);
		visible_set(named(child, "scroll_up_button", 0), FALSE);
		visible_set(named(child, "scroll_down_button", 0), FALSE);
	}
	return row == NONE || row < 1 || row > shown ? NONE : (short)(*first + row - 1);
}

/* the Xbox's multiplayer maps: the first of the multiplayer maps
(ui_widget_port_multiplayer_maps), which go on with the Custom Edition ones */
static short xbox_multiplayer_map_count(short multiplayer_map_count)
{
	return (short)MAX(multiplayer_map_count - custom_edition_maps_count(FALSE), 0);
}

static void multiplayer_map_text(short map, wchar_t *text)
{
	string_get("pc\\main_menu\\mp_map_list", map, text);
}

/* a Custom Edition map's name (the menus find its name, picture and
description by its display index: custom_edition_maps.c) */
static void custom_map_text(boolean campaign, short map, wchar_t *text)
{
	wchar_t const *name = custom_edition_maps_name(custom_edition_maps_display_index_of(campaign, map));

	ustrncpy(text, name ? name : L"", ROW_TEXT_LENGTH - 1);
	text[ROW_TEXT_LENGTH - 1] = 0;
}

/* one level of custom_maps: the top is loose maps and folder names, and a
folder's rows are the maps in it. Rebuilt when the kind or folder changes. */
struct map_browse_entry
{
	boolean folder;
	/* the map's index in custom_edition_maps_count's list; a folder's is
	one map in that folder, so its name can be read */
	short flat;
};

static struct map_browse_entry map_browse_entries[CUSTOM_EDITION_MAPS_MAXIMUM];
static short map_browse_count_cache;
static boolean map_browse_campaign_cache;
static char map_browse_category_cache[CUSTOM_EDITION_MAP_CATEGORY_LENGTH];
static boolean map_browse_valid;
static boolean map_browse_sorting_campaign;

static void map_browse_forget(void)
{
	map_browse_valid = FALSE;
}

static void map_browse_entry_text(boolean campaign, struct map_browse_entry const *entry, wchar_t *text)
{
	short index;

	if (entry->folder)
	{
		char const *name = custom_edition_maps_category(campaign, entry->flat);

		if (!name)
			name = "";
		for (index = 0; name[index] && index < ROW_TEXT_LENGTH - 1; index++)
			text[index] = (wchar_t)(unsigned char)name[index];
		text[index] = 0;
		return;
	}
	custom_map_text(campaign, entry->flat, text);
}

static int map_browse_compare(void const *left, void const *right)
{
	wchar_t left_name[ROW_TEXT_LENGTH];
	wchar_t right_name[ROW_TEXT_LENGTH];

	map_browse_entry_text(map_browse_sorting_campaign, left, left_name);
	map_browse_entry_text(map_browse_sorting_campaign, right, right_name);

	return wcsicmp(left_name, right_name);
}

/* the rows of `category` (empty: the maps folder's loose maps and its
folders). Inside a folder the maps are already sorted by name. */
static void map_browse_build(boolean campaign, char const *category)
{
	short total;
	short index;
	short count;
	char const *open = category ? category : "";

	if (map_browse_valid && map_browse_campaign_cache == campaign &&
		!csstrcasecmp(map_browse_category_cache, open))
	{
		return;
	}
	csstrncpy(map_browse_category_cache, open, sizeof(map_browse_category_cache) - 1);
	map_browse_category_cache[sizeof(map_browse_category_cache) - 1] = 0;
	map_browse_campaign_cache = campaign;
	map_browse_valid = TRUE;
	total = custom_edition_maps_count(campaign);
	count = 0;
	if (open[0])
	{
		for (index = 0; index < total && count < CUSTOM_EDITION_MAPS_MAXIMUM; index++)
		{
			char const *map_category = custom_edition_maps_category(campaign, index);

			if (map_category && !csstrcasecmp(map_category, open))
			{
				map_browse_entries[count].folder = FALSE;
				map_browse_entries[count].flat = index;
				count++;
			}
		}
	}
	else
	{
		for (index = 0; index < total && count < CUSTOM_EDITION_MAPS_MAXIMUM; index++)
		{
			char const *map_category = custom_edition_maps_category(campaign, index);
			short have;

			if (!map_category || !map_category[0])
			{
				map_browse_entries[count].folder = FALSE;
				map_browse_entries[count].flat = index;
				count++;
				continue;
			}
			for (have = 0; have < count; have++)
			{
				char const *have_category;

				if (!map_browse_entries[have].folder)
					continue;
				have_category = custom_edition_maps_category(campaign, map_browse_entries[have].flat);
				if (have_category && !csstrcasecmp(have_category, map_category))
					break;
			}
			if (have == count)
			{
				map_browse_entries[count].folder = TRUE;
				map_browse_entries[count].flat = index;
				count++;
			}
		}
		map_browse_sorting_campaign = campaign;
		if (count > 1)
			qsort(map_browse_entries, (size_t)count, sizeof(map_browse_entries[0]), map_browse_compare);
	}
	map_browse_count_cache = count;
}

static short map_browse_count(boolean campaign, char const *category)
{
	map_browse_build(campaign, category);

	return map_browse_count_cache;
}

static boolean map_browse_folder(boolean campaign, char const *category, short entry)
{
	map_browse_build(campaign, category);
	if (entry < 0 || entry >= map_browse_count_cache)
		return FALSE;

	return map_browse_entries[entry].folder;
}

/* the map's index, or NONE when the row is a folder or past the list */
static short map_browse_flat(boolean campaign, char const *category, short entry)
{
	map_browse_build(campaign, category);
	if (entry < 0 || entry >= map_browse_count_cache || map_browse_entries[entry].folder)
		return NONE;

	return map_browse_entries[entry].flat;
}

static char const *map_browse_folder_name(boolean campaign, char const *category, short entry)
{
	char const *name;

	map_browse_build(campaign, category);
	if (entry < 0 || entry >= map_browse_count_cache || !map_browse_entries[entry].folder)
		return "";
	name = custom_edition_maps_category(campaign, map_browse_entries[entry].flat);

	return name ? name : "";
}

/* the top list's row for the folder `name`, or NONE */
static short map_browse_find_folder(boolean campaign, char const *name)
{
	short entry;

	map_browse_build(campaign, "");
	for (entry = 0; entry < map_browse_count_cache; entry++)
	{
		char const *folder;

		if (!map_browse_entries[entry].folder)
			continue;
		folder = custom_edition_maps_category(campaign, map_browse_entries[entry].flat);
		if (folder && name && !csstrcasecmp(folder, name))
			return entry;
	}

	return NONE;
}

/* the row of the map `flat` in `category`, or NONE */
static short map_browse_find(boolean campaign, char const *category, short flat)
{
	short entry;

	map_browse_build(campaign, category);
	for (entry = 0; entry < map_browse_count_cache; entry++)
	{
		if (!map_browse_entries[entry].folder && map_browse_entries[entry].flat == flat)
			return entry;
	}

	return NONE;
}

static void map_browse_text(boolean campaign, char const *category, short entry, wchar_t *text)
{
	map_browse_build(campaign, category);
	if (entry < 0 || entry >= map_browse_count_cache)
	{
		text[0] = 0;
		return;
	}
	map_browse_entry_text(campaign, &map_browse_entries[entry], text);
}

/* the description's map (each list's has both kinds' widgets): a campaign
level's picture, name and words (level_description sets them), or a map's
by its display index (a multiplayer map's, or a Custom Edition map's), or
(both NONE) neither */
static void map_description_show(struct widget_instance *description, short level, short map)
{
	struct widget_instance *widget;
	short marker;

	visible_set(named(description, "replay_level_right_name", 0), level != NONE);
	visible_set(named(description, "replay_level_right_pic", 0), level != NONE);
	visible_set(named(description, "replay_level_right_data", 0), level != NONE);
	visible_set(named(description, "mp_map_right_name", 0), map != NONE);
	visible_set(named(description, "mp_map_right_pic", 0), map != NONE);
	visible_set(named(description, "mp_map_right_data", 0), map != NONE);
	if (level == NONE)
	{
		for (marker = 0; marker < 3; marker++)
			visible_set(named(description, "difficulty_indicator", marker), FALSE);
	}
	if (map == NONE)
		return;
	if ((widget = named(description, "mp_map_right_name", 0)) != NULL)
		widget->parameters.text_box.string_list_index = map;
	if ((widget = named(description, "mp_map_right_pic", 0)) != NULL)
		widget->animation.current_frame_index = map;
	if ((widget = named(description, "mp_map_right_data", 0)) != NULL)
		widget->parameters.text_box.string_list_index = map;
}

/* New Game's list: SINGLEPLAYER's levels, the profile's reached, or
MULTIPLAYER's maps, played alone to walk around (no game engine: a campaign
game on the map); CUSTOM SINGLEPLAYER's maps played as the campaign's levels
are, at the difficulty chosen next, and CUSTOM MULTIPLAYER's walked around
as MULTIPLAYER's are. A custom folder's name is a row; choosing it lists
the maps in that folder (B returns here). */
static struct
{
	short kind;
	short first, chosen;
	/* the multiplayer maps (ui_widget_port_multiplayer_maps) */
	char const *const *map_names;
	short map_count;
	/* the custom folder open, or empty at the top; category_entry is that
	folder's row, focused on the way out */
	char category[CUSTOM_EDITION_MAP_CATEGORY_LENGTH];
	short category_entry;
} level_list;

/* a map list's entries of a kind (the Map screen's difficulties aside).
`category` is the custom folder open, or empty at the top. */
static short map_kind_count(short kind, short multiplayer_map_count, char const *category)
{
	switch (kind)
	{
	case MAP_KIND_SINGLEPLAYER:
		return NUMBER_OF_SINGLE_PLAYER_LEVELS;
	case MAP_KIND_MULTIPLAYER:
		return xbox_multiplayer_map_count(multiplayer_map_count);
	case MAP_KIND_CUSTOM_SINGLEPLAYER:
		return map_browse_count(TRUE, category);
	case MAP_KIND_DAILY:
		return (short)(1 + community_history_count());
	default:
		return map_browse_count(FALSE, category);
	}
}

/* ---- the map lists' DAILY CHALLENGES: RANDOM RUN first (a run made at
random as it is chosen: community.c), then the past challenges, today's
first (community.c's, asked for as the kind is shown), each by its day, and
the whole challenge of the one chosen, asked for as it is chosen, which the
list's OK plays. Beside them the challenge's level: its picture, with its
difficulty's badge and its skulls' icons over it */
#define DAILY_LEVEL_SKULLS 10

static struct
{
	boolean asked;
	/* the day whose challenge was asked for last */
	char day[COMMUNITY_DAY_LENGTH];
} daily_entries;

static void daily_entries_forget(void)
{
	daily_entries.asked = FALSE;
	daily_entries.day[0] = 0;
}

/* the past day of an entry (RANDOM RUN's, the first: NULL) */
static struct community_history_day const *daily_entry_day(short entry)
{
	return entry >= 1 ? community_history_day((short)(entry - 1)) : NULL;
}

static void daily_entry_text(short entry, wchar_t *text)
{
	struct community_history_day const *day = daily_entry_day(entry);

	text[0] = 0;
	if (entry == 0)
		usprintf(text, L"RANDOM RUN");
	else if (day && day->today)
		usprintf(text, L"TODAY");
	else if (day)
		community_day_text(day->day, TRUE, text, ROW_TEXT_LENGTH);
}

/* the challenges asked for, and the chosen one's whole challenge */
static void daily_entries_want(short chosen)
{
	struct community_history_day const *day;
	struct community_challenge const *challenge = community_daily();

	if (!daily_entries.asked)
	{
		daily_entries.asked = TRUE;
		community_history_open();
	}
	day = daily_entry_day(chosen);
	if (!day || !strcmp(daily_entries.day, day->day))
		return;
	if (challenge && challenge->online == community_enabled() && !strcmp(challenge->day, day->day))
	{
		snprintf(daily_entries.day, sizeof(daily_entries.day), "%s", day->day);
		return;
	}
	if (community_daily_state() == _community_loading)
		return;
	snprintf(daily_entries.day, sizeof(daily_entries.day), "%s", day->day);
	community_daily_open(day->today ? NULL : day->day);
}

/* (a map list's description) a challenge's badge and skulls taken away:
the other kinds' levels and maps have none (level_description shows a
level's own badges) */
static void daily_overlays_hide(struct widget_instance *description)
{
	short index;

	visible_set(named(description, "difficulty_badges_plate", 0), FALSE);
	for (index = 0; index < 3; index++)
		visible_set(named(description, "difficulty_badge", index), FALSE);
	for (index = 0; index < DAILY_LEVEL_SKULLS; index++)
		visible_set(named(description, "level_skull_icon", index), FALSE);
}

/* the description of the challenge chosen: its level's picture, name and
words, its difficulty's badge and its skulls' icons over the picture, and
in the records' lines what it is */
static void level_description(struct widget_instance *description, char const *prefix, short level, boolean in_progress,
	boolean const *finished, short saved_difficulty);

static void daily_entry_description(struct widget_instance *description, short chosen)
{
	struct community_history_day const *day = daily_entry_day(chosen);
	wchar_t difficulty[ROW_TEXT_LENGTH];
	wchar_t text[ROW_TEXT_LENGTH];
	struct widget_instance *badge;
	short index;

	daily_overlays_hide(description);
	if (chosen == 0)
	{
		map_description_show(description, NONE, NONE);
		text_set(named(description, "replay_level_right_record", 0),
			L"RANDOM RUN\rA RANDOM LEVEL, DIFFICULTY,\rENEMIES AND SKULLS");
		return;
	}
	if (!day || day->level_index == NONE)
	{
		map_description_show(description, NONE, NONE);
		text_set(named(description, "replay_level_right_record", 0),
			community_history_state() == _community_loading ? L"LOADING..." : L"");
		return;
	}
	map_description_show(description, day->level_index, NONE);
	level_description(description, "replay_level", day->level_index, FALSE, NULL, NONE);
	daily_overlays_hide(description);
	/* (its difficulty's badge, on its plate, at the picture's bottom) */
	visible_set(named(description, "difficulty_badges_plate", 0), TRUE);
	if ((badge = named(description, "difficulty_badge", 1)) != NULL)
	{
		badge->visible = TRUE;
		badge->animation.current_frame_index = day->difficulty;
	}
	/* (its skulls' icons, a frame a skull, over the picture's top) */
	for (index = 0; index < day->skull_count && index < DAILY_LEVEL_SKULLS; index++)
	{
		struct widget_instance *icon = named(description, "level_skull_icon", index);

		if (icon && day->skulls[index] != NONE)
		{
			icon->visible = TRUE;
			icon->animation.current_frame_index = day->skulls[index];
		}
	}
	string_get("pc\\main_menu\\player_profiles_select\\difficulty_names", day->difficulty, difficulty);
	ustrupr(difficulty);
	usnprintf(text, NUMBEROF(text), L"DAILY CHALLENGE\r%s, %d SKULL%hs", difficulty, (int)day->skull_count,
		day->skull_count == 1 ? "" : "S");
	text_set(named(description, "replay_level_right_record", 0), text);
}

/* the chosen challenge, set for its run (community_daily_begin; RANDOM
RUN's made now): FALSE while it is still being asked for, or could not be */
static boolean daily_entry_begin(short chosen, char const **map_name, short *difficulty)
{
	struct community_history_day const *day = daily_entry_day(chosen);
	struct community_challenge const *challenge = community_daily();

	if (chosen == 0)
		return community_random_begin(map_name, difficulty);
	if (!day || !challenge || strcmp(challenge->day, day->day) || challenge->online != community_enabled())
		return FALSE;
	return community_daily_begin(map_name, difficulty);
}

/* the display index of a multiplayer or Custom Edition kind's entry, for its
description (map_description_show) */
static short map_kind_display_index(short kind, short entry)
{
	switch (kind)
	{
	case MAP_KIND_CUSTOM_SINGLEPLAYER:
		return custom_edition_maps_display_index_of(TRUE, entry);
	case MAP_KIND_CUSTOM_MULTIPLAYER:
		return custom_edition_maps_display_index_of(FALSE, entry);
	default:
		return entry;
	}
}

/* the level name of a Custom Edition kind's entry, or NULL */
static char const *custom_map_level_name(short kind, short entry)
{
	return custom_edition_maps_level_name(map_kind_display_index(kind, entry));
}

/* "initialize sp level list solo" starts on the level last played */
static boolean level_list_initialize(struct widget_instance *list, short controller)
{
	struct player_profile profile;
	short level, last_used;

	if (!campaign_profile(controller, &profile))
		return FALSE;
	campaign_levels_read(&profile);
	level = PIN(profile.last_single_player_map_played, 0, NUMBER_OF_SINGLE_PLAYER_LEVELS - 1);
	if (campaign.saved_level != NONE)
		level = campaign.saved_level;
	if (!campaign.levels[level].available)
		level = 0;
	campaign.shown_level = level;
	level_list.map_count = ui_widget_port_multiplayer_maps(&level_list.map_names, &last_used);
	map_browse_forget();
	daily_entries_forget();
	level_list.kind = MAP_KIND_SINGLEPLAYER;
	level_list.category[0] = 0;
	level_list.category_entry = 0;
	level_list.chosen = level;
	level_list.first = map_kind_first(level, NUMBER_OF_SINGLE_PLAYER_LEVELS);
	map_kind_set(list, level_list.kind);
	map_kind_focus(list, level_list.first, level_list.chosen);
	return TRUE;
}

static void level_row_text(short level, wchar_t *text)
{
	string_get("pc\\main_menu\\map_list", campaign.levels[level].available ? level : LEVEL_UNAVAILABLE, text);
}

static void level_list_custom_text(short entry, wchar_t *text)
{
	map_browse_text(level_list.kind == MAP_KIND_CUSTOM_SINGLEPLAYER, level_list.category, entry, text);
}

/* choosing a folder lists its maps; B (level_folder_back) returns */
static void level_folder_open(struct widget_instance *list, short entry)
{
	boolean campaign = level_list.kind == MAP_KIND_CUSTOM_SINGLEPLAYER;
	char const *name = map_browse_folder_name(campaign, level_list.category, entry);

	level_list.category_entry = entry;
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	csstrncpy(level_list.category, name, sizeof(level_list.category) - 1);
	level_list.category[sizeof(level_list.category) - 1] = 0;
	map_browse_forget();
	level_list.chosen = 0;
	level_list.first = 0;
	map_kind_focus(list, 0, 0);
}

static boolean level_folder_back(struct widget_instance *list)
{
	short count;
	short entry;

	if (!level_list.category[0] ||
		(level_list.kind != MAP_KIND_CUSTOM_SINGLEPLAYER && level_list.kind != MAP_KIND_CUSTOM_MULTIPLAYER))
	{
		return FALSE;
	}
	entry = level_list.category_entry;
	level_list.category[0] = 0;
	map_browse_forget();
	count = map_kind_count(level_list.kind, level_list.map_count, "");
	level_list.chosen = (short)PIN(entry, 0, MAX(count - 1, 0));
	level_list.first = map_kind_first(level_list.chosen, count);
	map_kind_focus(list, level_list.first, level_list.chosen);

	return TRUE;
}

/* "solo map list update" */
static void level_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	short kind = map_kind_shown(list, TRUE, level_list.kind), count, entry, level;

	daily_overlays_hide(description);
	if (kind != level_list.kind)
	{
		level_list.kind = kind;
		level_list.category[0] = 0;
		level_list.first = level_list.chosen = 0;
	}
	count = map_kind_count(kind, level_list.map_count, level_list.category);
	entry = map_kind_rows_update(list, &level_list.first, count,
		kind == MAP_KIND_SINGLEPLAYER ? level_row_text : kind == MAP_KIND_DAILY ? daily_entry_text :
		kind == MAP_KIND_MULTIPLAYER ? multiplayer_map_text : level_list_custom_text);
	if (entry != NONE)
		level_list.chosen = entry;
	/* (the daily challenges: the one chosen's level and what it is) */
	if (kind == MAP_KIND_DAILY)
	{
		daily_entries_want(level_list.chosen);
		visible_set(named(description, "replay_level_right_item", 0), TRUE);
		daily_entry_description(description, level_list.chosen);
		visible_set(named(screen_of(list), "button_leaderboard", 0), FALSE);
		profile_name_show(description);
		return;
	}
	if (kind != MAP_KIND_SINGLEPLAYER)
	{
		short display = NONE;

		if (kind == MAP_KIND_CUSTOM_SINGLEPLAYER || kind == MAP_KIND_CUSTOM_MULTIPLAYER)
		{
			short flat = map_browse_flat(kind == MAP_KIND_CUSTOM_SINGLEPLAYER, level_list.category,
				level_list.chosen);

			if (flat != NONE)
				display = custom_edition_maps_display_index_of(kind == MAP_KIND_CUSTOM_SINGLEPLAYER, flat);
		}
		else if (level_list.chosen < count)
		{
			display = map_kind_display_index(kind, level_list.chosen);
		}
		visible_set(named(description, "replay_level_right_item", 0), display != NONE);
		map_description_show(description, NONE, display);
		/* (no LEADERBOARD for a map: only the campaign's levels have records) */
		visible_set(named(screen_of(list), "button_leaderboard", 0), FALSE);
		/* (a map's: no level's records) */
		text_set(named(description, "replay_level_right_record", 0), L"");
		profile_name_show(description);
		return;
	}
	visible_set(named(description, "replay_level_right_item", 0), TRUE);
	map_description_show(description, 0, NONE);
	/* (LEADERBOARD, the campaign's levels': tools/port_settings.py's
	_leaderboard; dimmed with scoring off, no scores kept) */
	visible_set(named(screen_of(list), "button_leaderboard", 0), TRUE);
	enabled_set(named(screen_of(list), "button_leaderboard", 0), community_scoring());
	campaign.shown_level = level_list.chosen;
	level = campaign.shown_level;
	if (!campaign.levels[level].available)
		level = LEVEL_UNAVAILABLE;
	level_description(description, "replay_level", level, level == campaign.saved_level,
		level == LEVEL_UNAVAILABLE ? NULL : campaign.levels[level].finished, NONE);
	profile_name_show(description);
}

/* "solo level set map": the level shown, if reached, or the Custom Edition
campaign map, for the difficulty menu; or the multiplayer map, played at
once (FALSE: no difficulty menu). A custom folder opens and stays here. The
list is read again first: a tap on the touchscreen focuses a row and
activates it at once, before the list's update has read which row it is (it
started the level shown before, Pillar of Autumn on a new profile). The map
and gametype lists do the same. */
static boolean level_choose(struct widget_instance *list, short controller)
{
	short level, count;
	boolean custom, custom_campaign;
	short flat = NONE;

	level_list_update(list);
	level = campaign.shown_level;
	count = map_kind_count(level_list.kind, level_list.map_count, level_list.category);
	custom = level_list.kind == MAP_KIND_CUSTOM_SINGLEPLAYER ||
		level_list.kind == MAP_KIND_CUSTOM_MULTIPLAYER;
	custom_campaign = level_list.kind == MAP_KIND_CUSTOM_SINGLEPLAYER;

	if (custom)
		flat = map_browse_flat(custom_campaign, level_list.category, level_list.chosen);
	if (level_list.kind != MAP_KIND_SINGLEPLAYER && (level_list.chosen < 0 || level_list.chosen >= count))
		return campaign_fail();
	/* (a daily challenge: played at once, at its difficulty, with its
	settings, by the players there are: split screen's too) */
	if (level_list.kind == MAP_KIND_DAILY)
	{
		struct player_profile profile;
		char const *map_name;
		short difficulty;

		if (!campaign_profile(controller, &profile) || !daily_entry_begin(level_list.chosen, &map_name, &difficulty))
			return campaign_fail();
		campaign_start(map_name, difficulty, controller, FALSE);
		return FALSE;
	}
	if (custom && map_browse_folder(custom_campaign, level_list.category, level_list.chosen))
	{
		level_folder_open(list, level_list.chosen);
		return FALSE;
	}
	if (level_list.kind == MAP_KIND_MULTIPLAYER || level_list.kind == MAP_KIND_CUSTOM_MULTIPLAYER)
	{
		struct player_profile profile;
		char const *map_name = level_list.kind == MAP_KIND_MULTIPLAYER ? level_list.map_names[level_list.chosen] :
			custom_map_level_name(level_list.kind, flat);

		if (!map_name || !campaign_profile(controller, &profile))
			return campaign_fail();
		campaign_start(map_name, main_get_difficulty(), controller, FALSE);
		return FALSE;
	}
	if (level_list.kind == MAP_KIND_CUSTOM_SINGLEPLAYER)
	{
		char const *map_name = custom_map_level_name(level_list.kind, flat);

		if (!map_name)
			return campaign_fail();
		main_set_map_name(map_name);
		main_defer_map_map_change();
		return TRUE;
	}
	if (level < 0 || level >= NUMBER_OF_SINGLE_PLAYER_LEVELS || !campaign.levels[level].available)
		return campaign_fail();
	/* (not yet: setting it at the main menu changes map, as the Xbox's has it
	not) */
	main_set_map_name(main_get_solo_level_name(level));
	main_defer_map_map_change();
	return TRUE;
}

static void settings_each(struct widget_instance *widget, boolean (*visit)(struct widget_instance *spinner,
	struct pc_menu_setting *setting));
static struct widget_instance *screen_of(struct widget_instance *widget);
static boolean setting_changed_save(struct widget_instance *spinner, struct pc_menu_setting *setting);
static boolean text_field_editing(struct widget_instance *row);
static void text_field_end(boolean keep);
static void text_field_begin(struct widget_instance *row, char const *text, short maximum,
	void (*done)(char const *text));
static void text_field_show(struct widget_instance *value, char const *text, boolean editing);
static short campaign_level_of(char const *map_name);

/* the campaign randomizer's named setups, in config.toml as the other
settings are (game.randomizer_profile_1 to _8, which the Switch's card keeps
as it keeps them: a file of their own beside it could not be made there):
a name, then the settings below, separated by semicolons. Eight, the
oldest first; a new name past eight drops the oldest. A setting added at
the end is its default in a setup saved before it */
#define RANDOMIZER_PROFILE_MAX 8
#define RANDOMIZER_PROFILE_NAME 24
#define RANDOMIZER_KEY_COUNT 19
/* the longest value of a setup's setting (a custom type's name) */
#define RANDOMIZER_VALUE_LENGTH TAG_INJECTOR_CUSTOM_TYPE_LENGTH
/* a setup, as config.toml has it: its name, and each value after a ; */
#define RANDOMIZER_SETUP_LENGTH (RANDOMIZER_PROFILE_NAME + RANDOMIZER_KEY_COUNT * (RANDOMIZER_VALUE_LENGTH + 1) + 1)

static char const *const randomizer_keys[RANDOMIZER_KEY_COUNT] =
{
	"game.randomizer_enemies",
	"game.randomizer_allies",
	"game.randomizer_pool",
	"game.randomizer_type",
	"game.randomizer_grunt_rank",
	"game.randomizer_elite_rank",
	"game.randomizer_jackal_rank",
	"game.randomizer_hunter_rank",
	"game.randomizer_flood_combat_rank",
	"game.randomizer_pickups",
	"game.randomizer_pickup_pool",
	"game.randomizer_pickup_type",
	"game.randomizer_enemy_weapons",
	"game.randomizer_weapon_pool",
	"game.randomizer_weapon_type",
	/* (Go Crazy, once the last, is Settings > Gameplay > Randomizer's: a
	setup saved with it has its field, unread) */
	"",
	/* (the Tag Injector profile's custom types, in the order the screen
	shows them; a setup saved before them has none) */
	"game.randomizer_custom_type",
	"game.randomizer_pickup_custom",
	"game.randomizer_weapon_custom",
};

static struct
{
	struct
	{
		char name[RANDOMIZER_PROFILE_NAME + 1];
		char value[RANDOMIZER_KEY_COUNT][RANDOMIZER_VALUE_LENGTH];
	} profiles[RANDOMIZER_PROFILE_MAX];
	short count;
	boolean naming;
	wchar_t name[RANDOMIZER_PROFILE_NAME + 1];
	/* each setup loaded: the randomizer screen shows it when it next draws */
	long applied;
	/* the row X was pressed on once, which a second X deletes (NONE) */
	short deleting;
	/* Load's DELETE on: A on a row is X (a touch screen has no X) */
	boolean delete_mode;
} randomizer_menu = { { { { 0 } } }, 0, FALSE, { 0 }, 0, NONE, FALSE };

/* the level the randomizer's and the skulls' screens are before: its map,
difficulty, and the controller that chose it (difficulty_start) */
static struct
{
	char map_name[256];
	short controller;
	short difficulty;
} campaign_pending;

/* the skulls screen's skulls, from game.skulls_active as it opens, written
by its OK (tools/port_settings.py's SKULLS); and the screen Back returns to
from it, or from the question of how the skulls are had before it */
static struct skull_set skulls_menu_set;
static long skulls_menu_back_tag = NONE;
/* the question of how the skulls are had, asked by itself (skulls_setup_poll)
rather than before the skulls screen: asked already, and open, its answer
going back to the screen it opened over */
static struct
{
	boolean asked;
	boolean open;
} skulls_setup;
/* the randomizer's and the skulls' screens opened from Server Setup (a
co-op game's: "ss randomizer", "ss skulls"), whose OK writes them and goes
back there, rather than from a level's difficulty, whose OK starts it */
static boolean campaign_screens_for_server;

static boolean skulls_open(struct widget_instance *widget, boolean *widget_deleted);

extern boolean virtual_keyboard_launch(wchar_t *text_buffer, word buffer_size, short caption_index);
extern void virtual_keyboard_allow_any_name(void);
extern boolean virtual_keyboard_active(void);
extern boolean virtual_keyboard_last_exit_saved_text(void);

static void randomizer_profile_setting(char *name, unsigned int size, short slot)
{
	snprintf(name, size, "game.randomizer_profile_%d", slot + 1);
}

static void randomizer_profiles_read(void)
{
	short slot;

	randomizer_menu.count = 0;
	for (slot = 0; slot < RANDOMIZER_PROFILE_MAX; slot++)
	{
		char setting[40];
		char text[RANDOMIZER_SETUP_LENGTH];
		char *field;
		char *next;
		short index = randomizer_menu.count;
		short key;

		randomizer_profile_setting(setting, sizeof(setting), slot);
		if (!config_text(setting, text, sizeof(text)) || !text[0])
			continue;
		next = strchr(text, ';');
		if (next)
			*next++ = 0;
		if (!text[0])
			continue;
		snprintf(randomizer_menu.profiles[index].name, sizeof(randomizer_menu.profiles[index].name), "%s", text);
		for (key = 0; key < RANDOMIZER_KEY_COUNT; key++)
		{
			field = next;
			next = field ? strchr(field, ';') : NULL;
			if (next)
				*next++ = 0;
			/* (Go Crazy's field, unread) */
			if (!randomizer_keys[key][0])
				randomizer_menu.profiles[index].value[key][0] = 0;
			else if (!field || !field[0])
				config_default(randomizer_keys[key], randomizer_menu.profiles[index].value[key],
					sizeof(randomizer_menu.profiles[index].value[key]));
			else
				snprintf(randomizer_menu.profiles[index].value[key],
					sizeof(randomizer_menu.profiles[index].value[key]), "%s", field);
		}
		randomizer_menu.count++;
	}
}

/* the setups, in order, into the slots (an emptied slot written empty) */
static boolean randomizer_profiles_write(void)
{
	short slot;
	boolean written = TRUE;

	for (slot = 0; slot < RANDOMIZER_PROFILE_MAX; slot++)
	{
		char setting[40];
		char text[RANDOMIZER_SETUP_LENGTH] = "";
		char current[RANDOMIZER_SETUP_LENGTH] = "";

		randomizer_profile_setting(setting, sizeof(setting), slot);
		if (slot < randomizer_menu.count)
		{
			short key;
			unsigned int length;

			length = (unsigned int)snprintf(text, sizeof(text), "%s", randomizer_menu.profiles[slot].name);
			for (key = 0; key < RANDOMIZER_KEY_COUNT && length < sizeof(text); key++)
				length += (unsigned int)snprintf(text + length, sizeof(text) - length, ";%s",
					randomizer_menu.profiles[slot].value[key]);
		}
		/* (a slot unchanged is not written again: each write is the card's) */
		if (config_text(setting, current, sizeof(current)) && !strcmp(current, text))
			continue;
		if (!config_write(setting, text))
		{
			error(_error_silent, "randomizer: could not write %s to config.toml", setting);
			written = FALSE;
		}
	}

	return written;
}

static int randomizer_name_clean(char *name)
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

static void randomizer_profile_saved(char const *text)
{
	char name[RANDOMIZER_PROFILE_NAME + 1];
	char value[RANDOMIZER_KEY_COUNT][RANDOMIZER_VALUE_LENGTH];
	short field;
	short index;

	snprintf(name, sizeof(name), "%s", text);
	if (!randomizer_name_clean(name))
	{
		error(_error_silent, "randomizer: '%s' has no letter or digit to name a profile by", text);
		return;
	}
	randomizer_profiles_read();
	for (field = 0; field < RANDOMIZER_KEY_COUNT; field++)
	{
		if (!randomizer_keys[field][0] || !setting_text(randomizer_keys[field], value[field], sizeof(value[field]), FALSE))
			snprintf(value[field], sizeof(value[field]), "%s", "");
	}
	for (index = 0; index < randomizer_menu.count; index++)
	{
		if (!_stricmp(randomizer_menu.profiles[index].name, name))
			break;
	}
	if (index == randomizer_menu.count)
	{
		if (randomizer_menu.count == RANDOMIZER_PROFILE_MAX)
		{
			memmove(&randomizer_menu.profiles[0], &randomizer_menu.profiles[1],
				sizeof(randomizer_menu.profiles[0]) * (RANDOMIZER_PROFILE_MAX - 1));
			index = RANDOMIZER_PROFILE_MAX - 1;
		}
		else
		{
			index = randomizer_menu.count;
			randomizer_menu.count++;
		}
	}
	snprintf(randomizer_menu.profiles[index].name, sizeof(randomizer_menu.profiles[index].name), "%s", name);
	for (field = 0; field < RANDOMIZER_KEY_COUNT; field++)
		snprintf(randomizer_menu.profiles[index].value[field],
			sizeof(randomizer_menu.profiles[index].value[field]), "%s", value[field]);
	if (randomizer_profiles_write())
		error(_error_silent, "randomizer: saved profile %s (slot %d)", name, index + 1);
}

static boolean setting_show_saved(struct widget_instance *spinner, struct pc_menu_setting *setting)
{
	(void)setting;
	setting_load(spinner);
	return TRUE;
}

static short randomizer_profile_index(struct widget_instance *widget)
{
	char const *leaf;

	if (!widget || !widget->name)
		return NONE;
	leaf = strrchr(widget->name, '/');
	if (!leaf)
		leaf = strrchr(widget->name, '\\');
	leaf = leaf ? leaf + 1 : widget->name;
	if (strncmp(leaf, "profile_", 8))
		return NONE;

	return (short)atoi(leaf + 8);
}

/* the profile row with the focus, walking from the screen down to it */
static short randomizer_focused_profile(struct widget_instance *widget)
{
	struct widget_instance *cursor = screen_of(widget);
	short index = randomizer_profile_index(widget);

	if (index != NONE)
		return index;
	while (cursor)
	{
		index = randomizer_profile_index(cursor->focused_child);
		if (index != NONE)
			return index;
		cursor = cursor->focused_child;
	}

	return NONE;
}

static boolean randomizer_profile_apply(short index)
{
	short field;

	if (index < 0 || index >= randomizer_menu.count)
		return campaign_fail();
	for (field = 0; field < RANDOMIZER_KEY_COUNT; field++)
	{
		char current[RANDOMIZER_VALUE_LENGTH];

		if (!randomizer_keys[field][0] ||
			(setting_text(randomizer_keys[field], current, sizeof(current), FALSE) &&
			!strcmp(current, randomizer_menu.profiles[index].value[field])))
			continue;
		if (!config_write(randomizer_keys[field], randomizer_menu.profiles[index].value[field]))
		{
			error(_error_silent, "randomizer: could not write %s", randomizer_keys[field]);
			return campaign_fail();
		}
	}
	randomizer_menu.applied++;
	return TRUE;
}

/* "randomizer profile save": the setup shown, under the name typed on the
on-screen keyboard (a controller has no text field) */
static boolean randomizer_profile_save_event(struct widget_instance *widget)
{
	if (randomizer_menu.naming)
		return TRUE;
	settings_each(screen_of(widget), setting_changed_save);
	csmemset(randomizer_menu.name, 0, sizeof(randomizer_menu.name));
	virtual_keyboard_allow_any_name();
	if (!virtual_keyboard_launch(randomizer_menu.name, (word)sizeof(randomizer_menu.name), 8))
	{
		error(_error_silent, "randomizer: the keyboard did not open for the profile's name");
		return campaign_fail();
	}
	randomizer_menu.naming = TRUE;
	error(_error_silent, "randomizer: naming a profile");
	return TRUE;
}

/* "randomizer profile delete" (X on a row of Load's screen, or A with
DELETE on): the first marks the setup, the second deletes it */
static boolean randomizer_profile_delete_event(struct widget_instance *widget)
{
	short index = randomizer_focused_profile(widget);

	randomizer_profiles_read();
	if (index == NONE || index >= randomizer_menu.count)
		return campaign_fail();
	if (randomizer_menu.deleting != index)
	{
		randomizer_menu.deleting = index;
		ui_play_audio_feedback_sound(SOUND_CURSOR);
		return TRUE;
	}
	randomizer_menu.deleting = NONE;
	error(_error_silent, "randomizer: deleted profile %s", randomizer_menu.profiles[index].name);
	memmove(&randomizer_menu.profiles[index], &randomizer_menu.profiles[index + 1],
		sizeof(randomizer_menu.profiles[0]) * (unsigned long)(randomizer_menu.count - index - 1));
	randomizer_menu.count--;
	/* (none left to delete: DELETE off) */
	if (!randomizer_menu.count)
		randomizer_menu.delete_mode = FALSE;
	ui_play_audio_feedback_sound(SOUND_BACK);

	return randomizer_profiles_write();
}

/* "randomizer profile load" (a row of Load's screen): that setup, which
the randomizer screen shows on going back to it; with DELETE on, the row
marked or deleted instead, and no going back */
static boolean randomizer_profile_load_event(struct widget_instance *widget, boolean *widget_deleted)
{
	short index = randomizer_focused_profile(widget);

	if (randomizer_menu.delete_mode)
		return randomizer_profile_delete_event(widget);
	randomizer_profiles_read();
	if (index == NONE || index >= randomizer_menu.count)
	{
		error(_error_silent, "randomizer: no profile to load (%d saved)", randomizer_menu.count);
		return campaign_fail();
	}
	error(_error_silent, "randomizer: loaded profile %s", randomizer_menu.profiles[index].name);
	if (!randomizer_profile_apply(index))
		return FALSE;
	ui_play_audio_feedback_sound(SOUND_BACK);
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;

	return TRUE;
}

/* "randomizer profiles delete mode" (Load's DELETE, then DONE): A on a row
deletes it (twice) or uses it */
static boolean randomizer_profiles_delete_mode(void)
{
	randomizer_profiles_read();
	if (!randomizer_menu.delete_mode && !randomizer_menu.count)
	{
		ui_play_audio_feedback_sound(SOUND_ERROR);
		return TRUE;
	}
	randomizer_menu.delete_mode = !randomizer_menu.delete_mode;
	randomizer_menu.deleting = NONE;
	ui_play_audio_feedback_sound(randomizer_menu.delete_mode ? SOUND_FORWARD : SOUND_BACK);

	return TRUE;
}

/* "randomizer start": the setup shown, then the skulls' screen (with
game.skulls on) or the level */
static boolean randomizer_ok(struct widget_instance *widget, boolean *widget_deleted)
{
	settings_each(screen_of(widget), setting_changed_save);
	/* (from Server Setup: back there, which has the skulls' row) */
	if (campaign_screens_for_server)
	{
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		ui_widget_port_go_back(widget);
		*widget_deleted = TRUE;
		return TRUE;
	}
	if (config_boolean("game.skulls") && skulls_open(widget, widget_deleted))
		return TRUE;
	campaign_start(campaign_pending.map_name, campaign_pending.difficulty, campaign_pending.controller, FALSE);
	return TRUE;
}

/* the name typed for a profile being saved, once the keyboard has closed.
Each frame from the main loop (main.c), not only while the screen draws */
void randomizer_profiles_poll(void)
{
	char name[RANDOMIZER_PROFILE_NAME + 1];
	short character;

	if (!randomizer_menu.naming || virtual_keyboard_active())
		return;
	randomizer_menu.naming = FALSE;
	for (character = 0; randomizer_menu.name[character] && character < RANDOMIZER_PROFILE_NAME; character++)
		name[character] = (char)randomizer_menu.name[character];
	name[character] = 0;
	if (!virtual_keyboard_last_exit_saved_text())
	{
		error(_error_silent, "randomizer: the profile's name was cancelled ('%s')", name);
		return;
	}
	error(_error_silent, "randomizer: saving profile '%s'", name);
	randomizer_profile_saved(name);
}

/* "randomizer profiles update" (Load's screen): the names saved, or that
there are none, DELETE's words, and the help of what A does */
static void randomizer_profiles_update(struct widget_instance *list)
{
	short index;
	wchar_t const *delete_note = randomizer_menu.delete_mode ? L"  - A AGAIN TO DELETE" : L"  - X AGAIN TO DELETE";
	struct widget_instance *help = list->parameters.list.extended_description;

	randomizer_profiles_read();
	text_set(named(list, "profiles_button_delete", 0), randomizer_menu.delete_mode ? L"DONE" : L"DELETE");
	if (help)
	{
		long_text_set(help, randomizer_menu.delete_mode ?
			L"A on a setup marks it, A again deletes it.\nDONE goes back to using them." :
			L"A uses the setup chosen. DELETE (or X twice) deletes\none. No setup is saved until Save names one.");
	}
	/* (the mark goes with the focus) */
	if (randomizer_menu.deleting != NONE &&
		randomizer_profile_index(list->focused_child) != randomizer_menu.deleting)
		randomizer_menu.deleting = NONE;
	for (index = 0; index < RANDOMIZER_PROFILE_MAX; index++)
	{
		char name[16];
		struct widget_instance *row;
		wchar_t text[RANDOMIZER_PROFILE_NAME + 24];
		short character;

		snprintf(name, sizeof(name), "profile_%d", index);
		row = named(list, name, 0);
		if (!row)
			continue;
		row->visible = index < randomizer_menu.count || index == 0;
		row->disabled = index >= randomizer_menu.count;
		if (index >= randomizer_menu.count)
		{
			if (index == 0)
				text_set(row, L"NO SETUP SAVED");
			continue;
		}
		for (character = 0; randomizer_menu.profiles[index].name[character]; character++)
			text[character] = (wchar_t)(unsigned char)randomizer_menu.profiles[index].name[character];
		if (index == randomizer_menu.deleting)
		{
			short note;

			for (note = 0; delete_note[note]; note++)
				text[character++] = delete_note[note];
		}
		text[character] = 0;
		text_set(row, text);
	}
}

/* whether the spinner of this name (in the list) shows this value */
static boolean randomizer_spinner_is(struct widget_instance *list, char const *spinner, char const *value)
{
	struct widget_instance *widget = named(list, spinner, 0);
	struct pc_menu_setting *setting = widget ? pc_menu_setting_get(widget->definition_tag_index) : NULL;
	short index = widget ? widget->parameters.list.selected_index : NONE;

	return setting && index >= 0 && index < setting->value_count && !strcmp(setting->values[index], value);
}

/* (port/linux/src/port_tag_profiles.c's: port_tag_profiles.h) */
long port_tag_profiles_scan(void);
long port_tag_profiles_count(void);
unsigned long port_tag_profiles_scans(void);
char const *port_tag_profiles_name(long profile);
char const *port_tag_profiles_folder(long profile);
long port_tag_profiles_find(char const *folder);
long port_tag_profiles_create(char const *name);
long port_tag_profiles_duplicate(long profile, char const *name);
int port_tag_profiles_delete(long profile);
long port_tag_profiles_tag_count(long profile);
char const *port_tag_profiles_tag_map(long profile, long tag);
unsigned long port_tag_profiles_tag_group(long profile, long tag);
char const *port_tag_profiles_tag_name(long profile, long tag);
unsigned long port_tag_profiles_tag_size(long profile, long tag);
unsigned long port_tag_profiles_size(long profile);
int port_tag_profiles_tag_enabled(long profile, long tag);
int port_tag_profiles_tag_set_enabled(long profile, long tag, int enabled);
char const *port_tag_profiles_tag_option(long profile, long tag, char const *key);
int port_tag_profiles_tag_set_option(long profile, long tag, char const *key, char const *value);
int port_tag_profiles_tag_remove(long profile, long tag);
/* (port/linux/src/port_tag_mp_profiles.c's: port_tag_mp_profiles.h) */
long port_tag_mp_profiles_scan(void);
long port_tag_profiles_shared_import_all(void);
long port_tag_mp_profiles_count(void);
unsigned long port_tag_mp_profiles_scans(void);
char const *port_tag_mp_profiles_name(long profile);
char const *port_tag_mp_profiles_file(long profile);
long port_tag_mp_profiles_find(char const *file);
long port_tag_mp_profiles_create(char const *name, char const *profile);
int port_tag_mp_profiles_delete(long profile);
int port_tag_mp_profiles_rename(long profile, char const *name);
char const *port_tag_mp_profiles_option(long profile, char const *key);
int port_tag_mp_profiles_set_option(long profile, char const *key, char const *value);
long port_tag_mp_profiles_manual_count(long profile, char const *key);
int port_tag_mp_profiles_manual_has(long profile, char const *key, char const *path);
int port_tag_mp_profiles_manual_set(long profile, char const *key, char const *path, int on);
long port_tag_campaign_profiles_scan(void);
long port_tag_campaign_profiles_count(void);
unsigned long port_tag_campaign_profiles_scans(void);
char const *port_tag_campaign_profiles_name(long profile);
char const *port_tag_campaign_profiles_file(long profile);
long port_tag_campaign_profiles_find(char const *file);
long port_tag_campaign_profiles_create(char const *name, char const *profile);
int port_tag_campaign_profiles_delete(long profile);
int port_tag_campaign_profiles_rename(long profile, char const *name);
char const *port_tag_campaign_profiles_option(long profile, char const *key);
int port_tag_campaign_profiles_set_option(long profile, char const *key, char const *value);
long port_tag_campaign_profiles_list_count(long profile, char const *section, char const *key);
int port_tag_campaign_profiles_list_has(long profile, char const *section, char const *key, char const *path);
int port_tag_campaign_profiles_list_set(long profile, char const *section, char const *key, char const *path, int on);

/* a size as the Tag Injector's menus show it */
static void tags_size_text(unsigned long size, char *text, size_t text_size)
{
	if (size >= 1024UL * 1024UL)
		snprintf(text, text_size, "%.1f MB", (double)size / (1024.0 * 1024.0));
	else
		snprintf(text, text_size, "%lu KB", (size + 1023) / 1024);
}

/* the most custom types of a kind the randomizer screen steps through */
#define RANDOMIZER_CUSTOM_TYPES 64

/* the Tag Injector's profile the randomizer plays, as its screen shows it,
and the custom types of its AI and its weapons on: found again each second
(from its folder), and none from Server Setup (a network game's machines
must all have the same tags) */
static struct
{
	unsigned long checked;
	long profile;
	char actor_types[RANDOMIZER_CUSTOM_TYPES][TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
	long actor_type_count;
	char weapon_types[RANDOMIZER_CUSTOM_TYPES][TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
	long weapon_type_count;
} randomizer_tags = { 0, NONE };

static void randomizer_tags_refresh(void)
{
	if (randomizer_tags.checked && system_milliseconds() - randomizer_tags.checked <= 1000)
		return;
	randomizer_tags.checked = system_milliseconds();
	randomizer_tags.profile = campaign_screens_for_server ? NONE : tag_injector_profile_playable();
	randomizer_tags.actor_type_count = randomizer_tags.profile == NONE ? 0 :
		tag_injector_custom_types(randomizer_tags.profile, 'actv', randomizer_tags.actor_types, RANDOMIZER_CUSTOM_TYPES);
	randomizer_tags.weapon_type_count = randomizer_tags.profile == NONE ? 0 :
		tag_injector_custom_types(randomizer_tags.profile, 'weap', randomizer_tags.weapon_types, RANDOMIZER_CUSTOM_TYPES);
}

/* whether a value is one of those given (each NULL passed over) */
static boolean randomizer_value_among(char const *value, char const *const *values, short value_count)
{
	short index;

	for (index = 0; index < value_count; index++)
	{
		if (values[index] && !strcmp(values[index], value))
			return TRUE;
	}
	return FALSE;
}

/* a spinner's choices that are not to be had (values: NULL for none),
passed over the way the spinner went from the one it showed last (kept in
last), else forward */
static void randomizer_spinner_pass_over(struct widget_instance *list, char const *spinner, char const *const *values,
	short value_count, short *last)
{
	struct widget_instance *widget = named(list, spinner, 0);
	struct pc_menu_setting *setting = widget ? pc_menu_setting_get(widget->definition_tag_index) : NULL;
	short count = setting ? setting->value_count : 0;
	short index;

	if (count < 2)
		return;
	index = widget->parameters.list.selected_index;
	if (index >= 0 && index < count && randomizer_value_among(setting->values[index], values, value_count))
	{
		/* (back: from the one after it, or round from the first to the last) */
		boolean back = *last >= 0 && *last < count &&
			((*last > index && !(*last == count - 1 && index == 0)) || (*last == 0 && index == count - 1));
		short tries;

		for (tries = 0; tries < count && randomizer_value_among(setting->values[index], values, value_count); tries++)
			index = (short)((index + (back ? count - 1 : 1)) % count);
		widget->parameters.list.selected_index = index;
	}
	*last = index;
}

static void randomizer_custom_rows_show(struct widget_instance *list);

/* "randomizer screen update": the rows that mean something with what the
spinners above them show (tools/port_settings.py's RANDOMIZER_ROWS), one
under the other (closer when there are more than twelve); a hidden row takes
no focus. The flamethrower, with Go Crazy's FLAMETHROWER off, not a type to
be chosen, nor TAG INJECTOR without a profile of tags of its kind played
(randomizer_tags), whose custom types the rows of TAG INJECTOR's show. And a
setup loaded on Load's screen, shown */
static void randomizer_screen_update(struct widget_instance *list)
{
	static long shown_applied;
	static short pool_last = NONE;
	static short type_last = NONE;
	static short pickup_pool_last = NONE;
	static short pickup_type_last = NONE;
	static short weapon_pool_last = NONE;
	static short weapon_type_last = NONE;
	boolean flamethrower = config_boolean("game.randomizer_crazy") &&
		config_boolean("game.randomizer_crazy_flamethrower");
	char const *actors_off[1];
	char const *weapon_pools_off[1];
	char const *weapons_off[2];
	boolean enemies;
	boolean one_type;
	boolean custom_type;
	boolean pickups;
	boolean pickup_one;
	boolean weapons;
	boolean weapon_one;
	struct
	{
		char const *row;
		boolean shown;
	} rows[RANDOMIZER_KEY_COUNT - 1];
	short index;
	short shown = 0;
	short pitch;

	if (shown_applied != randomizer_menu.applied)
	{
		shown_applied = randomizer_menu.applied;
		settings_each(screen_of(list), setting_show_saved);
	}
	randomizer_tags_refresh();
	actors_off[0] = randomizer_tags.actor_type_count ? NULL : "tag_injector";
	weapon_pools_off[0] = randomizer_tags.weapon_type_count ? NULL : "tag_injector";
	weapons_off[0] = weapon_pools_off[0];
	weapons_off[1] = flamethrower ? NULL : "flamethrower";
	randomizer_spinner_pass_over(list, "pool_spinner", actors_off, 1, &pool_last);
	randomizer_spinner_pass_over(list, "type_spinner", actors_off, 1, &type_last);
	randomizer_spinner_pass_over(list, "pickup_pool_spinner", weapon_pools_off, 1, &pickup_pool_last);
	randomizer_spinner_pass_over(list, "pickup_type_spinner", weapons_off, 2, &pickup_type_last);
	randomizer_spinner_pass_over(list, "weapon_pool_spinner", weapon_pools_off, 1, &weapon_pool_last);
	randomizer_spinner_pass_over(list, "weapon_type_spinner", weapons_off, 2, &weapon_type_last);
	enemies = randomizer_spinner_is(list, "enemies_spinner", "true");
	one_type = enemies && randomizer_spinner_is(list, "pool_spinner", "one");
	custom_type = one_type && randomizer_spinner_is(list, "type_spinner", "tag_injector");
	pickups = randomizer_spinner_is(list, "pickups_spinner", "true");
	pickup_one = pickups && randomizer_spinner_is(list, "pickup_pool_spinner", "one");
	weapons = randomizer_spinner_is(list, "enemy_weapons_spinner", "true");
	weapon_one = weapons && randomizer_spinner_is(list, "weapon_pool_spinner", "one");
	rows[0].row = "op_enemies", rows[0].shown = TRUE;
	rows[1].row = "op_allies", rows[1].shown = enemies;
	rows[2].row = "op_pool", rows[2].shown = enemies;
	rows[3].row = "op_type", rows[3].shown = one_type;
	rows[4].row = "op_custom_type", rows[4].shown = custom_type;
	rows[5].row = "op_grunt_rank", rows[5].shown = one_type && randomizer_spinner_is(list, "type_spinner", "grunt");
	rows[6].row = "op_elite_rank", rows[6].shown = one_type && randomizer_spinner_is(list, "type_spinner", "elite");
	rows[7].row = "op_jackal_rank", rows[7].shown = one_type && randomizer_spinner_is(list, "type_spinner", "jackal");
	rows[8].row = "op_hunter_rank", rows[8].shown = one_type && randomizer_spinner_is(list, "type_spinner", "hunter");
	rows[9].row = "op_flood_combat_rank";
	rows[9].shown = one_type && randomizer_spinner_is(list, "type_spinner", "flood_combat");
	rows[10].row = "op_pickups", rows[10].shown = TRUE;
	rows[11].row = "op_pickup_pool", rows[11].shown = pickups;
	rows[12].row = "op_pickup_type", rows[12].shown = pickup_one;
	rows[13].row = "op_pickup_custom";
	rows[13].shown = pickup_one && randomizer_spinner_is(list, "pickup_type_spinner", "tag_injector");
	rows[14].row = "op_enemy_weapons", rows[14].shown = TRUE;
	rows[15].row = "op_weapon_pool", rows[15].shown = weapons;
	rows[16].row = "op_weapon_type", rows[16].shown = weapon_one;
	rows[17].row = "op_weapon_custom";
	rows[17].shown = weapon_one && randomizer_spinner_is(list, "weapon_type_spinner", "tag_injector");
	for (index = 0; index < (short)NUMBEROF(rows); index++)
		shown = (short)(shown + rows[index].shown);
	/* (more than twelve rows: closer, above the help) */
	pitch = shown > 12 ? 24 : 26;
	shown = 0;
	for (index = 0; index < (short)NUMBEROF(rows); index++)
	{
		struct widget_instance *row = named(list, rows[index].row, 0);

		if (!row)
			continue;
		row->visible = rows[index].shown;
		if (!rows[index].shown)
			continue;
		row->vertical_offset = (short)(70 + shown * pitch);
		shown++;
	}
	randomizer_custom_rows_show(list);
}

/* the rows of TAG INJECTOR's custom types (tools/port_settings.py's
_randomizer_value_row): each its setting, the text it is shown in, and
whether its types are the weapons' */
static struct
{
	char const *row;
	char const *value;
	char const *setting;
	boolean weapons;
} const randomizer_custom_rows[] =
{
	{ "op_custom_type", "custom_type_value", "game.randomizer_custom_type", FALSE },
	{ "op_pickup_custom", "pickup_custom_value", "game.randomizer_pickup_custom", TRUE },
	{ "op_weapon_custom", "weapon_custom_value", "game.randomizer_weapon_custom", TRUE },
};

/* a custom type row's types, and the one its setting names (NONE) */
static long randomizer_custom_types(short custom, char const (**types)[TAG_INJECTOR_CUSTOM_TYPE_LENGTH],
	long *chosen)
{
	long count = randomizer_custom_rows[custom].weapons ? randomizer_tags.weapon_type_count :
		randomizer_tags.actor_type_count;
	char const *setting = config_string(randomizer_custom_rows[custom].setting);
	long index;

	*types = (char const (*)[TAG_INJECTOR_CUSTOM_TYPE_LENGTH])(randomizer_custom_rows[custom].weapons ?
		randomizer_tags.weapon_types : randomizer_tags.actor_types);
	*chosen = NONE;
	for (index = 0; setting && index < count; index++)
	{
		if (!_stricmp((*types)[index], setting))
			*chosen = index;
	}
	return count;
}

/* each custom type row shown: its type, the first of them while its setting
names none of the profile's */
static void randomizer_custom_rows_show(struct widget_instance *list)
{
	short custom;

	for (custom = 0; custom < (short)NUMBEROF(randomizer_custom_rows); custom++)
	{
		struct widget_instance *row = named(list, randomizer_custom_rows[custom].row, 0);
		struct widget_instance *value = row ? named(row, randomizer_custom_rows[custom].value, 0) : NULL;
		char const (*types)[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
		long chosen;
		long count;
		wchar_t wide[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
		char const *text;
		short character;

		if (!row || !value || !row->visible)
			continue;
		count = randomizer_custom_types(custom, &types, &chosen);
		if (chosen == NONE && count > 0)
		{
			chosen = 0;
			if (!config_write(randomizer_custom_rows[custom].setting, types[0]))
				error(_error_silent, "randomizer: could not write %s", randomizer_custom_rows[custom].setting);
		}
		text = chosen == NONE ? "NONE" : types[chosen];
		for (character = 0; text[character] && character < (short)NUMBEROF(wide) - 1; character++)
			wide[character] = (wchar_t)(unsigned char)text[character];
		wide[character] = 0;
		text_set(value, wide);
	}
}

/* "randomizer custom left" and "right" (and A): a custom type row's type
before or after */
static boolean randomizer_custom_step(struct widget_instance *widget, short step)
{
	struct widget_instance *row;
	short custom = NONE;

	for (row = widget; row && custom == NONE; row = row->parent)
	{
		short index;

		for (index = 0; index < (short)NUMBEROF(randomizer_custom_rows); index++)
		{
			if (!strcmp(row->name, randomizer_custom_rows[index].row))
				custom = index;
		}
	}
	if (custom != NONE)
	{
		char const (*types)[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
		long chosen;
		long count = randomizer_custom_types(custom, &types, &chosen);

		if (count > 1)
		{
			chosen = chosen == NONE ? 0 : (chosen + step + count) % count;
			if (!config_write(randomizer_custom_rows[custom].setting, types[chosen]))
				error(_error_silent, "randomizer: could not write %s", randomizer_custom_rows[custom].setting);
			ui_play_audio_feedback_sound(SOUND_CURSOR);
			return TRUE;
		}
	}
	ui_play_audio_feedback_sound(SOUND_ERROR);
	return TRUE;
}

/* the screen before the randomizer's (tools/port_settings.py's
_randomizer_tag_profiles_screen), with the Tag Injector on and a profile:
NONE, then the campaign profiles (tag_campaign.c), then with the randomizer
on the profiles it plays, a list at a time from this place */
#define RANDOMIZER_TAG_PROFILE_ROWS 9
#define RANDOMIZER_TAG_PROFILE_SCREEN "pc\\main_menu\\difficulty_select\\randomizer\\tag_profile_screen"
#define RANDOMIZER_SCREEN "pc\\main_menu\\difficulty_select\\randomizer\\randomizer_screen"
static short randomizer_tag_profile_first;

/* the screen's rows past NONE: the campaign profiles, then (the randomizer
on) the profiles; of a place, the campaign profile it is (NONE: not one) and
the profile (NONE: not one) */
static short randomizer_tag_profile_count(void)
{
	return (short)(1 + port_tag_campaign_profiles_count() +
		(config_boolean("game.randomizer") ? port_tag_profiles_count() : 0));
}

static long randomizer_tag_profile_place(short place, long *profile)
{
	long campaign_profiles = port_tag_campaign_profiles_count();

	*profile = NONE;
	if (place <= 0)
		return NONE;
	if (place <= campaign_profiles)
		return place - 1;
	*profile = place - 1 - campaign_profiles;
	return NONE;
}

/* the row (tag_profile_<n>) a widget is or is in, or NONE */
static short randomizer_tag_profile_row(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strncmp(widget->name, "tag_profile_", 12) && widget->name[12] >= '0' && widget->name[12] <= '9')
			return (short)atoi(widget->name + 12);
	}
	return NONE;
}

/* "randomizer tag profiles update": the rows (the list moving on at its
ends while there are more past them), each NONE or a profile, and the help
of the one chosen: what it plays */
static void randomizer_tag_profiles_update(struct widget_instance *list)
{
	struct widget_instance *help = list->parameters.list.extended_description;
	short count = randomizer_tag_profile_count();
	short focused = randomizer_tag_profile_row(list->focused_child);
	short *first = &randomizer_tag_profile_first;
	short index;

	if (focused == RANDOMIZER_TAG_PROFILE_ROWS - 1 && *first + RANDOMIZER_TAG_PROFILE_ROWS < count &&
		list_scrolls_at_end())
	{
		(*first)++;
		focus_row(list, --focused);
	}
	else if (focused == 0 && *first > 0 && list_scrolls_at_end())
	{
		(*first)--;
		focus_row(list, ++focused);
	}
	if (*first > 0 && *first + RANDOMIZER_TAG_PROFILE_ROWS > count)
		*first = (short)MAX(0, count - RANDOMIZER_TAG_PROFILE_ROWS);
	for (index = 0; index < RANDOMIZER_TAG_PROFILE_ROWS; index++)
	{
		char name[24];
		struct widget_instance *row;
		short place = (short)(*first + index);
		char text[160];
		wchar_t wide[160];
		short character;

		snprintf(name, sizeof(name), "tag_profile_%d", index);
		row = named(list, name, 0);
		if (!row)
			continue;
		row->visible = place < count;
		if (place >= count)
			continue;
		{
			long profile;
			long campaign_profile = randomizer_tag_profile_place(place, &profile);

			/* (a campaign profile by its name, a profile the randomizer
			plays marked so) */
			if (campaign_profile != NONE)
				snprintf(text, sizeof(text), "%s", port_tag_campaign_profiles_name(campaign_profile));
			else if (profile != NONE)
				snprintf(text, sizeof(text), "%s (RANDOMIZER)", port_tag_profiles_name(profile));
			else
				snprintf(text, sizeof(text), "NONE");
			for (character = 0; text[character] && character < ROW_TEXT_LENGTH - 1; character++)
				wide[character] = (wchar_t)(unsigned char)text[character];
			wide[character] = 0;
			text_set(row, wide);
			if (index != focused || !help)
				continue;
			if (campaign_profile != NONE)
			{
				long base = port_tag_profiles_find(port_tag_campaign_profiles_option(campaign_profile, "profile"));

				if (base == NONE)
					snprintf(text, sizeof(text), "A campaign profile whose profile is not there:\nthe level plays as it is.");
				else
					snprintf(text, sizeof(text), "Campaign profile: %ld AI and %ld weapons of %.24s,\nin the randomizer's place.",
						tag_injector_profile_enabled_count(base, 'actv'), tag_injector_profile_enabled_count(base, 'weap'),
						port_tag_profiles_name(base));
			}
			else if (profile != NONE)
			{
				char size[24];

				tags_size_text(port_tag_profiles_size(profile), size, sizeof(size));
				snprintf(text, sizeof(text), "%ld AI and %ld weapons on (%s): they join\nthe pools, and TAG INJECTOR's.",
					tag_injector_profile_enabled_count(profile, 'actv'),
					tag_injector_profile_enabled_count(profile, 'weap'), size);
			}
			else if (config_boolean("game.randomizer"))
			{
				snprintf(text, sizeof(text), "No Tag Injector tags: the randomizer plays the\ncampaign's alone.");
			}
			else
			{
				snprintf(text, sizeof(text), "No Tag Injector tags: the level as it is.");
			}
		}
		for (character = 0; text[character] && character < (short)NUMBEROF(wide) - 1; character++)
			wide[character] = (wchar_t)(unsigned char)text[character];
		wide[character] = 0;
		long_text_set(help, wide);
	}
}

/* "randomizer tag profile choose" (A on a row): the profile played (or
none), then the randomizer's screen, whose Back comes back here; a campaign
profile (or none, the randomizer off), the skulls' screen or the level */
static boolean randomizer_tag_profile_choose(struct widget_instance *widget, boolean *widget_deleted)
{
	short row = randomizer_tag_profile_row(widget);
	short place = row == NONE ? NONE : (short)(randomizer_tag_profile_first + row);
	long profile;
	long campaign_profile;

	if (place == NONE || place >= randomizer_tag_profile_count())
		return campaign_fail();
	campaign_profile = randomizer_tag_profile_place(place, &profile);
	tag_campaign_profile_choose(campaign_profile);
	if (!config_write("game.randomizer_tag_profile", profile != NONE ? port_tag_profiles_folder(profile) : ""))
		error(_error_silent, "randomizer: could not write game.randomizer_tag_profile");
	error(_error_silent, "randomizer: the Tag Injector's profile played: %s",
		profile != NONE ? port_tag_profiles_name(profile) : "none");
	randomizer_tags.checked = 0;
	if (campaign_profile != NONE || !config_boolean("game.randomizer"))
	{
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		if (config_boolean("game.skulls") && skulls_open(widget, widget_deleted))
			return TRUE;
		campaign_start(campaign_pending.map_name, campaign_pending.difficulty, campaign_pending.controller, FALSE);
		return TRUE;
	}
	if (!ui_widget_load_by_name_or_tag(RANDOMIZER_SCREEN, NONE, NULL, NONE, screen_of(widget)->definition_tag_index,
		NONE, NONE))
	{
		return campaign_fail();
	}
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	*widget_deleted = TRUE;
	return TRUE;
}

/* the skulls screen (tools/port_settings.py's _skulls_screen), opened over
the screen of the widget, which Back returns to; the skulls shown are
game.skulls_active's */
static boolean skulls_open_over(long back_tag, boolean *widget_deleted)
{
	/* (the first time: how the skulls are had, then this screen) */
	char const *screen = skulls_unlock_mode() == _skulls_unlock_unset ?
		"pc\\main_menu\\difficulty_select\\skulls\\unlock_screen" :
		"pc\\main_menu\\difficulty_select\\skulls\\skulls_screen";

	skulls_menu_back_tag = back_tag;
	skulls_setup.open = FALSE;
	skulls_menu_set = skull_set_intersection(skulls_set_from_text(config_string("game.skulls_active")),
		skulls_unlocked_set());
	if (!ui_widget_load_by_name_or_tag(screen, NONE, NULL, NONE, back_tag, NONE, NONE))
		return FALSE;
	*widget_deleted = TRUE;
	return TRUE;
}

static boolean skulls_open(struct widget_instance *widget, boolean *widget_deleted)
{
	return skulls_open_over(screen_of(widget)->definition_tag_index, widget_deleted);
}

/* "skulls unlock all", "skulls unlock pickup", and "skulls death lose" and
"skulls death keep" (the question's answers, the last two UNLOCK VIA
GAMEPLAY's, after the question of a death): the answers kept, then the
skulls screen, over the screen the questions were over (asked by itself:
back to that screen) */
static boolean skulls_unlock_choose(struct widget_instance *widget, boolean play, boolean instant,
	boolean lose_on_death, boolean *widget_deleted)
{
	/* (every answer unlocking by playing, with ALL SKULLS on for ALL
	UNLOCKED, and INSTANT UNLOCK on for UNLOCK ON PICKUP: Settings >
	Gameplay > Skulls changes them later) */
	if (!config_write("game.skulls_unlock_mode", "play") ||
		!config_write("game.skulls_all_unlocked", play ? "false" : "true") ||
		!config_write("game.skulls_instant_unlock", instant ? "true" : "false") ||
		!config_write("game.skulls_lose_on_death", lose_on_death ? "true" : "false"))
	{
		error(_error_silent, "skulls: could not write game.skulls_unlock_mode");
	}
	if (play && skulls_drop())
		skulls_pack_request();
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	if (skulls_setup.open)
	{
		skulls_setup.open = FALSE;
		ui_widget_port_go_back(widget);
		*widget_deleted = TRUE;
		return TRUE;
	}
	return skulls_open_over(skulls_menu_back_tag, widget_deleted);
}

/* "skulls unlock play" (UNLOCK VIA GAMEPLAY), and "skulls death back" (B
on the question it asks): the question of whether a death loses the skulls
picked up, and back from it to the first, each over the screen the first
was over (the first's already in the history, which Back pops once) */
static boolean skulls_question_open(char const *screen, boolean forward, boolean *widget_deleted)
{
	if (!ui_widget_load_by_name_or_tag(screen, NONE, NULL, NONE, NONE, NONE, NONE))
		return FALSE;
	ui_play_audio_feedback_sound(forward ? SOUND_FORWARD : SOUND_BACK);
	*widget_deleted = TRUE;
	return TRUE;
}

/* the skull a row's item is (its name skull_<key>), or NONE */
static int skull_of(struct widget_instance *widget)
{
	int skull;

	if (!widget || strncmp(widget->name, "skull_", 6))
		return NONE;
	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		if (!strcmp(widget->name + 6, skull_descriptions[skull].key))
			return skull;
	}
	return NONE;
}

/* "skull toggle" (A on a skull): it on or off */
static boolean skull_toggle(struct widget_instance *widget)
{
	int skull = skull_of(widget);
	struct skull_set unlocked = skulls_unlocked_set();

	if (skull == NONE)
		return FALSE;
	/* (one not found yet stays off) */
	if (!skull_set_has(&unlocked, skull))
	{
		ui_play_audio_feedback_sound(SOUND_ERROR);
		return TRUE;
	}
	if (skull_set_has(&skulls_menu_set, skull))
		skulls_menu_set = skull_set_difference(skulls_menu_set, skull_set_of(skull));
	else
		skull_set_add(&skulls_menu_set, skull);
	ui_play_audio_feedback_sound(skull_set_has(&skulls_menu_set, skull) ? SOUND_FORWARD : SOUND_BACK);
	return TRUE;
}

/* "skulls random roll" (ROLL, on RANDOM's screen over the skulls screen):
the fewest and the most chosen written, then a number between them (of as
many as are had, at most) of the skulls had on, each at random, and the rest
off; back to the skulls screen, which shows them */
static boolean skulls_random_roll(struct widget_instance *widget, boolean *widget_deleted)
{
	static unsigned long state;
	struct skull_set unlocked = skulls_unlocked_set();
	struct skull_set chosen = skull_set_none();
	int had[NUMBER_OF_SKULLS];
	long count = 0;
	long fewest, most, wanted, index;
	int skull;

	settings_each(screen_of(widget), setting_changed_save);
	fewest = config_integer("game.skulls_random_min");
	most = config_integer("game.skulls_random_max");
	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		if (skull_set_has(&unlocked, skull))
			had[count++] = skull;
	}
	/* (the fewer of the two first, neither more than are had) */
	if (fewest > most)
	{
		long swap = fewest;

		fewest = most;
		most = swap;
	}
	fewest = PIN(fewest, 0, count);
	most = PIN(most, 0, count);
	/* (xorshift, stirred by the clock each roll) */
	state ^= system_milliseconds() * 2654435761UL;
	if (!(state & 0xFFFFFFFFUL))
		state = 1;
#define SKULLS_RANDOM_NEXT(limit) \
	(state ^= (state << 13) & 0xFFFFFFFFUL, state ^= (state & 0xFFFFFFFFUL) >> 17, \
	state ^= (state << 5) & 0xFFFFFFFFUL, (long)((state & 0xFFFFFFFFUL) % (unsigned long)(limit)))
	wanted = fewest + SKULLS_RANDOM_NEXT(most - fewest + 1);
	/* (the first wanted of those had, shuffled) */
	for (index = 0; index < wanted; index++)
	{
		long other = index + SKULLS_RANDOM_NEXT(count - index);
		int swap = had[index];

		had[index] = had[other];
		had[other] = swap;
		skull_set_add(&chosen, had[index]);
	}
#undef SKULLS_RANDOM_NEXT
	skulls_menu_set = chosen;
	error(_error_silent, "skulls: %ld turned on at random (%ld to %ld of %ld had)", wanted, fewest, most, count);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* the skulls screen's grid (tools/port_settings.py's _skulls_screen): its
list has the skulls row by row, four to a row, then a row of its buttons
(Toggle All, Random, OK, Cancel). The list steps none itself: each of its
items moves the focus here, so only the item focused is lit */
#define SKULL_COLUMNS 4
#define SKULL_BUTTONS 4

void ui_widget_port_give_focus(struct widget_instance *widget);

static boolean skulls_move(struct widget_instance *widget, short across, short down)
{
	struct widget_instance *list = widget ? widget->parent : NULL;
	struct widget_instance *child;
	short rows = (NUMBER_OF_SKULLS + SKULL_COLUMNS - 1) / SKULL_COLUMNS;
	short index = 0;
	short row, column, target;

	if (!list)
		return FALSE;
	for (child = list->child; child && child != widget; child = child->next)
		index++;
	if (!child)
		return FALSE;
	if (index < NUMBER_OF_SKULLS)
		row = index / SKULL_COLUMNS, column = index % SKULL_COLUMNS;
	else
		row = rows, column = index - NUMBER_OF_SKULLS;
	if (across)
	{
		column += across;
		if (column < 0 || column >= (row == rows ? SKULL_BUTTONS : SKULL_COLUMNS))
			return TRUE;
	}
	else
	{
		boolean buttons_row = row == rows;

		/* (wrapping round, the buttons' row below the last of the skulls) */
		row = (short)((row + down + rows + 1) % (rows + 1));
		/* (Toggle All, Random, OK and Cancel under the four columns) */
		if (buttons_row != (row == rows))
			column = (short)MIN(column, (row == rows ? SKULL_BUTTONS : SKULL_COLUMNS) - 1);
	}
	if (row == rows)
		target = (short)(NUMBER_OF_SKULLS + column);
	else
	{
		target = (short)(row * SKULL_COLUMNS + column);
		if (target >= NUMBER_OF_SKULLS)
			target = (short)(row * SKULL_COLUMNS);
	}
	for (child = list->child; child && target > 0; child = child->next)
		target--;
	if (child && child != widget)
		ui_widget_port_give_focus(child);
	return TRUE;
}

/* "skulls start" (OK): the skulls shown written, then the level (from
Server Setup: back there) */
static boolean skulls_start(struct widget_instance *widget, boolean *widget_deleted)
{
	char text[SKULLS_TEXT_LENGTH];

	skulls_text_from_set(skull_set_intersection(skulls_menu_set, skulls_unlocked_set()), text, sizeof(text));
	if (!config_write("game.skulls_active", text))
		error(_error_silent, "skulls: could not write game.skulls_active");
	/* (Outbreak's Flood, from skulls.pack: written at the main menu after
	this level, which reads the map until then) */
	if (skulls_pack_wanted())
		skulls_pack_request();
	if (campaign_screens_for_server)
	{
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		ui_widget_port_go_back(widget);
		*widget_deleted = TRUE;
		return TRUE;
	}
	campaign_start(campaign_pending.map_name, campaign_pending.difficulty, campaign_pending.controller, FALSE);
	return TRUE;
}

/* text of ASCII, widened */
static void text_widen(wchar_t *text, unsigned int length, char const *ascii)
{
	unsigned int index;

	for (index = 0; ascii[index] && index + 1 < length; index++)
		text[index] = (wchar_t)(unsigned char)ascii[index];
	text[index] = 0;
}

/* "skulls screen update": each skull ticked or not, and the help line the
focused skull's description */
static void skulls_screen_update(struct widget_instance *list)
{
	struct widget_instance *focused = list->focused_child;
	int focused_skull = skull_of(focused);
	struct widget_instance *help = list->parameters.list.extended_description;
	struct skull_set unlocked = skulls_unlocked_set();
	char line[256];
	wchar_t text[ROW_TEXT_LENGTH];
	int skull;

	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		struct widget_instance *item;
		boolean locked = !skull_set_has(&unlocked, skull);

		snprintf(line, sizeof(line), "skull_%s", skull_descriptions[skull].key);
		item = named(list, line, 0);
		if (!item)
			continue;
		/* (one not found yet, faint) */
		snprintf(line, sizeof(line), "%s %s", locked ? "[-]" : skull_set_has(&skulls_menu_set, skull) ? "[X]" : "[ ]",
			skull_descriptions[skull].label);
		text_widen(text, NUMBEROF(text), line);
		text_set(item, text);
		item->alpha_modifier = locked ? 0.4f : 1.0f;
	}
	if (!help)
		return;
	if (focused_skull != NONE && !skull_set_has(&unlocked, focused_skull))
		snprintf(line, sizeof(line), "Locked. An enemy carries it: pick it up,\nthen finish the level to keep it.");
	else if (focused_skull != NONE)
		snprintf(line, sizeof(line), "%s", skull_descriptions[focused_skull].description);
	else
		line[0] = 0;
	text_widen(text, NUMBEROF(text), line);
	text_set(help, text);
}

/* "skulls reset progress" (Settings > Gameplay > Skulls' last row): the
first A asks, the second forgets the skulls found and how they are had, and
the skulls chosen; the help line says which (gameplay_screen_update) */
static enum
{
	_skulls_reset_none,
	_skulls_reset_asked,
	_skulls_reset_done
} skulls_reset_state;

static void skulls_reset_progress(struct widget_instance *widget)
{
	if (skulls_reset_state != _skulls_reset_asked)
	{
		skulls_reset_state = _skulls_reset_asked;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return;
	}
	if (!config_write("game.skulls_unlocked", "") || !config_write("game.skulls_active", "") ||
		!config_write("game.skulls_all_unlocked", "false") || !config_write("game.skulls_unlock_mode", "") ||
		!config_write("game.skulls_lose_on_death", "true"))
	{
		error(_error_silent, "skulls: could not reset the skulls' progress");
		skulls_reset_state = _skulls_reset_none;
		ui_play_audio_feedback_sound(SOUND_ERROR);
		return;
	}
	platform_log("menus: skull progress reset");
	skulls_reset_state = _skulls_reset_done;
	/* (the question asked again before the skulls screen, not now) */
	skulls_setup.asked = TRUE;
	/* (the spinners, what is saved now) */
	settings_each(screen_of(widget), setting_show_saved);
	ui_play_audio_feedback_sound(SOUND_BACK);
}

/* "gameplay screen update" (Settings > Gameplay > Skulls, > Randomizer and
> Scoring): ALL SKULLS only with the skulls on and the skulls screen's
question answered, INSTANT UNLOCK, SKULLS PER LEVEL and DROP CHANCE only
with ALL SKULLS off too, Go Crazy's
parts only with it on, MULTIPLAYER's (MEDALS, SCORE HUD) only with it on,
CUSTOM COLOR only with HUD COLOR's CUSTOM; a hidden row takes no focus. And RESET SKULL
PROGRESS's question or done in the help line, while it has the focus (its
help's next strings: tools/port_settings.py) */
static void gameplay_screen_update(struct widget_instance *list)
{
	static char const *const crazy_rows[] = {
		"op_randomizer_crazy_weapons", "op_randomizer_crazy_enemies", "op_randomizer_crazy_flame",
		"op_randomizer_crazy_pods"
	};
	static char const *const multiplayer_rows[] = { "op_multiplayer_medals", "op_multiplayer_score_hud" };
	struct widget_instance *all_row = named(list, "op_skulls_all_unlocked", 0);
	struct widget_instance *instant_row = named(list, "op_skulls_instant_unlock", 0);
	struct widget_instance *death_row = named(list, "op_skulls_lose_on_death", 0);
	struct widget_instance *count_row = named(list, "op_skulls_drop_count", 0);
	struct widget_instance *chance_row = named(list, "op_skulls_drop_chance", 0);
	struct widget_instance *reset_row = named(list, "op_reset_progress", 0);
	struct widget_instance *help = list->parameters.list.extended_description;
	/* (ALL SKULLS once the skulls screen's question is answered; INSTANT
	UNLOCK with it off, the skulls to be found) */
	boolean answered = randomizer_spinner_is(list, "skulls_spinner", "true") &&
		skulls_unlock_mode() != _skulls_unlock_unset;
	boolean crazy = randomizer_spinner_is(list, "randomizer_crazy_spinner", "true");
	boolean multiplayer = randomizer_spinner_is(list, "multiplayer_scoring_spinner", "true");
	struct widget_instance *hud_color_row = named(list, "op_score_hud_color", 0);
	struct widget_instance *custom_color_row = named(list, "op_score_hud_custom_color", 0);
	boolean custom_color = randomizer_spinner_is(list, "score_hud_color_spinner", "custom");
	short row;

	for (row = 0; row < (short)NUMBEROF(crazy_rows); row++)
	{
		struct widget_instance *crazy_row = named(list, crazy_rows[row], 0);

		if (crazy_row)
			crazy_row->visible = crazy;
	}
	for (row = 0; row < (short)NUMBEROF(multiplayer_rows); row++)
	{
		struct widget_instance *multiplayer_row = named(list, multiplayer_rows[row], 0);

		if (multiplayer_row)
			multiplayer_row->visible = multiplayer;
	}
	if (all_row)
		all_row->visible = answered;
	if (instant_row)
		instant_row->visible = answered && !randomizer_spinner_is(list, "skulls_all_unlocked_spinner", "true");
	if (death_row)
	{
		death_row->visible = answered && !randomizer_spinner_is(list, "skulls_all_unlocked_spinner", "true") &&
			!randomizer_spinner_is(list, "skulls_instant_unlock_spinner", "true");
	}
	/* (SKULLS PER LEVEL and DROP CHANCE with ALL SKULLS off: the skulls
	dropped to be found) */
	if (count_row)
		count_row->visible = answered && !randomizer_spinner_is(list, "skulls_all_unlocked_spinner", "true");
	if (chance_row)
		chance_row->visible = answered && !randomizer_spinner_is(list, "skulls_all_unlocked_spinner", "true");
	/* (Scoring's CUSTOM COLOR with HUD COLOR's CUSTOM, the rows below it
	closing its gap, as Graphics' button style's) */
	if (custom_color_row && custom_color_row->visible != custom_color)
	{
		struct widget_instance *child;

		for (child = custom_color_row->next; child; child = child->next)
			if (!strncmp(child->name, "op_", 3))
				child->vertical_offset += custom_color ? 30 : -30;
		custom_color_row->visible = custom_color;
		if (!custom_color && list->focused_child == custom_color_row && hud_color_row)
		{
			for (row = 0, child = list->child; child && child != hud_color_row; child = child->next)
				row++;
			list->focused_child = hud_color_row;
			list->parameters.list.selected_index = row;
		}
	}
	if (!reset_row || list->focused_child != reset_row)
	{
		skulls_reset_state = _skulls_reset_none;
		return;
	}
	if (help && skulls_reset_state != _skulls_reset_none && reset_row->child)
	{
		short label = pc_menu_string_index(reset_row->child->definition_tag_index);

		if (label != NONE)
			help->parameters.text_box.string_list_index = (short)(label + 1 + skulls_reset_state);
	}
}

/* the difficulty menu: "difficulty item select" starts the game at the
item's; "set difficulty" (its OK button) at the one its description shows.
With the randomizer on, one of the ten campaign levels opens its screen
first (before it, with the Tag Injector on and a profile, the screen
choosing the profile it plays, or none; that screen with a campaign profile
too, the randomizer on or not), and with the skulls on, theirs (after
the randomizer's: tools/port_settings.py); Continue and Load Game do not */
static boolean difficulty_start(struct widget_instance *widget, short difficulty, short controller,
	boolean *widget_deleted)
{
	struct player_profile profile;
	char const *map_name = main_get_map_name();
	int base_campaign = campaign_level_of(map_name) != NONE;

	if (!campaign_profile(controller, &profile))
		return campaign_fail();
	/* (a campaign level, or a Custom Edition campaign map) */
	if (!custom_edition_maps_level_campaign(map_name))
		map_name = main_get_solo_level_name(0);
	difficulty = PIN(difficulty, 0, 3);
	snprintf(campaign_pending.map_name, sizeof(campaign_pending.map_name), "%s", map_name);
	campaign_pending.controller = controller;
	campaign_pending.difficulty = difficulty;
	campaign_screens_for_server = FALSE;
	/* (a campaign profile is played only as chosen there: none without the
	screen) */
	if (base_campaign && config_boolean("game.tag_injection"))
	{
		port_tag_profiles_scan();
		port_tag_campaign_profiles_scan();
	}
	if (!base_campaign || !config_boolean("game.tag_injection") || !port_tag_campaign_profiles_count())
		tag_campaign_profile_choose(NONE);
	if ((config_boolean("game.randomizer") || (config_boolean("game.tag_injection") &&
		port_tag_campaign_profiles_count() > 0)) && base_campaign)
	{
		long screen_tag = screen_of(widget)->definition_tag_index;
		boolean choose_tags = config_boolean("game.tag_injection") && (port_tag_campaign_profiles_count() > 0 ||
			port_tag_profiles_count() > 0);
		struct widget_instance *opened;

		main_set_difficulty(difficulty);
		randomizer_tags.checked = 0;
		randomizer_tag_profile_first = 0;
		opened = ui_widget_load_by_name_or_tag(choose_tags ? RANDOMIZER_TAG_PROFILE_SCREEN : RANDOMIZER_SCREEN,
			NONE, NULL, NONE, screen_tag, NONE, NONE);
		if (opened)
		{
			*widget_deleted = TRUE;
			return TRUE;
		}
	}
	if (config_boolean("game.skulls"))
	{
		main_set_difficulty(difficulty);
		if (skulls_open(widget, widget_deleted))
			return TRUE;
	}
	campaign_start(map_name, difficulty, controller, FALSE);
	return TRUE;
}

static short difficulty_shown(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strcmp(widget->name, "difficulty_select_list") && widget->parameters.list.extended_description)
		{
			struct widget_instance *picture = named(widget->parameters.list.extended_description,
				"difficulty_options", 0);

			return picture ? picture->animation.current_frame_index : main_get_difficulty();
		}
	}
	return main_get_difficulty();
}

static short sibling_index(struct widget_instance *widget)
{
	struct widget_instance *child;
	short index = 0;

	for (child = widget->parent ? widget->parent->child : widget; child && child != widget; child = child->next)
		index++;
	return index;
}

/* Load Game's list: each profile's saved game (read with the profile as
player 1's, which then is again) */
static void saved_games_read(short controller)
{
	struct player_profile profile, active;
	long profiles[MAXIMUM_PROFILES];
	long active_index;
	word count = MAXIMUM_PROFILES;
	short index;

	campaign.saved_game_count = 0;
	if (!campaign_profile(controller, &active))
		return;
	active_index = player_ui_get_active_player_profile_index(0);
	player_profiles_enumerate_available_to_local_player_index(NONE, &count, profiles, FALSE);
	for (index = 0; index < (short)count && campaign.saved_game_count < MAXIMUM_ROWS; index++)
	{
		struct campaign_saved_game *saved_game = &campaign.saved_games[campaign.saved_game_count];
		char const *map_name;

		if (!((unsigned long)profiles[index] & PROFILE_VALID_BIT) || !player_profile_get(profiles[index], &profile))
			continue;
		player_ui_set_active_player_profile(0, profiles[index], &profile);
		if (ui_widget_port_saved_game(&map_name, &saved_game->level, &saved_game->difficulty))
		{
			saved_game->profile_index = profiles[index];
			ustrncpy(saved_game->profile_name, profile.player_name, MAXIMUM_PLAYER_PROFILE_NAME_LENGTH);
			saved_game->profile_name[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH] = 0;
			campaign.saved_game_count++;
		}
	}
	player_ui_set_active_player_profile(0, active_index, &active);
	campaign_levels_read(&active);
}

/* New Game's LEADERBOARD (tools/port_settings.py's _leaderboard): the level
shown's records, the online leaderboard's (community.c: every player's best,
and this install's own place), or every profile's of this machine that has
one (its campaign_records.txt: campaign_score.c), best score first, read as
its screen opens; VIEW switches between them. The daily challenge's
LEADERBOARD is the same screen of the day's (_daily) */
#define LEADERBOARD_ROWS 10

enum
{
	_leaderboard_local = 0,
	_leaderboard_online,
	_leaderboard_daily,
};

static struct
{
	short mode;
	/* (VIEW's choice, kept while the game runs) */
	boolean local_view;
	short count;
	wchar_t level[64];
	char map_name[64];
	struct
	{
		wchar_t name[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH + 1];
		struct campaign_score_record record;
	} rows[MAXIMUM_PROFILES];
} leaderboard;

/* "port leaderboard init" */
static boolean leaderboard_initialize(void)
{
	short level = campaign.shown_level;
	char const *map_name = level >= 0 && level < NUMBER_OF_SINGLE_PLAYER_LEVELS ? main_get_solo_level_name(level) : NULL;
	wchar_t const *title = map_name ? custom_edition_maps_level_title(map_name) : NULL;
	long profiles[MAXIMUM_PROFILES];
	word count = MAXIMUM_PROFILES;
	short index;

	leaderboard.count = 0;
	ustrncpy(leaderboard.level, title ? title : L"", NUMBEROF(leaderboard.level) - 1);
	leaderboard.level[NUMBEROF(leaderboard.level) - 1] = 0;
	snprintf(leaderboard.map_name, sizeof(leaderboard.map_name), "%s", map_name ? map_name : "");
	leaderboard.mode = community_enabled() && !leaderboard.local_view ? _leaderboard_online : _leaderboard_local;
	if (!map_name)
		return TRUE;
	if (leaderboard.mode == _leaderboard_online)
		community_board_open_level(map_name);
	player_profiles_enumerate_available_to_local_player_index(NONE, &count, profiles, FALSE);
	for (index = 0; index < (short)count && leaderboard.count < MAXIMUM_PROFILES; index++)
	{
		struct player_profile profile;
		struct campaign_score_record record;
		short place;

		if (!((unsigned long)profiles[index] & PROFILE_VALID_BIT) || !player_profile_get(profiles[index], &profile) ||
			!campaign_score_level_record(map_name, profiles[index], &record))
		{
			continue;
		}
		/* (in its place: the best score first) */
		for (place = leaderboard.count; place > 0 && leaderboard.rows[place - 1].record.score < record.score; place--)
			leaderboard.rows[place] = leaderboard.rows[place - 1];
		ustrncpy(leaderboard.rows[place].name, profile.player_name, MAXIMUM_PLAYER_PROFILE_NAME_LENGTH);
		leaderboard.rows[place].name[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH] = 0;
		leaderboard.rows[place].record = record;
		leaderboard.count++;
	}
	return TRUE;
}

/* "port leaderboard view" (VIEW): the online leaderboard, or this
machine's */
static boolean leaderboard_view(void)
{
	if (leaderboard.mode == _leaderboard_daily)
		return FALSE;
	if (!community_enabled())
	{
		leaderboard.local_view = TRUE;
		leaderboard.mode = _leaderboard_local;
		return FALSE;
	}
	leaderboard.local_view = leaderboard.mode == _leaderboard_online;
	leaderboard.mode = leaderboard.local_view ? _leaderboard_local : _leaderboard_online;
	if (leaderboard.mode == _leaderboard_online && leaderboard.map_name[0])
		community_board_open_level(leaderboard.map_name);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

/* a row's columns, of a record ("-" for those it has not) */
static void leaderboard_row_text(long rank, wchar_t const *player, long skulls_count, long medals_count, long points,
	long seconds, wchar_t *name, wchar_t *skulls, wchar_t *medals, wchar_t *score, wchar_t *time)
{
	usnprintf(name, ROW_TEXT_LENGTH, L"%ld.  %s", rank, player);
	if (skulls_count == NONE)
		usprintf(skulls, L"-");
	else
		usnprintf(skulls, 24, L"%ld", skulls_count);
	if (medals_count == NONE)
		usprintf(medals, L"-");
	else
		usnprintf(medals, 24, L"%ld", medals_count);
	usnprintf(score, 24, L"%ld", points);
	usnprintf(time, 24, L"%ld:%02ld:%02ld", seconds / 3600, seconds / 60 % 60, seconds % 60);
}

/* "port leaderboard update" */
static void leaderboard_update(struct widget_instance *screen)
{
	boolean online = leaderboard.mode != _leaderboard_local;
	short state = online ? community_board_state() : _community_ready;
	struct community_row const *mine = online ? community_board_mine() : NULL;
	short count = online ? community_board_count() : leaderboard.count;
	/* (a day's: its top three while it lasted on top, DAY 1. to DAY 3., then
	every run's, whenever played) */
	short top = online ? community_board_day_top_count() : 0;
	short row;

	if (online && community_enabled() && state == _community_ready && community_board_players() > 0)
	{
		wchar_t level[96];

		usnprintf(level, NUMBEROF(level), L"%s  (%ld PLAYER%hs)", leaderboard.level, community_board_players(),
			community_board_players() == 1 ? "" : "S");
		text_set(named(screen, "level", 0), level);
	}
	else
		text_set(named(screen, "level", 0), leaderboard.level);
	/* (players online; this machine's profiles, a daily challenge's offline too) */
	text_set(named(screen, "heading_name", 0), leaderboard.mode == _leaderboard_online ||
		(leaderboard.mode == _leaderboard_daily && community_enabled()) ? L"PLAYER" : L"PROFILE");
	text_set(named(screen, "button_view", 0), online ? L"VIEW: ONLINE" : L"VIEW: THIS MACHINE");
	/* (offline, only this machine's) */
	enabled_set(named(screen, "button_view", 0), community_enabled());
	for (row = 0; row < LEADERBOARD_ROWS; row++)
	{
		wchar_t name[ROW_TEXT_LENGTH] = { 0 };
		wchar_t skulls[24] = { 0 };
		wchar_t medals[24] = { 0 };
		wchar_t score[24] = { 0 };
		wchar_t time[24] = { 0 };

		if (online && row < top)
		{
			struct community_row const *entry = community_board_day_top(row);
			wchar_t day_name[COMMUNITY_NAME_LENGTH + 8];

			usnprintf(day_name, NUMBEROF(day_name), L"DAY %ld.", entry->rank);
			leaderboard_row_text(entry->rank, entry->name, entry->skulls, entry->medals, entry->score,
				entry->seconds, name, skulls, medals, score, time);
			usnprintf(name, ROW_TEXT_LENGTH, L"%s  %s", day_name, entry->name);
		}
		else if (online)
		{
			struct community_row const *entry = community_board_row((short)(row - top));

			/* (this install's own place on the last row, when it is below
			those shown) */
			if (row == LEADERBOARD_ROWS - 1 && mine && mine->rank > LEADERBOARD_ROWS - top)
				entry = mine;
			if (entry)
			{
				leaderboard_row_text(entry->rank, entry->name, entry->skulls, entry->medals, entry->score,
					entry->seconds, name, skulls, medals, score, time);
			}
		}
		else if (row < leaderboard.count)
		{
			struct campaign_score_record const *record = &leaderboard.rows[row].record;

			leaderboard_row_text(row + 1, leaderboard.rows[row].name, record->skulls, record->medals, record->score,
				record->seconds, name, skulls, medals, score, time);
		}
		text_set(named(screen, "row_name", row), name);
		text_set(named(screen, "row_skulls", row), skulls);
		text_set(named(screen, "row_medals", row), medals);
		text_set(named(screen, "row_score", row), score);
		text_set(named(screen, "row_time", row), time);
	}
	if (state == _community_loading)
		long_text_set(named(screen, "note", 0), L"Loading the online leaderboard...");
	else if (state != _community_ready)
		long_text_set(named(screen, "note", 0), community_status_text());
	else if (count)
		long_text_set(named(screen, "note", 0), L"");
	else if (leaderboard.mode == _leaderboard_daily)
		long_text_set(named(screen, "note", 0), L"No one has finished this challenge yet.");
	else if (online)
		long_text_set(named(screen, "note", 0), L"No online scores yet. Win this level with scoring on\r(Settings > Gameplay > Scoring) to set one.");
	else
		long_text_set(named(screen, "note", 0), L"No scores yet. Win this level with scoring on\r(Settings > Gameplay > Scoring) to set one.");
}

/* ---------- the main menu's DAILY CHALLENGE (tools/port_settings.py's
_daily; community.c): the day's challenge, today's or a past day's (PAST
DAYS), what it is and this install's best of it; PLAY plays it, its
settings set for the run alone. The screens are made again as they are gone
back to: the day shown is kept here, and its challenge asked for again only
when it is another's */

static short list_scroll(struct widget_instance *list, short *first, short count, short rows);
static void daily_connection_want(struct widget_instance *screen);
static void daily_connection_reset(void);
extern int randomizer_pack_ready(void);
extern int skulls_pack_ready(void);

#define DAILY_HISTORY_ROWS 10
/* the challenge's skulls' icons (tools/port_settings.py's DAILY_SKULL_ICONS) */
#define DAILY_SKULL_ICONS 16
/* (the skulls' lines, of as many characters as the screen's small font fits) */
#define DAILY_SKULLS_LINE 60

static struct
{
	/* the day shown: "" for today's */
	char day[COMMUNITY_DAY_LENGTH];
	/* the challenge whose packs were asked for (randomizer_pack_request) */
	char packs_day[COMMUNITY_DAY_LENGTH];
	short history_first;
	short history_chosen;
} daily;

/* "port daily today" (Campaign's DAILY CHALLENGE) */
static boolean daily_today(void)
{
	daily.day[0] = 0;
	daily_connection_reset();
	return TRUE;
}

/* "port daily init": the day's challenge, asked for unless it is the one
already here */
static boolean daily_initialize(void)
{
	struct community_challenge const *challenge = community_daily();
	/* (and from where it is now: the server's online, this machine's offline) */
	boolean same = challenge && challenge->online == community_enabled() &&
		(daily.day[0] ? !strcmp(challenge->day, daily.day) : community_daily_is_today());

	if (!same)
		community_daily_open(daily.day[0] ? daily.day : NULL);
	return TRUE;
}

/* "port daily update": the challenge (its level's picture, number and
name, its difficulty's icon, what is swapped, its skulls' icons and names,
and with scoring on this install's best of it), or what is wrong; and
LEADERBOARD, only with scoring on */
static void daily_update(struct widget_instance *screen)
{
	struct community_challenge const *challenge = community_daily();
	short state = community_daily_state();
	boolean scoring = community_scoring();
	wchar_t text[512];
	wchar_t day[64];
	short index;

	text_set(named(screen, "button_play", 0),
		state == _community_unavailable || state == _community_refused ? L"RETRY" : L"PLAY");
	enabled_set(named(screen, "button_leaderboard", 0), scoring && challenge);
	/* (no community server answered: the window that asks what to do) */
	if (community_daily_unreachable())
		daily_connection_want(screen);
	visible_set(named(screen, "picture", 0), challenge && challenge->level_index != NONE);
	visible_set(named(screen, "difficulty_icon", 0), challenge != NULL);
	for (index = 0; index < DAILY_SKULL_ICONS; index++)
		visible_set(named(screen, "skull_icon", index), FALSE);
	if (!challenge)
	{
		long_text_set(named(screen, "date", 0), L"");
		text_set(named(screen, "mission", 0), L"");
		text_set(named(screen, "level", 0), state == _community_loading ? L"CONNECTING..." : L"");
		text_set(named(screen, "difficulty", 0), L"");
		long_text_set(named(screen, "details", 0), state == _community_loading ?
			L"Connecting to the community server..." : community_status_text());
		text_set(named(screen, "skulls_heading", 0), L"");
		long_text_set(named(screen, "skulls", 0), L"");
		long_text_set(named(screen, "mine", 0), L"");
		long_text_set(named(screen, "note", 0), L"");
		return;
	}
	/* (its packs written at the main menu, before it is played:
	randomizer_pack.c; the level plays without them, more slowly) */
	if (strcmp(daily.packs_day, challenge->day))
	{
		boolean wants_skulls_pack = FALSE;

		snprintf(daily.packs_day, sizeof(daily.packs_day), "%s", challenge->day);
		for (index = 0; index < challenge->skull_count; index++)
		{
			wants_skulls_pack |= !strcmp(challenge->skulls[index], "outbreak") ||
				!strcmp(challenge->skulls[index], "scarab");
		}
		if (!randomizer_pack_ready())
			randomizer_pack_request();
		if (wants_skulls_pack && !skulls_pack_ready())
			skulls_pack_request();
	}
	community_day_text(challenge->day, FALSE, day, NUMBEROF(day));
	if (challenge->open)
	{
		long left = community_daily_seconds_left();

		usnprintf(text, NUMBEROF(text), L"%s   -   ENDS IN %ldH %02ldM", day, left / 3600, left / 60 % 60);
	}
	else
		usnprintf(text, NUMBEROF(text), L"%s   -   ENDED", day);
	long_text_set(named(screen, "date", 0), text);
	/* (the level: its picture, a frame a level, its number and its name) */
	if (named(screen, "picture", 0) && challenge->level_index != NONE)
		named(screen, "picture", 0)->animation.current_frame_index = challenge->level_index;
	if (challenge->level_index != NONE)
	{
		wchar_t const *title = custom_edition_maps_level_title(main_get_solo_level_name(challenge->level_index));

		usnprintf(text, NUMBEROF(text), L"MISSION %d", (int)challenge->level_index + 1);
		text_set(named(screen, "mission", 0), text);
		ustrncpy(text, title ? title : L"", NUMBEROF(text) - 1);
		text[NUMBEROF(text) - 1] = 0;
		ustrupr(text);
		text_set(named(screen, "level", 0), text);
	}
	else
	{
		text_set(named(screen, "mission", 0), L"");
		text_set(named(screen, "level", 0), L"A LEVEL THIS GAME HAS NOT");
	}
	/* (the difficulty: its icon, Easy to Legendary, and its name) */
	if (named(screen, "difficulty_icon", 0))
		named(screen, "difficulty_icon", 0)->animation.current_frame_index = challenge->difficulty;
	string_get("pc\\main_menu\\player_profiles_select\\difficulty_names", challenge->difficulty, text);
	ustrupr(text);
	text_set(named(screen, "difficulty", 0), text);
	text[0] = 0;
	for (index = 0; index < challenge->detail_count; index++)
	{
		wchar_t line[COMMUNITY_DETAIL_LENGTH + 2];

		usnprintf(line, NUMBEROF(line), L"%hs%hs", index ? "\r" : "", challenge->details[index]);
		ustrncat(text, line, NUMBEROF(text) - ustrlen(text) - 1);
	}
	long_text_set(named(screen, "details", 0), text);
	if (challenge->skull_count)
		usnprintf(text, NUMBEROF(text), L"SKULLS (%d)", (int)challenge->skull_count);
	else
		usprintf(text, L"NO SKULLS");
	text_set(named(screen, "skulls_heading", 0), text);
	/* (the skulls: an icon each, a frame a skull, and their names, a line
	at a time) */
	{
		char lines[DAILY_SKULLS_LINE * 3];
		size_t length = 0, line_start = 0;
		short shown = 0;

		lines[0] = 0;
		for (index = 0; index < challenge->skull_count; index++)
		{
			char const *label = challenge->skulls[index];
			short skull;

			for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
			{
				if (strcmp(skull_descriptions[skull].key, challenge->skulls[index]))
					continue;
				label = skull_descriptions[skull].label;
				if (shown < DAILY_SKULL_ICONS)
				{
					struct widget_instance *icon = named(screen, "skull_icon", shown++);

					visible_set(icon, TRUE);
					if (icon)
						icon->animation.current_frame_index = skull;
				}
			}
			if (index && length - line_start + strlen(label) + 2 > DAILY_SKULLS_LINE)
			{
				length += (size_t)snprintf(lines + length, sizeof(lines) - length, ",\r");
				line_start = length;
			}
			else if (index)
				length += (size_t)snprintf(lines + length, sizeof(lines) - length, ", ");
			length += (size_t)snprintf(lines + length, sizeof(lines) - length, "%s", label);
			if (length >= sizeof(lines))
				break;
		}
		usnprintf(text, NUMBEROF(text), L"%hs", lines);
	}
	long_text_set(named(screen, "skulls", 0), text);
	/* (this install's best, with scoring on) */
	text[0] = 0;
	if (scoring && challenge->has_mine)
	{
		usnprintf(text, NUMBEROF(text), L"YOUR BEST: %ld   IN %ld:%02ld:%02ld   (%ld TR%hs)", challenge->mine_score,
			challenge->mine_seconds / 3600, challenge->mine_seconds / 60 % 60, challenge->mine_seconds % 60,
			challenge->mine_attempts, challenge->mine_attempts == 1 ? "Y" : "IES");
	}
	else if (scoring && challenge->open)
		usprintf(text, L"NOT PLAYED YET");
	long_text_set(named(screen, "mine", 0), text);
	if (community_status_text()[0])
		long_text_set(named(screen, "note", 0), community_status_text());
	else if (!scoring)
		long_text_set(named(screen, "note", 0), L"Scoring is off (Settings > Gameplay > Scoring): no scores are kept.");
	else if (!challenge->open)
		long_text_set(named(screen, "note", 0), L"This day is over: runs still count, its day's top three stay on top.");
	else if (!challenge->online)
		long_text_set(named(screen, "note", 0), L"Offline: the leaderboard is this machine's profiles'.");
	else
		long_text_set(named(screen, "note", 0), L"The same enemies, skulls and difficulty for everyone.");
		
	long_text_set(named(screen, "note", 0), L"");
}

/* "port daily play" (PLAY): the challenge played, with its settings, by
player 1's profile alone; asked for again (RETRY) when it could not be */
static boolean daily_play(short controller)
{
	struct player_profile profile;
	char const *map_name;
	short difficulty;

	if (community_daily_state() == _community_loading)
		return FALSE;
	if (!community_daily())
	{
		daily_connection_reset();
		community_daily_open(daily.day[0] ? daily.day : NULL);
		return TRUE;
	}
	player_spawn_count = 1;
	player_ui_reset_single_player_local_player_controllers();
	if (!campaign_profile(controller, &profile))
	{
		display_error_text_deferred(L"Make a profile first (Profiles),\r\nthen play the challenge.", controller);
		return campaign_fail();
	}
	if (!community_daily_begin(&map_name, &difficulty))
		return campaign_fail();
	campaign_start(map_name, difficulty, controller, FALSE);
	return TRUE;
}

/* "port daily random" (RANDOM RUN): a run made at random, played as
PLAY plays the challenge */
static boolean daily_random(short controller)
{
	struct player_profile profile;
	char const *map_name;
	short difficulty;

	player_spawn_count = 1;
	player_ui_reset_single_player_local_player_controllers();
	if (!campaign_profile(controller, &profile))
	{
		display_error_text_deferred(L"Make a profile first (Profiles),\r\nthen play a random run.", controller);
		return campaign_fail();
	}
	if (!community_random_begin(&map_name, &difficulty))
		return campaign_fail();
	campaign_start(map_name, difficulty, controller, FALSE);
	return TRUE;
}

/* "port daily back" (the challenge's screen's B and BACK): to Campaign's
menu, whose own back is the main menu's, however many days were looked at
(each day's screen opened over the one before) */
#define CAMPAIGN_MENU_NAME "pc\\main_menu\\campaign_select\\campaign_menu"
#define MAIN_MENU_NAME "pc\\main_menu\\main_menu"
/* (menu_tags.c's: a widget's tag group) */
#define DAILY_WIDGET_DEFINITION_TAG 'DeLa'

static boolean daily_back(boolean *widget_deleted)
{
	if (!ui_widget_load_by_name_or_tag(CAMPAIGN_MENU_NAME, NONE, NULL, NONE,
		tag_loaded(DAILY_WIDGET_DEFINITION_TAG, MAIN_MENU_NAME), NONE, NONE))
	{
		return campaign_fail();
	}
	ui_play_audio_feedback_sound(SOUND_BACK);
	*widget_deleted = TRUE;
	return TRUE;
}

/* (ui_widget.c's main_screen_shell_load, the main menu just opened) after a
challenge's level won here, its day's leaderboard, whose BACK is the
challenge's screen (and its own Campaign's menu); after a random run's, the
challenge's screen */
#define DAILY_SCREEN_NAME "pc\\main_menu\\daily\\daily_screen"
#define DAILY_LEADERBOARD_NAME "pc\\main_menu\\daily\\leaderboard\\leaderboard_screen"

void pc_menus_shell_loaded(void)
{
	struct community_challenge const *challenge;
	long main_menu, daily_screen;
	boolean random;

	if (!community_daily_won_take(&random))
		return;
	main_menu = tag_loaded(DAILY_WIDGET_DEFINITION_TAG, MAIN_MENU_NAME);
	daily_screen = tag_loaded(DAILY_WIDGET_DEFINITION_TAG, DAILY_SCREEN_NAME);
	if (main_menu == NONE || daily_screen == NONE)
		return;
	/* (the day played shown, a random run's the one shown before it) */
	challenge = community_daily();
	if (!random && challenge)
		snprintf(daily.day, sizeof(daily.day), "%s", community_daily_is_today() ? "" : challenge->day);
	if (!ui_widget_load_by_name_or_tag(NULL, daily_screen, NULL, NONE, main_menu, NONE, NONE) || random ||
		!challenge)
	{
		return;
	}
	ui_widget_load_by_name_or_tag(DAILY_LEADERBOARD_NAME, NONE, NULL, NONE, daily_screen, NONE, NONE);
}

/* "port daily leaderboard init" (the challenge's LEADERBOARD) */
static boolean daily_leaderboard_initialize(void)
{
	struct community_challenge const *challenge = community_daily();
	wchar_t day[64];

	leaderboard.mode = _leaderboard_daily;
	leaderboard.count = 0;
	leaderboard.map_name[0] = 0;
	if (!challenge)
	{
		usprintf(leaderboard.level, L"DAILY CHALLENGE");
		return TRUE;
	}
	community_day_text(challenge->day, TRUE, day, NUMBEROF(day));
	usnprintf(leaderboard.level, NUMBEROF(leaderboard.level), L"DAILY CHALLENGE, %s", day);
	community_board_open_day(challenge->day);
	return TRUE;
}

/* "port history init" (PAST DAYS): the days asked for; going back from it
is to today's */
static boolean daily_history_initialize(void)
{
	daily.day[0] = 0;
	daily.history_first = 0;
	daily.history_chosen = 0;
	community_history_open();
	return TRUE;
}

/* the past day of a row of PAST DAYS, NULL for none */
static struct community_history_day const *daily_history_row(struct widget_instance *row)
{
	if (!row || strncmp(row->name, "history_row_", 12))
		return NULL;
	return community_history_day((short)(daily.history_first + atoi(row->name + 12) - 1));
}

/* "port daily choose" (A on a row of PAST DAYS) */
static boolean daily_choose(struct widget_instance *widget)
{
	struct community_history_day const *day;

	for (; widget && strncmp(widget->name, "history_row_", 12); widget = widget->parent)
		;
	day = daily_history_row(widget);
	if (!day)
		return FALSE;
	snprintf(daily.day, sizeof(daily.day), "%s", day->day);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

/* "port history update": the rows, scrolled on at their ends */
static void daily_history_update(struct widget_instance *list)
{
	struct widget_instance *row;
	struct widget_instance *screen = screen_of(list);
	short state = community_history_state();
	short count = community_history_count();
	short chosen = list_scroll(list, &daily.history_first, count, DAILY_HISTORY_ROWS);

	/* (PLAYERS online only, and PLAYERS and RANK with scoring on) */
	boolean players = community_enabled() && community_scoring();
	boolean ranks = community_scoring();

	if (chosen != NONE)
		daily.history_chosen = chosen;
	visible_set(named(screen, "history_heading_players", 0), players);
	visible_set(named(screen, "history_heading_mine", 0), ranks);
	for (row = list->child; row; row = row->next)
	{
		struct community_history_day const *day = daily_history_row(row);
		wchar_t text[96];

		if (strncmp(row->name, "history_row_", 12))
			continue;
		row->animation.current_frame_index = row == list->focused_child ? 1 : 0;
		row->visible = day != NULL;
		if (!day)
		{
			if (row == list->focused_child)
				focus_row(list, count ? (short)(count - 1 - daily.history_first) : NONE);
			continue;
		}
		if (day->today)
			usprintf(text, L"TODAY");
		else
			community_day_text(day->day, TRUE, text, NUMBEROF(text));
		text_set(named(row, "history_date", 0), text);
		/* (its name: ASSAULT ON THE CONTROL ROOM) */
		{
			wchar_t const *title = day->level_index != NONE ?
				custom_edition_maps_level_title(main_get_solo_level_name(day->level_index)) : NULL;

			ustrncpy(text, title ? title : L"?", NUMBEROF(text) - 1);
			text[NUMBEROF(text) - 1] = 0;
			ustrupr(text);
		}
		text_set(named(row, "history_level", 0), text);
		string_get("pc\\main_menu\\player_profiles_select\\difficulty_names", day->difficulty, text);
		ustrupr(text);
		text_set(named(row, "history_difficulty", 0), text);
		usnprintf(text, NUMBEROF(text), L"%ld", day->players);
		text_set(named(row, "history_players", 0), text);
		visible_set(named(row, "history_players", 0), players);
		visible_set(named(row, "history_mine", 0), ranks);
		/* (this install's place on the day's leaderboard, "-" for none) */
		if (day->has_mine && day->mine_rank > 0)
			usnprintf(text, NUMBEROF(text), L"#%ld", day->mine_rank);
		else
			usprintf(text, L"-");
		text_set(named(row, "history_mine", 0), text);
	}
	if (state == _community_loading)
		long_text_set(named(screen, "history_note", 0), L"Loading the past challenges...");
	else if (state != _community_ready)
		long_text_set(named(screen, "history_note", 0), community_status_text());
	else
		long_text_set(named(screen, "history_note", 0), count ? L"" : L"No challenges yet.");
}

/* "load game menu init" */
static boolean saved_game_list_initialize(struct widget_instance *list, short controller)
{
	short index;

	saved_games_read(controller);
	campaign.shown_saved_game = 0;
	for (index = 0; index < campaign.saved_game_count; index++)
	{
		if (campaign.saved_games[index].profile_index == player_ui_get_active_player_profile_index(0))
			campaign.shown_saved_game = index;
	}
	focus_row(list, campaign.saved_game_count ? campaign.shown_saved_game : NONE);
	return TRUE;
}

static void saved_game_row_text(short row, wchar_t *text)
{
	ustrncpy(text, campaign.saved_games[row].profile_name, ROW_TEXT_LENGTH - 1);
	text[ROW_TEXT_LENGTH - 1] = 0;
}

/* "load game list update" */
static void saved_game_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *item = named(description, "load_level_right_item", 0);
	short row = focused_row(list);

	rows_update(list, campaign.saved_game_count, saved_game_row_text);
	if (row != NONE && row < campaign.saved_game_count)
		campaign.shown_saved_game = row;
	if (campaign.shown_saved_game >= campaign.saved_game_count)
		campaign.shown_saved_game = campaign.saved_game_count - 1;
	if (item)
		item->visible = campaign.saved_game_count > 0;
	if (campaign.saved_game_count > 0)
	{
		struct campaign_saved_game const *saved_game = &campaign.saved_games[campaign.shown_saved_game];

		level_description(description, "load_level", saved_game->level, FALSE, NULL, saved_game->difficulty);
	}
	else if (row != NONE)
	{
		focus_row(list, NONE);
	}
	profile_name_show(description);
}

/* "load game menu activated": the saved game goes on, with its profile as
player 1's */
static boolean saved_game_continue(short controller)
{
	struct campaign_saved_game const *saved_game;
	struct player_profile profile;

	if (campaign.shown_saved_game < 0 || campaign.shown_saved_game >= campaign.saved_game_count)
		return campaign_fail();
	saved_game = &campaign.saved_games[campaign.shown_saved_game];
	if (!player_profile_get(saved_game->profile_index, &profile))
		return campaign_fail();
	player_ui_set_active_player_profile(0, saved_game->profile_index, &profile);
	campaign_start(main_get_solo_level_name(saved_game->level), saved_game->difficulty, controller, TRUE);
	return TRUE;
}

/* "load game menu delete request" (before its question) and "delete
finish" (its OK): the saved game shown goes */
static boolean saved_game_delete_request(void)
{
	if (campaign.shown_saved_game < 0 || campaign.shown_saved_game >= campaign.saved_game_count)
		return campaign_fail();
	campaign.saved_game_to_delete = campaign.shown_saved_game;
	return TRUE;
}

static boolean saved_game_delete(short controller)
{
	char path[256];

	if (campaign.saved_game_to_delete < 0 || campaign.saved_game_to_delete >= campaign.saved_game_count ||
		!player_profile_get_enclosing_directory_path(campaign.saved_games[campaign.saved_game_to_delete].profile_index,
			path) || strlen(path) + strlen("savegame.bin") >= sizeof(path))
	{
		return campaign_fail();
	}
	strcat(path, "savegame.bin");
	if (!DeleteFileA(path))
		platform_log("menus: could not delete %s", path);
	campaign.saved_game_to_delete = NONE;
	saved_games_read(controller);
	return TRUE;
}

/* the settings' screens (tools/port_settings.py): every spinner of a
setting in the screen */
static void settings_each(struct widget_instance *widget, boolean (*visit)(struct widget_instance *spinner,
	struct pc_menu_setting *setting))
{
	struct widget_instance *child;

	for (child = widget->child; child; child = child->next)
	{
		struct pc_menu_setting *setting = child->type == 2 && !spinner_item(child) ?
			pc_menu_setting_get(child->definition_tag_index) : NULL;

		if (setting)
			visit(child, setting);
		else
			settings_each(child, visit);
	}
}

static struct widget_instance *screen_of(struct widget_instance *widget)
{
	while (widget->parent)
		widget = widget->parent;
	return widget;
}

/* a setting just written. The randomizer turned on wants its pack, written
at the main menu after a notice (randomizer_pack.c) */
static void setting_written(char const *name, char const *value)
{
	if (!strcmp(name, "game.randomizer") && !strcmp(value, "true"))
		randomizer_pack_request();
	/* (the skulls turned on, their question asked once the pack is written:
	skulls_setup_poll) */
	if (!strcmp(name, "game.skulls") && !strcmp(value, "true"))
		skulls_setup.asked = FALSE;
	/* (and the skulls': skulls to be found, carried by the enemies) */
	if ((!strcmp(name, "game.skulls") || !strcmp(name, "game.skulls_all_unlocked")) && skulls_pack_wanted())
	{
		skulls_pack_request();
	}
}

/* "port settings save" (OK): those changed written, and applied */
static boolean setting_changed_save(struct widget_instance *spinner, struct pc_menu_setting *setting)
{
	short index = spinner->parameters.list.selected_index;

	if (index < 0 || index >= setting->value_count || index == setting->loaded_index)
		return TRUE;
	if (!setting_write(setting->setting, setting->values[index]))
	{
		platform_log("menus: could not set %s", setting->setting);
		return FALSE;
	}
	platform_log("menus: %s = %s", setting->setting, setting->values[index]);
	setting->loaded_index = index;
	setting_written(setting->setting, setting->values[index]);
	return TRUE;
}

/* "port settings defaults" */
static boolean setting_default_show(struct widget_instance *spinner, struct pc_menu_setting *setting)
{
	char text[300];

	if (setting_text(setting->setting, text, sizeof(text), TRUE))
		spinner->parameters.list.selected_index = setting_value_index(setting, text);
	return TRUE;
}

/* "port settings help": the line of the row chosen (by its label's string:
the buttons' is the first) */
static void settings_help(struct widget_instance *list)
{
	struct widget_instance *help = list->parameters.list.extended_description;
	struct widget_instance *row = list->focused_child;
	short index = 0;

	if (!help)
		return;
	if (row && row->child && row->child->type == 1 && strncmp(row->name, "button", 6))
	{
		short label = pc_menu_string_index(row->child->definition_tag_index);

		if (label != NONE)
			index = label + 1;
	}
	help->parameters.text_box.string_list_index = index;
}

/* Video Setup's Resolution and Window Size, in the one row's place
(tools/port_settings.py): the one the display mode shown uses (Window Size
the window's, Resolution fullscreen's and borderless's) is shown, and the
list passes over the other, which takes no focus while it is hidden */
static void video_rows_show(struct widget_instance *list)
{
	struct widget_instance *mode = named(list, "mode_spinner", 0);
	struct widget_instance *resolution = named(list, "op_resolution", 0);
	struct widget_instance *window_size = named(list, "op_window_size", 0);
	struct pc_menu_setting *setting = mode ? pc_menu_setting_get(mode->definition_tag_index) : NULL;
	struct widget_instance *shown, *child;
	short index;

	if (!setting || !resolution || !window_size)
		return;
	index = mode->parameters.list.selected_index;
	shown = index >= 0 && index < setting->value_count && !strcmp(setting->values[index], "windowed") ?
		window_size : resolution;
	resolution->visible = shown == resolution;
	window_size->visible = shown == window_size;
	/* (the focus on the one hidden goes to the one shown: Defaults can
	change the mode while it has it) */
	if (list->focused_child == (shown == resolution ? window_size : resolution))
	{
		for (index = 0, child = list->child; child && child != shown; child = child->next)
			index++;
		list->focused_child = shown;
		list->parameters.list.selected_index = index;
	}
}

/* Video Setup's Vulkan Driver (Android's: tools/port_settings.py), shown
while the backend shown is Vulkan; the list passes over it while it is
hidden, and the focus on it goes to the backend's row */
static void vk_driver_row_show(struct widget_instance *list)
{
	struct widget_instance *renderer = named(list, "renderer_spinner", 0);
	struct widget_instance *renderer_row = named(list, "op_renderer", 0);
	struct widget_instance *driver_row = named(list, "op_vk_driver", 0);
	struct pc_menu_setting *setting = renderer ? pc_menu_setting_get(renderer->definition_tag_index) : NULL;
	struct widget_instance *child;
	short index;

	if (!setting || !renderer_row || !driver_row)
		return;
	index = renderer->parameters.list.selected_index;
	driver_row->visible = index >= 0 && index < setting->value_count && !strcmp(setting->values[index], "vulkan");
	if (!driver_row->visible && list->focused_child == driver_row)
	{
		for (index = 0, child = list->child; child && child != renderer_row; child = child->next)
			index++;
		list->focused_child = renderer_row;
		list->parameters.list.selected_index = index;
	}
}

/* Network Setup's SERVER BROWSER's MASTER SERVERS (tools/port_settings.py:
its words): how many of masters.txt's answered when the browser last asked,
"<answered> OF <count>"; none listed, not asked yet, or off (network.listings
is "brokers"). masters.txt is read again every few seconds */
#define SERVER_BROWSER_WORDS \
	"pc\\main_menu\\settings_select\\player_setup\\player_profile_edit\\network_setup\\server_browser\\words"
enum
{
	_master_servers_word_of,
	_master_servers_word_none,
	_master_servers_word_not_asked,
	_master_servers_word_off,
};

static void master_servers_show(struct widget_instance *list)
{
	static int listed;
	static unsigned long read_time;
	static boolean read;
	struct widget_instance *value = named(list, "master_servers_value", 0);
	wchar_t text[ROW_TEXT_LENGTH], word[ROW_TEXT_LENGTH];
	int count, answered, asked;

	if (!value)
		return;
	if (!read || system_milliseconds() - read_time > 3000)
	{
		static char urls[MASTER_LIST_MAXIMUM_MASTERS][MASTER_LIST_URL_SIZE];
		static unsigned char keys[MASTER_LIST_MAXIMUM_MASTERS][MASTER_LIST_KEY_SIZE];

		listed = master_list_read(config_string("network.masters_file"), urls, keys, MASTER_LIST_MAXIMUM_MASTERS);
		read_time = system_milliseconds();
		read = TRUE;
	}
	master_list_counts(&count, &answered, &asked);
	if (master_list_policy() == _master_listings_brokers)
		words_get(SERVER_BROWSER_WORDS, _master_servers_word_off, text, NUMBEROF(text));
	else if (!listed)
		words_get(SERVER_BROWSER_WORDS, _master_servers_word_none, text, NUMBEROF(text));
	else if (!asked)
		words_get(SERVER_BROWSER_WORDS, _master_servers_word_not_asked, text, NUMBEROF(text));
	else
	{
		words_get(SERVER_BROWSER_WORDS, _master_servers_word_of, word, NUMBEROF(word));
		usnprintf(text, NUMBEROF(text) - 1, L"%d %s %d", answered, word, count);
		text[NUMBEROF(text) - 1] = 0;
	}
	text_set(value, text);
}

/* Graphics' button style follows the pending HUD choice, including Defaults.
The remaining options close the gap; the help and bottom buttons stay put. */
static void hud_button_style_row_show(struct widget_instance *list)
{
	struct widget_instance *hud = named(list, "high_res_hud_spinner", 0);
	struct widget_instance *row = named(list, "op_hud_button_style", 0);
	struct pc_menu_setting *setting = hud ? pc_menu_setting_get(hud->definition_tag_index) : NULL;
	struct widget_instance *child;
	short index;
	boolean visible;

	if (!setting || !row) return;
	index = hud->parameters.list.selected_index;
	visible = index >= 0 && index < setting->value_count && !strcmp(setting->values[index], "true");
	if (row->visible != visible)
	{
		for (child = row->next; child; child = child->next)
			if (!strncmp(child->name, "op_", 3))
				child->vertical_offset += visible ? 30 : -30;
		row->visible = visible;
	}
	if (!visible && list->focused_child == row)
	{
		for (index = 0, child = list->child; child && child != hud->parent; child = child->next)
			index++;
		list->focused_child = hud->parent;
		list->parameters.list.selected_index = index;
	}
}

/* ---------- Change Color: the profile's colour, from a list of the
game's colours (more than its rows: it scrolls), by the Xbox's names for
its spinner's functions */

#define COLOR_COUNT 18
#define COLOR_ROWS 11

static struct
{
	/* the colour on the first row, and the colour chosen */
	short first;
	short color;
} color_list;

static void color_row_text(short row, wchar_t *text)
{
	string_get("pc\\main_menu\\settings_select\\player_setup\\player_profile_edit\\color_edit\\colors_list",
		(short)(color_list.first + row), text);
}

/* "color picker menu initialize": on the profile's colour */
static boolean color_list_initialize(struct widget_instance *list)
{
	struct player_profile *profile = player_ui_get_edit_player_profile();

	if (!profile)
		return FALSE;
	color_list.color = (short)PIN(profile->primary_color_index, 0, COLOR_COUNT - 1);
	color_list.first = (short)PIN(color_list.color - COLOR_ROWS / 2, 0, COLOR_COUNT - COLOR_ROWS);
	focus_row(list, (short)(color_list.color - color_list.first));
	return TRUE;
}

/* (ui_widget.c) whether the pointer moved the menus' focus last */
boolean ui_widget_port_pointer_focused(void);
boolean ui_widget_port_hover_focused(void);

/* Whether a list whose focus is on its first or last row moves on: when the
d-pad put it there. The pointer resting there would make it move every
frame; the wheel scrolls it instead. */
static boolean list_scrolls_at_end(void)
{
	return !ui_widget_port_pointer_focused();
}

/* a list of more items than its rows: the row chosen kept off its ends
while there are more past them, the list moving instead; the item chosen,
or NONE (its buttons) */
static short list_scroll(struct widget_instance *list, short *first, short count, short rows)
{
	short row = focused_row(list);

	if (row == rows - 1 && *first + rows < count && list_scrolls_at_end())
	{
		(*first)++;
		focus_row(list, --row);
	}
	else if (row == 0 && *first > 0 && list_scrolls_at_end())
	{
		(*first)--;
		focus_row(list, ++row);
	}
	return row == NONE || row >= rows ? NONE : (short)(*first + row);
}

/* "color picker update": the list scrolled on at its ends, the colour's
name and picture */
static void color_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *widget;
	short color = list_scroll(list, &color_list.first, COLOR_COUNT, COLOR_ROWS);

	if (color != NONE)
		color_list.color = color;
	rows_update(list, COLOR_ROWS, color_row_text);
	if ((widget = named(description, "color_right_name", 0)) != NULL)
		widget->parameters.text_box.string_list_index = color_list.color;
	if ((widget = named(description, "color_right_pic", 0)) != NULL)
		widget->animation.current_frame_index = color_list.color;
	profile_name_show(description);
}

/* "color picker select color" */
static boolean color_choose(void)
{
	struct player_profile *profile = player_ui_get_edit_player_profile();

	if (!profile)
		return FALSE;
	profile->primary_color_index = color_list.color;
	return TRUE;
}

/* ---------- Edit Profile Settings: its picture of Gamepad Setup, the
profile's button settings as the Xbox's Controller Setup shows them */

/* the picture's frames (the bitmap's: tools/port_settings.py's
BITMAP_FRAMES): Gamepad Setup's row's, as the list numbers its rows
(player_profile_edit_select_menu_update_extended_description), and after
the PC version's nine the Xbox's five of the button settings. The port's
own presets (Boxer and the others after the Xbox's five) have no picture
of their own: Gamepad Setup's is shown for them */
#define PROFILE_GAMEPAD_FRAME 2
#define PROFILE_FIRST_LAYOUT_FRAME 9
#define PROFILE_LAYOUT_FRAMES 5

/* "port gamepad layout preview" (the picture's own): on Gamepad Setup's
row, the edited profile's button settings; the port's own (Bumper Jumper),
of which the Xbox has no picture, keeps the row's */
static void profile_gamepad_layout(struct widget_instance *picture)
{
	struct player_profile *profile = player_ui_get_edit_player_profile();
	short frame = picture->animation.current_frame_index;

	if (frame == PROFILE_GAMEPAD_FRAME ||
		(frame >= PROFILE_FIRST_LAYOUT_FRAME && frame < PROFILE_FIRST_LAYOUT_FRAME + PROFILE_LAYOUT_FRAMES))
	{
		short preset = profile && profile->controller_settings.button_preset < NUMBER_OF_BUTTON_PRESETS ?
			profile->controller_settings.button_preset : _button_preset_standard;

		picture->animation.current_frame_index = preset < PROFILE_LAYOUT_FRAMES ?
			(short)(PROFILE_FIRST_LAYOUT_FRAME + preset) : PROFILE_GAMEPAD_FRAME;
	}
}

/* ---------- Profiles: the player profiles, and a row to make one (as the
PC version's list has them, but by the Xbox's names for its spinner's
functions); choosing one makes it player 1's, the profile the campaign and
Settings use; deleting one asks first */

#define PROFILE_ROWS 11
#define PROFILE_DEFAULT_BIT 0x40000000UL

static struct
{
	long indices[MAXIMUM_PROFILES];
	struct player_profile profiles[MAXIMUM_PROFILES];
	/* the profiles; the row after them makes a new one */
	short count;
	short first;
	short chosen;
	long to_delete;
} profile_list;

/* the profiles, read again when they are not those read last */
static void profile_list_read(boolean always)
{
	long indices[MAXIMUM_PROFILES];
	word count = MAXIMUM_PROFILES;
	short index, valid = 0;

	player_profiles_enumerate_available_to_local_player_index(NONE, &count, indices, FALSE);
	for (index = 0; index < (short)count; index++)
	{
		if ((unsigned long)indices[index] & PROFILE_VALID_BIT)
			indices[valid++] = indices[index];
	}
	if (!always && valid == profile_list.count &&
		!memcmp(indices, profile_list.indices, valid * sizeof(indices[0])))
	{
		return;
	}
	profile_list.count = 0;
	for (index = 0; index < valid; index++)
	{
		if (player_profile_get(indices[index], &profile_list.profiles[profile_list.count]))
			profile_list.indices[profile_list.count++] = indices[index];
	}
	profile_list.chosen = (short)PIN(profile_list.chosen, 0, profile_list.count);
}

static short profile_list_rows(void)
{
	return (short)MIN(profile_list.count + 1, PROFILE_ROWS);
}

/* "player profile list initialize": on player 1's profile */
static boolean profile_list_initialize(struct widget_instance *list)
{
	long active = player_ui_get_active_player_profile_index(0);
	short index;

	profile_list_read(TRUE);
	if (active == NONE)
		active = player_ui_get_player1_last_used_profile_index();
	profile_list.chosen = 0;
	for (index = 0; index < profile_list.count; index++)
	{
		if (profile_list.indices[index] == active)
			profile_list.chosen = index;
	}
	profile_list.first = (short)PIN(profile_list.chosen - PROFILE_ROWS / 2, 0,
		MAX(0, profile_list.count + 1 - PROFILE_ROWS));
	focus_row(list, (short)(profile_list.chosen - profile_list.first));
	return TRUE;
}

static void profile_row_text(short row, wchar_t *text)
{
	short item = (short)(profile_list.first + row);

	if (item < profile_list.count)
	{
		ustrncpy(text, profile_list.profiles[item].player_name, MAXIMUM_PLAYER_PROFILE_NAME_LENGTH);
		text[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH] = 0;
	}
	else
	{
		string_get("pc\\main_menu\\player_profiles_select\\profile_description_labels", 4, text);
	}
}

static void visible_set(struct widget_instance *widget, boolean visible)
{
	if (widget)
		widget->visible = visible;
}

/* "3wide player profile list update": the rows, and the profile chosen:
its name, colour, current level, best difficulty and controls */
static void profile_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *widget;
	short item;
	boolean profile;

	profile_list_read(FALSE);
	item = list_scroll(list, &profile_list.first, (short)(profile_list.count + 1), profile_list_rows());
	if (item != NONE)
		profile_list.chosen = item;
	rows_update(list, profile_list_rows(), profile_row_text);
	profile = profile_list.chosen < profile_list.count;
	text_set(named(description, "player_profile_right_name", 0),
		profile ? profile_list.profiles[profile_list.chosen].player_name : L"");
	visible_set(named(description, "empty_profile_slot_text", 0), !profile);
	visible_set(named(description, "qtr_screen_profile_color_pic", 0), profile);
	visible_set(named(description, "current_level_label", 0), profile);
	visible_set(named(description, "current_level", 0), profile);
	visible_set(named(description, "joystick_controls_label", 0), profile);
	visible_set(named(description, "joystick_controls", 0), profile);
	visible_set(named(description, "skill_level_label", 0), FALSE);
	visible_set(named(description, "skill_level", 0), FALSE);
	if (profile)
	{
		struct player_profile *chosen = &profile_list.profiles[profile_list.chosen];
		short level, difficulty;

		if ((widget = named(description, "qtr_screen_profile_color_pic", 0)) != NULL)
			widget->animation.current_frame_index = (short)PIN(chosen->primary_color_index, 0, COLOR_COUNT - 1);
		if ((widget = named(description, "current_level", 0)) != NULL)
		{
			widget->parameters.text_box.string_list_index =
				(short)PIN(chosen->last_single_player_map_played, 0, NUMBER_OF_SINGLE_PLAYER_LEVELS - 1);
		}
		if ((widget = named(description, "joystick_controls", 0)) != NULL)
			widget->parameters.text_box.string_list_index = chosen->controller_settings.invert_look ? 1 : 0;
		player_profile_get_highest_completed_solo_level(chosen, &level, &difficulty);
		if (level != NONE && (widget = named(description, "skill_level", 0)) != NULL)
		{
			widget->parameters.text_box.string_list_index = difficulty;
			widget->visible = TRUE;
			visible_set(named(description, "skill_level_label", 0), TRUE);
		}
	}
	profile_name_show(description);
}

/* "profile manager select": the profile chosen made player 1's (and the
one used last); FALSE for the row that makes one (its handler then asks) */
static boolean profile_choose(short controller)
{
	struct player_profile profile;

	if (profile_list.chosen >= profile_list.count ||
		!player_profile_get(profile_list.indices[profile_list.chosen], &profile))
	{
		return FALSE;
	}
	player_ui_set_active_player_profile(0, profile_list.indices[profile_list.chosen], &profile);
	player_ui_set_single_player_local_player_controller(0, controller);
	player_ui_remember_player1_profile(TRUE);
	return TRUE;
}

/* "request del player profile" (before its question) */
static boolean profile_delete_request(void)
{
	if (profile_list.chosen >= profile_list.count)
		return campaign_fail();
	profile_list.to_delete = profile_list.indices[profile_list.chosen];
	return TRUE;
}

/* "final del player profile" (its OK): deleted, and if it was player 1's,
the first left is */
static boolean profile_delete(void)
{
	long index = profile_list.to_delete;
	boolean active = index == player_ui_get_active_player_profile_index(0);

	profile_list.to_delete = NONE;
	if (index == NONE || ((unsigned long)index & PROFILE_DEFAULT_BIT))
		return campaign_fail();
	player_profile_delete(index);
	profile_list_read(TRUE);
	if (active)
	{
		struct player_profile profile;

		if (profile_list.count && player_profile_get(profile_list.indices[0], &profile))
		{
			player_ui_set_active_player_profile(0, profile_list.indices[0], &profile);
			player_ui_remember_player1_profile(TRUE);
		}
		else
		{
			memset(&profile, 0, sizeof(profile));
			player_ui_set_active_player_profile(0, NONE, &profile);
		}
	}
	return TRUE;
}

/* ---------- Co-op: the campaign for two players on this machine, in split
screen (the Xbox's Cooperative Play, which the PC version has not):
Multiplayer's CO-OP CAMPAIGN ("port coop begin"), player 2's profile, chosen
with player 2's controller ("port coop player 2"), then New Game's levels
(those either has reached) and difficulty, with either player's controller
(ui_widget.c, widget_takes_events_of_controller). The main menu and
Multiplayer go back to one player (main_menu_initialize,
multiplayer_type_menu_initialize).
With one gamepad, it is player 2's (pc_menu_split_players) */

/* "port coop begin": two players, player 1 on its profile (campaign_profile)
and the controller that chose co-op */
static boolean coop_begin(short controller)
{
	struct player_profile profile;

	player_spawn_count = 1;
	player_ui_reset_single_player_local_player_controllers();
	if (!campaign_profile(controller, &profile))
		return campaign_fail();
	player_spawn_count = 2;
	return TRUE;
}

/* "port coop player 2 list initialize": on player 2's profile, else the first
that is not player 1's */
static boolean coop_player2_list_initialize(struct widget_instance *list)
{
	long player1 = player_ui_get_active_player_profile_index(0);
	long player2 = player_ui_get_active_player_profile_index(1);
	short index;

	profile_list_read(TRUE);
	profile_list.chosen = 0;
	for (index = profile_list.count - 1; index >= 0; index--)
	{
		if (profile_list.indices[index] != player1)
			profile_list.chosen = index;
	}
	for (index = 0; index < profile_list.count && player2 != NONE; index++)
	{
		if (profile_list.indices[index] == player2)
			profile_list.chosen = index;
	}
	profile_list.first = (short)PIN(profile_list.chosen - PROFILE_ROWS / 2, 0,
		MAX(0, profile_list.count + 1 - PROFILE_ROWS));
	focus_row(list, (short)(profile_list.chosen - profile_list.first));
	return TRUE;
}

/* "port coop player 2": the profile chosen made player 2's, on the controller
that chose it, which must not be player 1's */
static boolean coop_player2_choose(short controller)
{
	struct player_profile profile;

	if (player_spawn_count < 2 || profile_list.chosen >= profile_list.count ||
		!player_profile_get(profile_list.indices[profile_list.chosen], &profile))
	{
		return campaign_fail();
	}
	if (controller == player_ui_get_single_player_local_player_controller(0))
	{
		display_error_text_deferred(L"Player 2 chooses their\r\nprofile with their own\r\ncontroller.",
			NONE);
		return campaign_fail();
	}
	player_ui_set_single_player_local_player_controller(1, controller);
	player_ui_set_active_player_profile(1, profile_list.indices[profile_list.chosen], &profile);
	return TRUE;
}

/* ---------- Controls Setup: the keyboard and mouse's controls
(port/linux/include/halo_keyboard.h), shown a group at a time, two
bindings each */

#define CONTROL_BINDINGS 2
#define CONTROL_ROWS 7
#define CONTROL_NAME_LENGTH 32

static struct
{
	char const *setting;
	wchar_t const *label;
	short group;
} const controls[] =
{
	{ "controls.move_forward", L"MOVE FORWARD", 0 },
	{ "controls.move_backward", L"MOVE BACKWARD", 0 },
	{ "controls.strafe_left", L"STRAFE LEFT", 0 },
	{ "controls.strafe_right", L"STRAFE RIGHT", 0 },
	{ "controls.jump", L"JUMP", 0 },
	{ "controls.crouch", L"CROUCH", 0 },
	{ "controls.fire", L"FIRE", 1 },
	{ "controls.throw_grenade", L"THROW GRENADE", 1 },
	{ "controls.melee", L"MELEE", 1 },
	{ "controls.reload", L"RELOAD", 1 },
	{ "controls.zoom", L"ZOOM", 1 },
	{ "controls.switch_weapon", L"SWITCH WEAPON", 1 },
	{ "controls.switch_grenade", L"SWITCH GRENADE", 1 },
	{ "controls.action", L"ACTION", 2 },
	{ "controls.flashlight", L"FLASHLIGHT", 2 },
	{ "controls.scoreboard", L"SHOW SCORES", 2 },
	{ "controls.pause", L"PAUSE MENU", 2 },
	{ "controls.screenshot", L"SCREENSHOT", 2 },
	{ "controls.push_to_talk", L"PUSH TO TALK", 3 },
	{ "controls.chat", L"CHAT", 3 },
	{ "controls.team_chat", L"TEAM CHAT", 3 },
};

static struct
{
	/* the bindings shown, until OK writes them */
	char bindings[NUMBEROF(controls)][CONTROL_BINDINGS][CONTROL_NAME_LENGTH];
	/* the binding left and right choose, and the one being set */
	short slot;
	short capturing_control;
	short capturing_slot;
} controls_screen = { { { { 0 } } }, 0, NONE, 0 };

/* a setting's bindings ("Left Ctrl, C") into the control's */
static void control_bindings_read(short control, char const *text)
{
	short slot;

	for (slot = 0; slot < CONTROL_BINDINGS; slot++)
	{
		char *binding = controls_screen.bindings[control][slot];
		unsigned int length;

		while (*text == ' ' || *text == ',')
			text++;
		length = (unsigned int)strcspn(text, ",");
		if (length >= CONTROL_NAME_LENGTH)
			length = CONTROL_NAME_LENGTH - 1;
		memcpy(binding, text, length);
		binding[length] = 0;
		while (length && binding[length - 1] == ' ')
			binding[--length] = 0;
		text += strcspn(text, ",");
	}
}

static void control_bindings_text(short control, char *text, unsigned int size)
{
	char const *first = controls_screen.bindings[control][0];
	char const *second = controls_screen.bindings[control][1];

	snprintf(text, size, "%s%s%s", *first ? first : second, *first && *second ? ", " : "", *first ? second : "");
}

/* "controls screen init" (and "controls screen defaults": the defaults) */
static boolean controls_load(boolean defaults)
{
	short control;

	for (control = 0; control < NUMBEROF(controls); control++)
	{
		char text[128];

		if (defaults)
			config_default(controls[control].setting, text, sizeof(text));
		else
			snprintf(text, sizeof(text), "%s", config_string(controls[control].setting));
		control_bindings_read(control, text);
	}
	controls_screen.capturing_control = NONE;
	return TRUE;
}

/* "controls screen change set" (OK) */
static boolean controls_save(void)
{
	short control;
	boolean result = TRUE;

	for (control = 0; control < NUMBEROF(controls); control++)
	{
		char text[128];

		control_bindings_text(control, text, sizeof(text));
		if (strcmp(text, config_string(controls[control].setting)))
		{
			if (config_write(controls[control].setting, text))
				platform_log("menus: %s = %s", controls[control].setting, text);
			else
				result = FALSE;
		}
	}
	return result;
}

static struct widget_instance *control_row(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strncmp(widget->name, "op_command_", 11))
			return widget;
	}
	return NULL;
}

/* the control on the row (op_command_<n>) in the group shown, else NONE */
static short control_of_row(struct widget_instance *row, short group)
{
	short wanted = row ? (short)(atoi(row->name + 11) - 1) : NONE;
	short control;

	for (control = 0; control < NUMBEROF(controls) && wanted >= 0; control++)
	{
		if (controls[control].group == group && wanted-- == 0)
			return control;
	}
	return NONE;
}

static short controls_group(struct widget_instance *screen)
{
	struct widget_instance *spinner = named(screen, "group_spinner", 0);

	return spinner ? spinner->parameters.list.selected_index : 0;
}

/* "controls begin binding" (A on a row): the next key or button pressed */
static boolean control_capture_begin(struct widget_instance *widget)
{
	short control = control_of_row(control_row(widget), controls_group(screen_of(widget)));

	if (control == NONE)
		return FALSE;
	controls_screen.capturing_control = control;
	controls_screen.capturing_slot = controls_screen.slot;
	platform_binding_capture_begin();
	return TRUE;
}

/* the key or button taken (then bound to no other control), or cleared */
static void control_capture_poll(void)
{
	short control = controls_screen.capturing_control;
	short slot = controls_screen.capturing_slot;
	int input = -1;
	int result;

	if (control == NONE || !(result = platform_binding_capture_poll(&input)))
		return;
	controls_screen.capturing_control = NONE;
	if (result == 1)
	{
		char name[CONTROL_NAME_LENGTH];
		short other, other_slot;

		halo_input_name(input, name, sizeof(name));
		if (!*name)
			return;
		for (other = 0; other < NUMBEROF(controls); other++)
		{
			for (other_slot = 0; other_slot < CONTROL_BINDINGS; other_slot++)
			{
				if (!_stricmp(controls_screen.bindings[other][other_slot], name))
					controls_screen.bindings[other][other_slot][0] = 0;
			}
		}
		snprintf(controls_screen.bindings[control][slot], CONTROL_NAME_LENGTH, "%s", name);
	}
	else if (result == 2)
	{
		controls_screen.bindings[control][slot][0] = 0;
	}
}

/* "controls update menu": the group's rows, their bindings (the one left
and right choose marked on the row chosen), and the help */
static void controls_update(struct widget_instance *list)
{
	short group = controls_group(list);
	struct widget_instance *row;
	struct widget_instance *focused_row = control_row(list->focused_child);

	control_capture_poll();
	for (row = list->child; row; row = row->next)
	{
		short control = control_of_row(row, group);
		short slot;

		if (strncmp(row->name, "op_command_", 11))
			continue;
		row->visible = control != NONE;
		if (control == NONE)
		{
			if (row == focused_row)
				focus_row(list, 0);
			continue;
		}
		text_set(named(row, "command_label", 0), controls[control].label);
		for (slot = 0; slot < CONTROL_BINDINGS; slot++)
		{
			char const *binding = controls_screen.bindings[control][slot];
			wchar_t text[ROW_TEXT_LENGTH];
			char name[CONTROL_NAME_LENGTH];
			char shown[64];
			short index;

			/* (the key as the keyboard's layout labels it: xinput_sdl.c) */
			if (*binding)
				halo_input_shown_name(binding, name, sizeof(name));
			else
				snprintf(name, sizeof(name), "%s", "-");
			if (controls_screen.capturing_control == control && controls_screen.capturing_slot == slot)
				snprintf(shown, sizeof(shown), "%s", "PRESS A KEY");
			else if (row == focused_row && controls_screen.slot == slot)
				snprintf(shown, sizeof(shown), "> %s <", name);
			else
				snprintf(shown, sizeof(shown), "%s", name);
			for (index = 0; shown[index] && index < ROW_TEXT_LENGTH - 1; index++)
				text[index] = (wchar_t)(unsigned char)shown[index];
			text[index] = 0;
			text_set(named(row, "command_binding", slot), text);
		}
	}
	if (list->parameters.list.extended_description)
	{
		list->parameters.list.extended_description->parameters.text_box.string_list_index =
			controls_screen.capturing_control != NONE ? 2 : focused_row ? 1 : 0;
	}
}

/* ---------- Mods (the main menu's: tools/port_settings.py's _mods), laid
out as the lobby is: the mods folder's mods, a row each (op_mod_<n>), as a
resource pack list is, the one that plays over the others at the top (the
load order upside down: the mod loaded last first), turned on or off and
moved up or down there, and in the frame beside them the mod chosen's
details; OK keeps it (port/linux/src/port_mods.c's load_order.toml) and
plays them on the map now (mods.c), Cancel forgets it */

#define MOD_ROWS 11
#define MOD_DETAILS_LENGTH 1024
/* the details' lines: as many as fit down the frame */
#define MOD_DETAILS_LINES 13
/* (from the shadow of each box's last character) */
#define MOD_TEXT_MARGIN 8

/* the text boxes whose lines are measured: the details' mod name and the
rest, and the rows' names */
enum
{
	_mod_details_name,
	_mod_details_text,
	_mod_row_name,
	NUMBER_OF_MOD_TEXT_BOXES
};

/* the text boxes' names, and as many characters as fit across each until
it has been drawn, and its font is known */
static char const *const mod_text_box_names[NUMBER_OF_MOD_TEXT_BOXES] = { "mod_details_name", "mod_details_text",
	"mod_name" };
/* ... and the Tag Injector's, laid out as theirs are, which share their
measures (one screen is shown at a time) */
static char const *const tag_text_box_names[NUMBER_OF_MOD_TEXT_BOXES] = { "tag_details_name", "tag_details_text",
	"tag_name" };
static short const mod_text_box_characters[NUMBER_OF_MOD_TEXT_BOXES] = { 17, 30, 24 };

/* (port/linux/src/port_mods.c, port_mods.h's) */
enum
{
	_port_mod_fine,
	_port_mod_out_of_order,
	_port_mod_incompatible,
	_port_mod_unplayable,
};

long port_mods_count(void);
long port_mods_at(long position);
char const *port_mods_name(long index);
int port_mods_on(long index);
int port_mods_status(long index, char *text, unsigned long size);
int port_mods_details(long index, char *text, unsigned long size);
long port_mods_move(long position, long places);
void port_mods_turn(long index, int on);
int port_mods_save(void);
void port_mods_revert(void);
/* (mods.c) */
void mods_apply(void);

static struct
{
	/* the place on the list (0 the top: the mod loaded last) of the first
	row, and of the mod chosen last (MOVE UP and MOVE DOWN's, which take the
	focus from the rows) */
	short first;
	short chosen;
	/* the text boxes' fonts and widths as they were last drawn
	(menu_functions_text_box_drawn), which their lines are measured in;
	NONE before then */
	long fonts[NUMBER_OF_MOD_TEXT_BOXES];
	short widths[NUMBER_OF_MOD_TEXT_BOXES];
	/* what the details show (they are wrapped again when it changes), and
	the mod each row's name is of, in which font (NONE: none yet) */
	char shown[128];
	long row_mods[MOD_ROWS];
	long row_fonts[MOD_ROWS];
} mods_screen;

/* the row (op_mod_<n>) a widget is or is in, else NULL */
static struct widget_instance *mod_row(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strncmp(widget->name, "op_mod_", 7))
			return widget;
	}
	return NULL;
}

/* the place on the list of the mod on a row, else NONE */
static short mod_row_place(struct widget_instance *row)
{
	short place = row ? (short)(mods_screen.first + atoi(row->name + 7) - 1) : NONE;

	return place >= 0 && place < port_mods_count() ? place : NONE;
}

/* the mod at a place on the list (its place in the load order counted from
the end), else NONE */
static long mod_at_place(short place)
{
	long count = port_mods_count();

	return place >= 0 && place < count ? port_mods_at(count - 1 - place) : NONE;
}

/* UTF-8 text (mod.toml's) as the menus' (a line break the game's \r\n; a
character the menus' UTF-16 cannot hold, or a broken one, a ?) */
static void mods_wide_text(char const *text, wchar_t *wide, short length)
{
	short index = 0;

	while (*text && index < length - 2)
	{
		unsigned char first = (unsigned char)*text++;
		unsigned long character = first;
		short following = first >= 0xF0 ? 3 : first >= 0xE0 ? 2 : first >= 0xC0 ? 1 : 0;

		if (first >= 0x80)
		{
			character = first & (0x3F >> following);
			for (; following > 0 && ((unsigned char)*text & 0xC0) == 0x80; following--)
				character = (character << 6) | ((unsigned char)*text++ & 0x3F);
			if (following || first < 0xC0 || character > 0xFFFF)
				character = '?';
		}
		if (character == '\n')
			wide[index++] = '\r';
		wide[index++] = (wchar_t)character;
	}
	wide[index] = 0;
}

/* the start of the UTF-8 character before `at` in text */
static size_t mods_character_before(char const *text, size_t at)
{
	if (at > 0)
		at--;
	while (at > 0 && ((unsigned char)text[at] & 0xC0) == 0x80)
		at--;
	return at;
}

/* whether `count` bytes of text fit across a text box (box): measured in
its font, from its left as it is drawn (leading spaces and all), else
counted */
static boolean mods_text_fits(short box, char const *text, size_t count)
{
	static real_argb_color const white = { 1.0f, 1.0f, 1.0f, 1.0f };
	char part[MOD_DETAILS_LENGTH];
	wchar_t wide[MOD_DETAILS_LENGTH];
	rectangle2d bounds;
	rectangle2d text_bounds;
	rectangle2d cursor;

	if (!count)
		return TRUE;
	if (mods_screen.fonts[box] == NONE)
		return count <= (size_t)mod_text_box_characters[box];
	if (count >= sizeof(part))
		return FALSE;
	memcpy(part, text, count);
	part[count] = 0;
	mods_wide_text(part, wide, NUMBEROF(wide));
	bounds.x0 = 0;
	bounds.y0 = 0;
	bounds.x1 = 0x3FFF;
	bounds.y1 = 480;
	draw_string_set_draw_mode(mods_screen.fonts[box], NONE, 0, 0, &white);
	draw_unicode_string_compute_bounds(&bounds, wide, &text_bounds, &cursor);
	return MAX(text_bounds.x1, cursor.x0) - bounds.x0 <= mods_screen.widths[box] - MOD_TEXT_MARGIN;
}

/* `count` bytes of text, as much of them as fits across a text box (box)
with "..." after it, into `shortened` */
static void mods_text_shorten(short box, char const *text, size_t count, char *shortened, size_t size)
{
	size_t kept = count;

	for (;;)
	{
		snprintf(shortened, size, "%.*s...", (int)kept, text);
		if (!kept || mods_text_fits(box, shortened, strlen(shortened)))
			return;
		kept = mods_character_before(text, kept);
	}
}

/* text, after what `wrapped` holds, in lines that fit across a text box
(box), broken at spaces and at its own line breaks (word by word: a word
too long for a line broken where it fits); `lines` lines in all at most,
the last cut short with "..." when there is more: how many it holds now */
static short mods_wrap(char const *text, char *wrapped, size_t size, short box, short lines, short line)
{
	size_t length = strlen(wrapped);

	while (line < lines && *text)
	{
		char const *end = strchr(text, '\n');
		size_t count = end ? (size_t)(end - text) : strlen(text);
		size_t cut = 0;
		boolean more;

		for (;;)
		{
			size_t next = cut;

			while (next < count && text[next] == ' ')
				next++;
			while (next < count && text[next] != ' ')
				next++;
			if (next == cut || !mods_text_fits(box, text, next))
				break;
			cut = next;
		}
		if (!cut && count)
		{
			for (cut = 0; cut < count && text[cut] == ' '; cut++)
			{
			}
			while (cut < count && text[cut] != ' ')
				cut++;
			while (cut > 1 && !mods_text_fits(box, text, cut))
				cut = mods_character_before(text, cut);
			if (!cut)
				cut = 1;
		}
		more = cut < count || (text[count] == '\n' && text[count + 1]);
		if (line == lines - 1 && more)
		{
			char shortened[MOD_DETAILS_LENGTH];

			mods_text_shorten(box, text, cut, shortened, sizeof(shortened));
			snprintf(wrapped + length, size - length, "%s%s", length ? "\n" : "", shortened);
			return lines;
		}
		snprintf(wrapped + length, size - length, "%s%.*s", length ? "\n" : "", (int)cut, text);
		length = strlen(wrapped);
		line++;
		text += cut;
		while (*text == ' ')
			text++;
		if (*text == '\n')
			text++;
	}
	return line;
}

/* "port mods init" (the list's created), and "port mods close" (its
deleted: what OK has not kept is forgotten) */
static boolean mods_screen_reset(void)
{
	short index;

	port_mods_revert();
	mods_screen.first = 0;
	mods_screen.chosen = 0;
	mods_screen.shown[0] = 0;
	for (index = 0; index < NUMBER_OF_MOD_TEXT_BOXES; index++)
	{
		mods_screen.fonts[index] = NONE;
		mods_screen.widths[index] = 0;
	}
	for (index = 0; index < MOD_ROWS; index++)
		mods_screen.row_mods[index] = NONE;
	return TRUE;
}

/* "port mods toggle" (A on a row; a mod that cannot be played stays as it
is, so that it is still off when it can be) */
static boolean mods_toggle(struct widget_instance *widget)
{
	short place = mod_row_place(mod_row(widget));
	long mod = mod_at_place(place);

	if (place == NONE)
		return FALSE;
	if (port_mods_status(mod, NULL, 0) != _port_mod_unplayable)
		port_mods_turn(mod, !port_mods_on(mod));
	return TRUE;
}

/* "port mods move up" and "port mods move down" (left and right on a row,
and the buttons): the mod of the row, else the one chosen last, moved one
place on the list (up: later in the load order, over one more mod); the
focus follows it on its row */
static boolean mods_move(struct widget_instance *widget, short places)
{
	struct widget_instance *row = mod_row(widget);
	short count = (short)port_mods_count();
	short place = row ? mod_row_place(row) : mods_screen.chosen;
	short moved;

	/* (no mods: nothing to move) */
	if (!count)
		return TRUE;
	if (place == NONE || place >= count)
		return FALSE;
	moved = (short)(count - 1 - port_mods_move(count - 1 - place, -places));
	mods_screen.chosen = moved;
	if (moved < mods_screen.first)
		mods_screen.first = moved;
	else if (moved >= mods_screen.first + MOD_ROWS)
		mods_screen.first = (short)(moved - MOD_ROWS + 1);
	if (row && row->parent)
		focus_row(row->parent, (short)(moved - mods_screen.first));
	return TRUE;
}

/* the details of the mod at `place` on the list, in the frame (the list's
description): its name; what is wrong with it, beside its icon; whether it
is on and its priority (1 the top, the mod that wins); and what its
mod.toml says */
static void mods_details_show(struct widget_instance *details, short place, short count)
{
	long mod = mod_at_place(place);
	struct widget_instance *icon = named(details, "mod_details_status", 0);
	char problem[MOD_DETAILS_LENGTH];
	char about[MOD_DETAILS_LENGTH];
	char text[MOD_DETAILS_LENGTH];
	wchar_t wide[MOD_DETAILS_LENGTH];
	int status;
	short line;
	char shown[sizeof(mods_screen.shown)];

	/* (wrapped again only when what they show, or its fonts, change) */
	snprintf(shown, sizeof(shown), "%ld %d %d %d %d %ld %d %ld %d", mod, port_mods_status(mod, NULL, 0),
		port_mods_on(mod), place, count, mods_screen.fonts[_mod_details_name],
		mods_screen.widths[_mod_details_name], mods_screen.fonts[_mod_details_text],
		mods_screen.widths[_mod_details_text]);
	if (!strcmp(shown, mods_screen.shown))
		return;
	strcpy(mods_screen.shown, shown);
	if (mod < 0)
	{
		/* (no mods: where they go) */
		mods_wide_text(count ? "" : "No mods", wide, NUMBEROF(wide));
		text_set_length(named(details, "mod_details_name", 0), wide, NUMBEROF(wide));
		text[0] = 0;
		if (!count)
		{
			mods_wrap("Each mod is a folder of its own in the mods folder, beside the maps folder.", text,
				sizeof(text), _mod_details_text, MOD_DETAILS_LINES, 0);
		}
		mods_wide_text(text, wide, NUMBEROF(wide));
		text_set_length(named(details, "mod_details_text", 0), wide, NUMBEROF(wide));
		visible_set(icon, FALSE);
		return;
	}
	status = port_mods_status(mod, problem, sizeof(problem));
	text[0] = 0;
	mods_wrap(port_mods_name(mod), text, sizeof(text), _mod_details_name, 2, 0);
	mods_wide_text(text, wide, NUMBEROF(wide));
	text_set_length(named(details, "mod_details_name", 0), wide, NUMBEROF(wide));
	visible_set(icon, status != _port_mod_fine);
	if (icon)
		icon->animation.current_frame_index = status == _port_mod_out_of_order ? 1 : 2;
	text[0] = 0;
	line = 0;
	if (status != _port_mod_fine)
	{
		/* (its first line indented, for the icon) */
		snprintf(text, sizeof(text), "     %s", problem);
		strcpy(problem, text);
		text[0] = 0;
		line = mods_wrap(problem, text, sizeof(text), _mod_details_text, MOD_DETAILS_LINES, line);
		line = mods_wrap(" ", text, sizeof(text), _mod_details_text, MOD_DETAILS_LINES, line);
	}
	snprintf(about, sizeof(about), "%s\nPriority: %d of %d", status == _port_mod_unplayable ? "Cannot be played" :
		port_mods_on(mod) ? "On" : "Off", place + 1, count);
	line = mods_wrap(about, text, sizeof(text), _mod_details_text, MOD_DETAILS_LINES, line);
	line = mods_wrap(" ", text, sizeof(text), _mod_details_text, MOD_DETAILS_LINES, line);
	port_mods_details(mod, about, sizeof(about));
	mods_wrap(about, text, sizeof(text), _mod_details_text, MOD_DETAILS_LINES, line);
	mods_wide_text(text, wide, NUMBEROF(wide));
	text_set_length(named(details, "mod_details_text", 0), wide, NUMBEROF(wide));
}

/* "port mods update": the rows (the list moving on at its ends while there
are more mods past them), each mod's name (cut short where it is too long),
ON or OFF and its icon (out of the order it asks for; another says it cannot
be played with it, or it cannot be played), and the details of the mod
chosen */
static void mods_update(struct widget_instance *list)
{
	struct widget_instance *row;
	struct widget_instance *focused = mod_row(list->focused_child);
	short count = (short)port_mods_count();
	short focused_index = focused ? (short)(atoi(focused->name + 7) - 1) : NONE;

	if (focused_index == MOD_ROWS - 1 && mods_screen.first + MOD_ROWS < count && list_scrolls_at_end())
	{
		mods_screen.first++;
		focus_row(list, --focused_index);
	}
	else if (focused_index == 0 && mods_screen.first > 0 && list_scrolls_at_end())
	{
		mods_screen.first--;
		focus_row(list, ++focused_index);
	}
	if (mods_screen.first > 0 && mods_screen.first + MOD_ROWS > count)
		mods_screen.first = (short)MAX(0, count - MOD_ROWS);
	if (focused_index != NONE && mods_screen.first + focused_index < count)
		mods_screen.chosen = (short)(mods_screen.first + focused_index);
	for (row = list->child; row; row = row->next)
	{
		short place;
		short row_index;
		long mod;
		struct widget_instance *icon;
		int status;

		if (strncmp(row->name, "op_mod_", 7))
			continue;
		/* (its background: the focused one's outlined frame, which the
		engine shows only on a bitmap of two frames, as the browser's) */
		row->animation.current_frame_index = row == focused ? 1 : 0;
		place = mod_row_place(row);
		row->visible = place != NONE;
		if (place == NONE)
		{
			if (row == focused)
				focus_row(list, count ? (short)(count - 1 - mods_screen.first) : NONE);
			continue;
		}
		mod = mod_at_place(place);
		status = port_mods_status(mod, NULL, 0);
		row_index = (short)PIN(atoi(row->name + 7) - 1, 0, MOD_ROWS - 1);
		if (mods_screen.row_mods[row_index] != mod ||
			mods_screen.row_fonts[row_index] != mods_screen.fonts[_mod_row_name])
		{
			char const *name = port_mods_name(mod);
			char shortened[MOD_DETAILS_LENGTH];
			wchar_t wide[ROW_TEXT_LENGTH];

			if (mods_text_fits(_mod_row_name, name, strlen(name)))
				snprintf(shortened, sizeof(shortened), "%s", name);
			else
				mods_text_shorten(_mod_row_name, name, strlen(name), shortened, sizeof(shortened));
			mods_wide_text(shortened, wide, ROW_TEXT_LENGTH);
			text_set(named(row, "mod_name", 0), wide);
			mods_screen.row_mods[row_index] = mod;
			mods_screen.row_fonts[row_index] = mods_screen.fonts[_mod_row_name];
		}
		text_set(named(row, "mod_state", 0), port_mods_on(mod) ? L"ON" : L"OFF");
		icon = named(row, "mod_status", 0);
		visible_set(icon, status != _port_mod_fine);
		if (icon)
			icon->animation.current_frame_index = status == _port_mod_out_of_order ? 1 : 2;
	}
	mods_details_show(list->parameters.list.extended_description, count ? mods_screen.chosen : NONE, count);
}

/* "port mods save" (OK): kept, and played at once (port_mods_save keeps
what it cannot write until the game quits) */
static boolean mods_save(void)
{
	port_mods_save();
	mods_apply();
	return TRUE;
}

/* ---------- the Tag Injector (Settings > Gameplay > Tag Injector,
tools/port_settings.py's TAG_INJECTOR): laid out as the Mods screen is, its
rows (op_tag_<n>) a list at a time, and in the frame beside them the
details of the row chosen. The first list has whether the randomizer plays
a profile's tags (game.tag_injection), TAG PROFILES and MAPS. TAG PROFILES
lists the profiles, each with its size; a profile's, its tags (where each
came from, and its size), left and right turning one on or off (it stays in
the profile), A opening its settings: on, its faction and its type or class
(the randomizer's pools it joins), its custom type (which TAG INJECTOR's one
type chooses it by: typed) and taking it out. MAPS has the profile tags are
added to (game.tag_injection_profile: the profile opened last, or made),
CUSTOM SINGLEPLAYER and CUSTOM MULTIPLAYER, each its folders of custom_maps
and its loose maps as the map lists have them (map_browse_*), a folder its
maps; a map's, its kinds of tags; a kind's, its tags, each added to the
profile or taken out (tag_injector.c). B goes back up a list. A profile is
made with a name typed (tag_injector_profiles_poll) and deleted with DELETE
PROFILE, twice, which TAG PROFILES has; elsewhere the button is VIEW
PROFILE, the profile tags are added to shown from any list (B going back
there). A profile's tag can be given a name of its own (NAME), which the
menus show it by: a protected map's tags have names made for them.
MULTIPLAYER PROFILES lists the multiplayer profiles (port_tag_mp_profiles.c,
tag_multiplayer.c): each a profile whose weapons a multiplayer game plays,
and how (its settings' list; MANUAL's weapons a list of their own, the
profile's weapons on and ORIGINAL each turned on or off), made with NEW
PROFILE and deleted with DELETE PROFILE, twice. CAMPAIGN PROFILES lists the
campaign profiles (port_tag_mp_profiles.c's, tag_campaign.c) the same way:
each a profile whose weapons and AI a campaign level plays, its WEAPONS'
and ENEMIES' lists how (ROTATION, REPLACEMENT, MANUAL's choices for each of
the campaign's weapons, and each of its enemies: its ranks, if it has any,
each with its REPLACEMENT and, if it holds weapons, its LOADOUT). */

#define TAG_ROWS MOD_ROWS
#define TAG_PROFILE_NAME 24
/* how long the word of the last thing done stays at the details' top */
#define TAG_MESSAGE_MILLISECONDS 5000
/* the details' lines above the preview of a tag's model (tag_preview.c),
which a map's tags and a profile's have under them, and the lines under it
of what the tag needs to work */
#define TAG_PREVIEW_DETAILS_LINES 6
#define TAG_NEEDS_LINES 3

/* the lists */
enum
{
	_tags_list_top,
	_tags_list_profiles,
	_tags_list_profile_categories,
	_tags_list_profile,
	_tags_list_tag,
	_tags_list_kinds,
	_tags_list_browse,
	_tags_list_folder,
	_tags_list_categories,
	_tags_list_tags,
	/* MULTIPLAYER PROFILES, one's settings, and MANUAL's weapons for one of
	the map's */
	_tags_list_mp_profiles,
	_tags_list_mp_profile,
	_tags_list_mp_manual,
	/* CAMPAIGN PROFILES, one's settings, its WEAPONS and ENEMIES, MANUAL's
	choices for one of the campaign's weapons or enemy slots, an enemy's
	ranks, a slot's REPLACEMENT and LOADOUT, and LOADOUT's weapons */
	_tags_list_campaign_profiles,
	_tags_list_campaign_profile,
	_tags_list_campaign_weapons,
	_tags_list_campaign_enemies,
	_tags_list_campaign_manual,
	_tags_list_campaign_ranks,
	_tags_list_campaign_slot,
	_tags_list_campaign_loadout,
	NUMBER_OF_TAGS_LISTS
};

/* the first list's rows, and MAPS' */
enum
{
	_tags_top_injection,
	_tags_top_profiles,
	_tags_top_campaign,
	_tags_top_multiplayer,
	_tags_top_maps,
	NUMBER_OF_TAGS_TOP_ROWS
};

/* a profile's list's row past its kinds of tags: FIRING RANGE */
enum
{
	TAGS_FIRING_RANGE_ROW = NUMBER_OF_TAG_INJECTOR_CATEGORIES,
};

enum
{
	_tags_kind_profile,
	_tags_kind_campaign,
	_tags_kind_multiplayer,
	NUMBER_OF_TAGS_KIND_ROWS
};

/* a tag's settings' rows (tags_setting_rows: an actor variant's, a
weapon's and equipment's) */
enum
{
	_tags_setting_name,
	_tags_setting_enabled,
	_tags_setting_faction,
	_tags_setting_type,
	_tags_setting_class,
	_tags_setting_custom_type,
	_tags_setting_hud,
	_tags_setting_replace,
	_tags_setting_remove,
	MAXIMUM_TAGS_SETTING_ROWS = 8
};

/* the name typed: none, a new profile's, the tag's custom type, the name
the tag is shown by, or a copy's of the profile shown */
enum
{
	_tags_naming_none,
	_tags_naming_profile,
	_tags_naming_custom_type,
	_tags_naming_tag_name,
	_tags_naming_duplicate,
	/* a new multiplayer profile's, and the one shown's */
	_tags_naming_mp_profile,
	_tags_naming_mp_name,
	/* a new campaign profile's, and the one shown's */
	_tags_naming_campaign_profile,
	_tags_naming_campaign_name
};

/* a campaign profile's rows, and its WEAPONS' and ENEMIES' (tags_campaign_row_of:
a row each of the campaign's weapons, with MANUAL, or of its enemies) */
enum
{
	_tags_campaign_name,
	_tags_campaign_profile,
	_tags_campaign_weapons,
	_tags_campaign_enemies,
	NUMBER_OF_TAGS_CAMPAIGN_ROWS
};

enum
{
	_tags_campaign_row_rotation,
	_tags_campaign_row_replace,
	_tags_campaign_row_stock
};

/* a multiplayer profile's settings' rows (tags_mp_setting_of: MANUAL's
weapons a row each of the map's, SET's two weapons) */
enum
{
	_tags_mp_setting_name,
	_tags_mp_setting_profile,
	_tags_mp_setting_rotation,
	_tags_mp_setting_replace,
	_tags_mp_setting_weapon,
	_tags_mp_setting_loadout,
	_tags_mp_setting_primary,
	_tags_mp_setting_secondary
};

/* a setting's values as port_tag_profiles.c keeps them ("": AUTO, the
randomizer finding it by the tag's name) and as the rows show them */
struct tags_choice
{
	char const *value;
	char const *shown;
};

static struct tags_choice const tags_actor_factions[] =
{
	{ "", "AUTO" }, { "covenant", "COVENANT" }, { "flood", "FLOOD" }, { "human", "HUMAN" },
	{ "none", "ANY POOL ONLY" },
};
static struct tags_choice const tags_weapon_factions[] = { { "", "AUTO" }, { "covenant", "COVENANT" }, { "human", "HUMAN" } };
static struct tags_choice const tags_actor_types[] =
{
	{ "", "AUTO" }, { "none", "NONE" }, { "grunt", "GRUNT" }, { "elite", "ELITE" }, { "jackal", "JACKAL" },
	{ "hunter", "HUNTER" }, { "flood_combat", "COMBAT FORM" }, { "flood_carrier", "CARRIER FORM" },
	{ "marine", "MARINE" }, { "engineer", "ENGINEER" }, { "sentinel", "SENTINEL" },
};
static struct tags_choice const tags_weapon_classes[] =
{
	{ "", "AUTO" }, { "precision", "PRECISION" }, { "automatic", "AUTOMATIC" }, { "power", "POWER" },
	{ "none", "NONE" },
};
/* (the stock ones' values are tag_injector.c's stock HUDs') */
static struct tags_choice const tags_weapon_huds[] =
{
	{ "", "OWN" }, { "auto", "STOCK: AUTO" }, { "assault_rifle", "ASSAULT RIFLE" }, { "pistol", "PISTOL" },
	{ "sniper_rifle", "SNIPER RIFLE" }, { "rocket_launcher", "ROCKET LAUNCHER" }, { "shotgun", "SHOTGUN" },
	{ "flamethrower", "FLAMETHROWER" }, { "plasma_rifle", "PLASMA RIFLE" }, { "plasma_pistol", "PLASMA PISTOL" },
	{ "needler", "NEEDLER" },
};
static struct tags_choice const tags_replace_choices[] = { { "", "OFF" }, { "true", "ON" } };
/* a multiplayer profile's settings' values (tag_multiplayer.c's) */
static struct tags_choice const tags_mp_rotations[] = { { "add", "ADD TO MAP'S" }, { "only", "PROFILE'S ONLY" } };
static struct tags_choice const tags_mp_replaces[] =
{
	{ "random", "FULLY RANDOM" }, { "position", "ONCE PER POSITION" }, { "weapon", "ONCE PER WEAPON" },
	{ "manual", "MANUAL" },
};
static struct tags_choice const tags_mp_loadouts[] = { { "map", "GAMETYPE'S" }, { "random", "RANDOM" }, { "set", "SET" } };
/* a campaign profile's (tag_campaign.c's) */
static struct tags_choice const tags_campaign_rotations[] = { { "add", "ADD TO LEVEL'S" }, { "only", "PROFILE'S ONLY" } };
static struct tags_choice const tags_campaign_replaces[] = { { "random", "RANDOM" }, { "manual", "MANUAL" } };

static struct
{
	/* the list shown; the profile shown (whose tags are listed, and the
	maps add to), the kind of its tags listed and its tag whose settings are;
	MAPS' kind (campaign maps or not) and folder; the map and the kind of tags
	it is of; and each list's first row's place and place chosen (kept to go
	back up to) */
	short list;
	long profile;
	short profile_category;
	long tag;
	/* whether the tag shown is of a campaign map's path (REPLACE ORIGINAL) */
	boolean tag_stock;
	boolean campaign;
	char folder[CUSTOM_EDITION_MAP_CATEGORY_LENGTH];
	short map;
	short category;
	short first[NUMBER_OF_TAGS_LISTS];
	short chosen[NUMBER_OF_TAGS_LISTS];
	/* the name being typed (_tags_naming_*) */
	short naming;
	wchar_t name[TAG_INJECTOR_CUSTOM_TYPE_LENGTH + 1];
	/* DELETE PROFILE pressed once, for this profile, and REMOVE TAG, for this
	tag (NONE) */
	long deleting;
	long removing;
	/* the list VIEW PROFILE was pressed in, which B on the profile goes back
	to (NONE: TAG PROFILES, as from there) */
	short viewed_from;
	/* the word of the last thing done, and when */
	char message[256];
	unsigned long message_time;
	/* what the details, rows and button bar show (they are made again when it
	changes) */
	char shown[384];
	char row_shown[TAG_ROWS][128];
	char buttons_shown[64];
	/* each row's whether it is of what failed to load (tags_row_failed) */
	boolean row_failed[TAG_ROWS];
	/* the multiplayer profile shown, the map's weapon MANUAL's list is of,
	and DELETE PROFILE pressed once for that profile (NONE) */
	long mp_profile;
	short mp_weapon;
	long mp_deleting;
	/* the campaign profile shown; whether its ENEMIES are shown (else its
	WEAPONS); the campaign's weapon or enemy shown, and the enemy's rank (0:
	one without ranks); and DELETE PROFILE pressed once for that profile
	(NONE) */
	long campaign_profile;
	boolean campaign_enemies;
	short campaign_stock;
	short campaign_rank;
	long campaign_deleting;
} tags_screen = { 0, NONE, NONE, NONE, FALSE, FALSE, "", NONE, NONE, { 0 }, { 0 }, _tags_naming_none, { 0 }, NONE, NONE,
	NONE, "", 0, "", { { 0 } }, "", { FALSE }, NONE, 0, NONE, NONE, FALSE, 0, 0, NONE };

static void tags_message(char const *text)
{
	snprintf(tags_screen.message, sizeof(tags_screen.message), "%s", text ? text : "");
	tags_screen.message_time = system_milliseconds();
}

/* the profile shown, and the one the maps add to (game.tag_injection_profile) */
static void tags_profile_choose(long profile)
{
	long count = port_tag_profiles_count();

	tags_screen.deleting = NONE;
	tags_screen.profile = count ? (profile + count) % count : NONE;
	if (!config_write("game.tag_injection_profile", count ? port_tag_profiles_folder(tags_screen.profile) : ""))
		error(_error_silent, "tag injector: could not write game.tag_injection_profile to config.toml");
}

/* the row (op_tag_<n>) a widget is or is in, else NULL */
static struct widget_instance *tag_row(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strncmp(widget->name, "op_tag_", 7))
			return widget;
	}
	return NULL;
}

/* the settings' rows of the tag shown: how many (equipment joins no pool;
a weapon alone has a HUD, whatever carries it; REPLACE ORIGINAL is a tag's
of a campaign map's path) */
static short tags_setting_rows(short *rows)
{
	unsigned long group = port_tag_profiles_tag_group(tags_screen.profile, tags_screen.tag);
	short count = 0;

	rows[count++] = _tags_setting_name;
	rows[count++] = _tags_setting_enabled;
	if (group == 'weap' || group == 'actv')
	{
		rows[count++] = _tags_setting_faction;
		rows[count++] = group == 'weap' ? _tags_setting_class : _tags_setting_type;
		rows[count++] = _tags_setting_custom_type;
	}
	if (group == 'weap')
		rows[count++] = _tags_setting_hud;
	if (tags_screen.tag_stock)
		rows[count++] = _tags_setting_replace;
	rows[count++] = _tags_setting_remove;
	return count;
}

/* the setting of a row of the tag shown (NONE past them) */
static short tags_setting_of(short place)
{
	short rows[MAXIMUM_TAGS_SETTING_ROWS];
	short count = tags_setting_rows(rows);

	return place >= 0 && place < count ? rows[place] : NONE;
}

/* a setting's key and values (NULL for one of none) */
static struct tags_choice const *tags_setting_choices(short setting, char const **key, short *count)
{
	boolean weapon = port_tag_profiles_tag_group(tags_screen.profile, tags_screen.tag) == 'weap';

	*key = NULL;
	*count = 0;
	switch (setting)
	{
	case _tags_setting_faction:
		*key = "faction";
		*count = weapon ? (short)NUMBEROF(tags_weapon_factions) : (short)NUMBEROF(tags_actor_factions);
		return weapon ? tags_weapon_factions : tags_actor_factions;
	case _tags_setting_type:
		*key = "type";
		*count = (short)NUMBEROF(tags_actor_types);
		return tags_actor_types;
	case _tags_setting_class:
		*key = "class";
		*count = (short)NUMBEROF(tags_weapon_classes);
		return tags_weapon_classes;
	case _tags_setting_hud:
		*key = "hud";
		*count = (short)NUMBEROF(tags_weapon_huds);
		return tags_weapon_huds;
	case _tags_setting_replace:
		*key = "replace";
		*count = (short)NUMBEROF(tags_replace_choices);
		return tags_replace_choices;
	}
	return NULL;
}

/* the value a setting of the tag shown has, of its choices (AUTO for one
not among them) */
static short tags_setting_value(short setting)
{
	char const *key;
	short count;
	struct tags_choice const *choices = tags_setting_choices(setting, &key, &count);
	char const *value = key ? port_tag_profiles_tag_option(tags_screen.profile, tags_screen.tag, key) : "";
	short index;

	for (index = 0; choices && index < count; index++)
	{
		if (!strcmp(choices[index].value, value))
			return index;
	}
	return 0;
}

/* how many Custom Edition maps of a kind are in a folder */
static short tags_folder_count(boolean campaign, char const *folder)
{
	short total = custom_edition_maps_count(campaign);
	short count = 0;
	short index;

	for (index = 0; index < total; index++)
	{
		char const *category = custom_edition_maps_category(campaign, index);

		count = (short)(count + (category && !csstrcasecmp(category, folder)));
	}
	return count;
}

/* the folder MAPS lists (its top: "") */
static char const *tags_browse_folder(void)
{
	return tags_screen.list == _tags_list_folder ? tags_screen.folder : "";
}

/* how many of the profile shown's tags are of a category (as a map's are
listed) */
static long tags_profile_category_count(short category)
{
	unsigned long group = tag_injector_category_group(category);
	long count = port_tag_profiles_tag_count(tags_screen.profile);
	long found = 0;
	long tag;

	for (tag = 0; group && tag < count; tag++)
		found += port_tag_profiles_tag_group(tags_screen.profile, tag) == group;
	return found;
}

/* whether a profile's tag failed to load in the level last loaded (and was
turned off: its "failed" setting); and whether any of a profile's did, of a
group (0: of all) */
static boolean tags_tag_failed(long profile, long tag)
{
	return !port_tag_profiles_tag_enabled(profile, tag) && port_tag_profiles_tag_option(profile, tag, "failed")[0];
}

static boolean tags_profile_failed(long profile, unsigned long group)
{
	long count = port_tag_profiles_tag_count(profile);
	long tag;

	for (tag = 0; tag < count; tag++)
	{
		if ((!group || port_tag_profiles_tag_group(profile, tag) == group) && tags_tag_failed(profile, tag))
			return TRUE;
	}
	return FALSE;
}

/* the profile shown's tags that failed to load, as they were when a kind's
list of them was opened (tags_list_show forgets them): listed first, and one
turned on again keeps its place until the list is opened anew, not moving
from under the focus. Taken again for another profile, or once a tag is
added or taken out (its tags' indices moved) */
#define TAGS_LISTED_MAX 256 /* (port_tag_profiles.c's MAXIMUM_PROFILE_TAGS) */

static struct
{
	boolean taken;
	long profile;
	long count;
	boolean failed[TAGS_LISTED_MAX];
} tags_listed;

static boolean tags_listed_failed(long tag)
{
	long count = port_tag_profiles_tag_count(tags_screen.profile);

	if (!tags_listed.taken || tags_listed.profile != tags_screen.profile || tags_listed.count != count)
	{
		long index;

		tags_listed.taken = TRUE;
		tags_listed.profile = tags_screen.profile;
		tags_listed.count = count;
		for (index = 0; index < TAGS_LISTED_MAX; index++)
			tags_listed.failed[index] = index < count && tags_tag_failed(tags_screen.profile, index);
	}
	return tag >= 0 && tag < TAGS_LISTED_MAX && tags_listed.failed[tag];
}

/* the profile shown's tag at `place` of the category listed, or NONE: those
that failed to load first (tags_listed_failed), then the rest, each in the
profile's order */
static long tags_profile_tag_of(short place)
{
	unsigned long group = tag_injector_category_group(tags_screen.profile_category);
	long count = port_tag_profiles_tag_count(tags_screen.profile);
	short pass;
	long tag;

	for (pass = 0; pass < 2; pass++)
	{
		for (tag = 0; group && place >= 0 && tag < count; tag++)
		{
			if (port_tag_profiles_tag_group(tags_screen.profile, tag) == group &&
				tags_listed_failed(tag) == !pass && !place--)
			{
				return tag;
			}
		}
	}
	return NONE;
}

/* ---- the multiplayer profiles */

static void upper_copy(char *destination, size_t size, char const *source);

/* the profile the multiplayer profile shown plays (NONE: none, or not found) */
static long tags_mp_base(void)
{
	return port_tag_profiles_find(port_tag_mp_profiles_option(tags_screen.mp_profile, "profile"));
}

/* the weapons on of the profile it plays (those it shares): how many, and
the profile's tag of one */
static long tags_mp_weapon_count(void)
{
	long base = tags_mp_base();
	long count = port_tag_profiles_tag_count(base);
	long found = 0;
	long tag;

	for (tag = 0; tag < count; tag++)
		found += port_tag_profiles_tag_group(base, tag) == 'weap' && port_tag_profiles_tag_enabled(base, tag);
	return found;
}

static long tags_mp_weapon_tag(long weapon)
{
	long base = tags_mp_base();
	long count = port_tag_profiles_tag_count(base);
	long tag;

	for (tag = 0; tag < count; tag++)
	{
		if (port_tag_profiles_tag_group(base, tag) == 'weap' && port_tag_profiles_tag_enabled(base, tag) && !weapon--)
			return tag;
	}
	return NONE;
}

/* the settings' rows of the multiplayer profile shown: how many (MANUAL's
weapons with MANUAL, SET's with SET), and of each its setting and, of
MANUAL's, the map's weapon */
static short tags_mp_setting_rows(short *rows, short *weapons)
{
	short count = 0;
	short weapon;

	rows[count++] = _tags_mp_setting_name;
	rows[count++] = _tags_mp_setting_profile;
	rows[count++] = _tags_mp_setting_rotation;
	rows[count++] = _tags_mp_setting_replace;
	if (!strcmp(port_tag_mp_profiles_option(tags_screen.mp_profile, "replace"), "manual"))
	{
		for (weapon = 0; weapon < TAG_MULTIPLAYER_STOCK_WEAPONS; weapon++)
		{
			weapons[count] = weapon;
			rows[count++] = _tags_mp_setting_weapon;
		}
	}
	rows[count++] = _tags_mp_setting_loadout;
	if (!strcmp(port_tag_mp_profiles_option(tags_screen.mp_profile, "loadout"), "set"))
	{
		rows[count++] = _tags_mp_setting_primary;
		rows[count++] = _tags_mp_setting_secondary;
	}
	return count;
}

#define MAXIMUM_TAGS_MP_SETTING_ROWS (8 + TAG_MULTIPLAYER_STOCK_WEAPONS)

static short tags_mp_setting_of(short place, short *weapon)
{
	short rows[MAXIMUM_TAGS_MP_SETTING_ROWS];
	short weapons[MAXIMUM_TAGS_MP_SETTING_ROWS];
	short count = tags_mp_setting_rows(rows, weapons);

	*weapon = NONE;
	if (place < 0 || place >= count)
		return NONE;
	if (rows[place] == _tags_mp_setting_weapon)
		*weapon = weapons[place];
	return rows[place];
}

/* a setting's key and choices (NULL for one of none) */
static struct tags_choice const *tags_mp_setting_choices(short setting, char const **key, short *count)
{
	*key = NULL;
	*count = 0;
	switch (setting)
	{
	case _tags_mp_setting_rotation:
		*key = "rotation";
		*count = (short)NUMBEROF(tags_mp_rotations);
		return tags_mp_rotations;
	case _tags_mp_setting_replace:
		*key = "replace";
		*count = (short)NUMBEROF(tags_mp_replaces);
		return tags_mp_replaces;
	case _tags_mp_setting_loadout:
		*key = "loadout";
		*count = (short)NUMBEROF(tags_mp_loadouts);
		return tags_mp_loadouts;
	}
	return NULL;
}

static short tags_mp_setting_value(short setting)
{
	char const *key;
	short count;
	struct tags_choice const *choices = tags_mp_setting_choices(setting, &key, &count);
	char const *value = key ? port_tag_mp_profiles_option(tags_screen.mp_profile, key) : "";
	short index;

	for (index = 0; choices && index < count; index++)
	{
		if (!strcmp(choices[index].value, value))
			return index;
	}
	return 0;
}

/* SET's weapons' choices: NONE, the map's (stock:<key>), then the profile's
weapons on (by their paths): how many, and of one its value and the name it
is shown by */
static long tags_mp_loadout_count(void)
{
	return 1 + TAG_MULTIPLAYER_STOCK_WEAPONS + tags_mp_weapon_count();
}

static void tags_mp_loadout_choice(long choice, char *value, size_t value_size, char *shown, size_t shown_size)
{
	value[0] = 0;
	snprintf(shown, shown_size, "NONE");
	if (choice >= 1 && choice <= TAG_MULTIPLAYER_STOCK_WEAPONS)
	{
		snprintf(value, value_size, "stock:%s", tag_multiplayer_stock_key((short)(choice - 1)));
		snprintf(shown, shown_size, "MAP'S %s", tag_multiplayer_stock_name((short)(choice - 1)));
	}
	else if (choice > TAG_MULTIPLAYER_STOCK_WEAPONS)
	{
		long tag = tags_mp_weapon_tag(choice - 1 - TAG_MULTIPLAYER_STOCK_WEAPONS);
		char name[256];

		if (tag == NONE)
			return;
		snprintf(value, value_size, "%s", port_tag_profiles_tag_name(tags_mp_base(), tag));
		tag_injector_tag_display_name(tags_mp_base(), tag, name, sizeof(name));
		upper_copy(shown, shown_size, name);
	}
}

/* the choice a SET weapon has (0, NONE, for one not among them) */
static long tags_mp_loadout_value(char const *key)
{
	char const *value = port_tag_mp_profiles_option(tags_screen.mp_profile, key);
	long count = tags_mp_loadout_count();
	long choice;

	for (choice = 1; value[0] && choice < count; choice++)
	{
		char option[256];
		char shown[ROW_TEXT_LENGTH];

		tags_mp_loadout_choice(choice, option, sizeof(option), shown, sizeof(shown));
		if (!csstrcasecmp(option, value))
			return choice;
	}
	return 0;
}

/* MANUAL's list's row: ORIGINAL (0), or the profile's weapon's path and
the name it is shown by */
static char const *tags_mp_manual_path(short place, char *shown, size_t shown_size)
{
	long tag;

	if (place == 0)
	{
		snprintf(shown, shown_size, "ORIGINAL (MAP'S %s)", tag_multiplayer_stock_name(tags_screen.mp_weapon));
		return "original";
	}
	tag = tags_mp_weapon_tag(place - 1);
	if (tag == NONE)
	{
		shown[0] = 0;
		return NULL;
	}
	tag_injector_tag_display_name(tags_mp_base(), tag, shown, (long)shown_size);
	return port_tag_profiles_tag_name(tags_mp_base(), tag);
}

/* a setting of the multiplayer profile shown, its value before or after */
static void tags_mp_setting_step(short setting, short step)
{
	char const *key;
	short count;
	struct tags_choice const *choices = tags_mp_setting_choices(setting, &key, &count);
	boolean written = TRUE;

	if (setting == _tags_mp_setting_profile)
	{
		long profiles = port_tag_profiles_count();
		long base = tags_mp_base();

		if (!profiles)
		{
			tags_message("No profiles: TAG PROFILES' NEW PROFILE makes one.");
			ui_play_audio_feedback_sound(SOUND_ERROR);
			return;
		}
		base = base == NONE ? 0 : (base + step + profiles) % profiles;
		written = port_tag_mp_profiles_set_option(tags_screen.mp_profile, "profile", port_tag_profiles_folder(base));
	}
	else if (setting == _tags_mp_setting_primary || setting == _tags_mp_setting_secondary)
	{
		char const *key_name = setting == _tags_mp_setting_primary ? "primary" : "secondary";
		long choices_count = tags_mp_loadout_count();
		char value[256];
		char shown[ROW_TEXT_LENGTH];

		tags_mp_loadout_choice((tags_mp_loadout_value(key_name) + step + choices_count) % choices_count, value,
			sizeof(value), shown, sizeof(shown));
		written = port_tag_mp_profiles_set_option(tags_screen.mp_profile, key_name, value);
	}
	else if (choices)
	{
		written = port_tag_mp_profiles_set_option(tags_screen.mp_profile, key,
			choices[(tags_mp_setting_value(setting) + step + count) % count].value);
	}
	else
	{
		return;
	}
	if (!written)
		tags_message("The multiplayer profile cannot be written (the log says why).");
	ui_play_audio_feedback_sound(SOUND_CURSOR);
}

/* MANUAL's weapon of a row turned on or off for the map's weapon shown */
static void tags_mp_manual_toggle(short place)
{
	char shown[ROW_TEXT_LENGTH];
	char const *path = tags_mp_manual_path(place, shown, sizeof(shown));
	char const *key = tag_multiplayer_stock_key(tags_screen.mp_weapon);

	if (!path)
		return;
	if (!port_tag_mp_profiles_manual_set(tags_screen.mp_profile, key, path,
		!port_tag_mp_profiles_manual_has(tags_screen.mp_profile, key, path)))
		tags_message("The multiplayer profile cannot be written (the log says why).");
	ui_play_audio_feedback_sound(SOUND_CURSOR);
}

/* the profile the campaign profile shown plays (NONE: none, or not found) */
static long tags_campaign_base(void)
{
	return port_tag_profiles_find(port_tag_campaign_profiles_option(tags_screen.campaign_profile, "profile"));
}

/* the tags on of a group of the profile it plays: how many, and the
profile's tag of one */
static long tags_campaign_tag_count(unsigned long group)
{
	long base = tags_campaign_base();
	long count = port_tag_profiles_tag_count(base);
	long found = 0;
	long tag;

	for (tag = 0; tag < count; tag++)
		found += port_tag_profiles_tag_group(base, tag) == group && port_tag_profiles_tag_enabled(base, tag);
	return found;
}

static long tags_campaign_tag(unsigned long group, long index)
{
	long base = tags_campaign_base();
	long count = port_tag_profiles_tag_count(base);
	long tag;

	for (tag = 0; tag < count; tag++)
	{
		if (port_tag_profiles_tag_group(base, tag) == group && port_tag_profiles_tag_enabled(base, tag) && !index--)
			return tag;
	}
	return NONE;
}

/* the WEAPONS' or ENEMIES' shown: its settings' keys, its table's, and how
many of the campaign's weapons or enemies it has a row for */
static char const *tags_campaign_key(boolean replace)
{
	if (tags_screen.campaign_enemies)
		return replace ? "enemy_replace" : "enemy_rotation";
	return replace ? "weapon_replace" : "weapon_rotation";
}

static char const *tags_campaign_section(void)
{
	return tags_screen.campaign_enemies ? "enemies" : "weapons";
}

static boolean tags_campaign_manual(void)
{
	return !strcmp(port_tag_campaign_profiles_option(tags_screen.campaign_profile, tags_campaign_key(TRUE)), "manual");
}

/* (the weapons' rows with MANUAL alone; the enemies' always, for their
loadouts) */
static short tags_campaign_stock_count(void)
{
	if (tags_screen.campaign_enemies)
		return TAG_CAMPAIGN_STOCK_ENEMIES;
	return tags_campaign_manual() ? TAG_CAMPAIGN_STOCK_WEAPONS : 0;
}

/* the key and name of what MANUAL's list is of: the campaign's weapon, or
the enemy's slot (its rank's) */
static char const *tags_campaign_stock_key(void)
{
	static char key[64];

	if (!tags_screen.campaign_enemies)
		return tag_campaign_weapon_key(tags_screen.campaign_stock);
	tag_campaign_slot_key(tags_screen.campaign_stock, tags_screen.campaign_rank, key, sizeof(key));
	return key;
}

static char const *tags_campaign_stock_name(void)
{
	static char name[64];

	if (!tags_screen.campaign_enemies)
		return tag_campaign_weapon_name(tags_screen.campaign_stock);
	tag_campaign_slot_name(tags_screen.campaign_stock, tags_screen.campaign_rank, name, sizeof(name));
	return name;
}

/* the WEAPONS' or ENEMIES' rows: ROTATION, REPLACEMENT, and one for each of
the campaign's weapons (with MANUAL) or enemies: how many, and of a row its
kind and (of the campaign's) which */
static short tags_campaign_row_count(void)
{
	return (short)(2 + tags_campaign_stock_count());
}

static short tags_campaign_row_of(short place, short *stock)
{
	*stock = NONE;
	if (place == 0)
		return _tags_campaign_row_rotation;
	if (place == 1)
		return _tags_campaign_row_replace;
	*stock = (short)(place - 2);
	return _tags_campaign_row_stock;
}

/* a ROTATION's or REPLACEMENT's choice */
static short tags_campaign_value(boolean replace)
{
	struct tags_choice const *choices = replace ? tags_campaign_replaces : tags_campaign_rotations;
	short count = replace ? (short)NUMBEROF(tags_campaign_replaces) : (short)NUMBEROF(tags_campaign_rotations);
	char const *value = port_tag_campaign_profiles_option(tags_screen.campaign_profile, tags_campaign_key(replace));
	short index;

	for (index = 0; index < count; index++)
	{
		if (!strcmp(choices[index].value, value))
			return index;
	}
	return 0;
}

static void tags_campaign_written(boolean written)
{
	if (!written)
		tags_message("The campaign profile cannot be written (the log says why).");
	ui_play_audio_feedback_sound(SOUND_CURSOR);
}

/* the campaign profile's TAG PROFILE, its value before or after */
static void tags_campaign_profile_step(short step)
{
	long profiles = port_tag_profiles_count();
	long base = tags_campaign_base();

	if (!profiles)
	{
		tags_message("No profiles: TAG PROFILES' NEW PROFILE makes one.");
		ui_play_audio_feedback_sound(SOUND_ERROR);
		return;
	}
	base = base == NONE ? 0 : (base + step + profiles) % profiles;
	tags_campaign_written(port_tag_campaign_profiles_set_option(tags_screen.campaign_profile, "profile",
		port_tag_profiles_folder(base)));
}

/* ROTATION's or REPLACEMENT's value before or after */
static void tags_campaign_value_step(boolean replace, short step)
{
	struct tags_choice const *choices = replace ? tags_campaign_replaces : tags_campaign_rotations;
	short count = replace ? (short)NUMBEROF(tags_campaign_replaces) : (short)NUMBEROF(tags_campaign_rotations);

	tags_campaign_written(port_tag_campaign_profiles_set_option(tags_screen.campaign_profile,
		tags_campaign_key(replace), choices[(tags_campaign_value(replace) + step + count) % count].value));
}

/* the slots' list of an enemy with ranks (each a rank), and a slot's rows
(REPLACEMENT, LOADOUT) */
enum
{
	_tags_campaign_slot_replace,
	_tags_campaign_slot_loadout,
	NUMBER_OF_TAGS_CAMPAIGN_SLOT_ROWS
};

/* how many of a slot's tables' tags are chosen for it */
static long tags_campaign_slot_chosen(short enemy, short rank, char const *section)
{
	char key[64];

	tag_campaign_slot_key(enemy, rank, key, sizeof(key));
	return port_tag_campaign_profiles_list_count(tags_screen.campaign_profile, section, key);
}

/* what a slot's row says: its REPLACEMENT's and LOADOUT's choices (an
enemy's: its ranks', all of them) */
static void tags_campaign_slot_state(short enemy, short rank, char *state, size_t state_size)
{
	long replaced = 0;
	long armed = 0;
	short count = tag_campaign_enemy_rank_count(enemy);
	short index;

	for (index = 0; index < MAX(1, count); index++)
	{
		short each = count ? tag_campaign_enemy_rank(enemy, index) : 0;

		if (rank != NONE && each != rank && count)
			continue;
		replaced += tags_campaign_slot_chosen(enemy, each, "enemies");
		armed += tags_campaign_slot_chosen(enemy, each, "loadout");
	}
	if (!replaced && !armed)
		snprintf(state, state_size, "AS IT IS");
	else if (!tag_campaign_enemy_armed(enemy) || !armed)
		snprintf(state, state_size, "%ld AI", replaced);
	else
		snprintf(state, state_size, "%ld AI, %ld WEAPONS", replaced, armed);
}

/* MANUAL's list's row: ORIGINAL (0), or the profile's weapon's or AI's path
and the name it is shown by */
static char const *tags_campaign_manual_path(short place, char *shown, size_t shown_size)
{
	long tag;

	if (place == 0)
	{
		snprintf(shown, shown_size, "ORIGINAL (LEVEL'S %s)", tags_campaign_stock_name());
		return "original";
	}
	tag = tags_campaign_tag(tags_screen.campaign_enemies ? 'actv' : 'weap', place - 1);
	if (tag == NONE)
	{
		shown[0] = 0;
		return NULL;
	}
	tag_injector_tag_display_name(tags_campaign_base(), tag, shown, (long)shown_size);
	return port_tag_profiles_tag_name(tags_campaign_base(), tag);
}

static void tags_campaign_manual_toggle(short place)
{
	char shown[ROW_TEXT_LENGTH];
	char const *path = tags_campaign_manual_path(place, shown, sizeof(shown));
	char const *key = tags_campaign_stock_key();

	if (!path)
		return;
	tags_campaign_written(port_tag_campaign_profiles_list_set(tags_screen.campaign_profile, tags_campaign_section(),
		key, path, !port_tag_campaign_profiles_list_has(tags_screen.campaign_profile, tags_campaign_section(), key,
		path)));
}

/* LOADOUT's weapons of the slot shown: ORIGINAL (its own), the profile's
weapons on, then the campaign's (stock:<key>): how many, and of one its
value and the name it is shown by */
static long tags_campaign_loadout_count(void)
{
	return 1 + tags_campaign_tag_count('weap') + TAG_CAMPAIGN_STOCK_WEAPONS;
}

static char const *tags_campaign_loadout_choice(long place, char *value, size_t value_size, char *shown,
	size_t shown_size)
{
	long weapons = tags_campaign_tag_count('weap');

	value[0] = 0;
	shown[0] = 0;
	if (place == 0)
	{
		snprintf(value, value_size, "original");
		snprintf(shown, shown_size, "ORIGINAL (ITS OWN)");
	}
	else if (place <= weapons)
	{
		long tag = tags_campaign_tag('weap', place - 1);
		char name[256];

		if (tag == NONE)
			return NULL;
		snprintf(value, value_size, "%s", port_tag_profiles_tag_name(tags_campaign_base(), tag));
		tag_injector_tag_display_name(tags_campaign_base(), tag, name, sizeof(name));
		upper_copy(shown, shown_size, name);
	}
	else if (place - 1 - weapons < TAG_CAMPAIGN_STOCK_WEAPONS)
	{
		snprintf(value, value_size, "stock:%s", tag_campaign_weapon_key((short)(place - 1 - weapons)));
		snprintf(shown, shown_size, "CAMPAIGN'S %s", tag_campaign_weapon_name((short)(place - 1 - weapons)));
	}
	else
	{
		return NULL;
	}
	return value;
}

static void tags_campaign_loadout_toggle(short place)
{
	char value[256];
	char shown[ROW_TEXT_LENGTH];
	char const *key = tags_campaign_stock_key();

	if (!tags_campaign_loadout_choice(place, value, sizeof(value), shown, sizeof(shown)))
		return;
	tags_campaign_written(port_tag_campaign_profiles_list_set(tags_screen.campaign_profile, "loadout", key, value,
		!port_tag_campaign_profiles_list_has(tags_screen.campaign_profile, "loadout", key, value)));
}

/* how many rows the list shown has */
static short tags_count(void)
{
	short rows[MAXIMUM_TAGS_SETTING_ROWS];

	switch (tags_screen.list)
	{
	case _tags_list_top:
		return NUMBER_OF_TAGS_TOP_ROWS;
	case _tags_list_profiles:
		return (short)port_tag_profiles_count();
	case _tags_list_profile_categories:
		/* (and FIRING RANGE, after them) */
		return NUMBER_OF_TAG_INJECTOR_CATEGORIES + 1;
	case _tags_list_profile:
		return (short)MIN(tags_profile_category_count(tags_screen.profile_category), 0x7FFF);
	case _tags_list_tag:
		return tags_setting_rows(rows);
	case _tags_list_kinds:
		return NUMBER_OF_TAGS_KIND_ROWS;
	case _tags_list_browse:
	case _tags_list_folder:
		return map_browse_count(tags_screen.campaign, tags_browse_folder());
	case _tags_list_categories:
		return NUMBER_OF_TAG_INJECTOR_CATEGORIES;
	case _tags_list_mp_profiles:
		return (short)port_tag_mp_profiles_count();
	case _tags_list_mp_profile:
	{
		short mp_rows[MAXIMUM_TAGS_MP_SETTING_ROWS];
		short weapons[MAXIMUM_TAGS_MP_SETTING_ROWS];

		return tags_mp_setting_rows(mp_rows, weapons);
	}
	case _tags_list_mp_manual:
		return (short)MIN(1 + tags_mp_weapon_count(), 0x7FFF);
	case _tags_list_campaign_profiles:
		return (short)port_tag_campaign_profiles_count();
	case _tags_list_campaign_profile:
		return NUMBER_OF_TAGS_CAMPAIGN_ROWS;
	case _tags_list_campaign_weapons:
	case _tags_list_campaign_enemies:
		return tags_campaign_row_count();
	case _tags_list_campaign_manual:
		return (short)MIN(1 + tags_campaign_tag_count(tags_screen.campaign_enemies ? 'actv' : 'weap'), 0x7FFF);
	case _tags_list_campaign_ranks:
		return tag_campaign_enemy_rank_count(tags_screen.campaign_stock);
	case _tags_list_campaign_slot:
		return NUMBER_OF_TAGS_CAMPAIGN_SLOT_ROWS;
	case _tags_list_campaign_loadout:
		return (short)MIN(tags_campaign_loadout_count(), 0x7FFF);
	default:
		return (short)MIN(tag_injector_category_count(tags_screen.category), 0x7FFF);
	}
}

static short tag_row_place(struct widget_instance *row)
{
	short place = row ? (short)(tags_screen.first[tags_screen.list] + atoi(row->name + 7) - 1) : NONE;

	return place >= 0 && place < tags_count() ? place : NONE;
}

/* whether a row is of what failed to load in the level last loaded: TAG
PROFILES (any profile's tag), a profile (any of its tags), a profile's kind
of tags (any of them) or a profile's tag; its name and state are drawn
yellow (pc_menu_text_color) */
static boolean tags_row_failed(short place)
{
	long profile;

	switch (tags_screen.list)
	{
	case _tags_list_top:
		if (place != _tags_top_profiles)
			return FALSE;
		for (profile = 0; profile < port_tag_profiles_count(); profile++)
		{
			if (tags_profile_failed(profile, 0))
				return TRUE;
		}
		return FALSE;
	case _tags_list_profiles:
		return tags_profile_failed(place, 0);
	case _tags_list_profile_categories:
		return tag_injector_category_group(place) && tags_profile_failed(tags_screen.profile,
			tag_injector_category_group(place));
	case _tags_list_profile:
		return tags_profile_tag_of(place) != NONE && tags_tag_failed(tags_screen.profile, tags_profile_tag_of(place));
	default:
		return FALSE;
	}
}

/* the last part of a tag's path (the name it is listed by) */
static char const *tag_leaf(char const *path)
{
	char const *slash = strrchr(path, '\\');

	return slash ? slash + 1 : path;
}

static void upper_copy(char *destination, size_t size, char const *source)
{
	size_t index;

	for (index = 0; index + 1 < size && source[index]; index++)
		destination[index] = (char)toupper((unsigned char)source[index]);
	if (size)
		destination[index] = 0;
}

/* a name of the menus' as ASCII (their files' names are) */
static void tags_ascii_copy(char *destination, size_t size, wchar_t const *source)
{
	size_t index;

	for (index = 0; index + 1 < size && source && source[index]; index++)
		destination[index] = source[index] < 0x80 ? (char)source[index] : '?';
	if (size)
		destination[index] = 0;
}

/* the name a profile's tag is shown by (tag_injector_tag_display_name) */
static char const *tags_tag_shown(long profile, long tag)
{
	/* (a path's last part may be longer than a name given) */
	static char shown[256];

	tag_injector_tag_display_name(profile, tag, shown, sizeof(shown));
	return shown;
}

/* the kind of a profile's tag, as the menu names it */
static char const *tags_group_name(unsigned long group)
{
	return group == 'weap' ? "WEAPON" : group == 'actv' ? "AI" : group == 'eqip' ? "EQUIPMENT" : "TAG";
}

/* MAPS' row at `place`: a folder (its name), or a map (its index of
custom_edition_maps_count's, and its name) */
static boolean tags_browse_row(short place, char *name, size_t name_size, short *flat)
{
	char const *folder = tags_browse_folder();
	wchar_t wide[ROW_TEXT_LENGTH];

	*flat = NONE;
	if (map_browse_folder(tags_screen.campaign, folder, place))
	{
		snprintf(name, name_size, "%s", map_browse_folder_name(tags_screen.campaign, folder, place));
		return TRUE;
	}
	*flat = map_browse_flat(tags_screen.campaign, folder, place);
	wide[0] = 0;
	if (*flat != NONE)
		custom_map_text(tags_screen.campaign, *flat, wide);
	tags_ascii_copy(name, name_size, wide);
	return FALSE;
}

/* a row's name and state, as it shows them */
static void tags_row_text(short place, char *name, size_t name_size, char *state, size_t state_size)
{
	char size[24];

	name[0] = 0;
	state[0] = 0;
	switch (tags_screen.list)
	{
	case _tags_list_top:
		if (place == _tags_top_injection)
		{
			snprintf(name, name_size, "TAG INJECTION");
			snprintf(state, state_size, "%s", config_boolean("game.tag_injection") ? "ON" : "OFF");
		}
		else if (place == _tags_top_profiles)
		{
			snprintf(name, name_size, "TAG PROFILES");
			snprintf(state, state_size, "%ld", port_tag_profiles_count());
		}
		else if (place == _tags_top_campaign)
		{
			snprintf(name, name_size, "CAMPAIGN PROFILES");
			snprintf(state, state_size, "%ld", port_tag_campaign_profiles_count());
		}
		else if (place == _tags_top_multiplayer)
		{
			snprintf(name, name_size, "MULTIPLAYER PROFILES");
			snprintf(state, state_size, "%ld", port_tag_mp_profiles_count());
		}
		else
		{
			snprintf(name, name_size, "MAPS");
			snprintf(state, state_size, "%d", tag_injector_map_count());
		}
		break;
	case _tags_list_profiles:
		snprintf(name, name_size, "%s", port_tag_profiles_name(place));
		if (port_tag_profiles_tag_count(place))
			tags_size_text(port_tag_profiles_size(place), state, state_size);
		else
			snprintf(state, state_size, "EMPTY");
		break;
	case _tags_list_profile_categories:
		if (place == TAGS_FIRING_RANGE_ROW)
		{
			snprintf(name, name_size, "FIRING RANGE");
			break;
		}
		snprintf(name, name_size, "%s", tag_injector_category_name(place));
		snprintf(state, state_size, "%ld", tags_profile_category_count(place));
		break;
	case _tags_list_profile:
	{
		long tag = tags_profile_tag_of(place);

		snprintf(name, name_size, "%s", tags_tag_shown(tags_screen.profile, tag));
		tags_size_text(port_tag_profiles_tag_size(tags_screen.profile, tag), size, sizeof(size));
		if (!port_tag_profiles_tag_size(tags_screen.profile, tag))
			snprintf(state, state_size, "MISSING");
		/* (it could not be played in the level last loaded, and was turned
		off) */
		else if (tags_tag_failed(tags_screen.profile, tag))
			snprintf(state, state_size, "FAILED  %s", size);
		else
			snprintf(state, state_size, "%s  %s", port_tag_profiles_tag_enabled(tags_screen.profile, tag) ? "ON" : "OFF",
				size);
		break;
	}
	case _tags_list_tag:
		switch (tags_setting_of(place))
		{
		case _tags_setting_name:
			snprintf(name, name_size, "NAME");
			upper_copy(state, state_size, tags_tag_shown(tags_screen.profile, tags_screen.tag));
			break;
		case _tags_setting_enabled:
			snprintf(name, name_size, "ENABLED");
			snprintf(state, state_size, "%s", port_tag_profiles_tag_enabled(tags_screen.profile, tags_screen.tag) ?
				"ON" : "OFF");
			break;
		case _tags_setting_custom_type:
			snprintf(name, name_size, "CUSTOM TYPE");
			tag_injector_tag_custom_type(tags_screen.profile, tags_screen.tag, state, (long)state_size);
			break;
		case _tags_setting_remove:
			snprintf(name, name_size, "REMOVE TAG");
			snprintf(state, state_size, "%s", tags_screen.removing == tags_screen.tag ? "A AGAIN" : "");
			break;
		default:
		{
			short setting = tags_setting_of(place);
			char const *key;
			short count;
			struct tags_choice const *choices = tags_setting_choices(setting, &key, &count);

			snprintf(name, name_size, "%s", setting == _tags_setting_faction ? "FACTION" :
				setting == _tags_setting_type ? "COUNTS AS" : setting == _tags_setting_hud ? "HUD" :
				setting == _tags_setting_replace ? "REPLACE ORIGINAL" : "CLASS");
			snprintf(state, state_size, "%s", choices ? choices[tags_setting_value(setting)].shown : "");
			break;
		}
		}
		break;
	case _tags_list_kinds:
		if (place == _tags_kind_profile)
		{
			snprintf(name, name_size, "ADD TO");
			upper_copy(state, state_size, tags_screen.profile != NONE ? port_tag_profiles_name(tags_screen.profile) :
				"NONE");
		}
		else
		{
			boolean campaign = place == _tags_kind_campaign;

			snprintf(name, name_size, "%s", campaign ? "CUSTOM SINGLEPLAYER" : "CUSTOM MULTIPLAYER");
			snprintf(state, state_size, "%d", custom_edition_maps_count(campaign));
		}
		break;
	case _tags_list_browse:
	case _tags_list_folder:
	{
		short flat;

		if (tags_browse_row(place, name, name_size, &flat))
			snprintf(state, state_size, "%d MAPS", tags_folder_count(tags_screen.campaign, name));
		break;
	}
	case _tags_list_categories:
		snprintf(name, name_size, "%s", tag_injector_category_name(place));
		snprintf(state, state_size, "%ld", tag_injector_category_count(place));
		break;
	case _tags_list_mp_profiles:
	{
		long base = port_tag_profiles_find(port_tag_mp_profiles_option(place, "profile"));

		snprintf(name, name_size, "%s", port_tag_mp_profiles_name(place));
		upper_copy(state, state_size, base != NONE ? port_tag_profiles_name(base) : "NO PROFILE");
		break;
	}
	case _tags_list_mp_profile:
	{
		short weapon;
		short setting = tags_mp_setting_of(place, &weapon);

		switch (setting)
		{
		case _tags_mp_setting_name:
			snprintf(name, name_size, "NAME");
			upper_copy(state, state_size, port_tag_mp_profiles_name(tags_screen.mp_profile));
			break;
		case _tags_mp_setting_profile:
			snprintf(name, name_size, "TAG PROFILE");
			upper_copy(state, state_size, tags_mp_base() != NONE ? port_tag_profiles_name(tags_mp_base()) : "NONE");
			break;
		case _tags_mp_setting_weapon:
		{
			long chosen = port_tag_mp_profiles_manual_count(tags_screen.mp_profile, tag_multiplayer_stock_key(weapon));

			snprintf(name, name_size, "  %s", tag_multiplayer_stock_name(weapon));
			if (chosen)
				snprintf(state, state_size, "%ld CHOSEN", chosen);
			else
				snprintf(state, state_size, "AS IT IS");
			break;
		}
		case _tags_mp_setting_primary:
		case _tags_mp_setting_secondary:
		{
			char value[256];
			char const *key = setting == _tags_mp_setting_primary ? "primary" : "secondary";

			snprintf(name, name_size, "  %s", setting == _tags_mp_setting_primary ? "PRIMARY" : "SECONDARY");
			tags_mp_loadout_choice(tags_mp_loadout_value(key), value, sizeof(value), state, state_size);
			break;
		}
		default:
		{
			char const *key;
			short count;
			struct tags_choice const *choices = tags_mp_setting_choices(setting, &key, &count);

			snprintf(name, name_size, "%s", setting == _tags_mp_setting_rotation ? "ROTATION" :
				setting == _tags_mp_setting_replace ? "REPLACEMENT" : "LOADOUT");
			snprintf(state, state_size, "%s", choices ? choices[tags_mp_setting_value(setting)].shown : "");
			break;
		}
		}
		break;
	}
	case _tags_list_mp_manual:
	{
		char shown[ROW_TEXT_LENGTH];
		char const *path = tags_mp_manual_path(place, shown, sizeof(shown));

		upper_copy(name, name_size, shown);
		snprintf(state, state_size, "%s", path && port_tag_mp_profiles_manual_has(tags_screen.mp_profile,
			tag_multiplayer_stock_key(tags_screen.mp_weapon), path) ? "ON" : "OFF");
		break;
	}
	case _tags_list_campaign_profiles:
	{
		long base = port_tag_profiles_find(port_tag_campaign_profiles_option(place, "profile"));

		snprintf(name, name_size, "%s", port_tag_campaign_profiles_name(place));
		upper_copy(state, state_size, base != NONE ? port_tag_profiles_name(base) : "NO PROFILE");
		break;
	}
	case _tags_list_campaign_profile:
		switch (place)
		{
		case _tags_campaign_name:
			snprintf(name, name_size, "NAME");
			upper_copy(state, state_size, port_tag_campaign_profiles_name(tags_screen.campaign_profile));
			break;
		case _tags_campaign_profile:
			snprintf(name, name_size, "TAG PROFILE");
			upper_copy(state, state_size, tags_campaign_base() != NONE ? port_tag_profiles_name(tags_campaign_base()) :
				"NONE");
			break;
		case _tags_campaign_weapons:
		case _tags_campaign_enemies:
		{
			boolean enemies = place == _tags_campaign_enemies;
			char const *rotation = port_tag_campaign_profiles_option(tags_screen.campaign_profile,
				enemies ? "enemy_rotation" : "weapon_rotation");
			char const *replace = port_tag_campaign_profiles_option(tags_screen.campaign_profile,
				enemies ? "enemy_replace" : "weapon_replace");

			snprintf(name, name_size, "%s", enemies ? "ENEMIES" : "WEAPONS");
			snprintf(state, state_size, "%s, %s", !strcmp(rotation, "only") ? "ONLY" : "ADD",
				!strcmp(replace, "manual") ? "MANUAL" : "RANDOM");
			break;
		}
		}
		break;
	case _tags_list_campaign_weapons:
	case _tags_list_campaign_enemies:
	{
		short stock;

		switch (tags_campaign_row_of(place, &stock))
		{
		case _tags_campaign_row_rotation:
			snprintf(name, name_size, "ROTATION");
			snprintf(state, state_size, "%s", tags_campaign_rotations[tags_campaign_value(FALSE)].shown);
			break;
		case _tags_campaign_row_replace:
			snprintf(name, name_size, "REPLACEMENT");
			snprintf(state, state_size, "%s", tags_campaign_replaces[tags_campaign_value(TRUE)].shown);
			break;
		default:
			if (tags_screen.campaign_enemies)
			{
				snprintf(name, name_size, "  %s", tag_campaign_enemy_name(stock));
				tags_campaign_slot_state(stock, NONE, state, state_size);
			}
			else
			{
				long chosen = port_tag_campaign_profiles_list_count(tags_screen.campaign_profile, "weapons",
					tag_campaign_weapon_key(stock));

				snprintf(name, name_size, "  %s", tag_campaign_weapon_name(stock));
				if (chosen)
					snprintf(state, state_size, "%ld CHOSEN", chosen);
				else
					snprintf(state, state_size, "AS IT IS");
			}
			break;
		}
		break;
	}
	case _tags_list_campaign_manual:
	{
		char shown[ROW_TEXT_LENGTH];
		char const *path = tags_campaign_manual_path(place, shown, sizeof(shown));

		upper_copy(name, name_size, shown);
		snprintf(state, state_size, "%s", path && port_tag_campaign_profiles_list_has(tags_screen.campaign_profile,
			tags_campaign_section(), tags_campaign_stock_key(), path) ? "ON" : "OFF");
		break;
	}
	case _tags_list_campaign_ranks:
	{
		short rank = tag_campaign_enemy_rank(tags_screen.campaign_stock, place);

		snprintf(name, name_size, "%s", tag_campaign_rank_name(rank));
		tags_campaign_slot_state(tags_screen.campaign_stock, rank, state, state_size);
		break;
	}
	case _tags_list_campaign_slot:
	{
		boolean replace = place == _tags_campaign_slot_replace;
		long chosen = tags_campaign_slot_chosen(tags_screen.campaign_stock, tags_screen.campaign_rank,
			replace ? "enemies" : "loadout");

		snprintf(name, name_size, "%s", replace ? "REPLACEMENT" : "LOADOUT");
		if (replace && !tags_campaign_manual())
			snprintf(state, state_size, "RANDOM");
		else if (!replace && !tag_campaign_enemy_armed(tags_screen.campaign_stock))
			snprintf(state, state_size, "NO WEAPONS");
		else if (chosen)
			snprintf(state, state_size, "%ld CHOSEN", chosen);
		else
			snprintf(state, state_size, replace ? "AS IT IS" : "ITS OWN");
		break;
	}
	case _tags_list_campaign_loadout:
	{
		char value[256];
		char shown[ROW_TEXT_LENGTH];

		if (!tags_campaign_loadout_choice(place, value, sizeof(value), shown, sizeof(shown)))
			break;
		snprintf(name, name_size, "%s", shown);
		snprintf(state, state_size, "%s", port_tag_campaign_profiles_list_has(tags_screen.campaign_profile, "loadout",
			tags_campaign_stock_key(), value) ? "ON" : "OFF");
		break;
	}
	default:
		snprintf(name, name_size, "%s", tag_leaf(tag_injector_category_tag(tags_screen.category, place)));
		if (tag_injector_category_addable(tags_screen.category) &&
			tag_injector_tag_in_profile(tags_screen.profile, tags_screen.category, place) != NONE)
			snprintf(state, state_size, "ADDED");
		break;
	}
}

/* the details of a profile: its tags, and how many are on */
static void tags_profile_details(long profile, char *text, size_t text_size, char const *then)
{
	long count = port_tag_profiles_tag_count(profile);
	char size[24];
	long tag;

	tags_size_text(port_tag_profiles_size(profile), size, sizeof(size));
	snprintf(text, text_size, "%ld tag%s, %ld on: %ld AI, %ld weapons. %s.\n \n%s\n", count, count == 1 ? "" : "s",
		tag_injector_profile_enabled_count(profile, 0), tag_injector_profile_enabled_count(profile, 'actv'),
		tag_injector_profile_enabled_count(profile, 'weap'), size, then);
	for (tag = 0; tag < count; tag++)
	{
		size_t length = strlen(text);

		snprintf(text + length, text_size - length, "\n%s (%s)%s", tags_tag_shown(profile, tag),
			port_tag_profiles_tag_map(profile, tag), port_tag_profiles_tag_enabled(profile, tag) ? "" : ", off");
	}
}

/* the details of the row at `place`: its name in the frame, and below it
what it is and what A does */
/* what the details of a profile's tag say first when it could not be played
in the level last loaded (its "failed" setting: tag_injector.c turned it
off), else nothing */
static void tags_failure_text(long profile, long tag, char *text, size_t text_size)
{
	char const *failed = port_tag_profiles_tag_option(profile, tag, "failed");

	if (failed[0] && !port_tag_profiles_tag_enabled(profile, tag))
	{
		snprintf(text, text_size, "It failed to load last time: %s. It was turned off: turn it on to try it "
			"again.\n \n", tag_injector_failure_describe(failed));
	}
	else
	{
		text[0] = 0;
	}
}

static void tags_details_text(short place, char *title, size_t title_size, char *text, size_t text_size)
{
	char profile[96];
	char size[24];

	snprintf(profile, sizeof(profile), "%s", tags_screen.profile != NONE ? port_tag_profiles_name(tags_screen.profile) :
		"");
	title[0] = 0;
	text[0] = 0;
	switch (tags_screen.list)
	{
	case _tags_list_top:
		if (place == _tags_top_injection)
		{
			snprintf(title, title_size, "Tag Injection");
			snprintf(text, text_size, "On: a campaign level played alone can be given a profile's weapons and AI, by "
				"the randomizer or as a campaign profile says. A screen before the level chooses which, or none.\n \n"
				"A, left or right turns it on or off.");
		}
		else if (place == _tags_top_campaign)
		{
			snprintf(title, title_size, "Campaign Profiles");
			snprintf(text, text_size, "How a campaign level played alone plays a profile's weapons and AI, in the "
				"randomizer's place: the level's weapons and enemies swapped for them, and the weapons its AI "
				"hold. The screen before the level chooses one.\n \nA lists them.");
		}
		else if (place == _tags_top_multiplayer)
		{
			snprintf(title, title_size, "Multiplayer Profiles");
			snprintf(text, text_size, "How a multiplayer game on the game's own maps plays a profile's weapons: "
				"swapped for the map's, and as the players' loadouts. Server Setup's TAG PROFILE chooses one for a "
				"game you host; its players download the weapons from you.\n \nA lists them.");
		}
		else if (place == _tags_top_profiles)
		{
			snprintf(title, title_size, "Tag Profiles");
			snprintf(text, text_size, "The profiles, each with its tags' size. A lists them, to see a profile's tags, "
				"turn them on or off and change their settings.\n \nThere NEW PROFILE makes one. Each is a folder of "
				"the tag_injector folder, beside the maps folder.");
		}
		else
		{
			char reason[160];

			snprintf(title, title_size, "Maps");
			if (!tag_injector_maps_available(reason, sizeof(reason)))
				snprintf(text, text_size, "%s", reason);
			else
				snprintf(text, text_size, "The Custom Edition maps of custom_maps, singleplayer and multiplayer, by "
					"their folders.\n \nA lists them, to add their weapons and AI to a profile.");
		}
		break;
	case _tags_list_profiles:
		if (place == NONE)
		{
			snprintf(title, title_size, "No profiles");
			snprintf(text, text_size, "NEW PROFILE makes one, to add tags to. Each is a folder of the tag_injector "
				"folder, beside the maps folder.");
			break;
		}
		snprintf(title, title_size, "%s", port_tag_profiles_name(place));
		tags_profile_details(place, text, text_size, "A lists its tags: there DUPLICATE PROFILE copies it, and DELETE "
			"PROFILE, twice, deletes it");
		break;
	case _tags_list_profile_categories:
	{
		long count = tags_profile_category_count(place);

		if (place == TAGS_FIRING_RANGE_ROW)
		{
			snprintf(title, title_size, "FIRING RANGE");
			snprintf(text, text_size, "Try out all the weapon and enemy tags from your profile.\n \n"
				"When holding a weapon, you're able to edit its values (like HUD element positions, name, "
				"category, damage, etc) from the overlay menu.\n \n"
				"You'll be able to see all enemies and their animations, as well as spawn them into a small arena "
				"to fight them and make sure everything works properly.");
			break;
		}
		snprintf(title, title_size, "%s", profile);
		snprintf(text, text_size, "%ld %s.\n \n%s", count, tag_injector_category_name(place),
			count ? "A lists them, to turn them on or off and change their settings." :
			tag_injector_category_addable(place) ? "None yet: choose a map in MAPS, then these to add here." :
			"None: the randomizer cannot play these yet.");
		break;
	}
	case _tags_list_profile:
	{
		unsigned long group;
		long tag = tags_profile_tag_of(place);
		char failure[MOD_DETAILS_LENGTH / 2];

		if (tag == NONE)
		{
			snprintf(title, title_size, "%s", profile);
			snprintf(text, text_size, "No %s yet: choose a map in MAPS, then its %s to add here.",
				tag_injector_category_name(tags_screen.profile_category),
				tag_injector_category_name(tags_screen.profile_category));
			break;
		}
		group = port_tag_profiles_tag_group(tags_screen.profile, tag);
		snprintf(title, title_size, "%s", tags_tag_shown(tags_screen.profile, tag));
		tags_size_text(port_tag_profiles_tag_size(tags_screen.profile, tag), size, sizeof(size));
		if (!port_tag_profiles_tag_size(tags_screen.profile, tag))
			snprintf(size, sizeof(size), "missing: it is left out");
		tags_failure_text(tags_screen.profile, tag, failure, sizeof(failure));
		snprintf(text, text_size, "%s%s from %s, %s%s.\n \n%s: the randomizer %s it.\n \nLeft or right turns it %s. A "
			"opens its settings%s.", failure, tags_group_name(group),
			port_tag_profiles_tag_map(tags_screen.profile, tag), size,
			port_tag_profiles_tag_option(tags_screen.profile, tag, "replace")[0] ? ", replacing the original" : "",
			port_tag_profiles_tag_enabled(tags_screen.profile, tag) ? "On" : "Off",
			port_tag_profiles_tag_enabled(tags_screen.profile, tag) ? "plays" : "leaves out",
			port_tag_profiles_tag_enabled(tags_screen.profile, tag) ? "off" : "on",
			group == 'weap' ? ": its name, faction, class, custom type and HUD" :
			group == 'actv' ? ": its name, faction, type and custom type" : ": its name");
		break;
	}
	case _tags_list_tag:
	{
		char type[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];
		boolean weapon = port_tag_profiles_tag_group(tags_screen.profile, tags_screen.tag) == 'weap';

		snprintf(title, title_size, "%s", tags_tag_shown(tags_screen.profile, tags_screen.tag));
		switch (tags_setting_of(place))
		{
		case _tags_setting_name:
			snprintf(text, text_size, "The name the menus show it by: %s.\n%s %s\n \nA types another. Nothing typed is "
				"the last part of its path. Its path stays its map's: a level knows it by it.",
				tags_tag_shown(tags_screen.profile, tags_screen.tag),
				tags_group_name(port_tag_profiles_tag_group(tags_screen.profile, tags_screen.tag)),
				port_tag_profiles_tag_name(tags_screen.profile, tags_screen.tag));
			break;
		case _tags_setting_enabled:
		{
			char failure[MOD_DETAILS_LENGTH / 2];

			tags_failure_text(tags_screen.profile, tags_screen.tag, failure, sizeof(failure));
			snprintf(text, text_size, "%sOn: the randomizer plays it. Off: it stays in the profile, and is left out.\n \n"
				"A, left or right turns it on or off.", failure);
			break;
		}
		case _tags_setting_faction:
			snprintf(text, text_size, weapon ?
				"The weapon pools it joins: COVENANT's or HUMAN's.\n \nAUTO: by its name (a plasma, needle or carbine "
				"weapon is the Covenant's).\n \nLeft and right choose another." :
				"The enemy pool it joins: COVENANT, FLOOD or HUMAN, or ANY's alone.\n \nAUTO: by its unit's name (a "
				"grunt, elite, jackal, hunter or engineer is the Covenant's, a flood form the Flood's, a marine "
				"human). A brute is none of them: choose COVENANT.\n \nLeft and right choose another.");
			break;
		case _tags_setting_type:
			snprintf(text, text_size, "The campaign's type it counts as for ONE TYPE: as an ELITE, it is among the "
				"elites a pool of one type plays.\n \nAUTO: by its unit's name. NONE: no type's.\n \nLeft and right "
				"choose another.");
			break;
		case _tags_setting_class:
			snprintf(text, text_size, "The weapon pool it joins: PRECISION, AUTOMATIC or POWER.\n \nAUTO: by its name "
				"(a sniper or a pistol is precision, a rocket or a shotgun power, a rifle automatic). NONE: none of "
				"them.\n \nLeft and right choose another.");
			break;
		case _tags_setting_custom_type:
			tag_injector_tag_custom_type(tags_screen.profile, tags_screen.tag, type, sizeof(type));
			snprintf(text, text_size, "The randomizer's ONE TYPE has TAG INJECTOR, and its CUSTOM TYPE these: the "
				"profile's %s of the custom type %s alone. Give tags the same one (BRUTE) to play them together.\n \n"
				"A types it. Nothing typed is its name.", weapon ? "weapons" : "AI", type);
			break;
		case _tags_setting_hud:
			snprintf(text, text_size, "The HUD it is drawn with, whoever carries it (an AI of the profile too). "
				"OWN: its map's. STOCK: the campaign's HUD of the weapon chosen (its zoom and ammo art; the weapon "
				"zooms and fires as it did). AUTO: by its name, else by its zoom and magazine.\n \nLeft and right "
				"choose another.");
			break;
		case _tags_setting_replace:
			snprintf(text, text_size, "ON: it takes the place of the campaign's tag of its path, with the tags it "
				"names as its map has them, wherever the level has that tag (its own, or one the randomizer "
				"copies).\n \nOFF: the level's stays, and this one joins the pools as it is.\n \nA, left or "
				"right turns it on or off.");
			break;
		case _tags_setting_remove:
			snprintf(text, text_size, "A, twice, takes it out of %s and deletes its bundle. Its map can add it again.\n"
				" \nTo leave it out for now, turn it off.", profile);
			break;
		}
		break;
	}
	case _tags_list_kinds:
		if (place == _tags_kind_profile)
		{
			snprintf(title, title_size, "Add To");
			if (tags_screen.profile == NONE)
				snprintf(text, text_size, "No profiles: NEW PROFILE makes one, to add tags to.");
			else
				snprintf(text, text_size, "The profile a map's tags are added to: %s. Left and right choose another."
					"\n \nVIEW PROFILE, here and in the maps' lists, shows it.", profile);
		}
		else
		{
			boolean campaign = place == _tags_kind_campaign;

			snprintf(title, title_size, "%s", campaign ? "Custom Singleplayer" : "Custom Multiplayer");
			snprintf(text, text_size, "%d Custom Edition %s maps.\n \nA lists their folders of custom_maps, and the "
				"maps beside them.", custom_edition_maps_count(campaign), campaign ? "campaign" : "multiplayer");
		}
		break;
	case _tags_list_browse:
	case _tags_list_folder:
	{
		char name[ROW_TEXT_LENGTH];
		char reason[160];
		short flat;

		if (place == NONE)
		{
			snprintf(title, title_size, "No maps");
			snprintf(text, text_size, "No Custom Edition maps of this kind: put them in custom_maps.");
			break;
		}
		if (tags_browse_row(place, name, sizeof(name), &flat))
		{
			snprintf(title, title_size, "%s", name);
			snprintf(text, text_size, "A folder of custom_maps: %d maps.\n \nA lists them.",
				tags_folder_count(tags_screen.campaign, name));
			break;
		}
		snprintf(title, title_size, "%s", name);
		if (!tag_injector_maps_available(reason, sizeof(reason)))
			snprintf(text, text_size, "%s", reason);
		else
			snprintf(text, text_size, "A %s map%s%s.\n \nA lists its weapons, AI and the rest, to add to %s.",
				tags_screen.campaign ? "campaign" : "multiplayer", tags_browse_folder()[0] ? ", in " : "",
				tags_browse_folder(), profile[0] ? profile : "a profile");
		break;
	}
	case _tags_list_mp_profiles:
	{
		long base;

		if (place == NONE)
		{
			snprintf(title, title_size, "No multiplayer profiles");
			snprintf(text, text_size, "NEW PROFILE makes one, playing the weapons of a profile of TAG PROFILES. Each "
				"is a file of the tag_injector folder's multiplayer folder.");
			break;
		}
		base = port_tag_profiles_find(port_tag_mp_profiles_option(place, "profile"));
		snprintf(title, title_size, "%s", port_tag_mp_profiles_name(place));
		if (base == NONE)
			snprintf(text, text_size, "Its profile (%s) is not there: A, then TAG PROFILE, chooses another.",
				port_tag_mp_profiles_option(place, "profile"));
		else
			snprintf(text, text_size, "Plays the %ld weapons on of %s.\n \nA opens its settings: there DELETE "
				"PROFILE, twice, deletes it.", tag_injector_profile_enabled_count(base, 'weap'),
				port_tag_profiles_name(base));
		break;
	}
	case _tags_list_mp_profile:
	{
		short weapon;

		snprintf(title, title_size, "%s", port_tag_mp_profiles_name(tags_screen.mp_profile));
		switch (tags_mp_setting_of(place, &weapon))
		{
		case _tags_mp_setting_name:
			snprintf(text, text_size, "The name the menus and Server Setup show it by.\n \nA types another.");
			break;
		case _tags_mp_setting_profile:
			snprintf(text, text_size, "The profile whose weapons on it plays (%ld of them): its weapons alone, not its "
				"AI or equipment. A game you host shares them with its players, who download them from you.\n \n"
				"A, left and right choose another.", tags_mp_weapon_count());
			break;
		case _tags_mp_setting_rotation:
			snprintf(text, text_size, "ADD TO MAP'S: the profile's weapons join the map's, so a weapon may be either. "
				"PROFILE'S ONLY: the map's weapons are all the profile's.\n \nWith MANUAL, ADD keeps the map's "
				"weapon among those chosen for it.\n \nLeft and right choose.");
			break;
		case _tags_mp_setting_replace:
			snprintf(text, text_size, "How the map's weapons are swapped:\nFULLY RANDOM: any weapon, each time one "
				"spawns.\nONCE PER POSITION: each place rolls once a game.\nONCE PER WEAPON: each of the map's "
				"weapons rolls once (every shotgun the same).\nMANUAL: the weapons chosen for each.\n \nLeft and "
				"right choose.");
			break;
		case _tags_mp_setting_weapon:
			snprintf(text, text_size, "The weapons that take the place of the map's %s, one at random each time it "
				"spawns. None chosen: it stays as it is.\n \nA lists them.", tag_multiplayer_stock_name(weapon));
			break;
		case _tags_mp_setting_loadout:
			snprintf(text, text_size, "The weapons each player spawns with.\nGAMETYPE'S: as the gametype gives them."
				"\nRANDOM: two of the weapons a map's weapon may be swapped for.\nSET: the two chosen below.\n \n"
				"Left and right choose.");
			break;
		default:
			snprintf(text, text_size, "A weapon each player spawns with: one of the profile's, or one of the map's "
				"(when the map has it).\n \nA, left and right choose another.");
			break;
		}
		break;
	}
	case _tags_list_campaign_profiles:
	{
		long base;

		if (place == NONE)
		{
			snprintf(title, title_size, "No campaign profiles");
			snprintf(text, text_size, "NEW PROFILE makes one, playing the weapons and AI of a profile of TAG "
				"PROFILES. Each is a file of the tag_injector folder's campaign folder.");
			break;
		}
		base = port_tag_profiles_find(port_tag_campaign_profiles_option(place, "profile"));
		snprintf(title, title_size, "%s", port_tag_campaign_profiles_name(place));
		if (base == NONE)
			snprintf(text, text_size, "Its profile (%s) is not there: A, then TAG PROFILE, chooses another.",
				port_tag_campaign_profiles_option(place, "profile"));
		else
			snprintf(text, text_size, "Plays the %ld weapons and %ld AI on of %s.\n \nA opens its settings: there "
				"DELETE PROFILE, twice, deletes it.", tag_injector_profile_enabled_count(base, 'weap'),
				tag_injector_profile_enabled_count(base, 'actv'), port_tag_profiles_name(base));
		break;
	}
	case _tags_list_campaign_profile:
		snprintf(title, title_size, "%s", port_tag_campaign_profiles_name(tags_screen.campaign_profile));
		switch (place)
		{
		case _tags_campaign_name:
			snprintf(text, text_size, "The name the menus show it by.\n \nA types another.");
			break;
		case _tags_campaign_profile:
			snprintf(text, text_size, "The profile whose weapons (%ld) and AI (%ld) on it plays.\n \nA, left and "
				"right choose another.", tags_campaign_tag_count('weap'), tags_campaign_tag_count('actv'));
			break;
		case _tags_campaign_weapons:
			snprintf(text, text_size, "How the weapons lying in the level are swapped for the profile's.\n \nA "
				"opens them.");
			break;
		default:
			snprintf(text, text_size, "How the level's enemies are swapped for the profile's AI, and the weapons "
				"each kind and rank of them holds (its LOADOUT).\n \nA opens them.");
			break;
		}
		break;
	case _tags_list_campaign_weapons:
	case _tags_list_campaign_enemies:
	{
		short stock;
		boolean enemies = tags_screen.campaign_enemies;

		snprintf(title, title_size, "%s", enemies ? "Enemies" : "Weapons");
		switch (tags_campaign_row_of(place, &stock))
		{
		case _tags_campaign_row_rotation:
			snprintf(text, text_size, "ADD TO LEVEL'S: the profile's %s join the level's, so one may stay as it is. "
				"PROFILE'S ONLY: they are all the profile's.\n \nWith MANUAL, ADD keeps the level's own among those "
				"chosen for it.\n \nLeft and right choose.", enemies ? "AI" : "weapons");
			break;
		case _tags_campaign_row_replace:
			if (enemies)
				snprintf(text, text_size, "How the level's enemies are swapped:\nRANDOM: any of the profile's AI, each "
					"placed (your allies stay as they are).\nMANUAL: the AI chosen for each enemy (and rank) below.\n \n"
					"Left and right choose.");
			else
				snprintf(text, text_size, "How the level's weapons, lying in it or held by its AI, are swapped:\nRANDOM: "
					"any of the profile's weapons, each placed.\nMANUAL: the weapons chosen for each of the campaign's."
					"\n \nLeft and right choose.");
			break;
		default:
			if (!enemies)
				snprintf(text, text_size, "The weapons that take the place of the level's %s, lying in it or held by "
					"its AI (who keep it when they cannot hold them), one at random each placed. None chosen: it stays "
					"as it is.\n \nA lists them.", tag_campaign_weapon_name(stock));
			else if (tag_campaign_enemy_rank_count(stock))
				snprintf(text, text_size, "The level's %ss, each rank its own: the AI that take its place, and the "
					"weapons it holds.\n \nA lists its ranks.", tag_campaign_enemy_name(stock));
			else
				snprintf(text, text_size, "The AI that take the place of the level's %ss%s.\n \nA opens them.",
					tag_campaign_enemy_name(stock), tag_campaign_enemy_armed(stock) ? ", and the weapons it holds" : "");
			break;
		}
		break;
	}
	case _tags_list_campaign_manual:
	{
		char shown[ROW_TEXT_LENGTH];

		snprintf(title, title_size, "%s", tags_campaign_stock_name());
		tags_campaign_manual_path(place, shown, sizeof(shown));
		snprintf(text, text_size, "On: %s may take the place of the level's %s, one of those on at random each "
			"placed.\n \nA, left or right turns it on or off.", place == 0 ? "the level's own" : shown,
			tags_campaign_stock_name());
		break;
	}
	case _tags_list_campaign_ranks:
	{
		char name[64];

		tag_campaign_slot_name(tags_screen.campaign_stock, tag_campaign_enemy_rank(tags_screen.campaign_stock, place),
			name, sizeof(name));
		snprintf(title, title_size, "%s", name);
		snprintf(text, text_size, "The AI that take the place of the level's %ss%s.\n \nA opens them.", name,
			tag_campaign_enemy_armed(tags_screen.campaign_stock) ? ", and the weapons they hold" : "");
		break;
	}
	case _tags_list_campaign_slot:
		snprintf(title, title_size, "%s", tags_campaign_stock_name());
		if (place == _tags_campaign_slot_replace)
			snprintf(text, text_size, "%s", tags_campaign_manual() ? "The AI that take its place, one of those on at "
				"random each placed. None chosen: it stays as it is.\n \nA lists them." : "ENEMIES' REPLACEMENT is "
				"RANDOM: any of the profile's AI takes its place. MANUAL chooses them here.");
		else if (!tag_campaign_enemy_armed(tags_screen.campaign_stock))
			snprintf(text, text_size, "It holds no weapons a loadout can choose.");
		else
			snprintf(text, text_size, "The weapons it holds (it, or the AI that took its place), one of those on that "
				"it can hold, at random. None chosen: its own, swapped as WEAPONS says.\n \nA lists them.");
		break;
	case _tags_list_campaign_loadout:
	{
		char value[256];
		char shown[ROW_TEXT_LENGTH];

		snprintf(title, title_size, "%s", tags_campaign_stock_name());
		tags_campaign_loadout_choice(place, value, sizeof(value), shown, sizeof(shown));
		snprintf(text, text_size, "On: it may hold %s%s.\n \nA, left or right turns it on or off.",
			place == 0 ? "its own weapon" : shown,
			place > tags_campaign_tag_count('weap') ? " (when the level has it)" : "");
		break;
	}
	case _tags_list_mp_manual:
	{
		char shown[ROW_TEXT_LENGTH];

		snprintf(title, title_size, "%s", tag_multiplayer_stock_name(tags_screen.mp_weapon));
		tags_mp_manual_path(place, shown, sizeof(shown));
		snprintf(text, text_size, "On: %s may take the place of the map's %s, one of those on at random each time it "
			"spawns.\n \nA, left or right turns it on or off.", place == 0 ? "the map's own" : shown,
			tag_multiplayer_stock_name(tags_screen.mp_weapon));
		break;
	}
	case _tags_list_categories:
	{
		long count = tag_injector_category_count(place);

		tags_ascii_copy(title, title_size, tag_injector_map_name(tags_screen.map));
		snprintf(text, text_size, "%ld %s.\n \n%s", count, tag_injector_category_name(place),
			!count ? "None here." :
			tag_injector_category_addable(place) ? "A lists them, to add to the profile." :
			"A lists them. The randomizer cannot play these yet.");
		break;
	}
	default:
	{
		char const *path = tag_injector_category_tag(tags_screen.category, place);
		char reason[320];
		long tag_size = tag_injector_tag_size(tags_screen.category, place, reason, sizeof(reason));
		boolean added = tag_injector_tag_in_profile(tags_screen.profile, tags_screen.category, place) != NONE;
		size_t length;

		/* (its path is the frame's title's, the last part of it, and its
		size: what it would add is what it needs, under the preview) */
		snprintf(title, title_size, "%s", tag_leaf(path));
		if (tag_size == NONE)
			snprintf(text, text_size, "%s", reason);
		else if (!tag_injector_category_addable(tags_screen.category))
			snprintf(text, text_size, "%ld tags required. The randomizer cannot play these yet.", tag_size);
		else if (tags_screen.profile == NONE)
			snprintf(text, text_size, "%ld tags required. Make a profile to add it to.", tag_size);
		else
			snprintf(text, text_size, "%ld tags required.%s", tag_size, added ? " In the profile." : "");
		/* (a protected map's: its path is one made for it) */
		length = strlen(text);
		if (tag_injector_tag_name_made(tags_screen.category, place))
			snprintf(text + length, text_size - length, "\n \nIts map hides its tags' names: this one is made from "
				"what it is.");
		break;
	}
	}
}

/* "port tags init" (the list's created) and "port tags close" (its
deleted): the maps found again, and the profile shown game.tag_injection's */
static boolean tags_screen_reset(boolean opening)
{
	short list;

	tag_injector_map_close();
	tag_preview_dispose();
	tags_screen.list = _tags_list_top;
	tags_screen.tag = NONE;
	tags_screen.map = NONE;
	tags_screen.profile_category = NONE;
	tags_screen.category = NONE;
	tags_screen.folder[0] = 0;
	for (list = 0; list < NUMBER_OF_TAGS_LISTS; list++)
	{
		tags_screen.first[list] = 0;
		tags_screen.chosen[list] = 0;
	}
	tags_screen.deleting = NONE;
	tags_screen.removing = NONE;
	tags_screen.viewed_from = NONE;
	tags_screen.mp_profile = NONE;
	tags_screen.mp_weapon = 0;
	tags_screen.mp_deleting = NONE;
	tags_screen.campaign_profile = NONE;
	tags_screen.campaign_enemies = FALSE;
	tags_screen.campaign_stock = 0;
	tags_screen.campaign_rank = 0;
	tags_screen.campaign_deleting = NONE;
	tags_screen.message[0] = 0;
	tags_screen.shown[0] = 0;
	csmemset(tags_screen.row_shown, 0, sizeof(tags_screen.row_shown));
	tags_screen.buttons_shown[0] = 0;
	/* (the text boxes' measures it shares with the Mods screen: none until
	they are drawn, as mods_screen_reset leaves them, else a font of 0 would
	be asked for before the Mods screen has ever opened) */
	for (list = 0; list < NUMBER_OF_MOD_TEXT_BOXES; list++)
	{
		mods_screen.fonts[list] = NONE;
		mods_screen.widths[list] = 0;
	}
	if (!opening)
		return TRUE;
	/* (the profiles downloaded from hosts, copied as the player's own: those
	not copied as they came) */
	{
		long copied = port_tag_profiles_shared_import_all();

		if (copied)
		{
			char text[sizeof(tags_screen.message)];

			snprintf(text, sizeof(text), "Copied %ld profile%s downloaded from network games into TAG PROFILES "
				"(and their multiplayer profiles): yours to change.", copied, copied == 1 ? "" : "s");
			tags_message(text);
		}
	}
	port_tag_profiles_scan();
	port_tag_mp_profiles_scan();
	port_tag_campaign_profiles_scan();
	tag_injector_maps_find();
	map_browse_forget();
	tags_screen.profile = port_tag_profiles_find(config_string("game.tag_injection_profile"));
	if (tags_screen.profile == NONE && port_tag_profiles_count())
		tags_profile_choose(0);
	return TRUE;
}

/* the list shown another: its rows as they were left, the focus on the
place chosen there */
static void tags_list_show(struct widget_instance *list, short shown)
{
	/* (a kind's tags opened, not come back to from a tag's settings: those
	that failed now first) */
	if (shown == _tags_list_profile && tags_screen.list != _tags_list_tag)
		tags_listed.taken = FALSE;
	tags_screen.list = shown;
	tags_screen.shown[0] = 0;
	csmemset(tags_screen.row_shown, 0, sizeof(tags_screen.row_shown));
	if (list)
		focus_row(list, (short)(tags_screen.chosen[shown] - tags_screen.first[shown]));
}

/* a list shown from its first row */
static void tags_list_open(struct widget_instance *list, short shown, short chosen)
{
	tags_screen.first[shown] = (short)MAX(0, chosen - (TAG_ROWS - 1));
	tags_screen.chosen[shown] = chosen;
	tags_list_show(list, shown);
}

static struct widget_instance *tags_list_of(struct widget_instance *widget)
{
	struct widget_instance *screen = screen_of(widget);

	return screen ? named(screen, "tags_list", 0) : NULL;
}

/* the list MAPS' map was chosen in (a folder's, or the top's) */
static short tags_browse_list(void)
{
	return tags_screen.folder[0] ? _tags_list_folder : _tags_list_browse;
}

/* a map of MAPS (its index of custom_edition_maps_count's) opened, and its
kinds of tags shown */
static void tags_map_open(struct widget_instance *list, short flat)
{
	char reason[320];
	short map = tag_injector_map_of(tags_screen.campaign, flat);

	if (!tag_injector_maps_available(reason, sizeof(reason)))
	{
		tags_message(reason);
		ui_play_audio_feedback_sound(SOUND_ERROR);
	}
	else if (map == NONE || !tag_injector_map_open(map))
	{
		tags_message("The map cannot be read (the log says why).");
		ui_play_audio_feedback_sound(SOUND_ERROR);
	}
	else
	{
		tags_screen.map = map;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		tags_list_open(list, _tags_list_categories, 0);
	}
}

/* a setting of the tag shown, its value before or after */
static void tags_setting_step(short setting, short step)
{
	char const *key;
	short count;
	struct tags_choice const *choices = tags_setting_choices(setting, &key, &count);

	if (setting == _tags_setting_enabled)
	{
		if (!port_tag_profiles_tag_set_enabled(tags_screen.profile, tags_screen.tag,
			!port_tag_profiles_tag_enabled(tags_screen.profile, tags_screen.tag)))
			tags_message("The profile cannot be written (the log says why).");
		ui_play_audio_feedback_sound(SOUND_CURSOR);
		return;
	}
	if (!choices)
		return;
	if (!port_tag_profiles_tag_set_option(tags_screen.profile, tags_screen.tag, key,
		choices[(tags_setting_value(setting) + step + count) % count].value))
		tags_message("The profile cannot be written (the log says why).");
	ui_play_audio_feedback_sound(SOUND_CURSOR);
}

/* the keyboard asked for a name: a new profile's, or the tag's custom type */
static void tags_name_ask(short naming, char const *text, short length)
{
	short character;

	tags_screen.deleting = NONE;
	tags_screen.removing = NONE;
	csmemset(tags_screen.name, 0, sizeof(tags_screen.name));
	for (character = 0; text && text[character] && character < length; character++)
		tags_screen.name[character] = (wchar_t)(unsigned char)text[character];
	virtual_keyboard_allow_any_name();
	if (!virtual_keyboard_launch(tags_screen.name, (word)((length + 1) * sizeof(wchar_t)), 8))
	{
		error(_error_silent, "tag injector: the keyboard did not open for a name");
		ui_play_audio_feedback_sound(SOUND_ERROR);
		return;
	}
	tags_screen.naming = naming;
}

/* "port tags choose" (A on a row) */
static boolean tags_choose(struct widget_instance *widget)
{
	struct widget_instance *row = tag_row(widget);
	struct widget_instance *list = tags_list_of(widget);
	short place = tag_row_place(row);
	char reason[320];

	if (place == NONE)
		return FALSE;
	tags_screen.chosen[tags_screen.list] = place;
	tags_screen.message[0] = 0;
	tags_screen.shown[0] = 0;
	switch (tags_screen.list)
	{
	case _tags_list_top:
		if (place == _tags_top_injection)
		{
			config_write("game.tag_injection", config_boolean("game.tag_injection") ? "false" : "true");
			ui_play_audio_feedback_sound(SOUND_FORWARD);
		}
		else if (place == _tags_top_profiles)
		{
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			tags_list_open(list, _tags_list_profiles, (short)MAX(0, tags_screen.profile));
		}
		else if (place == _tags_top_multiplayer)
		{
			port_tag_mp_profiles_scan();
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			tags_list_open(list, _tags_list_mp_profiles, (short)MAX(0, tags_screen.mp_profile));
		}
		else if (place == _tags_top_campaign)
		{
			port_tag_campaign_profiles_scan();
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			tags_list_open(list, _tags_list_campaign_profiles, (short)MAX(0, tags_screen.campaign_profile));
		}
		else if (!tag_injector_maps_available(reason, sizeof(reason)))
		{
			tags_message(reason);
			ui_play_audio_feedback_sound(SOUND_ERROR);
		}
		else
		{
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			tags_list_show(list, _tags_list_kinds);
		}
		break;
	case _tags_list_profiles:
		tags_profile_choose(place);
		tags_screen.viewed_from = NONE;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		tags_list_open(list, _tags_list_profile_categories, 0);
		break;
	case _tags_list_profile_categories:
		/* (the range, played alone from the main menu: firing_range.c) */
		if (place == TAGS_FIRING_RANGE_ROW)
		{
			struct player_profile player;
			short controller = controller_of(widget);

			if (!main_menu_is_active())
			{
				tags_message("The firing range is opened from the main menu's settings.");
				ui_play_audio_feedback_sound(SOUND_ERROR);
				break;
			}
			if (!campaign_profile(controller, &player))
			{
				ui_play_audio_feedback_sound(SOUND_ERROR);
				break;
			}
			firing_range_request(port_tag_profiles_name(tags_screen.profile));
			campaign_start(firing_range_map_name(), main_get_difficulty(), controller, FALSE);
			return FALSE;
		}
		if (!tags_profile_category_count(place))
		{
			ui_play_audio_feedback_sound(SOUND_ERROR);
			break;
		}
		tags_screen.profile_category = place;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		tags_list_open(list, _tags_list_profile, 0);
		break;
	case _tags_list_profile:
		tags_screen.tag = tags_profile_tag_of(place);
		/* (the campaign maps are read the first time) */
		tags_screen.tag_stock = tag_injector_tag_stock(tags_screen.profile, tags_screen.tag);
		tags_screen.removing = NONE;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		tags_list_open(list, _tags_list_tag, 0);
		break;
	case _tags_list_tag:
		switch (tags_setting_of(place))
		{
		case _tags_setting_name:
		{
			char shown[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];

			tag_injector_tag_display_name(tags_screen.profile, tags_screen.tag, shown, sizeof(shown));
			tags_name_ask(_tags_naming_tag_name, shown, TAG_INJECTOR_CUSTOM_TYPE_LENGTH - 1);
			break;
		}
		case _tags_setting_custom_type:
		{
			char type[TAG_INJECTOR_CUSTOM_TYPE_LENGTH];

			tag_injector_tag_custom_type(tags_screen.profile, tags_screen.tag, type, sizeof(type));
			tags_name_ask(_tags_naming_custom_type, type, TAG_INJECTOR_CUSTOM_TYPE_LENGTH - 1);
			break;
		}
		case _tags_setting_remove:
		{
			char text[sizeof(tags_screen.message)];
			char const *name = tags_tag_shown(tags_screen.profile, tags_screen.tag);

			if (tags_screen.removing != tags_screen.tag)
			{
				tags_screen.removing = tags_screen.tag;
				snprintf(text, sizeof(text), "A again takes %s out of %s and deletes its bundle.", name,
					port_tag_profiles_name(tags_screen.profile));
				tags_message(text);
				ui_play_audio_feedback_sound(SOUND_CURSOR);
				break;
			}
			snprintf(text, sizeof(text), "Took %s out of %s.", name, port_tag_profiles_name(tags_screen.profile));
			tags_screen.removing = NONE;
			if (!port_tag_profiles_tag_remove(tags_screen.profile, tags_screen.tag))
			{
				tags_message("The profile cannot be written (the log says why).");
				ui_play_audio_feedback_sound(SOUND_ERROR);
				break;
			}
			tags_message(text);
			ui_play_audio_feedback_sound(SOUND_BACK);
			tags_screen.tag = NONE;
			/* (its kind's list, else the kinds with none of it left) */
			if (!tags_profile_category_count(tags_screen.profile_category))
				tags_list_show(list, _tags_list_profile_categories);
			else
				tags_list_open(list, _tags_list_profile, (short)MAX(0, MIN(tags_screen.chosen[_tags_list_profile],
					tags_profile_category_count(tags_screen.profile_category) - 1)));
			break;
		}
		default:
			tags_setting_step(tags_setting_of(place), 1);
			break;
		}
		break;
	case _tags_list_kinds:
		if (place == _tags_kind_profile)
		{
			if (tags_screen.profile == NONE)
				tags_message("No profiles: NEW PROFILE makes one.");
			else
				tags_profile_choose(tags_screen.profile + 1);
			ui_play_audio_feedback_sound(SOUND_CURSOR);
			break;
		}
		tags_screen.campaign = place == _tags_kind_campaign;
		tags_screen.folder[0] = 0;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		tags_list_open(list, _tags_list_browse, 0);
		break;
	case _tags_list_browse:
	case _tags_list_folder:
	{
		char name[CUSTOM_EDITION_MAP_CATEGORY_LENGTH];
		short flat;

		if (tags_browse_row(place, name, sizeof(name), &flat))
		{
			snprintf(tags_screen.folder, sizeof(tags_screen.folder), "%s", name);
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			tags_list_open(list, _tags_list_folder, 0);
		}
		else
		{
			tags_map_open(list, flat);
		}
		break;
	}
	case _tags_list_mp_profiles:
		tags_screen.mp_profile = place;
		tags_screen.mp_deleting = NONE;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		tags_list_open(list, _tags_list_mp_profile, 0);
		break;
	case _tags_list_mp_profile:
	{
		short weapon;
		short setting = tags_mp_setting_of(place, &weapon);

		if (setting == _tags_mp_setting_name)
		{
			char shown[TAG_PROFILE_NAME + 1];

			snprintf(shown, sizeof(shown), "%s", port_tag_mp_profiles_name(tags_screen.mp_profile));
			tags_name_ask(_tags_naming_mp_name, shown, TAG_PROFILE_NAME);
		}
		else if (setting == _tags_mp_setting_weapon)
		{
			tags_screen.mp_weapon = weapon;
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			tags_list_open(list, _tags_list_mp_manual, 0);
		}
		else
		{
			tags_mp_setting_step(setting, 1);
		}
		break;
	}
	case _tags_list_mp_manual:
		tags_mp_manual_toggle(place);
		break;
	case _tags_list_campaign_profiles:
		tags_screen.campaign_profile = place;
		tags_screen.campaign_deleting = NONE;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		tags_list_open(list, _tags_list_campaign_profile, 0);
		break;
	case _tags_list_campaign_profile:
		if (place == _tags_campaign_name)
		{
			char shown[TAG_PROFILE_NAME + 1];

			snprintf(shown, sizeof(shown), "%s", port_tag_campaign_profiles_name(tags_screen.campaign_profile));
			tags_name_ask(_tags_naming_campaign_name, shown, TAG_PROFILE_NAME);
		}
		else if (place == _tags_campaign_profile)
		{
			tags_campaign_profile_step(1);
		}
		else
		{
			tags_screen.campaign_enemies = place == _tags_campaign_enemies;
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			tags_list_open(list, tags_screen.campaign_enemies ? _tags_list_campaign_enemies : _tags_list_campaign_weapons,
				0);
		}
		break;
	case _tags_list_campaign_weapons:
	case _tags_list_campaign_enemies:
	{
		short stock;

		switch (tags_campaign_row_of(place, &stock))
		{
		case _tags_campaign_row_rotation:
			tags_campaign_value_step(FALSE, 1);
			break;
		case _tags_campaign_row_replace:
			tags_campaign_value_step(TRUE, 1);
			break;
		default:
			/* (a weapon's choices; an enemy's ranks, else its one slot) */
			tags_screen.campaign_stock = stock;
			tags_screen.campaign_rank = 0;
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			tags_list_open(list, !tags_screen.campaign_enemies ? _tags_list_campaign_manual :
				tag_campaign_enemy_rank_count(stock) ? _tags_list_campaign_ranks : _tags_list_campaign_slot, 0);
			break;
		}
		break;
	}
	case _tags_list_campaign_manual:
		tags_campaign_manual_toggle(place);
		break;
	case _tags_list_campaign_ranks:
		tags_screen.campaign_rank = tag_campaign_enemy_rank(tags_screen.campaign_stock, place);
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		tags_list_open(list, _tags_list_campaign_slot, 0);
		break;
	case _tags_list_campaign_slot:
		if (place == _tags_campaign_slot_replace && !tags_campaign_manual())
		{
			tags_message("ENEMIES' REPLACEMENT is RANDOM: MANUAL chooses each enemy's AI.");
			ui_play_audio_feedback_sound(SOUND_ERROR);
			break;
		}
		if (place == _tags_campaign_slot_loadout && !tag_campaign_enemy_armed(tags_screen.campaign_stock))
		{
			tags_message("It holds no weapons a loadout can choose.");
			ui_play_audio_feedback_sound(SOUND_ERROR);
			break;
		}
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		tags_list_open(list, place == _tags_campaign_slot_replace ? _tags_list_campaign_manual :
			_tags_list_campaign_loadout, 0);
		break;
	case _tags_list_campaign_loadout:
		tags_campaign_loadout_toggle(place);
		break;
	case _tags_list_categories:
		if (!tag_injector_category_count(place))
		{
			ui_play_audio_feedback_sound(SOUND_ERROR);
			break;
		}
		tags_screen.category = place;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		tags_list_open(list, _tags_list_tags, 0);
		break;
	default:
		if (!tag_injector_category_addable(tags_screen.category))
		{
			tags_message("The randomizer cannot play these yet: only weapons, AI and equipment can be added.");
			ui_play_audio_feedback_sound(SOUND_ERROR);
		}
		else if (tags_screen.profile == NONE)
		{
			tags_message("Make a profile first: NEW PROFILE.");
			ui_play_audio_feedback_sound(SOUND_ERROR);
		}
		else
		{
			long tag = tag_injector_tag_in_profile(tags_screen.profile, tags_screen.category, place);
			long carried = 0;
			char text[sizeof(tags_screen.message)];

			if (tag != NONE)
			{
				if (port_tag_profiles_tag_remove(tags_screen.profile, tag))
				{
					snprintf(text, sizeof(text), "Taken out of %s.", port_tag_profiles_name(tags_screen.profile));
					ui_play_audio_feedback_sound(SOUND_BACK);
				}
				else
				{
					snprintf(text, sizeof(text), "The profile cannot be written (the log says why).");
					ui_play_audio_feedback_sound(SOUND_ERROR);
				}
			}
			else if (tag_injector_tag_add(tags_screen.profile, tags_screen.category, place, &carried, reason,
				sizeof(reason)))
			{
				if (carried)
					snprintf(text, sizeof(text), "Added to %s, with %ld weapon%s and equipment it carries.",
						port_tag_profiles_name(tags_screen.profile), carried, carried == 1 ? "" : "s");
				else
					snprintf(text, sizeof(text), "Added to %s.", port_tag_profiles_name(tags_screen.profile));
				ui_play_audio_feedback_sound(SOUND_FORWARD);
			}
			else
			{
				snprintf(text, sizeof(text), "%s", reason[0] ? reason : "It cannot be taken out (the log says why).");
				ui_play_audio_feedback_sound(SOUND_ERROR);
			}
			tags_message(text);
			/* (a map that could not be read again is closed: back to the maps) */
			if (tag_injector_map_opened() == NONE)
				tags_list_show(list, tags_browse_list());
		}
		break;
	}
	return TRUE;
}

/* "port tags left" and "port tags right" (on a row): the tag injection
turned on or off, a tag on or off, a tag's setting's value before or after,
or the profile tags are added to */
static boolean tags_step(struct widget_instance *widget, short step)
{
	short place = tag_row_place(tag_row(widget));

	if (place == NONE)
		return TRUE;
	tags_screen.message[0] = 0;
	tags_screen.shown[0] = 0;
	switch (tags_screen.list)
	{
	case _tags_list_top:
		if (place != _tags_top_injection)
			return TRUE;
		config_write("game.tag_injection", config_boolean("game.tag_injection") ? "false" : "true");
		break;
	case _tags_list_profile:
	{
		long tag = tags_profile_tag_of(place);

		if (!port_tag_profiles_tag_set_enabled(tags_screen.profile, tag,
			!port_tag_profiles_tag_enabled(tags_screen.profile, tag)))
			tags_message("The profile cannot be written (the log says why).");
		break;
	}
	case _tags_list_tag:
	{
		short setting = tags_setting_of(place);

		if (setting == _tags_setting_name || setting == _tags_setting_custom_type || setting == _tags_setting_remove)
			return TRUE;
		tags_setting_step(setting, step);
		return TRUE;
	}
	case _tags_list_kinds:
		if (place != _tags_kind_profile || tags_screen.profile == NONE)
			return TRUE;
		tags_profile_choose(tags_screen.profile + step);
		break;
	case _tags_list_mp_profile:
	{
		short weapon;
		short setting = tags_mp_setting_of(place, &weapon);

		if (setting != _tags_mp_setting_name && setting != _tags_mp_setting_weapon)
			tags_mp_setting_step(setting, step);
		return TRUE;
	}
	case _tags_list_mp_manual:
		tags_mp_manual_toggle(place);
		return TRUE;
	case _tags_list_campaign_profile:
		if (place == _tags_campaign_profile)
			tags_campaign_profile_step(step);
		return TRUE;
	case _tags_list_campaign_weapons:
	case _tags_list_campaign_enemies:
	{
		short stock;
		short row = tags_campaign_row_of(place, &stock);

		if (row == _tags_campaign_row_rotation || row == _tags_campaign_row_replace)
			tags_campaign_value_step(row == _tags_campaign_row_replace, step);
		return TRUE;
	}
	case _tags_list_campaign_manual:
		tags_campaign_manual_toggle(place);
		return TRUE;
	case _tags_list_campaign_loadout:
		tags_campaign_loadout_toggle(place);
		return TRUE;
	default:
		return TRUE;
	}
	ui_play_audio_feedback_sound(SOUND_CURSOR);
	return TRUE;
}

/* "port tags back" (B, and BACK): up a list, else away */
static boolean tags_back(struct widget_instance *widget, boolean *widget_deleted)
{
	struct widget_instance *list = tags_list_of(widget);
	short up;

	ui_play_audio_feedback_sound(SOUND_BACK);
	tags_screen.message[0] = 0;
	tags_screen.removing = NONE;
	switch (tags_screen.list)
	{
	case _tags_list_profiles: up = _tags_list_top; break;
	case _tags_list_profile_categories:
		/* (back to the list VIEW PROFILE was pressed in) */
		up = tags_screen.viewed_from != NONE ? tags_screen.viewed_from : _tags_list_profiles;
		tags_screen.viewed_from = NONE;
		break;
	case _tags_list_profile: up = _tags_list_profile_categories; break;
	case _tags_list_tag: up = _tags_list_profile; break;
	case _tags_list_kinds: up = _tags_list_top; break;
	case _tags_list_browse: up = _tags_list_kinds; break;
	case _tags_list_folder: up = _tags_list_browse; break;
	case _tags_list_categories:
		tag_injector_map_close();
		tags_screen.map = NONE;
		up = tags_browse_list();
		break;
	case _tags_list_tags: up = _tags_list_categories; break;
	case _tags_list_mp_profiles: up = _tags_list_top; break;
	case _tags_list_mp_profile:
		tags_screen.mp_deleting = NONE;
		up = _tags_list_mp_profiles;
		break;
	case _tags_list_mp_manual: up = _tags_list_mp_profile; break;
	case _tags_list_campaign_profiles: up = _tags_list_top; break;
	case _tags_list_campaign_profile:
		tags_screen.campaign_deleting = NONE;
		up = _tags_list_campaign_profiles;
		break;
	case _tags_list_campaign_weapons:
	case _tags_list_campaign_enemies: up = _tags_list_campaign_profile; break;
	case _tags_list_campaign_manual:
		up = tags_screen.campaign_enemies ? _tags_list_campaign_slot : _tags_list_campaign_weapons;
		break;
	case _tags_list_campaign_ranks: up = _tags_list_campaign_enemies; break;
	case _tags_list_campaign_slot:
		up = tag_campaign_enemy_rank_count(tags_screen.campaign_stock) ? _tags_list_campaign_ranks :
			_tags_list_campaign_enemies;
		break;
	case _tags_list_campaign_loadout: up = _tags_list_campaign_slot; break;
	default:
		ui_widget_port_go_back(widget);
		*widget_deleted = TRUE;
		return TRUE;
	}
	if (up == _tags_list_browse)
		tags_screen.folder[0] = 0;
	tags_list_show(list, up);
	return TRUE;
}

/* the button bar's buttons as the list shown has them: none in the first
list; NEW PROFILE in TAG PROFILES; DUPLICATE PROFILE and DELETE PROFILE in a
profile's lists (the profile shown's); NEW PROFILE and VIEW <name> PROFILE
(the profile MAPS adds to) in MAPS' lists */
enum
{
	_tags_buttons_none,
	_tags_buttons_profiles,
	_tags_buttons_profile,
	_tags_buttons_maps,
	/* NEW PROFILE in MULTIPLAYER PROFILES; DELETE PROFILE in a multiplayer
	profile's lists */
	_tags_buttons_mp_profiles,
	_tags_buttons_mp_profile,
	/* NEW PROFILE in CAMPAIGN PROFILES; DELETE PROFILE in a campaign
	profile's lists */
	_tags_buttons_campaign_profiles,
	_tags_buttons_campaign_profile
};

static short tags_buttons(void)
{
	switch (tags_screen.list)
	{
	case _tags_list_top:
		return _tags_buttons_none;
	case _tags_list_profiles:
		return _tags_buttons_profiles;
	case _tags_list_profile_categories:
	case _tags_list_profile:
	case _tags_list_tag:
		return _tags_buttons_profile;
	case _tags_list_mp_profiles:
		return _tags_buttons_mp_profiles;
	case _tags_list_mp_profile:
	case _tags_list_mp_manual:
		return _tags_buttons_mp_profile;
	case _tags_list_campaign_profiles:
		return _tags_buttons_campaign_profiles;
	case _tags_list_campaign_profile:
	case _tags_list_campaign_weapons:
	case _tags_list_campaign_enemies:
	case _tags_list_campaign_manual:
	case _tags_list_campaign_ranks:
	case _tags_list_campaign_slot:
	case _tags_list_campaign_loadout:
		return _tags_buttons_campaign_profile;
	default:
		return _tags_buttons_maps;
	}
}

/* "port tags new profile": NEW PROFILE, a name asked for, then made; in a
profile's lists DUPLICATE PROFILE, a name asked for the copy
(tag_injector_profiles_poll) */
static boolean tags_new_profile(void)
{
	short buttons = tags_buttons();

	if (tags_screen.naming != _tags_naming_none || buttons == _tags_buttons_none ||
		buttons == _tags_buttons_mp_profile || buttons == _tags_buttons_campaign_profile)
		return TRUE;
	if (buttons == _tags_buttons_mp_profiles)
	{
		tags_name_ask(_tags_naming_mp_profile, NULL, TAG_PROFILE_NAME);
		return TRUE;
	}
	if (buttons == _tags_buttons_campaign_profiles)
	{
		tags_name_ask(_tags_naming_campaign_profile, NULL, TAG_PROFILE_NAME);
		return TRUE;
	}
	if (buttons == _tags_buttons_profile && tags_screen.profile != NONE)
	{
		char copy[TAG_PROFILE_NAME + 1];

		snprintf(copy, sizeof(copy), "%.*s 2", TAG_PROFILE_NAME - 2, port_tag_profiles_name(tags_screen.profile));
		tags_name_ask(_tags_naming_duplicate, copy, TAG_PROFILE_NAME);
		return TRUE;
	}
	tags_name_ask(_tags_naming_profile, NULL, TAG_PROFILE_NAME);
	return TRUE;
}

/* the profile DELETE PROFILE deletes: the one whose lists are shown; NONE
elsewhere */
static long tags_delete_target(void)
{
	return tags_buttons() == _tags_buttons_profile ? tags_screen.profile : NONE;
}

/* VIEW <name> PROFILE: the profile MAPS adds to shown, its kinds of tags, B
on them going back to the list it was pressed in (a map's, still open) */
static void tags_view_profile(struct widget_instance *widget)
{
	if (tags_screen.profile == NONE)
	{
		tags_message("No profiles: NEW PROFILE makes one.");
		ui_play_audio_feedback_sound(SOUND_ERROR);
		return;
	}
	tags_screen.viewed_from = tags_screen.list;
	tags_screen.message[0] = 0;
	tags_screen.removing = NONE;
	tags_screen.tag = NONE;
	tags_screen.chosen[_tags_list_profiles] = (short)tags_screen.profile;
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	tags_list_open(tags_list_of(widget), _tags_list_profile_categories, 0);
}

/* "port tags profile button": in a profile's lists DELETE PROFILE (once,
the question; again, the profile deleted with its tags), in MAPS' VIEW
<name> PROFILE */
static boolean tags_profile_button(struct widget_instance *widget)
{
	char text[sizeof(tags_screen.message)];
	long profile = tags_delete_target();

	if (tags_buttons() == _tags_buttons_maps)
	{
		tags_view_profile(widget);
		return TRUE;
	}
	/* (a campaign profile's: once the question, again its file deleted) */
	if (tags_buttons() == _tags_buttons_campaign_profile)
	{
		long campaign_profile = tags_screen.campaign_profile;

		if (tags_screen.campaign_deleting != campaign_profile)
		{
			tags_screen.campaign_deleting = campaign_profile;
			snprintf(text, sizeof(text), "DELETE PROFILE again deletes the campaign profile %s (not its tags' "
				"profile).", port_tag_campaign_profiles_name(campaign_profile));
			tags_message(text);
			ui_play_audio_feedback_sound(SOUND_CURSOR);
			return TRUE;
		}
		snprintf(text, sizeof(text), "Deleted %s.", port_tag_campaign_profiles_name(campaign_profile));
		if (!port_tag_campaign_profiles_delete(campaign_profile))
		{
			snprintf(text, sizeof(text), "%s cannot be deleted (the log says why).",
				port_tag_campaign_profiles_name(campaign_profile));
			ui_play_audio_feedback_sound(SOUND_ERROR);
		}
		else
		{
			ui_play_audio_feedback_sound(SOUND_BACK);
		}
		tags_screen.campaign_deleting = NONE;
		tags_screen.campaign_profile = NONE;
		tags_list_open(tags_list_of(widget), _tags_list_campaign_profiles, 0);
		tags_message(text);
		return TRUE;
	}
	/* (a multiplayer profile's: once the question, again its file deleted) */
	if (tags_buttons() == _tags_buttons_mp_profile)
	{
		long mp_profile = tags_screen.mp_profile;

		if (tags_screen.mp_deleting != mp_profile)
		{
			tags_screen.mp_deleting = mp_profile;
			snprintf(text, sizeof(text), "DELETE PROFILE again deletes the multiplayer profile %s (not its weapons' "
				"profile).", port_tag_mp_profiles_name(mp_profile));
			tags_message(text);
			ui_play_audio_feedback_sound(SOUND_CURSOR);
			return TRUE;
		}
		snprintf(text, sizeof(text), "Deleted %s.", port_tag_mp_profiles_name(mp_profile));
		if (!port_tag_mp_profiles_delete(mp_profile))
		{
			snprintf(text, sizeof(text), "%s cannot be deleted (the log says why).",
				port_tag_mp_profiles_name(mp_profile));
			ui_play_audio_feedback_sound(SOUND_ERROR);
		}
		else
		{
			ui_play_audio_feedback_sound(SOUND_BACK);
		}
		tags_screen.mp_deleting = NONE;
		tags_screen.mp_profile = NONE;
		tags_list_open(tags_list_of(widget), _tags_list_mp_profiles, 0);
		tags_message(text);
		return TRUE;
	}
	if (profile == NONE)
		return TRUE;
	if (tags_screen.deleting != profile)
	{
		tags_screen.deleting = profile;
		snprintf(text, sizeof(text), "DELETE PROFILE again deletes %s and its %ld tags. This cannot be undone.",
			port_tag_profiles_name(profile), port_tag_profiles_tag_count(profile));
		tags_message(text);
		ui_play_audio_feedback_sound(SOUND_CURSOR);
		return TRUE;
	}
	snprintf(text, sizeof(text), "Deleted %s.", port_tag_profiles_name(profile));
	if (!port_tag_profiles_delete(profile))
	{
		snprintf(text, sizeof(text), "%s cannot be deleted (the log says why).", port_tag_profiles_name(profile));
		ui_play_audio_feedback_sound(SOUND_ERROR);
	}
	else
	{
		ui_play_audio_feedback_sound(SOUND_BACK);
	}
	tags_profile_choose(0);
	tags_screen.tag = NONE;
	tags_screen.viewed_from = NONE;
	tags_list_open(tags_list_of(widget), _tags_list_profiles, 0);
	tags_message(text);
	return TRUE;
}

/* a button of the button bar shown with its words, or hidden (passed over by
the focus too) */
static void tags_button_show(struct widget_instance *button, wchar_t const *text)
{
	if (!button)
		return;
	visible_set(button, text != NULL);
	button->disabled = text == NULL;
	if (text)
		text_set(button, text);
}

/* the button bar as the list shown has it (tags_buttons), made again when
its words change */
static void tags_buttons_update(struct widget_instance *list)
{
	struct widget_instance *screen = screen_of(list);
	struct widget_instance *new_button;
	struct widget_instance *profile_button;
	short buttons = tags_buttons();
	char view[ROW_TEXT_LENGTH];
	char shown[sizeof(tags_screen.buttons_shown)];
	wchar_t wide[ROW_TEXT_LENGTH];

	/* (a long name cut short: the button is a third of the bar) */
	view[0] = 0;
	if (buttons == _tags_buttons_maps && tags_screen.profile != NONE)
	{
		char name[TAG_PROFILE_NAME + 1];

		upper_copy(name, sizeof(name), port_tag_profiles_name(tags_screen.profile));
		snprintf(view, sizeof(view), "VIEW %.14s%s PROFILE", name, strlen(name) > 14 ? "..." : "");
	}
	snprintf(shown, sizeof(shown), "%d %s", buttons, view);
	if (!strcmp(shown, tags_screen.buttons_shown))
		return;
	new_button = named(screen, "button_new", 0);
	profile_button = named(screen, "button_profile", 0);
	/* (not yet made, as the list is: done once they are) */
	if (!new_button || !profile_button)
		return;
	snprintf(tags_screen.buttons_shown, sizeof(tags_screen.buttons_shown), "%s", shown);
	tags_button_show(new_button, buttons == _tags_buttons_none || buttons == _tags_buttons_mp_profile ||
		buttons == _tags_buttons_campaign_profile ? NULL :
		buttons == _tags_buttons_profile ? L"DUPLICATE PROFILE" : L"NEW PROFILE");
	mods_wide_text(view, wide, ROW_TEXT_LENGTH);
	tags_button_show(profile_button, buttons == _tags_buttons_profile || buttons == _tags_buttons_mp_profile ||
		buttons == _tags_buttons_campaign_profile ? L"DELETE PROFILE" : view[0] ? wide : NULL);
}

/* the name typed, once the keyboard has closed: a profile made, and shown,
or the tag's custom type. Each frame from the main loop (main.c), as
randomizer_profiles_poll */
void tag_injector_profiles_poll(void)
{
	char name[TAG_INJECTOR_CUSTOM_TYPE_LENGTH + 1];
	char text[sizeof(tags_screen.message)];
	short naming = tags_screen.naming;
	long profile;
	short character;

	if (naming == _tags_naming_none || virtual_keyboard_active())
		return;
	tags_screen.naming = _tags_naming_none;
	for (character = 0; tags_screen.name[character] && character < TAG_INJECTOR_CUSTOM_TYPE_LENGTH; character++)
		name[character] = tags_screen.name[character] < 0x80 ? (char)tags_screen.name[character] : '?';
	name[character] = 0;
	if (!virtual_keyboard_last_exit_saved_text())
		return;
	if (naming == _tags_naming_campaign_name)
	{
		if (!port_tag_campaign_profiles_rename(tags_screen.campaign_profile, name))
			tags_message("A name needs a letter or a digit (the log says why).");
		tags_screen.shown[0] = 0;
		csmemset(tags_screen.row_shown, 0, sizeof(tags_screen.row_shown));
		return;
	}
	if (naming == _tags_naming_campaign_profile)
	{
		/* (playing the profile shown last, else the first) */
		long base = tags_screen.profile != NONE ? tags_screen.profile : port_tag_profiles_count() ? 0 : NONE;

		profile = port_tag_campaign_profiles_create(name, base != NONE ? port_tag_profiles_folder(base) : "");
		if (profile == NONE)
		{
			tags_message("A profile's name needs a letter or a digit (the log says why).");
			return;
		}
		tags_screen.campaign_profile = profile;
		tags_screen.chosen[_tags_list_campaign_profiles] = (short)profile;
		snprintf(text, sizeof(text), "Made %s, playing %s. A opens its settings.",
			port_tag_campaign_profiles_name(profile), base != NONE ? port_tag_profiles_name(base) : "no profile yet");
		tags_message(text);
		tags_screen.shown[0] = 0;
		csmemset(tags_screen.row_shown, 0, sizeof(tags_screen.row_shown));
		return;
	}
	if (naming == _tags_naming_mp_name)
	{
		if (!port_tag_mp_profiles_rename(tags_screen.mp_profile, name))
			tags_message("A name needs a letter or a digit (the log says why).");
		tags_screen.shown[0] = 0;
		csmemset(tags_screen.row_shown, 0, sizeof(tags_screen.row_shown));
		return;
	}
	if (naming == _tags_naming_mp_profile)
	{
		/* (playing the profile shown last, else the first) */
		long base = tags_screen.profile != NONE ? tags_screen.profile : port_tag_profiles_count() ? 0 : NONE;

		profile = port_tag_mp_profiles_create(name, base != NONE ? port_tag_profiles_folder(base) : "");
		if (profile == NONE)
		{
			tags_message("A profile's name needs a letter or a digit (the log says why).");
			return;
		}
		tags_screen.mp_profile = profile;
		tags_screen.chosen[_tags_list_mp_profiles] = (short)profile;
		snprintf(text, sizeof(text), "Made %s, playing %s. A opens its settings.", port_tag_mp_profiles_name(profile),
			base != NONE ? port_tag_profiles_name(base) : "no profile yet");
		tags_message(text);
		tags_screen.shown[0] = 0;
		csmemset(tags_screen.row_shown, 0, sizeof(tags_screen.row_shown));
		return;
	}
	if (naming == _tags_naming_custom_type)
	{
		if (!port_tag_profiles_tag_set_option(tags_screen.profile, tags_screen.tag, "custom_type", name))
			tags_message("The custom type cannot be kept (the log says why).");
		tags_screen.shown[0] = 0;
		return;
	}
	if (naming == _tags_naming_tag_name)
	{
		if (!port_tag_profiles_tag_set_option(tags_screen.profile, tags_screen.tag, "name", name))
			tags_message("The name cannot be kept (the log says why).");
		else
		{
			snprintf(text, sizeof(text), "It is %s now.", tags_tag_shown(tags_screen.profile, tags_screen.tag));
			tags_message(text);
		}
		tags_screen.shown[0] = 0;
		csmemset(tags_screen.row_shown, 0, sizeof(tags_screen.row_shown));
		return;
	}
	if (naming == _tags_naming_duplicate)
	{
		long original = tags_screen.profile;
		char original_name[TAG_PROFILE_NAME + 8];

		snprintf(original_name, sizeof(original_name), "%s", port_tag_profiles_name(original));
		profile = port_tag_profiles_duplicate(original, name);
		if (profile == NONE)
		{
			tags_message("The copy cannot be made: its name needs a letter or a digit, or the files could not be "
				"copied (the log says why).");
			return;
		}
		/* (the copy's lists shown, as the original's were) */
		tags_profile_choose(profile);
		tags_screen.chosen[_tags_list_profiles] = (short)profile;
		tags_screen.tag = NONE;
		if (tags_screen.list == _tags_list_tag)
			tags_screen.list = _tags_list_profile;
		snprintf(text, sizeof(text), "Made %s, a copy of %s.", port_tag_profiles_name(profile), original_name);
		tags_message(text);
		tags_screen.shown[0] = 0;
		csmemset(tags_screen.row_shown, 0, sizeof(tags_screen.row_shown));
		return;
	}
	profile = port_tag_profiles_create(name);
	if (profile == NONE)
	{
		tags_message("A profile's name needs a letter or a digit (the log says why).");
		return;
	}
	tags_profile_choose(profile);
	if (tags_screen.list == _tags_list_profiles)
		tags_screen.chosen[_tags_list_profiles] = (short)profile;
	snprintf(text, sizeof(text), "Made %s. Choose a map in MAPS, then its weapons or AI to add.",
		port_tag_profiles_name(profile));
	tags_message(text);
}

/* the profiles' tags an earlier build kept as bundles, stored one a frame
while the screen is open (tag_injector_profile_store_next), its word saying
how many are left and how much smaller the profiles are */
static void tags_store_step(void)
{
	static unsigned long saved = 0;
	static long stored = 0;
	unsigned long before;
	unsigned long after;
	char text[sizeof(tags_screen.message)];
	long left;

	if (!tag_injector_profiles_unstored() || !tag_injector_profile_store_next(&before, &after))
		return;
	if (before > after)
		saved += before - after;
	stored++;
	left = tag_injector_profiles_unstored();
	if (left)
		snprintf(text, sizeof(text), "Packing the profiles' tags: %ld left, %lu MB smaller so far.", left, saved >> 20);
	else
		snprintf(text, sizeof(text), "Packed %ld tags: the profiles are %lu MB smaller.", stored, saved >> 20);
	tags_message(text);
	tags_screen.shown[0] = 0;
}

/* What a tag the preview is of needs to work (tag_preview_needs), for the
lines under it: what it is missing, else that nothing is; nothing until it
is read. */
static void tags_needs_text(short state, struct tag_injector_needs const *needs, char *text, size_t text_size)
{
	short index;

	text[0] = 0;
	if (state == _tag_preview_needs_unreadable)
	{
		snprintf(text, text_size, "Its tags cannot be read.");
		return;
	}
	if (state != _tag_preview_needs_read || !needs->count)
		return;
	for (index = 0; index < needs->count; index++)
	{
		if (needs->needs[index].state != _tag_injector_need_missing)
			continue;
		snprintf(text + strlen(text), text_size - strlen(text), "%s%s", text[0] ? ", " : "MISSING: ",
			needs->needs[index].name);
	}
	/* (and what it is seen doing: the usability report, a weapon's and an
	actor variant's) */
	{
		struct tag_injector_usability const *usability = &needs->usability;
		boolean lacking = FALSE;
		size_t start;

		if (text[0])
			snprintf(text + strlen(text), text_size - strlen(text), ".");
		if (usability->third_person == _tag_injector_need_missing)
		{
			snprintf(text + strlen(text), text_size - strlen(text), "%sNo '%s' animations: %s", text[0] ? " " : "",
				usability->label, usability->actor ? "it holds its weapon wrong." :
				"the Spartan holds it as another weapon.");
		}
		start = strlen(text);
		for (index = 0; index < usability->count; index++)
		{
			char const *heading = usability->actor ? "CANNOT: " : "NO ANIMATION: ";

			if (usability->animations[index].state != _tag_injector_need_missing)
				continue;
			snprintf(text + strlen(text), text_size - strlen(text), "%s%s%s", strlen(text) > start ? ", " :
				text[0] ? " " : "", strlen(text) > start ? "" : heading, usability->animations[index].name);
			lacking = TRUE;
		}
		if (lacking)
			snprintf(text + strlen(text), text_size - strlen(text), ".");
		if (!text[0])
			snprintf(text, text_size, usability->count ? "Fully usable: nothing missing." : "No data missing.");
	}
}

/* a text's blank lines (" " between its parts) taken out */
static void tags_text_close_up(char *text)
{
	char *blank;

	while ((blank = strstr(text, "\n \n")) != NULL)
		memmove(blank + 1, blank + 3, strlen(blank + 3) + 1);
}

/* "port tags update": the rows (the list moving on at its ends while there
are more past them), each one's name (cut short where it is too long) and
state, and the details of the one chosen, the last thing done's word above
them for a while */
static void tags_update(struct widget_instance *list)
{
	struct widget_instance *row;
	struct widget_instance *focused = tag_row(list->focused_child);
	struct widget_instance *details = list->parameters.list.extended_description;
	short count;
	short focused_index = focused ? (short)(atoi(focused->name + 7) - 1) : NONE;
	short *first = &tags_screen.first[tags_screen.list];
	short *chosen = &tags_screen.chosen[tags_screen.list];
	boolean previewed;

	tags_store_step();
	/* (a profile made or deleted elsewhere, or by this screen; a tag taken
	out) */
	if (tags_screen.profile >= port_tag_profiles_count())
		tags_profile_choose(0);
	if ((tags_screen.list == _tags_list_profile_categories || tags_screen.list == _tags_list_profile ||
		tags_screen.list == _tags_list_tag) && tags_screen.profile == NONE)
		tags_list_show(list, _tags_list_profiles);
	if (tags_screen.list == _tags_list_tag && tags_screen.tag >= port_tag_profiles_tag_count(tags_screen.profile))
		tags_list_show(list, _tags_list_profile);
	if (tags_screen.list == _tags_list_profile && !tags_profile_category_count(tags_screen.profile_category))
		tags_list_show(list, _tags_list_profile_categories);
	/* (a multiplayer profile deleted) */
	if ((tags_screen.list == _tags_list_mp_profile || tags_screen.list == _tags_list_mp_manual) &&
		(tags_screen.mp_profile == NONE || tags_screen.mp_profile >= port_tag_mp_profiles_count()))
		tags_list_show(list, _tags_list_mp_profiles);
	if (tags_screen.list == _tags_list_mp_manual &&
		strcmp(port_tag_mp_profiles_option(tags_screen.mp_profile, "replace"), "manual"))
		tags_list_show(list, _tags_list_mp_profile);
	if (tags_screen.mp_deleting != NONE && tags_buttons() != _tags_buttons_mp_profile)
		tags_screen.mp_deleting = NONE;
	/* (a campaign profile deleted; MANUAL turned off) */
	if (tags_buttons() == _tags_buttons_campaign_profile &&
		(tags_screen.campaign_profile == NONE || tags_screen.campaign_profile >= port_tag_campaign_profiles_count()))
		tags_list_show(list, _tags_list_campaign_profiles);
	if (tags_screen.list == _tags_list_campaign_manual && !tags_campaign_manual())
		tags_list_show(list, tags_screen.campaign_enemies ? _tags_list_campaign_slot : _tags_list_campaign_weapons);
	if (tags_screen.campaign_deleting != NONE && tags_buttons() != _tags_buttons_campaign_profile)
		tags_screen.campaign_deleting = NONE;
	first = &tags_screen.first[tags_screen.list];
	chosen = &tags_screen.chosen[tags_screen.list];
	count = tags_count();
	if (focused_index == TAG_ROWS - 1 && *first + TAG_ROWS < count && list_scrolls_at_end())
	{
		(*first)++;
		focus_row(list, --focused_index);
	}
	else if (focused_index == 0 && *first > 0 && list_scrolls_at_end())
	{
		(*first)--;
		focus_row(list, ++focused_index);
	}
	if (*first > 0 && *first + TAG_ROWS > count)
		*first = (short)MAX(0, count - TAG_ROWS);
	if (focused_index != NONE && *first + focused_index < count)
		*chosen = (short)(*first + focused_index);
	if (tags_screen.deleting != NONE && tags_screen.deleting != tags_delete_target())
		tags_screen.deleting = NONE;
	tags_buttons_update(list);
	if (tags_screen.removing != NONE &&
		(tags_screen.list != _tags_list_tag || tags_setting_of(*chosen) != _tags_setting_remove))
		tags_screen.removing = NONE;
	for (row = list->child; row; row = row->next)
	{
		short place;
		short row_index;
		char name[ROW_TEXT_LENGTH];
		char state[ROW_TEXT_LENGTH];
		char shown[sizeof(tags_screen.row_shown[0])];

		if (strncmp(row->name, "op_tag_", 7))
			continue;
		row->animation.current_frame_index = row == focused ? 1 : 0;
		place = tag_row_place(row);
		row->visible = place != NONE;
		if (place == NONE)
		{
			if (row == focused)
				focus_row(list, count ? (short)(count - 1 - *first) : NONE);
			continue;
		}
		row_index = (short)PIN(atoi(row->name + 7) - 1, 0, TAG_ROWS - 1);
		tags_screen.row_failed[row_index] = tags_row_failed(place);
		tags_row_text(place, name, sizeof(name), state, sizeof(state));
		snprintf(shown, sizeof(shown), "%s|%s|%ld", name, state, mods_screen.fonts[_mod_row_name]);
		if (strcmp(shown, tags_screen.row_shown[row_index]))
		{
			char shortened[MOD_DETAILS_LENGTH];
			wchar_t wide[ROW_TEXT_LENGTH];

			if (mods_text_fits(_mod_row_name, name, strlen(name)))
				snprintf(shortened, sizeof(shortened), "%s", name);
			else
				mods_text_shorten(_mod_row_name, name, strlen(name), shortened, sizeof(shortened));
			mods_wide_text(shortened, wide, ROW_TEXT_LENGTH);
			text_set(named(row, "tag_name", 0), wide);
			mods_wide_text(state, wide, ROW_TEXT_LENGTH);
			text_set(named(row, "tag_state", 0), wide);
			snprintf(tags_screen.row_shown[row_index], sizeof(tags_screen.row_shown[0]), "%s", shown);
		}
	}
	if (tags_screen.message[0] && system_milliseconds() - tags_screen.message_time > TAG_MESSAGE_MILLISECONDS)
		tags_screen.message[0] = 0;
	/* (the tag chosen's model under its details: a map's tag, or a
	profile's) */
	previewed = count && (tags_screen.list == _tags_list_tags || tags_screen.list == _tags_list_profile);
	if (previewed && tags_screen.list == _tags_list_tags)
		tag_preview_map_tag(tags_screen.category, *chosen);
	else if (previewed)
		tag_preview_profile_tag(tags_screen.profile, tags_profile_tag_of(*chosen));
	else
		tag_preview_none();
	if (details)
	{
		char title[MOD_DETAILS_LENGTH];
		char text[MOD_DETAILS_LENGTH];
		char wrapped[MOD_DETAILS_LENGTH];
		char shown[sizeof(tags_screen.shown)];
		wchar_t wide[MOD_DETAILS_LENGTH];
		short line = 0;
		short lines = previewed ? TAG_PREVIEW_DETAILS_LINES : MOD_DETAILS_LINES;
		struct tag_injector_needs needs;
		short needs_state;

		/* (made again only when what they show, or its fonts, change) */
		needs_state = previewed ? tag_preview_needs(&needs) : _tag_preview_needs_waiting;
		snprintf(shown, sizeof(shown), "%d %ld %d %lu %ld %d %d %ld %lu %d %d %d %d %ld %ld %d %s %lu %d %ld %ld %ld "
			"%d %ld %d %ld %d %d %s",
			tags_screen.list, tags_screen.mp_profile, tags_screen.mp_weapon, port_tag_mp_profiles_scans(),
			tags_screen.campaign_profile, tags_screen.campaign_enemies, tags_screen.campaign_stock,
			(long)tags_screen.campaign_rank, port_tag_campaign_profiles_scans(), *chosen,
			count, tags_screen.category, tags_screen.profile_category, tags_screen.profile, tags_screen.tag,
			tags_screen.campaign,
			tags_screen.folder, port_tag_profiles_scans(), config_boolean("game.tag_injection"), tags_screen.removing,
			tags_screen.deleting, tags_screen.campaign_deleting, mods_screen.fonts[_mod_details_name],
			mods_screen.widths[_mod_details_name], mods_screen.fonts[_mod_details_text],
			mods_screen.widths[_mod_details_text], needs_state, tags_screen.message);
		if (strcmp(shown, tags_screen.shown))
		{
			snprintf(tags_screen.shown, sizeof(tags_screen.shown), "%s", shown);
			tags_details_text(count ? *chosen : NONE, title, sizeof(title), text, sizeof(text));
			wrapped[0] = 0;
			mods_wrap(title, wrapped, sizeof(wrapped), _mod_details_name, 2, 0);
			mods_wide_text(wrapped, wide, NUMBEROF(wide));
			text_set_length(named(details, "tag_details_name", 0), wide, NUMBEROF(wide));
			wrapped[0] = 0;
			/* (above a preview, without the blank lines between its parts) */
			if (previewed)
				tags_text_close_up(text);
			if (tags_screen.message[0])
			{
				line = mods_wrap(tags_screen.message, wrapped, sizeof(wrapped), _mod_details_text, lines, line);
				if (!previewed)
					line = mods_wrap(" ", wrapped, sizeof(wrapped), _mod_details_text, lines, line);
			}
			mods_wrap(text, wrapped, sizeof(wrapped), _mod_details_text, lines, line);
			mods_wide_text(wrapped, wide, NUMBEROF(wide));
			text_set_length(named(details, "tag_details_text", 0), wide, NUMBEROF(wide));
			/* (under the preview, what the tag needs to work: the details'
			measures, their box as wide) */
			tags_needs_text(needs_state, &needs, text, sizeof(text));
			wrapped[0] = 0;
			mods_wrap(text, wrapped, sizeof(wrapped), _mod_details_text, TAG_NEEDS_LINES, 0);
			mods_wide_text(wrapped, wide, NUMBEROF(wide));
			text_set_length(named(details, "tag_needs", 0), wide, NUMBEROF(wide));
		}
	}
}

/* ---------- Multiplayer (port/assets/menus' PLAN: Create Game > Internet
or LAN; Join Game > Server Browser (blank, for now), LAN, Direct Link): the
Xbox's networking, run by the engine's port entry points
(ui_widget_event_handler_functions.c) on our lists */

#define MAXIMUM_ADVERTISED_GAMES 9
#define GAMETYPE_ROWS 10
#define MAXIMUM_GAMETYPES 100
#define BROWSER_ROWS 15
/* the server browser's source tab's words and arrows while another item
has the focus (lobby_browser_source) */
#define TAB_UNFOCUSED_ALPHA 0.45f
#define LOBBY_ROWS 11
#define TEXT_FIELD_LENGTH 128
#define PLAYLIST_READ_ONLY_BIT 0x40000000UL
/* (a key stroke's modifier, as input_xbox.c has them: shift, control) */
#define KEY_MODIFIER_CONTROL_BIT 1
#define LOBBY_NAME "pc\\main_menu\\multiplayer_type_select\\lobby\\lobby_screen"
#define PREVIEW_NAME "pc\\main_menu\\multiplayer_type_select\\lobby\\preview_screen"
/* the server browser's password screen (tools/port_settings.py), and the
longest password */
#define PASSWORD_NAME "pc\\main_menu\\multiplayer_type_select\\join_game\\password\\password_screen"
#define PASSWORD_LENGTH 32
/* the lobby's panel's lines (the gametype, the players, the countdown, the
invite) */
#define LOBBY_TEXT_LENGTH (ROW_TEXT_LENGTH * 4)

enum
{
	_multiplayer_mode_server_browser,
	_multiplayer_mode_lan,
	_multiplayer_mode_direct_link,
	_multiplayer_mode_host_internet,
	_multiplayer_mode_host_lan,
	/* (Create Game's LOCAL: this machine alone, the bots' games) */
	_multiplayer_mode_host_local,
};

enum
{
	_client_state_searching,
	_client_state_joining,
	_client_state_pregame,
	_client_state_ingame,
	_client_state_postgame,
};

/* a game the client found, as network_client_manager.c has it */
struct advertised_game
{
	byte key_id[8];
	byte key[16];
	/* XNADDR: size, flags, abEnet (the port's machine identifier), ina */
	byte xnaddr[12];
	byte nonce[8];
	unsigned long update_time;
	wchar_t game_name[16];
	long map_version;
	char map_name[0x80];
	short engine_type;
	word machine_count;
	word player_count;
	short maximum_player_count;
	short unknown100;
	short platform;
	boolean open;
	boolean valid;
	boolean has_teams;
	boolean oddball_variant;
};

typedef char verify_advertised_game_platform_offset[offsetof(struct advertised_game, platform) == 0xDE ? 1 : -1];
typedef char verify_advertised_game_open_offset[offsetof(struct advertised_game, open) == 0xE0 ? 1 : -1];

/* the engine's (port) */
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
boolean ui_widget_port_multiplayer_map_choose(short level_index);
boolean ui_widget_port_cooperative_level_choose(char const *map_name, short difficulty);
short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);
boolean ui_widget_port_gametype_choose(long profile_index);
boolean ui_widget_port_host(struct widget_instance *widget, struct event_record *event, boolean *widget_deleted);
boolean ui_widget_port_host_local(struct widget_instance *widget, struct event_record *event, boolean *widget_deleted);
boolean ui_widget_port_browse(struct widget_instance *widget, struct event_record *event, boolean *widget_deleted);
boolean ui_widget_port_open(struct widget_instance *widget, char const *name, boolean *widget_deleted);
boolean network_game_client_advertised_game_in_progress(void *client, struct advertised_game *game);
byte network_game_client_advertised_game_kind(void *client, struct advertised_game *game);
boolean network_game_client_advertised_game_dedicated(void *client, struct advertised_game *game);
void network_game_client_tell_version(unsigned int theirs, boolean distributed);
boolean ui_widget_port_join(struct widget_instance *widget, void *advertised_game, char const *lobby_name,
	boolean *widget_deleted);
boolean ui_widget_port_multiplayer_player(short controller_index, long profile_index);
boolean ui_widget_port_unjoin_player(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted);
void network_game_server_port_set_settings(wchar_t const *name, long maximum_players);
void network_game_server_port_set_cooperative_friendly_fire(short friendly_fire);
void network_game_server_port_set_cooperative_player_collisions(boolean player_collisions);
void network_game_server_port_set_cooperative_ally_friendly_fire(boolean ally_friendly_fire);
void *global_network_game_client_get(void);
void *global_network_game_server_get(void);
boolean network_game_is_splitscreen_local(void);
struct advertised_game *network_game_client_get_available_games(void *client);
boolean network_game_client_advertised_game_is_valid(struct advertised_game *game);
short network_game_client_get_state(void *client, short *state_data);
struct network_game *network_game_client_get_game(void *client);
struct network_game *network_game_server_get_game(void *server);
short network_game_client_get_local_machine_index(void);
short network_game_client_get_seconds_to_game_start(void *client);
boolean network_player_is_valid(struct network_player *player);
boolean playlist_profile_get(long index, struct game_variant *variant);
boolean playlist_profile_get_display_name(long index, wchar_t *name);
boolean playlist_profile_get_options(long index, struct game_variant_options *options);
boolean input_get_key(struct key_stroke *key);
boolean game_engine_running(void);
void game_engine_end_game(void);
/* the platform layer's */
int p2p_join_invite(char const *text);
int p2p_join_listing(struct p2p_listing const *listing);
int p2p_invite_link(char *link, int size);
void p2p_set_hosting_allowed(int allowed);
int p2p_peer_address(unsigned char const *identifier, unsigned long *address);
int platform_clipboard_get(char *text, int size);
void platform_clipboard_set(char const *text);
void platform_text_field(int typing, int password);
int config_boolean(char const *name);
void ui_widget_port_post_button(short controller_index, short button_index);

static wchar_t const *const engine_names[] = { L"", L"CTF", L"SLAYER", L"ODDBALL", L"KING OF THE HILL", L"RACE" };
static short const maximum_players[] = { 2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128 };
/* the most players a game hosted here may have: on the Switch 48, which the
console was measured to hold (a LAN game of 48 machines, its socket budget
and its frames: port/switch/host/host_main.c, host_net.c); elsewhere 128 */
#ifdef HALO_SWITCH
#define HOSTED_PLAYERS_LIMIT 48
#else
#define HOSTED_PLAYERS_LIMIT 128
#endif

static struct
{
	short mode;
	/* Create Game's gametype list */
	long gametypes[MAXIMUM_GAMETYPES];
	short gametype_count;
	/* (those of the bank the list's spinner shows: STANDARD, OPENCE, CUSTOM) */
	short bank[MAXIMUM_GAMETYPES], bank_count, bank_shown;
	short gametype_first, gametype_chosen;
	/* the server settings */
	wchar_t game_name[16];
	short maximum_players_index;
	/* (co-op's own, so it leaves multiplayer's as it was: each game hosted
	starts with COOPERATIVE_DEFAULT_PLAYERS players at most) */
	short cooperative_maximum_players_index;
	boolean cooperative_maximum_players_set;
	/* the co-op game's FRIENDLY FIRE, and its PLAYER COLLISIONS OFF, as it
	starts (CO-OP OPTIONS': network.coop_friendly_fire's and
	network.coop_player_collisions': server_start) */
	short cooperative_friendly_fire;
	boolean cooperative_no_player_collisions;
	/* the browser's games */
	struct advertised_game *games[MAXIMUM_ADVERTISED_GAMES];
	short game_count, game_chosen;
	/* the lobby's */
	short lobby_first;
	/* the game under way whose lobby is shown before joining it: its key
	(the client's slot it is in may come to hold another game) */
	byte preview_key_id[8];
	byte preview_xnaddr[12];
	/* Server Setup's LISTING: PRIVATE (each new game starts with
	network.host_public's choice: PUBLIC unless set otherwise) */
	boolean game_private;
	/* Server Setup's PASSWORD (a public internet game's; empty: none), kept
	while the game runs, never written down */
	char game_password[PASSWORD_LENGTH + 1];
	/* whether the game's TAG PROFILE screen was shown (between the gametype
	and Server Setup, whose TAG PROFILE row shows its choice), and the first
	of its rows shown */
	boolean tag_profile_shown;
	short tag_profile_first;
} multiplayer = { .maximum_players_index = NONE };

/* ---- a text field (Direct Link's link, the game's name): the keyboard
types into it (Ctrl+V pastes), its row's A (enter) is done, B (escape)
cancels */
static struct
{
	struct widget_instance *row;
	char text[TEXT_FIELD_LENGTH];
	char before[TEXT_FIELD_LENGTH];
	short maximum;
	void (*done)(char const *text);
	/* a password's: shown as stars (text_field_begin_masked) */
	boolean masked;
} text_field;

/* when the field was last shown (a field not shown for a while is let go
of: its screen has gone) */
static unsigned long text_field_shown_time;

static boolean text_field_editing(struct widget_instance *row)
{
	return text_field.row && (!row || text_field.row == row);
}

static void text_field_open(struct widget_instance *row, char const *text, short maximum,
	void (*done)(char const *text), boolean masked)
{
	struct key_stroke key;

	text_field.row = row;
	snprintf(text_field.text, sizeof(text_field.text), "%s", text);
	snprintf(text_field.before, sizeof(text_field.before), "%s", text);
	text_field.maximum = (short)MIN(maximum, TEXT_FIELD_LENGTH - 1);
	text_field.done = done;
	text_field.masked = masked;
	text_field_shown_time = system_milliseconds();
	while (input_get_key(&key))
		;
	platform_text_field(TRUE, masked);
}

static void text_field_begin(struct widget_instance *row, char const *text, short maximum,
	void (*done)(char const *text))
{
	text_field_open(row, text, maximum, done, FALSE);
}

/* a password's field: as text_field_begin, its text shown as stars */
static void text_field_begin_masked(struct widget_instance *row, char const *text, short maximum,
	void (*done)(char const *text))
{
	text_field_open(row, text, maximum, done, TRUE);
}

static void text_field_end(boolean keep)
{
	void (*done)(char const *text) = text_field.done;

	platform_text_field(FALSE, FALSE);
	text_field.row = NULL;
	text_field.done = NULL;
	if (!keep)
		snprintf(text_field.text, sizeof(text_field.text), "%s", text_field.before);
	else if (done)
		done(text_field.text);
}

static void text_field_insert(char const *text)
{
	size_t length = strlen(text_field.text);

	for (; *text && length < (size_t)text_field.maximum; text++)
	{
		if (*text >= ' ' && *text <= '~')
			text_field.text[length++] = *text;
	}
	text_field.text[length] = 0;
}

/* the keys typed since the last frame, and the field shown (a caret
blinking while it is edited) */

static void text_field_show(struct widget_instance *value, char const *text, boolean editing)
{
	wchar_t shown[TEXT_FIELD_LENGTH + 2];
	short index;

	if (editing)
	{
		struct key_stroke key;

		text_field_shown_time = system_milliseconds();
		while (input_get_key(&key))
		{
			size_t length = strlen(text_field.text);

			if (key.key_code == _key_backspace)
			{
				if (length)
					text_field.text[length - 1] = 0;
			}
			else if (TEST_FLAG(key.modifier_flags, KEY_MODIFIER_CONTROL_BIT) && key.key_code == _key_v)
			{
				char clipboard[TEXT_FIELD_LENGTH];

				if (platform_clipboard_get(clipboard, sizeof(clipboard)))
					text_field_insert(clipboard);
			}
			else if ((unsigned char)key.ascii_code >= ' ' && (unsigned char)key.ascii_code <= '~')
			{
				char character[2] = { key.ascii_code, 0 };

				text_field_insert(character);
			}
		}
		text = text_field.text;
	}
	for (index = 0; text[index] && index < TEXT_FIELD_LENGTH; index++)
		shown[index] = editing && text_field.masked ? L'*' : (wchar_t)(unsigned char)text[index];
	if (editing && (system_milliseconds() / 500) % 2)
		shown[index++] = L'_';
	shown[index] = 0;
	text_set(value, shown);
	/* (what is being typed shown as it is: streamer mode hides nothing of it,
	text_filter.c) */
	if (editing && value)
		text_filter_exempt(value->parameters.text_box.text);
	else if (!text_field_editing(NULL))
		text_filter_exempt(NULL);
}

/* ---- the menu */

/* "mp type set mode": the item's (by its name) */
static void multiplayer_mode_set(struct widget_instance *widget)
{
	char const *name = widget->name;

	/* (an instance's name is cut to 31 characters:
	"multiplayer_type_create_interne") */
	multiplayer.mode =
		strstr(name, "create_inter") ? _multiplayer_mode_host_internet :
		strstr(name, "create_lan") ? _multiplayer_mode_host_lan :
		strstr(name, "create_local") ? _multiplayer_mode_host_local :
		strstr(name, "join_lan") ? _multiplayer_mode_lan :
		strstr(name, "join_direct") ? _multiplayer_mode_direct_link : _multiplayer_mode_server_browser;
}

/* player 1's profile for the controller's player in a network game */
static boolean multiplayer_player(short controller)
{
	struct player_profile profile;

	if (!campaign_profile(controller, &profile))
		return campaign_fail();
	return ui_widget_port_multiplayer_player(controller, player_ui_get_active_player_profile_index(0));
}

/* Create Game's ("join controller to mp game", first): player 1 in, and
the server made: an internet one (an invite, Discord), a LAN one, or a
LOCAL one, which no other machine joins (the bots': bots.c) */
static boolean multiplayer_host(struct widget_instance *widget, struct event_record *event, short controller,
	boolean *widget_deleted)
{
	multiplayer_mode_set(widget);
	if (!multiplayer_player(controller))
		return FALSE;
	p2p_set_hosting_allowed(multiplayer.mode == _multiplayer_mode_host_internet);
	/* (in the server browser, if PUBLIC: Server Setup's LISTING, which a
	new game starts with as network.host_public says) */
	multiplayer.game_private = !config_boolean("network.host_public");
	p2p_set_hosting_public(multiplayer.mode == _multiplayer_mode_host_internet && !multiplayer.game_private);
	multiplayer.cooperative_maximum_players_set = FALSE;
	/* (no Tag Injector profile until the TAG PROFILE screen chooses one: a
	game for every build) */
	multiplayer.tag_profile_shown = FALSE;
	tag_multiplayer_game_profile_set(NONE);
	if (multiplayer.mode == _multiplayer_mode_host_local)
		return ui_widget_port_host_local(widget, event, widget_deleted);
	return ui_widget_port_host(widget, event, widget_deleted);
}

/* ---- the map list (the Map screen's): its chooser's SINGLEPLAYER levels,
MULTIPLAYER maps, CUSTOM SINGLEPLAYER or CUSTOM MULTIPLAYER maps (the map
lists', above). A level or Custom Edition campaign map chosen lists the
difficulties (B goes back to the map) and is hosted as a network co-op game,
which goes to Server Setup; a multiplayer map goes on to the gametypes.
Split screen hosts only multiplayer maps, its chooser kept to the
multiplayer kinds. */

enum
{
	MAP_STEP_MAPS,
	MAP_STEP_DIFFICULTIES,
};

static struct
{
	short kind, step;
	/* hosting over the network, which co-op needs */
	boolean hosting;
	/* the multiplayer maps (ui_widget_port_multiplayer_maps): the Xbox's,
	then the Custom Edition ones */
	short map_count;
	short first, chosen;
	/* the level or Custom Edition campaign map (the kind's entry) whose
	difficulties are listed */
	short level;
	/* the custom folder open, or empty at the top; category_entry is that
	folder's row, focused on the way out */
	char category[CUSTOM_EDITION_MAP_CATEGORY_LENGTH];
	short category_entry;
} map_list;

#define SERVER_SETUP_NAME "pc\\main_menu\\multiplayer_type_select\\server_settings\\server_settings_screen"

static short map_step_count(void)
{
	if (map_list.step == MAP_STEP_DIFFICULTIES)
		return NUMBER_OF_GAME_DIFFICULTY_LEVELS;
	return map_kind_count(map_list.kind, map_list.map_count, map_list.category);
}

/* opens a step at the entry given */
static void map_step_open(struct widget_instance *list, short step, short entry)
{
	short count;

	map_list.step = step;
	count = map_step_count();
	map_list.chosen = (short)PIN(entry, 0, MAX(count - 1, 0));
	map_list.first = map_kind_first(map_list.chosen, count);
	map_kind_focus(list, map_list.first, map_list.chosen);
}

static void map_level_text(short level, wchar_t *text)
{
	string_get("pc\\main_menu\\map_list", level, text);
}

static void map_difficulty_text(short difficulty, wchar_t *text)
{
	string_get("pc\\main_menu\\player_profiles_select\\difficulty_names", difficulty, text);
}

static void map_list_custom_text(short entry, wchar_t *text)
{
	map_browse_text(map_list.kind == MAP_KIND_CUSTOM_SINGLEPLAYER, map_list.category, entry, text);
}

static boolean map_list_custom(void)
{
	return map_list.kind == MAP_KIND_CUSTOM_SINGLEPLAYER || map_list.kind == MAP_KIND_CUSTOM_MULTIPLAYER;
}

/* choosing a folder lists its maps */
static void map_folder_open(struct widget_instance *list, short entry)
{
	boolean campaign = map_list.kind == MAP_KIND_CUSTOM_SINGLEPLAYER;
	char const *name = map_browse_folder_name(campaign, map_list.category, entry);

	map_list.category_entry = entry;
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	csstrncpy(map_list.category, name, sizeof(map_list.category) - 1);
	map_list.category[sizeof(map_list.category) - 1] = 0;
	map_browse_forget();
	map_list.step = MAP_STEP_MAPS;
	map_list.chosen = 0;
	map_list.first = 0;
	map_kind_focus(list, 0, 0);
}

/* difficulties, then an open folder, back to the list they were chosen
from. FALSE when Back should leave the screen. */
static boolean map_folder_back(struct widget_instance *list)
{
	short entry;

	if (map_list.step == MAP_STEP_DIFFICULTIES)
	{
		map_step_open(list, MAP_STEP_MAPS, map_list.level);
		return TRUE;
	}
	if (!map_list.category[0] || !map_list_custom())
		return FALSE;
	entry = map_list.category_entry;
	map_list.category[0] = 0;
	map_browse_forget();
	map_step_open(list, MAP_STEP_MAPS, entry);

	return TRUE;
}

/* "mp level list initialize": on the multiplayer map used last */
static boolean map_list_initialize(struct widget_instance *list)
{
	char const *const *names;
	short last_used = 0;

	map_list.hosting = global_network_game_server_get() != NULL && !network_game_is_splitscreen_local();
	/* (the maps folders looked for again as the list opens) */
	custom_edition_maps_look_again();
	map_browse_forget();
	map_list.map_count = ui_widget_port_multiplayer_maps(&names, &last_used);
	daily_entries_forget();
	/* (a Custom Edition map's kind, when it was one) */
	map_list.kind = MAP_KIND_MULTIPLAYER;
	map_list.category[0] = 0;
	map_list.category_entry = 0;
	map_list.step = MAP_STEP_MAPS;
	if (last_used >= xbox_multiplayer_map_count(map_list.map_count))
	{
		char const *category;
		short flat;

		map_list.kind = MAP_KIND_CUSTOM_MULTIPLAYER;
		flat = (short)(last_used - xbox_multiplayer_map_count(map_list.map_count));
		category = custom_edition_maps_category(FALSE, flat);
		last_used = flat;
		if (category && category[0])
		{
			short folder_row = map_browse_find_folder(FALSE, category);
			short view;

			csstrncpy(map_list.category, category, sizeof(map_list.category) - 1);
			map_list.category[sizeof(map_list.category) - 1] = 0;
			map_list.category_entry = folder_row == NONE ? 0 : folder_row;
			view = map_browse_find(FALSE, map_list.category, flat);
			last_used = view == NONE ? 0 : view;
		}
	}
	map_kind_set(list, map_list.kind);
	map_step_open(list, MAP_STEP_MAPS, last_used);
	return TRUE;
}

/* "mp map list update": the chooser's kind, the step's rows, and beside
them the level or map chosen: its picture, name and words */
static void map_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	short kind = map_kind_shown(list, map_list.hosting, map_list.kind), count, entry;
	short shown;
	short display = NONE;
	boolean custom = kind == MAP_KIND_CUSTOM_SINGLEPLAYER || kind == MAP_KIND_CUSTOM_MULTIPLAYER;

	daily_overlays_hide(description);
	if (kind != map_list.kind)
	{
		map_list.kind = kind;
		map_list.step = MAP_STEP_MAPS;
		map_list.category[0] = 0;
		map_list.first = map_list.chosen = 0;
	}
	count = map_step_count();
	entry = map_kind_rows_update(list, &map_list.first, count, map_list.step == MAP_STEP_DIFFICULTIES ?
		map_difficulty_text : kind == MAP_KIND_SINGLEPLAYER ? map_level_text : kind == MAP_KIND_DAILY ? daily_entry_text :
		kind == MAP_KIND_MULTIPLAYER ? multiplayer_map_text : map_list_custom_text);
	if (entry != NONE)
		map_list.chosen = entry;
	/* (the daily challenges: the one chosen's level and what it is) */
	if (kind == MAP_KIND_DAILY)
	{
		daily_entries_want(map_list.chosen);
		visible_set(named(description, "mp_map_right_item", 0), TRUE);
		daily_entry_description(description, map_list.chosen);
		profile_name_show(description);
		return;
	}
	/* (the map whose difficulties are listed, else the one chosen) */
	shown = map_list.step == MAP_STEP_DIFFICULTIES ? map_list.level : map_list.chosen;
	if (kind == MAP_KIND_SINGLEPLAYER)
	{
		visible_set(named(description, "mp_map_right_item", 0), map_list.chosen < count);
		map_description_show(description, shown, NONE);
		level_description(description, "replay_level", shown, FALSE, NULL, NONE);
	}
	else
	{
		if (custom)
		{
			short flat = map_browse_flat(kind == MAP_KIND_CUSTOM_SINGLEPLAYER, map_list.category, shown);

			if (flat != NONE && (map_list.step == MAP_STEP_DIFFICULTIES || map_list.chosen < count))
				display = custom_edition_maps_display_index_of(kind == MAP_KIND_CUSTOM_SINGLEPLAYER, flat);
		}
		else if (map_list.step == MAP_STEP_DIFFICULTIES || map_list.chosen < count)
		{
			display = map_kind_display_index(kind, shown);
		}
		visible_set(named(description, "mp_map_right_item", 0), display != NONE);
		map_description_show(description, NONE, display);
	}
	profile_name_show(description);
}

/* "mp level select" (the list's OK): the map chosen, or a level's
difficulties, or the level at the difficulty chosen. FALSE stays on the Map
screen (the gametypes open on TRUE). */
static boolean map_list_choose(struct widget_instance *list, boolean *widget_deleted)
{
	short chosen;
	boolean custom_campaign;
	short flat = NONE;

	map_list_update(list);
	chosen = map_list.chosen;
	custom_campaign = map_list.kind == MAP_KIND_CUSTOM_SINGLEPLAYER;

	if (chosen >= map_step_count())
		return campaign_fail();
	/* (a daily challenge: the co-op game set up at its level and difficulty,
	its settings the lobby's, which Server Setup cannot change; anything
	else chosen, the lobby's own again) */
	if (map_list.kind == MAP_KIND_DAILY)
	{
		char const *map_name;
		short difficulty;

		if (!daily_entry_begin(chosen, &map_name, &difficulty))
			return campaign_fail();
		if (!ui_widget_port_cooperative_level_choose(map_name, difficulty))
		{
			community_daily_cancel();
			return campaign_fail();
		}
		return ui_widget_port_open(list, SERVER_SETUP_NAME, widget_deleted);
	}
	community_daily_cancel();
	if (map_list.step == MAP_STEP_MAPS && map_list_custom() &&
		map_browse_folder(custom_campaign, map_list.category, chosen))
	{
		map_folder_open(list, chosen);
		return FALSE;
	}
	if (map_list_custom())
		flat = map_browse_flat(custom_campaign, map_list.category,
			map_list.step == MAP_STEP_DIFFICULTIES ? map_list.level : chosen);
	if (map_list.step == MAP_STEP_DIFFICULTIES)
	{
		char const *map_name = map_list.kind == MAP_KIND_SINGLEPLAYER ? main_get_solo_level_name(map_list.level) :
			custom_map_level_name(map_list.kind, flat);

		/* the co-op game set up, then Server Setup in the gametypes' place */
		if (!ui_widget_port_cooperative_level_choose(map_name, chosen))
			return campaign_fail();
		return ui_widget_port_open(list, SERVER_SETUP_NAME, widget_deleted);
	}
	/* (a multiplayer map: the Xbox's, then the Custom Edition ones, in the
	multiplayer maps) */
	if (map_list.kind == MAP_KIND_MULTIPLAYER)
		return ui_widget_port_multiplayer_map_choose(chosen);
	if (map_list.kind == MAP_KIND_CUSTOM_MULTIPLAYER)
		return ui_widget_port_multiplayer_map_choose((short)(xbox_multiplayer_map_count(map_list.map_count) + flat));
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	map_list.level = chosen;
	map_step_open(list, MAP_STEP_DIFFICULTIES, (short)PIN(main_get_difficulty(), 0, NUMBER_OF_GAME_DIFFICULTY_LEVELS - 1));
	return FALSE;
}

/* "port map list back" (B on the list): from the difficulties back to the
level, from a folder back to the folders, else out of the Map screen */
static boolean map_list_back(struct widget_instance *list, boolean *widget_deleted)
{
	ui_play_audio_feedback_sound(SOUND_BACK);
	if (map_folder_back(list))
		return TRUE;
	ui_widget_port_go_back(list);
	*widget_deleted = TRUE;
	return TRUE;
}

/* port: the screen around a custom map list has no B handler, so this runs
before that screen closes (ui_widget.c). TRUE steps out of a folder, or the
Map screen's difficulties, and the screen stays. */
unsigned char menu_port_back(void *screen)
{
	struct widget_instance *widget = screen;
	struct widget_instance *list;

	list = named(widget, "mp_map_select_list_2", 0);
	if (list)
		return map_folder_back(list) ? 1 : 0;
	list = named(widget, "solo_level_select_list", 0);
	if (list)
		return level_folder_back(list) ? 1 : 0;

	return 0;
}

/* (the gametype editor's: Server Setup's copy of the game's gametype) */
static void gametype_setup_begin(void);
static void gametype_setup_end(void);
static boolean gametype_setup_apply(void);
static void gametype_setup_type(wchar_t *text);

/* ---- the gametype list: its spinner's bank (STANDARD: the Xbox's built-in
ones, OPENCE: OpenCE's built-in ones, game_modes.c, CUSTOM: those saved) in
its first row, then the bank's gametypes */

enum
{
	GAMETYPE_BANK_STANDARD = 0,
	GAMETYPE_BANK_OPENCE,
	GAMETYPE_BANK_CUSTOM,
	NUMBER_OF_GAMETYPE_BANKS
};

/* a gametype's bank */
static short gametype_bank(long profile_index)
{
	struct game_variant variant;
	struct game_variant_options options;

	if (!((unsigned long)profile_index & PLAYLIST_READ_ONLY_BIT))
		return GAMETYPE_BANK_CUSTOM;
	if (playlist_profile_get(profile_index, &variant) && playlist_profile_get_options(profile_index, &options) &&
		game_modes_variant_is_opence(&variant, &options))
	{
		return GAMETYPE_BANK_OPENCE;
	}
	return GAMETYPE_BANK_STANDARD;
}

/* what the gametype lists (their own and Edit Gametypes') show of the
gametypes, read once rather than every frame: the settings of the one whose
rules are shown (a saved gametype's are its file on the card, opened, read
and its checksum worked out), and the rows' names (each read from the saved
games' index file, z:\saved\hdmu.map, opened for every name: ten rows of
them took some 50 ms a frame on the Switch) */
static struct
{
	long profile;
	boolean valid;
	struct game_variant variant;
	short name_count;
	long name_profiles[MAXIMUM_GAMETYPES];
	wchar_t names[MAXIMUM_GAMETYPES][ROW_TEXT_LENGTH];
} gametype_shown = { NONE };

static struct game_variant const *gametype_shown_get(long profile)
{
	if (profile != gametype_shown.profile)
	{
		gametype_shown.profile = profile;
		gametype_shown.valid = playlist_profile_get(profile, &gametype_shown.variant);
	}
	return gametype_shown.valid ? &gametype_shown.variant : NULL;
}

/* (read again next time: the lists read again, or a gametype saved) */
static void gametype_shown_forget(void)
{
	gametype_shown.profile = NONE;
	gametype_shown.name_count = 0;
}

static void gametype_bank_read(short bank)
{
	short index;

	multiplayer.bank_shown = bank;
	multiplayer.bank_count = 0;
	for (index = 0; index < multiplayer.gametype_count; index++)
	{
		if (gametype_bank(multiplayer.gametypes[index]) == bank)
			multiplayer.bank[multiplayer.bank_count++] = index;
	}
	multiplayer.gametype_first = 0;
	multiplayer.gametype_chosen = 0;
	gametype_shown_forget();
}

/* a gametype's kind and rules, for the description beside a list: its game
engine's name (or its OpenCE gametype's), teams, and what wins */
static void gametype_rules_text(long profile_index, wchar_t *text, short size)
{
	struct game_variant variant;
	struct game_variant_options options;
	wchar_t const *kind;
	long engine;

	text[0] = 0;
	if (!playlist_profile_get(profile_index, &variant))
		return;
	if (!playlist_profile_get_options(profile_index, &options))
		game_variant_options_default(&variant, &options);
	engine = PIN(variant.game_engine_index, 0, 5);
	kind = game_modes_variant_kind_name(&variant, &options);
	switch (game_modes_variant_mode(&variant, &options))
	{
	case _game_mode_infection:
		if (options.time_limit > 0)
			usnprintf(text, size - 1, L"%s\r\nTeams\r\nRound time: %d min", kind, options.time_limit);
		else
			usnprintf(text, size - 1, L"%s\r\nTeams\r\nUntil no human is left", kind);
		break;
	case _game_mode_gun_game:
		usnprintf(text, size - 1, L"%s\r\nFree for all\r\nA kill with each weapon", kind);
		break;
	default:
		usnprintf(text, size - 1, L"%s\r\n%s\r\nScore to win: %ld", kind ? kind : engine_names[engine],
			variant.universal_variant.teams ? L"Teams" : L"Free for all", variant.universal_variant.score_to_win);
		break;
	}
	text[size - 1] = 0;
}

/* "mp profiles list initialize": on the gametype used last */
static boolean gametype_list_initialize(struct widget_instance *list)
{
	struct widget_instance *spinner = named(list, "list_item_0_chooser_spinner", 0);
	short last, index;

	multiplayer.gametype_count = ui_widget_port_gametypes(multiplayer.gametypes, MAXIMUM_GAMETYPES, &last);
	gametype_bank_read(multiplayer.gametype_count ? gametype_bank(multiplayer.gametypes[last]) : GAMETYPE_BANK_STANDARD);
	for (index = 0; index < multiplayer.bank_count; index++)
	{
		if (multiplayer.bank[index] == last)
			multiplayer.gametype_chosen = index;
	}
	multiplayer.gametype_first = (short)PIN(multiplayer.gametype_chosen - GAMETYPE_ROWS / 2, 0,
		MAX(0, multiplayer.bank_count - GAMETYPE_ROWS));
	if (spinner)
		spinner->parameters.list.selected_index = multiplayer.bank_shown;
	focus_row(list, (short)(1 + multiplayer.gametype_chosen - multiplayer.gametype_first));
	return TRUE;
}

/* a gametype's name, in a row's text (the name's own buffer is longer:
playlist_profile_get_display_name fills MAX_GAMENAME characters) */
static void gametype_display_name(long profile_index, wchar_t *text)
{
	wchar_t name[MAX_GAMENAME];
	short index;

	for (index = 0; index < gametype_shown.name_count; index++)
	{
		if (gametype_shown.name_profiles[index] == profile_index)
		{
			ustrncpy(text, gametype_shown.names[index], ROW_TEXT_LENGTH - 1);
			text[ROW_TEXT_LENGTH - 1] = 0;
			return;
		}
	}
	text[0] = 0;
	if (playlist_profile_get_display_name(profile_index, name))
	{
		ustrncpy(text, name, ROW_TEXT_LENGTH - 1);
		text[ROW_TEXT_LENGTH - 1] = 0;
	}
	if (gametype_shown.name_count < MAXIMUM_GAMETYPES)
	{
		gametype_shown.name_profiles[gametype_shown.name_count] = profile_index;
		ustrncpy(gametype_shown.names[gametype_shown.name_count], text, ROW_TEXT_LENGTH - 1);
		gametype_shown.names[gametype_shown.name_count][ROW_TEXT_LENGTH - 1] = 0;
		gametype_shown.name_count++;
	}
}

static void gametype_name(short item, wchar_t *text)
{
	text[0] = 0;
	if (item >= 0 && item < multiplayer.bank_count)
		gametype_display_name(multiplayer.gametypes[multiplayer.bank[item]], text);
}

/* "gt select list update": the bank shown, its rows (from the second), and
the gametype chosen: its name, picture and rules */
static void gametype_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *spinner = named(list, "list_item_0_chooser_spinner", 0);
	struct widget_instance *row;
	struct widget_instance *widget;
	short row_index = 0, focused = NONE, count;

	if (spinner && spinner->parameters.list.selected_index != multiplayer.bank_shown)
		gametype_bank_read((short)PIN(spinner->parameters.list.selected_index, 0, NUMBER_OF_GAMETYPE_BANKS - 1));
	count = (short)MIN(multiplayer.bank_count, GAMETYPE_ROWS);
	/* (the rows after the spinner's: scrolled as list_scroll does) */
	for (row = list->child; row; row = row->next, row_index++)
	{
		if (row == list->focused_child && row_index >= 1 && row_index <= GAMETYPE_ROWS)
			focused = (short)(row_index - 1);
	}
	if (focused == count - 1 && multiplayer.gametype_first + count < multiplayer.bank_count && list_scrolls_at_end())
	{
		multiplayer.gametype_first++;
		focus_row(list, (short)focused);
		focused--;
	}
	else if (focused == 0 && multiplayer.gametype_first > 0 && list_scrolls_at_end())
	{
		multiplayer.gametype_first--;
		focus_row(list, 2);
		focused++;
	}
	if (focused != NONE)
		multiplayer.gametype_chosen = (short)(multiplayer.gametype_first + focused);
	for (row = list->child, row_index = 0; row; row = row->next, row_index++)
	{
		wchar_t text[ROW_TEXT_LENGTH];

		if (row_index < 1 || row_index > GAMETYPE_ROWS)
			continue;
		row->visible = row_index - 1 < count;
		gametype_name((short)(multiplayer.gametype_first + row_index - 1), text);
		text_set(named(row, "list_item_text", 0), text);
		if (named(row, "list_item_arrows", 0))
			named(row, "list_item_arrows", 0)->visible = row == list->focused_child;
		if (named(row, "scroll_up_button", 0))
			named(row, "scroll_up_button", 0)->visible = FALSE;
		if (named(row, "scroll_down_button", 0))
			named(row, "scroll_down_button", 0)->visible = FALSE;
	}
	visible_set(named(description, "locked_gametype_icon", 0), FALSE);
	visible_set(named(description, "gametype_right_item", 0), multiplayer.bank_count > 0);
	if (multiplayer.gametype_chosen < multiplayer.bank_count)
	{
		wchar_t text[ROW_TEXT_LENGTH * 2];
		long profile = multiplayer.gametypes[multiplayer.bank[multiplayer.gametype_chosen]];
		struct game_variant const *variant = gametype_shown_get(profile);

		gametype_name(multiplayer.gametype_chosen, text);
		text_set(named(description, "gametype_right_name", 0), text);
		if (variant)
		{
			long engine = PIN(variant->game_engine_index, 0, 5);

			if ((widget = named(description, "gametype_right_pic", 0)) != NULL)
				widget->animation.current_frame_index = (short)engine;
			gametype_rules_text(multiplayer.gametypes[multiplayer.bank[multiplayer.gametype_chosen]], text,
				NUMBEROF(text));
			text_set(named(description, "gametype_right_data", 0), text);
		}
	}
	profile_name_show(description);
}

/* ---- the game's TAG PROFILE screen (tools/port_settings.py's
_tag_profile_screen), between the gametype and Server Setup, with the Tag
Injector on and a multiplayer profile made, on one of the game's own
multiplayer maps: NONE (the game listed for every build), then the Tag
Injector's multiplayer profiles (the game listed for builds with the mod
alone, its players downloading the profile's weapons: tag_multiplayer.c).
What is chosen stays the game's: Server Setup shows it, and B from there
comes back here to choose again */

#define TAG_PROFILE_ROWS 9
#define TAG_PROFILE_SCREEN_NAME \
	"pc\\main_menu\\multiplayer_type_select\\server_settings\\tag_profile\\tag_profile_screen"

/* whether the game hosted asks for its profile: a multiplayer game on one
of the game's own maps, with the Tag Injector on and a multiplayer profile
made */
static boolean tag_profile_asked(void)
{
	void *server = global_network_game_server_get();
	struct network_game *game = server ? network_game_server_get_game(server) : NULL;

	return game && config_boolean("game.tag_injection") && tag_multiplayer_map_takes(game->map.name) &&
		port_tag_mp_profiles_scan() > 0;
}

/* the row (tag_profile_<n>) a widget is or is in, or NONE */
static short tag_profile_row(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strncmp(widget->name, "tag_profile_", 12) && widget->name[12] >= '0' && widget->name[12] <= '9')
			return (short)atoi(widget->name + 12);
	}
	return NONE;
}

/* "mp tag profiles init": the profile chosen last focused, else NONE */
static boolean tag_profiles_initialize(struct widget_instance *list)
{
	long chosen = tag_multiplayer_profile_chosen();
	short place = (short)(chosen == NONE ? 0 : chosen + 1);

	multiplayer.tag_profile_first = (short)MAX(0, place - (TAG_PROFILE_ROWS - 1));
	focus_row(list, (short)(place - multiplayer.tag_profile_first));
	return TRUE;
}

/* "mp tag profiles update": the rows (the list moving on at its ends while
there are more past them), each NONE or a profile, and the help of the one
chosen: who sees the game, and what it plays */
static void tag_profiles_update(struct widget_instance *list)
{
	struct widget_instance *help = list->parameters.list.extended_description;
	short count = (short)(port_tag_mp_profiles_count() + 1);
	short focused = tag_profile_row(list->focused_child);
	short *first = &multiplayer.tag_profile_first;
	short index;

	if (focused == TAG_PROFILE_ROWS - 1 && *first + TAG_PROFILE_ROWS < count && list_scrolls_at_end())
	{
		(*first)++;
		focus_row(list, --focused);
	}
	else if (focused == 0 && *first > 0 && list_scrolls_at_end())
	{
		(*first)--;
		focus_row(list, ++focused);
	}
	if (*first > 0 && *first + TAG_PROFILE_ROWS > count)
		*first = (short)MAX(0, count - TAG_PROFILE_ROWS);
	for (index = 0; index < TAG_PROFILE_ROWS; index++)
	{
		char name[24];
		struct widget_instance *row;
		short place = (short)(*first + index);
		char text[200];
		wchar_t wide[200];
		short character;

		snprintf(name, sizeof(name), "tag_profile_%d", index);
		row = named(list, name, 0);
		if (!row)
			continue;
		row->visible = place < count;
		if (place >= count)
			continue;
		if (place)
			upper_copy(text, sizeof(text), port_tag_mp_profiles_name(place - 1));
		else
			snprintf(text, sizeof(text), "NONE");
		for (character = 0; text[character] && character < ROW_TEXT_LENGTH - 1; character++)
			wide[character] = (wchar_t)(unsigned char)text[character];
		wide[character] = 0;
		text_set(row, wide);
		if (index != focused || !help)
			continue;
		if (!place)
			snprintf(text, sizeof(text), "The map's own weapons. Every player can see and\njoin the game, on any build.");
		/* (the Tag Injector's bundles are Custom Edition maps': game.custom_edition) */
		else if (!halo_custom_edition_tag_cache())
			snprintf(text, sizeof(text), "Needs Custom Edition maps on (Settings > Gameplay).");
		else
		{
			long profile = port_tag_profiles_find(port_tag_mp_profiles_option(place - 1, "profile"));

			snprintf(text, sizeof(text), "Weapons of %s. Only players of this build see\nthe game; they download them from you.",
				profile == NONE ? "a profile not there" : port_tag_profiles_name(profile));
		}
		for (character = 0; text[character] && character < (short)NUMBEROF(wide) - 1; character++)
			wide[character] = (wchar_t)(unsigned char)text[character];
		wide[character] = 0;
		long_text_set(help, wide);
	}
}

/* "mp tag profile choose" (A on a row): the game's profile (or none),
then Server Setup, whose B comes back here */
static boolean tag_profile_choose(struct widget_instance *widget, boolean *widget_deleted)
{
	short row = tag_profile_row(widget);
	short place = row == NONE ? NONE : (short)(multiplayer.tag_profile_first + row);
	long profile = place > 0 ? place - 1 : NONE;

	if (place == NONE || place > port_tag_mp_profiles_count() || (profile != NONE && !halo_custom_edition_tag_cache()))
		return campaign_fail();
	tag_multiplayer_game_profile_set(profile);
	tag_multiplayer_profile_choose(profile);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return ui_widget_port_open(widget, SERVER_SETUP_NAME, widget_deleted);
}

/* "mp profile set for game": the gametype, then Server Setup, or first the
game's TAG PROFILE screen (tag_profile_asked); the game plays no profile
until that chooses one */
static boolean gametype_choose(struct widget_instance *list, boolean *widget_deleted)
{
	gametype_list_update(list);
	gametype_setup_end();
	if (multiplayer.gametype_chosen >= multiplayer.bank_count)
		return campaign_fail();
	if (!ui_widget_port_gametype_choose(multiplayer.gametypes[multiplayer.bank[multiplayer.gametype_chosen]]))
		return FALSE;
	tag_multiplayer_game_profile_set(NONE);
	multiplayer.tag_profile_shown = tag_profile_asked();
	if (multiplayer.tag_profile_shown)
		return ui_widget_port_open(list, TAG_PROFILE_SCREEN_NAME, widget_deleted);
	return TRUE;
}

/* ---- the server settings */

static void game_name_done(char const *text)
{
	short index;

	for (index = 0; text[index] && index < NUMBEROF(multiplayer.game_name) - 1; index++)
		multiplayer.game_name[index] = (wchar_t)(unsigned char)text[index];
	multiplayer.game_name[index] = 0;
}

/* Whether a network game is co-op: a campaign level or a Custom Edition
campaign map, with no game engine (set up by
ui_widget_port_cooperative_level_choose). A Custom Edition map this machine
has not is taken to be one: co-op is the only game with no game engine. */
static boolean game_cooperative(struct network_game const *game)
{
	return game && !game->variant.game_engine_index &&
		(custom_edition_maps_level_campaign(game->map.name) ||
		(custom_edition_level_name(game->map.name) && custom_edition_maps_display_index(game->map.name) == NONE));
}

/* the same, for the game this machine is hosting */
static boolean hosting_cooperative(void)
{
	void *server = global_network_game_server_get();

	return server && game_cooperative(network_game_server_get_game(server));
}

/* whether the co-op game hosted is one of the ten campaign levels, which
the randomizer swaps (randomizer.c's campaign_leaf) */
static boolean hosting_campaign_level(void)
{
	void *server = global_network_game_server_get();
	struct network_game *game = server ? network_game_server_get_game(server) : NULL;

	return game && game_cooperative(game) && campaign_level_of(game->map.name) != NONE;
}

/* Server Setup's gametype rows, which co-op hides */
static char const *const server_settings_gametype_rows[] =
{
	"op_game_type", "op_player_options", "op_item_options", "op_vehicle_options", "op_indicator_options",
	"op_team_options",
};

/* the most players a co-op game hosted starts with (maximum_players') */
#define COOPERATIVE_DEFAULT_PLAYERS 16
/* Server Setup's help for co-op's options' rows (CO-OP OPTIONS, VOICE AND
VOTING), PASSWORD's and MAP SHARING's (its help_strings, tools/port_settings.py) */
#define COOPERATIVE_OPTIONS_HELP 12
#define COOPERATIVE_VOICE_OPTIONS_HELP 13
#define SERVER_PASSWORD_HELP 14
#define MAP_SHARE_OPTIONS_HELP 15
/* Server Setup's RANDOMIZER and SKULLS rows (tools/port_settings.py's
CAMPAIGN_SETUP_SCREENS), a co-op game's: their help, and the rows' spacing,
SKULLS taking RANDOMIZER's place when that is hidden */
#define RANDOMIZER_SETUP_HELP 16
#define SKULLS_SETUP_HELP 17
#define SERVER_SETTINGS_ROW_SPACING 24
/* Server Setup's BOTS row (tools/port_settings.py's BOTS_SETUP), a LOCAL
game's, in MAP SHARING's place: its help */
#define BOTS_SETUP_HELP 18
/* (LISTING's PUBLIC on a machine with a publisher key: server_master.c), after
the rows above */
#define SERVER_LISTING_PUBLISHER_HELP 19
/* Server Setup's TAG PROFILE row (tools/port_settings.py's TAG_PROFILE_SETUP),
a multiplayer game's on the game's own maps: its help; and where the rows
are packed while it is shown (the first's place, and the most rows a
spacing of SERVER_SETTINGS_ROW_SPACING has above the help) */
#define TAG_PROFILE_SETUP_HELP 20
#define SERVER_SETTINGS_FIRST_ROW 73
#define SERVER_SETTINGS_ROWS 12

/* whether this machine has a publisher key (read as Server Setup opens) */
static boolean server_settings_publisher;

/* co-op's EXTRA ENEMIES' choices (network.coop_enemies_mode's values, as
gametype_options' coop_extra_enemies_spinner has them, in this order): its
amount's row is the choice's */
enum
{
	_cooperative_enemies_per_player,
	_cooperative_enemies_multiplier,
	_cooperative_enemies_none,
	NUMBER_OF_COOPERATIVE_ENEMIES_MODES
};

/* co-op's FRIENDLY FIRE's choices (network.coop_friendly_fire's values, as
gametype_options' coop_friendly_fire_spinner and server_start have them, in
this order) */
static short const cooperative_friendly_fire_modes[] =
{
	_friendly_fire_off, _friendly_fire_on, _friendly_fire_shields_only, _friendly_fire_explosives_only
};

/* the most players Server Setup shows and sets: the multiplayer game's, or
the co-op game's (COOPERATIVE_DEFAULT_PLAYERS the first time in each game
hosted: multiplayer_host) */
/* the count of Server Setup's choices of the most players: those up to
HOSTED_PLAYERS_LIMIT */
static short maximum_players_choices(void)
{
	short count = 0;

	while (count < NUMBEROF(maximum_players) && maximum_players[count] <= HOSTED_PLAYERS_LIMIT)
		count++;
	return count;
}

static short *server_settings_maximum_players_index(void)
{
	short index;

	if (!hosting_cooperative())
	{
		/* (the most there is, the first time) */
		if (multiplayer.maximum_players_index == NONE)
			multiplayer.maximum_players_index = (short)(maximum_players_choices() - 1);
		return &multiplayer.maximum_players_index;
	}
	if (!multiplayer.cooperative_maximum_players_set)
	{
		multiplayer.cooperative_maximum_players_index = NUMBEROF(maximum_players) - 1;
		for (index = 0; index < NUMBEROF(maximum_players); index++)
		{
			if (maximum_players[index] == COOPERATIVE_DEFAULT_PLAYERS)
				multiplayer.cooperative_maximum_players_index = index;
		}
		multiplayer.cooperative_maximum_players_set = TRUE;
	}
	return &multiplayer.cooperative_maximum_players_index;
}

/* Server Setup's LISTING, PRIVATE: the multiplayer game's (as
network.host_public started it), or co-op's, saved in network.coop_public */
static boolean server_settings_private(void)
{
	return hosting_cooperative() ? !config_boolean("network.coop_public") : multiplayer.game_private;
}

static void server_settings_private_set(boolean private_game)
{
	if (!hosting_cooperative())
		multiplayer.game_private = private_game;
	else if (private_game != server_settings_private() &&
		!config_write("network.coop_public", private_game ? "false" : "true"))
	{
		platform_log("menus: could not write network.coop_public to config.toml");
	}
}

/* "server settings init": the game's name (player 1's, else the one given
last), the most players, the gametype's copy (once: the screen is made
again on coming back from an option's screen) */
static boolean server_settings_initialize(struct widget_instance *list)
{
	struct widget_instance *spinner = named(list, "max_players_spinner", 0);

	/* (co-op has no gametype to edit) */
	if (!hosting_cooperative())
		gametype_setup_begin();
	if (!multiplayer.game_name[0] && player_ui_get_active_player_profile_index(0) != NONE)
	{
		struct player_profile profile;

		player_ui_get_active_player_profile(0, &profile);
		ustrncpy(multiplayer.game_name, profile.player_name, NUMBEROF(multiplayer.game_name) - 1);
	}
	if (spinner)
	{
		/* (the choices up to HOSTED_PLAYERS_LIMIT: the spinner's left and
		right wrap at its count of items) */
		spinner->parameters.list.number_of_items = (word)MIN(spinner->parameters.list.number_of_items,
			maximum_players_choices());
		*server_settings_maximum_players_index() = (short)PIN(*server_settings_maximum_players_index(), 0,
			maximum_players_choices() - 1);
		spinner->parameters.list.selected_index = *server_settings_maximum_players_index();
	}
	/* (PUBLIC or PRIVATE: this game's; the screen is made again on coming
	back from an option's screen) */
	if ((spinner = named(list, "listing_spinner", 0)) != NULL)
		spinner->parameters.list.selected_index = server_settings_private() ? 1 : 0;
	/* (TAG PROFILE's: the Tag Injector's multiplayer profiles found again) */
	port_tag_mp_profiles_scan();
	p2p_set_hosting_public(multiplayer.mode == _multiplayer_mode_host_internet && !server_settings_private());
	{
		unsigned char public_key[SERVER_MASTER_PUBLIC_KEY_SIZE];
		char key_id[2 * SERVER_MASTER_KEY_ID_SIZE + 1];

		server_settings_publisher = server_master_publisher(public_key, key_id) != 0;
	}
	return TRUE;
}

static void wide_to_text(wchar_t const *wide, char *text, short size)
{
	short index;

	for (index = 0; wide[index] && index < size - 1; index++)
		text[index] = wide[index] < 0x80 ? (char)wide[index] : '?';
	text[index] = 0;
}

/* whether Server Setup shows the game's TAG PROFILE: a multiplayer game
whose TAG PROFILE screen was shown (tag_profile_asked), which chose it */
static boolean server_settings_tag_profile_shown(void)
{
	void *server = global_network_game_server_get();
	struct network_game *game = server ? network_game_server_get_game(server) : NULL;

	return game && !hosting_cooperative() && multiplayer.tag_profile_shown &&
		tag_multiplayer_map_takes(game->map.name);
}

/* TAG PROFILE: shown for such a game, its value the multiplayer profile
the game plays (NONE: every build sees it), its help; and the rows shown
packed, closer together where there would be more than the screen's twelve */
static void server_settings_tag_profile_update(struct widget_instance *list)
{
	struct widget_instance *row = named(list, "op_tag_profile", 0);
	struct widget_instance *help = list->parameters.list.extended_description;
	struct widget_instance *child;
	boolean shown = server_settings_tag_profile_shown();
	short count = 0;
	short spacing = SERVER_SETTINGS_ROW_SPACING;
	short place = 0;

	visible_set(row, shown);
	if (!row || !shown)
		return;
	{
		long profile = tag_multiplayer_game_profile();
		char value[ROW_TEXT_LENGTH];

		if (profile != NONE)
			upper_copy(value, sizeof(value), port_tag_mp_profiles_name(profile));
		else
			snprintf(value, sizeof(value), "NONE (EVERY BUILD)");
		text_field_show(named(list, "tag_profile_value", 0), value, FALSE);
	}
	if (help && list->focused_child == row)
		help->parameters.text_box.string_list_index = TAG_PROFILE_SETUP_HELP;
	for (child = list->child; child; child = child->next)
		count = (short)(count + (!strncmp(child->name, "op_", 3) && child->visible));
	if (count > SERVER_SETTINGS_ROWS)
		spacing = (short)(SERVER_SETTINGS_ROW_SPACING * (SERVER_SETTINGS_ROWS - 1) / (count - 1));
	for (child = list->child; child; child = child->next)
	{
		if (strncmp(child->name, "op_", 3) || !child->visible)
			continue;
		child->vertical_offset = (short)(SERVER_SETTINGS_FIRST_ROW + place * spacing);
		place++;
	}
}

/* "ss tag profile change" (A on TAG PROFILE): back to the TAG PROFILE
screen, which Server Setup was opened from, to choose again */
static boolean server_settings_tag_profile_change(struct widget_instance *widget, boolean *widget_deleted)
{
	if (!server_settings_tag_profile_shown())
		return TRUE;
	if (text_field_editing(NULL))
		text_field_end(TRUE);
	ui_play_audio_feedback_sound(SOUND_BACK);
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* "server settings update" */
static void server_settings_update(struct widget_instance *list)
{
	struct widget_instance *spinner = named(list, "max_players_spinner", 0);
	struct widget_instance *row = named(list, "op_server_name", 0);
	char text[TEXT_FIELD_LENGTH];

	if (spinner)
		*server_settings_maximum_players_index() = (short)PIN(spinner->parameters.list.selected_index, 0,
			maximum_players_choices() - 1);
	wide_to_text(multiplayer.game_name, text, sizeof(text));
	text_field_show(named(list, "server_name_value", 0), text, text_field_editing(row));
	if (multiplayer.mode == _multiplayer_mode_host_local)
		snprintf(text, sizeof(text), "NONE: A LOCAL GAME");
	else if (multiplayer.mode != _multiplayer_mode_host_internet)
		snprintf(text, sizeof(text), "NONE: A LAN GAME");
	else if (!config_boolean("network.online"))
		snprintf(text, sizeof(text), "INTERNET PLAY IS OFF (SETTINGS)");
	else if (!p2p_invite_link(text, sizeof(text)))
		snprintf(text, sizeof(text), "MADE WHEN THE GAME STARTS");
#ifdef LINK_PAGE
	/* (the Switch and the PS5 show the link as a QR code for a phone instead: invite_copy) */
	else
		snprintf(text, sizeof(text), "CLICK TO OPEN A QR WITH THE LINK");
#endif
	text_field_show(named(list, "invite_value", 0), text, FALSE);
	{
		wchar_t type[ROW_TEXT_LENGTH];

		gametype_setup_type(type);
		text_set(named(list, "game_type_value", 0), type);
	}
	settings_help(list);
	{
		boolean cooperative = hosting_cooperative();
		short row;

		for (row = 0; row < NUMBEROF(server_settings_gametype_rows); row++)
			visible_set(named(list, server_settings_gametype_rows[row], 0), !cooperative);
		/* co-op's options in their place, rows opening their screens
		(tools/port_settings.py's COOP_SETUP_SCREENS), each its help */
		{
			struct widget_instance *help = list->parameters.list.extended_description;

			struct widget_instance *randomizer_row = named(list, "op_randomizer", 0);
			struct widget_instance *skulls_row = named(list, "op_skulls", 0);
			boolean daily = cooperative && community_challenge_running();

			visible_set(named(list, "op_coop_options", 0), cooperative);
			visible_set(named(list, "op_voice_options", 0), cooperative);
			/* (the randomizer's and the skulls' screens, with them on in
			Settings > Gameplay: a level of the campaign's, any campaign map;
			in a daily challenge's lobby both shown, dimmed, its own settings
			not to be changed: community.c) */
			visible_set(randomizer_row, daily ||
				(cooperative && config_boolean("game.randomizer") && hosting_campaign_level()));
			visible_set(skulls_row, daily || (cooperative && config_boolean("game.skulls")));
			enabled_set(randomizer_row, !daily);
			enabled_set(skulls_row, !daily);
			if (randomizer_row && skulls_row)
			{
				skulls_row->vertical_offset = (short)(randomizer_row->vertical_offset +
					(randomizer_row->visible ? SERVER_SETTINGS_ROW_SPACING : 0));
			}
			if (help && randomizer_row && list->focused_child == randomizer_row)
				help->parameters.text_box.string_list_index = RANDOMIZER_SETUP_HELP;
			else if (help && skulls_row && list->focused_child == skulls_row)
				help->parameters.text_box.string_list_index = SKULLS_SETUP_HELP;
			else if (help && list->focused_child == named(list, "op_coop_options", 0))
				help->parameters.text_box.string_list_index = COOPERATIVE_OPTIONS_HELP;
			else if (help && list->focused_child == named(list, "op_voice_options", 0))
				help->parameters.text_box.string_list_index = COOPERATIVE_VOICE_OPTIONS_HELP;
			/* (MAP SHARING's, any game's: map_share.c) */
			else if (help && list->focused_child == named(list, "op_map_share_options", 0))
				help->parameters.text_box.string_list_index = MAP_SHARE_OPTIONS_HELP;
		}
		/* BOTS in MAP SHARING's place, a LOCAL game's: no one joins it to
		share a map with, and only its lobby takes the bots (bots.c) */
		{
			boolean local = multiplayer.mode == _multiplayer_mode_host_local && !cooperative;
			struct widget_instance *help = list->parameters.list.extended_description;

			visible_set(named(list, "op_map_share_options", 0), !local);
			visible_set(named(list, "op_bots_options", 0), local);
			if (help && local && list->focused_child == named(list, "op_bots_options", 0))
				help->parameters.text_box.string_list_index = BOTS_SETUP_HELP;
			/* (its value, what its screen chose: "4, DISTRIBUTED, EVEN", or
			NONE) */
			if (local)
			{
				long count = PIN(config_integer("bots.count"), 0, BOTS_MAXIMUM);
				char const *skill = config_string("bots.skill");
				char value[64];
				short index;

				if (!count)
					snprintf(value, sizeof(value), "NONE");
				else
				{
					snprintf(value, sizeof(value), "%ld, %s, %s", count, skill ? skill : "",
						!strcmp(config_string("bots.teams"), "against") ? "AGAINST YOU" : "EVEN");
					for (index = 0; value[index]; index++)
						value[index] = (char)toupper((unsigned char)value[index]);
				}
				text_field_show(named(list, "bots_options_value", 0), value, FALSE);
			}
		}
	}
	/* LISTING (an internet game's): PUBLIC, listed in everyone's server
	browser, or PRIVATE, for this game. Its help is its choice's */
	visible_set(named(list, "op_listing", 0), multiplayer.mode == _multiplayer_mode_host_internet &&
		config_boolean("network.online") && config_boolean("network.public_lobby"));
	if ((spinner = named(list, "listing_spinner", 0)) != NULL && named(list, "op_listing", 0) &&
		named(list, "op_listing", 0)->visible)
	{
		boolean public = spinner->parameters.list.selected_index == 0;
		struct widget_instance *help = list->parameters.list.extended_description;

		server_settings_private_set(!public);
		p2p_set_hosting_public(public);
		/* (PUBLIC: through the brokers, and the master servers too with a
		publisher key) */
		if (help && list->focused_child == named(list, "op_listing", 0))
		{
			help->parameters.text_box.string_list_index = public && server_settings_publisher ?
				SERVER_LISTING_PUBLISHER_HELP : (short)(10 + spinner->parameters.list.selected_index);
		}
	}
	/* PASSWORD (a PUBLIC internet game's): its stars, NONE if it has none */
	row = named(list, "op_password", 0);
	visible_set(row, named(list, "op_listing", 0) && named(list, "op_listing", 0)->visible &&
		!server_settings_private());
	if (row && !row->visible && text_field_editing(row))
		text_field_end(FALSE);
	if (row && row->visible)
	{
		struct widget_instance *help = list->parameters.list.extended_description;
		short index;

		for (index = 0; multiplayer.game_password[index] && index < (short)sizeof(text) - 1; index++)
			text[index] = '*';
		text[index] = 0;
		text_field_show(named(list, "password_value", 0), index ? text : "NONE", text_field_editing(row));
		if (help && list->focused_child == row)
			help->parameters.text_box.string_list_index = SERVER_PASSWORD_HELP;
	}
	server_settings_tag_profile_update(list);
}

static void server_password_done(char const *text)
{
	snprintf(multiplayer.game_password, sizeof(multiplayer.game_password), "%s", text);
}

/* "ss edit server password" (its row's A: begins, or ends) */
static boolean server_password_edit(struct widget_instance *row)
{
	if (text_field_editing(row))
	{
		text_field_end(TRUE);
		return TRUE;
	}
	text_field_begin_masked(row, multiplayer.game_password, PASSWORD_LENGTH, server_password_done);
	return TRUE;
}

/* "ss edit server name" (its row's A: begins, or ends) */
static boolean server_name_edit(struct widget_instance *row)
{
	char text[TEXT_FIELD_LENGTH];

	if (text_field_editing(row))
	{
		text_field_end(TRUE);
		return TRUE;
	}
	wide_to_text(multiplayer.game_name, text, sizeof(text));
	text_field_begin(row, text, NUMBEROF(multiplayer.game_name) - 1, game_name_done);
	return TRUE;
}

/* ---- the daily challenge's server (tools/port_settings.py's
SCREENS["gameplay/community"] and _daily_connection; community.c): Settings >
Gameplay > Community's SERVER ADDRESS, typed in its row and written with
OK; and the window the challenge's screen opens when no community server
answers, over it (daily_menu_poll: not while its screen is being updated),
whose choices each go back to it, which asks again */

#define DAILY_CONNECTION_NAME "pc\\main_menu\\daily\\connection_screen"

/* (ui_widget.c's: an error dialog is up) */
extern boolean ui_widget_port_error_showing(void);
/* (the window opens only over the challenge's screen still shown: one
updated this recently) */
#define DAILY_CONNECTION_SHOWN_MILLISECONDS 500

static struct
{
	/* SERVER ADDRESS's, until OK writes it */
	char address[TEXT_FIELD_LENGTH];
	boolean changed;
} daily_server;

static struct
{
	/* the window wanted (and over which screen), and asked once already */
	boolean pending;
	boolean asked;
	unsigned long wanted_at;
	long back_tag;
	/* the address typed in it */
	char address[TEXT_FIELD_LENGTH];
} daily_connection;

static void daily_server_done(char const *text)
{
	snprintf(daily_server.address, sizeof(daily_server.address), "%s", text);
	daily_server.changed = TRUE;
}

/* "port daily settings init" */
static boolean daily_settings_initialize(void)
{
	snprintf(daily_server.address, sizeof(daily_server.address), "%s", config_string("community.custom_server"));
	daily_server.changed = FALSE;
	return TRUE;
}

/* "port daily settings update": SHARE LEVEL SCORES and SERVER online only,
and SERVER ADDRESS with SERVER CUSTOM too */
static void daily_settings_update(struct widget_instance *list)
{
	boolean online = randomizer_spinner_is(list, "enabled_spinner", "true");
	boolean custom = online && randomizer_spinner_is(list, "server_mode_spinner", "custom");
	struct widget_instance *row = named(list, "op_server_address", 0);

	visible_set(named(list, "op_upload_scores", 0), online);
	visible_set(named(list, "op_server_mode", 0), online);
	if (!row)
		return;
	if (!custom && text_field_editing(row))
		text_field_end(FALSE);
	row->visible = custom;
	text_field_show(named(row, "server_address_value", 0), daily_server.address[0] ? daily_server.address : "NONE",
		text_field_editing(row));
}

/* "port daily server edit" (SERVER ADDRESS's A: begins, or ends) */
static boolean daily_server_edit(struct widget_instance *row)
{
	if (text_field_editing(row))
	{
		text_field_end(TRUE);
		return TRUE;
	}
	text_field_begin(row, daily_server.address, TEXT_FIELD_LENGTH - 1, daily_server_done);
	return TRUE;
}

/* (OK, "port settings save") SERVER ADDRESS written, if it was typed; and
(Defaults) none */
static void daily_settings_save(void)
{
	if (!daily_server.changed)
		return;
	if (!config_write("community.custom_server", daily_server.address))
		platform_log("menus: could not write community.custom_server");
	daily_server.changed = FALSE;
}

static void daily_settings_defaults(void)
{
	daily_server.address[0] = 0;
	daily_server.changed = TRUE;
}

/* "port field back" (B on a field's row): the typing ends, as it was; else
back, as Cancel */
static boolean field_back(struct widget_instance *widget, boolean *widget_deleted)
{
	if (text_field_editing(NULL))
	{
		text_field_end(FALSE);
		return TRUE;
	}
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* ---- Settings > Gameplay's CUSTOM MAPS FOLDER (the Android app's:
tools/port_settings.py): game.custom_maps.path, a folder of the phone's
chosen with Android's folder picker (HaloActivity.pickCustomMapsFolder, which
asks for the storage access it needs and says itself why a folder will not
do), shown in its row and written with OK; xbox_files.c reads it again at
once, and the map lists scan it the next time they open */

#if defined(HALO_ANDROID) && !defined(HALO_SWITCH)
/* port/android/guest/runtime/guest_host.h */
int host_folder_pick_begin(void);
int host_folder_pick_result(char *buffer, unsigned int size);
#endif

static struct
{
	/* the row's, until OK writes it; empty for the data folder's custom_maps */
	char path[512];
	boolean changed;
	/* the picker open, its answer asked for each frame */
	boolean picking;
} custom_maps_folder;

/* "port custom maps init" */
static boolean custom_maps_folder_initialize(void)
{
	snprintf(custom_maps_folder.path, sizeof(custom_maps_folder.path), "%s", config_string("game.custom_maps.path"));
	custom_maps_folder.changed = FALSE;
	custom_maps_folder.picking = FALSE;
	return TRUE;
}

/* "port custom maps pick" (CUSTOM MAPS FOLDER's A) */
static boolean custom_maps_folder_pick(void)
{
#if defined(HALO_ANDROID) && !defined(HALO_SWITCH)
	if (!custom_maps_folder.picking)
		custom_maps_folder.picking = host_folder_pick_begin() != 0;
#endif
	return TRUE;
}

/* "port custom maps update": the folder chosen, once the picker has
answered, and the row's folder: DEFAULT, else the path without the phone's
own storage's start, its end alone if the row is too narrow for it */
static void custom_maps_folder_update(struct widget_instance *list)
{
	static char const internal_storage[] = "/storage/emulated/0/";
	struct widget_instance *row = named(list, "op_custom_maps_folder", 0);
	char const *shown = custom_maps_folder.path;
	char text[34];
	size_t length;

	if (!row)
		return;
#if defined(HALO_ANDROID) && !defined(HALO_SWITCH)
	if (custom_maps_folder.picking)
	{
		char chosen[sizeof(custom_maps_folder.path)];
		int result = host_folder_pick_result(chosen, sizeof(chosen));

		if (result)
			custom_maps_folder.picking = FALSE;
		if (result > 0)
		{
			snprintf(custom_maps_folder.path, sizeof(custom_maps_folder.path), "%s", chosen);
			custom_maps_folder.changed = TRUE;
		}
	}
#endif
	if (!shown[0])
	{
		shown = "DEFAULT";
	}
	else
	{
		if (!strncmp(shown, internal_storage, sizeof(internal_storage) - 1))
			shown += sizeof(internal_storage) - 1;
		length = strlen(shown);
		if (length >= sizeof(text))
		{
			snprintf(text, sizeof(text), "...%s", shown + length - (sizeof(text) - 4));
			shown = text;
		}
	}
	text_field_show(named(row, "custom_maps_folder_value", 0), shown, FALSE);
}

/* (OK, "port settings save") CUSTOM MAPS FOLDER written, if one was
chosen; and (Defaults) the data folder's */
static void custom_maps_folder_save(void)
{
	if (!custom_maps_folder.changed)
		return;
	if (!config_write("game.custom_maps.path", custom_maps_folder.path))
		platform_log("menus: could not write game.custom_maps.path");
	else
		platform_log("menus: game.custom_maps.path = %s", custom_maps_folder.path);
	custom_maps_folder.changed = FALSE;
}

static void custom_maps_folder_defaults(void)
{
	custom_maps_folder.path[0] = 0;
	custom_maps_folder.changed = TRUE;
}

/* (the challenge's screen, its server not reached) the window wanted, once
until the challenge is asked for again */
static void daily_connection_want(struct widget_instance *screen)
{
	if (daily_connection.asked)
		return;
	daily_connection.pending = TRUE;
	daily_connection.wanted_at = system_milliseconds();
	daily_connection.back_tag = screen->definition_tag_index;
}

static void daily_connection_reset(void)
{
	daily_connection.pending = FALSE;
	daily_connection.asked = FALSE;
}

/* (main.c's main loop, each frame) the window opened, over the challenge's
screen still shown */
void daily_menu_poll(void)
{
	if (!daily_connection.pending)
		return;
	daily_connection.pending = FALSE;
	if (system_milliseconds() - daily_connection.wanted_at > DAILY_CONNECTION_SHOWN_MILLISECONDS ||
		ui_widget_port_error_showing() || text_field_editing(NULL))
	{
		return;
	}
	snprintf(daily_connection.address, sizeof(daily_connection.address), "%s",
		config_string("community.custom_server"));
	if (ui_widget_load_by_name_or_tag(DAILY_CONNECTION_NAME, NONE, NULL, NONE, daily_connection.back_tag, NONE, NONE))
	{
		daily_connection.asked = TRUE;
		ui_play_audio_feedback_sound(SOUND_ERROR);
	}
}

/* (main.c's main loop, each frame, after randomizer_pack_update) the
question of how the skulls are had not answered (the first start, or after
RESET SKULL PROGRESS), with SKULLS on or off, as the daily challenge and a
co-op host's skulls apply either way: once the packs wanted are written
(randomizer_pack.c), the question, over the main menu, the settings screen
or the daily challenge's screen shown, and Back there. Asked once, until
SKULLS is turned on again; skipped, it is asked before the skulls screen
still */
#define SKULLS_UNLOCK_SCREEN_NAME "pc\\main_menu\\difficulty_select\\skulls\\unlock_screen"
#define SETTINGS_SCREEN_PREFIX "pc\\main_menu\\settings_select\\"

extern int randomizer_pack_busy(void);
extern struct widget_instance *ui_widget_port_active_screen(void);
extern char *tag_get_name(long tag_index);

void skulls_setup_poll(boolean main_menu)
{
	struct widget_instance *screen;
	char const *name;

	if (!main_menu || skulls_setup.asked || skulls_unlock_mode() != _skulls_unlock_unset)
		return;
	if (randomizer_pack_busy() || ui_widget_port_error_showing() || text_field_editing(NULL))
		return;
	screen = ui_widget_port_active_screen();
	name = screen ? tag_get_name(screen->definition_tag_index) : NULL;
	if (!name || (strcmp(name, MAIN_MENU_NAME) && strcmp(name, DAILY_SCREEN_NAME) &&
		strncmp(name, SETTINGS_SCREEN_PREFIX, strlen(SETTINGS_SCREEN_PREFIX))))
	{
		return;
	}
	skulls_setup.asked = TRUE;
	skulls_menu_back_tag = screen->definition_tag_index;
	if (ui_widget_load_by_name_or_tag(SKULLS_UNLOCK_SCREEN_NAME, NONE, NULL, NONE, skulls_menu_back_tag, NONE, NONE))
	{
		skulls_setup.open = TRUE;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
	}
}

/* "port daily connection update": what happened, and the choices: TRY THE
DEFAULT SERVER with a custom one, SERVER ADDRESS with the default */
static void daily_connection_update(struct widget_instance *list)
{
	boolean custom = community_net_custom() != 0;
	struct widget_instance *default_row = named(list, "connection_default", 0);
	struct widget_instance *address_row = named(list, "connection_address", 0);
	char server[TEXT_FIELD_LENGTH];
	wchar_t text[640];

	community_net_server_text(server, sizeof(server));
	usnprintf(text, NUMBEROF(text),
		L"The daily challenge could not be fetched from\r%hs\r\r%s\r\r%hs\r"
		L"GO OFFLINE: play a challenge made on this machine, its\rleaderboard this machine's profiles'.\r"
		L"TRY AGAIN: ask the server again.",
		server, community_status_text(), custom ? "TRY THE DEFAULT SERVER: ask the OpenCE server instead." :
		"SERVER ADDRESS: ask another community server, at the\raddress you type (A, then Enter).");
	long_text_set(named(screen_of(list), "connection_text", 0), text);
	if (default_row)
		default_row->visible = custom;
	if (address_row)
		address_row->visible = !custom;
	if (list->focused_child && !list->focused_child->visible)
		list->focused_child = custom ? default_row : address_row;
	text_field_show(named(list, "connection_address_value", 0),
		daily_connection.address[0] ? daily_connection.address : "https://", text_field_editing(address_row));
}

static void daily_connection_address_done(char const *text)
{
	snprintf(daily_connection.address, sizeof(daily_connection.address), "%s", text);
}

/* the window's choice made: back to the challenge, which asks again */
static boolean daily_connection_back(struct widget_instance *widget, boolean *widget_deleted)
{
	daily_connection_reset();
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* "port daily try default" (TRY THE DEFAULT SERVER) */
static boolean daily_connection_try_default(struct widget_instance *widget, boolean *widget_deleted)
{
	if (!config_write("community.server_mode", "default"))
		platform_log("menus: could not write community.server_mode");
	return daily_connection_back(widget, widget_deleted);
}

/* "port daily connection edit" (SERVER ADDRESS's A: the typing begins; its
end, Enter, asks that server, kept as the custom one) */
static boolean daily_connection_edit(struct widget_instance *row, boolean *widget_deleted)
{
	if (!text_field_editing(row))
	{
		text_field_begin(row, daily_connection.address, TEXT_FIELD_LENGTH - 1, daily_connection_address_done);
		return TRUE;
	}
	text_field_end(TRUE);
	if (!daily_connection.address[0])
		return campaign_fail();
	if (!config_write("community.custom_server", daily_connection.address) ||
		!config_write("community.server_mode", "custom"))
	{
		platform_log("menus: could not write community.custom_server");
	}
	return daily_connection_back(row, widget_deleted);
}

/* "port daily go offline" (GO OFFLINE: community.enabled off) */
static boolean daily_connection_offline(struct widget_instance *widget, boolean *widget_deleted)
{
	if (!config_write("community.enabled", "false"))
		platform_log("menus: could not write community.enabled");
	return daily_connection_back(widget, widget_deleted);
}

#define INVITE_QR_NAME "pc\\main_menu\\multiplayer_type_select\\server_settings\\invite_qr\\invite_qr_screen"

/* "ss copy invite": the invite link copied; on the Switch and the PS5, whose
clipboard nothing else reads, its QR code on a screen of its own
(invite_qr_draw), which only B closes ("ss invite qr back") */
static boolean invite_copy(struct widget_instance *widget, boolean *widget_deleted)
{
	char link[TEXT_FIELD_LENGTH];

	if (!p2p_invite_link(link, sizeof(link)))
		return campaign_fail();
#ifdef LINK_PAGE
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return ui_widget_port_open(widget, INVITE_QR_NAME, widget_deleted);
#else
	(void)widget;
	(void)widget_deleted;
	platform_clipboard_set(link);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
#endif
}

/* "ss invite qr back" (B, the screen's one handler): back to Server Setup */
static boolean invite_qr_back(struct widget_instance *widget, boolean *widget_deleted)
{
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* "ss randomizer" and "ss skulls" (a co-op game's rows): the randomizer's
screen, or the skulls' (their question first, the first time), over Server
Setup, whose OK writes them and comes back (campaign_screens_for_server) */
static boolean server_campaign_screen(struct widget_instance *widget, boolean skulls, boolean *widget_deleted)
{
	/* (a daily challenge's are its own) */
	if (community_challenge_running())
		return campaign_fail();
	campaign_screens_for_server = TRUE;
	if (skulls)
		return skulls_open(widget, widget_deleted);
	randomizer_tags.checked = 0;
	if (!ui_widget_load_by_name_or_tag(RANDOMIZER_SCREEN, NONE, NULL, NONE, screen_of(widget)->definition_tag_index,
		NONE, NONE))
	{
		return campaign_fail();
	}
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	*widget_deleted = TRUE;
	return TRUE;
}

/* "ss start game": the settings given the server, then the lobby */
static boolean server_start(void)
{
	if (text_field_editing(NULL))
		text_field_end(TRUE);
	network_game_server_port_set_settings(multiplayer.game_name,
		maximum_players[PIN(*server_settings_maximum_players_index(), 0, maximum_players_choices() - 1)]);
	/* (its listing's password: a PUBLIC internet game's, else none) */
	p2p_set_hosting_password(multiplayer.mode == _multiplayer_mode_host_internet && !server_settings_private() ?
		multiplayer.game_password : NULL);
	/* the gametype as Server Setup's options left it (co-op keeps its own,
	and its FRIENDLY FIRE) */
	if (hosting_cooperative())
	{
		/* (as CO-OP OPTIONS left them: network.coop_friendly_fire, in
		cooperative_friendly_fire_modes' order, network.coop_player_collisions
		and network.coop_ally_friendly_fire) */
		static char const *const friendly_fire_names[] = { "off", "on", "shields_only", "explosives_only" };
		char const *friendly_fire = config_string("network.coop_friendly_fire");
		short index;

		gametype_setup_end();
		multiplayer.cooperative_friendly_fire = _friendly_fire_on;
		for (index = 0; friendly_fire && index < (short)NUMBEROF(friendly_fire_names); index++)
		{
			if (!strcmp(friendly_fire, friendly_fire_names[index]))
				multiplayer.cooperative_friendly_fire = cooperative_friendly_fire_modes[index];
		}
		multiplayer.cooperative_no_player_collisions = !config_boolean("network.coop_player_collisions");
		network_game_server_port_set_cooperative_friendly_fire(multiplayer.cooperative_friendly_fire);
		network_game_server_port_set_cooperative_player_collisions(!multiplayer.cooperative_no_player_collisions);
		network_game_server_port_set_cooperative_ally_friendly_fire(config_boolean("network.coop_ally_friendly_fire"));
	}
	else if (!gametype_setup_apply())
		return campaign_fail();
	return global_network_game_server_get() != NULL;
}

/* ---- the browser (Join Game > Server Browser, LAN, Direct Link): the
games found on the LAN, or (Direct Link) from internet play's peers
(reached by an invite link, the clipboard, or Discord) */

#define JOIN_GAME_LABELS "pc\\main_menu\\multiplayer_type_select\\join_game\\join_game_ticker_labels"
enum { _join_game_label_servers = 6, _join_game_label_players, _join_game_label_page };

static boolean game_from_peer(struct advertised_game const *game)
{
	unsigned long address;

	return p2p_peer_address(game->xnaddr + 2, &address) != 0;
}

/* (network_test.c's probe) whether the client's advertised game at the
index is a peer's (game_from_peer) */
boolean menu_advertised_game_from_peer(long index)
{
	void *client = global_network_game_client_get();
	struct advertised_game *games = client ? network_game_client_get_available_games(client) : NULL;

	return games && index >= 0 && index < MAXIMUM_ADVERTISED_GAMES && game_from_peer(&games[index]);
}

static boolean advertised_in_progress(struct advertised_game *game)
{
	return network_game_client_advertised_game_in_progress(global_network_game_client_get(), game);
}

/* (network_client_manager.c's) an advertised game's, by its place */
boolean network_game_client_port_advertised_modded(short game_index);

static void browser_games_read(void)
{
	void *client = global_network_game_client_get();
	struct advertised_game *games = client ? network_game_client_get_available_games(client) : NULL;
	short pass, index;

	multiplayer.game_count = 0;
	if (!games || multiplayer.mode == _multiplayer_mode_server_browser)
		return;
	/* (the open games, then those under way; of each, the modded games
	first, the randomizer's or skulls') */
	for (pass = 0; pass < 4; pass++)
	{
		for (index = 0; index < MAXIMUM_ADVERTISED_GAMES; index++)
		{
			struct advertised_game *game = &games[index];

			if (network_game_client_advertised_game_is_valid(game) && !advertised_in_progress(game) == (pass < 2) &&
				!network_game_client_port_advertised_modded(index) == (pass % 2 == 1) &&
				game_from_peer(game) == (multiplayer.mode == _multiplayer_mode_direct_link))
			{
				multiplayer.games[multiplayer.game_count++] = game;
			}
		}
	}
	if (multiplayer.game_chosen >= multiplayer.game_count)
		multiplayer.game_chosen = (short)MAX(0, multiplayer.game_count - 1);
}

static short browser_row_index(struct widget_instance *row)
{
	return row && !strncmp(row->name, "server_item_", 12) ? (short)(atoi(row->name + 12) - 1) : NONE;
}

/* the ping each row shows (-1: none), coloured by pc_menu_text_color: green
under PING_GOOD milliseconds, yellow under PING_FAIR, red above; a row's
status word (JOINING, NEWER, OLDER) in yellow (marked), and a game of
another network version's row grey */
static short browser_row_pings[BROWSER_ROWS];
static boolean browser_row_marked[BROWSER_ROWS];
static boolean browser_row_greyed[BROWSER_ROWS];
enum
{
	PING_GOOD = 80,
	PING_FAIR = 160,
};
#define PING_COLOR_GOOD 0x5ED38CFFUL
#define PING_COLOR_FAIR 0xE8C547FFUL
#define PING_COLOR_POOR 0xE86A5AFFUL
/* the Tag Injector's rows of what failed to load (tags_row_failed) */
#define TAG_FAILED_COLOR 0xE8C547FFUL
#define ROW_COLOR_GREYED 0x8A8F99FFUL

/* ui_widget.c: a text's colour in place of its own (FALSE: its own): a
server browser row's ping by how good it is, and the Tag Injector's rows of
what failed to load in yellow */
boolean pc_menu_text_color(struct widget_instance *widget, real_argb_color *color)
{
	short row = widget->parent ? browser_row_index(widget->parent) : NONE;
	struct widget_instance *tag_row_widget = widget->parent && (!strcmp(widget->name, "tag_name") ||
		!strcmp(widget->name, "tag_state")) ? tag_row(widget->parent) : NULL;
	short ping;
	unsigned long rgba;

	if (tag_row_widget)
	{
		short tag_row_index = (short)(atoi(tag_row_widget->name + 7) - 1);

		if (tag_row_index < 0 || tag_row_index >= TAG_ROWS || !tags_screen.row_failed[tag_row_index])
			return FALSE;
		rgba = TAG_FAILED_COLOR;
		color->red = (real)((rgba >> 24) & 0xFF) / 255.0f;
		color->green = (real)((rgba >> 16) & 0xFF) / 255.0f;
		color->blue = (real)((rgba >> 8) & 0xFF) / 255.0f;
		return TRUE;
	}
	if (row < 0 || row >= BROWSER_ROWS || strncmp(widget->name, "server_item_", 12))
		return FALSE;
	ping = browser_row_pings[row];
	if (strcmp(widget->name, "server_item_ping"))
		rgba = ROW_COLOR_GREYED;
	else if (browser_row_marked[row])
		rgba = PING_COLOR_FAIR;
	else if (ping >= 0)
		rgba = ping < PING_GOOD ? PING_COLOR_GOOD : ping < PING_FAIR ? PING_COLOR_FAIR : PING_COLOR_POOR;
	else
		rgba = ROW_COLOR_GREYED;
	if (rgba == ROW_COLOR_GREYED && !browser_row_greyed[row])
		return FALSE;
	color->red = (real)((rgba >> 24) & 0xFF) / 255.0f;
	color->green = (real)((rgba >> 16) & 0xFF) / 255.0f;
	color->blue = (real)((rgba >> 8) & 0xFF) / 255.0f;
	return TRUE;
}

/* a bar of buttons' focus, off one hidden: on its first one shown */
static void focus_off_hidden(struct widget_instance *bar)
{
	struct widget_instance *button;
	short button_index = 0;

	for (button = bar && bar->focused_child && !bar->focused_child->visible ? bar->child : NULL; button;
		button = button->next, button_index++)
	{
		if (button->visible)
		{
			bar->focused_child = button;
			bar->parameters.list.selected_index = button_index;
			break;
		}
	}
}

/* whether the browser's list's child can take focus (once visible): the
server browser's tab (its source) and column titles (its order) too */
static boolean browser_item_usable(struct widget_instance *child)
{
	return browser_row_index(child) != NONE || !strcmp(child->name, "join_game_button_bar") ||
		(multiplayer.mode == _multiplayer_mode_server_browser &&
			(!strcmp(child->name, "op_browser_mode") || !strcmp(child->name, "join_game_header")));
}

/* the browser's focus, off what cannot have it (at the start, or a row whose
game went): the first game, else the buttons (off those hidden); the row of
the game chosen is `chosen_row` (NONE: none shown) */
static void browser_focus(struct widget_instance *list, short chosen_row)
{
	char const *const choices[] = { "server_item_1", "join_game_button_bar" };
	struct widget_instance *focused = list->focused_child;
	struct widget_instance *row;
	short index;

	/* (the rows' backgrounds, as Custom Edition's: the chosen game's
	outlined in yellow, the focused one's in blue. The engine shows a list
	item's focus only on a bitmap of two frames, and theirs has three:
	normal, focused, selected) */
	for (row = list->child; row; row = row->next)
	{
		short row_index = browser_row_index(row);

		if (row_index != NONE)
			row->animation.current_frame_index = row_index == chosen_row ? 2 : row == focused ? 1 : 0;
	}
	focus_off_hidden(named(list, "join_game_button_bar", 0));
	if (focused && focused->visible && !focused->disabled)
		return;
	for (index = 0; index < NUMBEROF(choices); index++)
	{
		struct widget_instance *child;
		short child_index = 0;

		for (child = list->child; child; child = child->next, child_index++)
		{
			if (!strcmp(child->name, choices[index]) && child->visible)
			{
				list->focused_child = child;
				list->parameters.list.selected_index = child_index;
				return;
			}
		}
	}
}

/* (the server browser's: below) */
static void lobby_browser_begin(void);
static void lobby_browser_update(struct widget_instance *list);
static boolean lobby_browser_select(struct widget_instance *widget, short controller, short row,
	boolean *widget_deleted);

/* "gamespy screen init": the mode's title and parts; the games' client */
static boolean browser_initialize(struct widget_instance *screen, struct event_record *event,
	boolean *widget_deleted)
{
	char const *title = multiplayer.mode == _multiplayer_mode_lan ? "header_lan" :
		multiplayer.mode == _multiplayer_mode_direct_link ? "header_direct_link" : "header_server_browser";
	char const *const titles[] = { "header_internet", "header_lan", "header_direct_link", "header_server_browser" };
	char const *const unused[] = { "op_browser_mode", "join_game_button_update", "join_game_button_filters" };
	boolean server_browser = multiplayer.mode == _multiplayer_mode_server_browser;
	short index;

	server_browser_initialize(screen);
	for (index = 0; index < NUMBEROF(titles); index++)
		visible_set(named(screen, titles[index], 0), !strcmp(titles[index], title));
	/* (the server browser's tab, its source of games, and FILTERS are its
	own; Update stays hidden: REFRESH does what it did) */
	for (index = 0; index < NUMBEROF(unused); index++)
		visible_set(named(screen, unused[index], 0), server_browser && index != 1);
	if (server_browser && named(screen, "browser_mode_spinner", 0))
		setting_load(named(screen, "browser_mode_spinner", 0));
#ifdef LINK_PAGE
	/* (the Switch's and the PS5's clipboard is the game's own: the link comes from the link page's QR code
	instead) */
	visible_set(named(screen, "button_clipboard", 0), FALSE);
#else
	visible_set(named(screen, "button_clipboard", 0), multiplayer.mode == _multiplayer_mode_direct_link);
#endif
	visible_set(named(screen, "join_game_button_refresh", 0), multiplayer.mode == _multiplayer_mode_server_browser);
	{
		struct widget_instance *list = named(screen, "join_game_items_list", 0);
		struct widget_instance *child;

		/* (only the games' rows, the buttons and the server browser's tab
		and column titles take focus: not the ticker) */
		for (child = list ? list->child : NULL; child; child = child->next)
			child->disabled = !browser_item_usable(child);
		if (list)
			browser_focus(list, NONE);
	}
	visible_set(named(screen, "scroll_up_button", 0), FALSE);
	visible_set(named(screen, "scroll_down_button", 0), FALSE);
	/* (the columns' sort arrows, both of each over its title: LAN's and
	Direct Link's lists are not sorted; the server browser shows its order's,
	lobby_browser_sort_arrows) */
	for (index = 0; named(screen, "header_sort_arrows", index); index++)
		visible_set(named(screen, "header_sort_arrows", index), FALSE);
	multiplayer.game_chosen = 0;
	/* (the server browser's games are internet play's listings; joining
	one reaches its host, whose game the client then finds as Direct Link's) */
	if (multiplayer.mode == _multiplayer_mode_server_browser)
		lobby_browser_begin();
	return ui_widget_port_browse(screen, event, widget_deleted);
}

/* a scenario's name without its path */
static char const *scenario_name(char const *path)
{
	char const *name = strrchr(path, '\\');

	return name ? name + 1 : path;
}

/* the campaign level whose scenario a path or name is (a network co-op
game's map), else NONE */
static short campaign_level_of(char const *map_name)
{
	short level;

	/* (a Custom Edition map, custom_maps\a30, is never one) */
	if (custom_edition_level_name(map_name))
		return NONE;
	for (level = 0; level < NUMBER_OF_SINGLE_PLAYER_LEVELS; level++)
	{
		if (!_stricmp(scenario_name(main_get_solo_level_name(level)), scenario_name(map_name)))
			return level;
	}
	return NONE;
}

/* a game's type as the browsers and lobby show it: its engine's name, or
CO-OP for one with none (network co-op: ui_widget_port_cooperative_level_choose) */
static wchar_t const *game_type_name(long engine_type)
{
	return engine_type == 0 ? L"CO-OP" : engine_names[PIN(engine_type, 0, 5)];
}

/* ... or its OpenCE gametype's kind (its advertisement's or listing's:
game_modes.h), "HC" for Hardcore in the browsers' narrow column */
static wchar_t const *game_kind_name(long engine_type, byte kind, boolean abbreviated)
{
	wchar_t const *name = game_modes_kind_name(engine_type, kind, abbreviated);

	return name ? name : game_type_name(engine_type);
}

/* an advertised (LAN, Direct Link) game's kind */
static byte advertised_game_kind(struct advertised_game *game)
{
	return network_game_client_advertised_game_kind(global_network_game_client_get(), game);
}

/* a map's name as the menus show it (a campaign level's too, hosted as
network co-op; its scenario's name, if not one of theirs), from its
scenario's path or name */
static void map_display_name(char const *map_name, wchar_t *text)
{
	char const *const *names;
	short index;
	short count = xbox_multiplayer_map_count(ui_widget_port_multiplayer_maps(&names, NULL));

	/* (a Custom Edition map's, if this machine has it: custom_edition_maps.c) */
	if (custom_edition_level_name(map_name))
	{
		short display_index = custom_edition_maps_display_index(map_name);
		wchar_t const *name = display_index != NONE ? custom_edition_maps_name(display_index) : NULL;

		if (name)
		{
			ustrncpy(text, name, ROW_TEXT_LENGTH - 1);
			text[ROW_TEXT_LENGTH - 1] = 0;
			return;
		}
		map_name = scenario_name(map_name);
		count = 0;
	}
	for (index = 0; index < count; index++)
	{
		if (!_stricmp(scenario_name(names[index]), scenario_name(map_name)))
		{
			string_get("pc\\main_menu\\mp_map_list", index, text);
			return;
		}
	}
	if ((index = campaign_level_of(map_name)) != NONE)
	{
		string_get("pc\\main_menu\\map_list", index, text);
		return;
	}
	for (index = 0; map_name[index] && index < ROW_TEXT_LENGTH - 1; index++)
		text[index] = (wchar_t)(unsigned char)map_name[index];
	text[index] = 0;
}

static void randomizer_prefix_name(wchar_t *text)
{
	static wchar_t const prefix[] = L"[RNDM MOD] ";
	wchar_t shifted[ROW_TEXT_LENGTH];
	short out;
	short index;

	for (out = 0; prefix[out] && out < ROW_TEXT_LENGTH - 1; out++)
		shifted[out] = prefix[out];
	for (index = 0; text[index] && out < ROW_TEXT_LENGTH - 1; index++, out++)
		shifted[out] = text[index];
	shifted[out] = 0;
	ustrncpy(text, shifted, ROW_TEXT_LENGTH - 1);
	text[ROW_TEXT_LENGTH - 1] = 0;
}

static void game_map_name(struct advertised_game const *game, wchar_t *text)
{
	extern int network_game_client_advertised_randomizer(void const *game);

	map_display_name(game->map_name, text);
	if (network_game_client_advertised_randomizer(game))
		randomizer_prefix_name(text);
}

/* ---- Join Game > Server Browser: internet play's public games (their
hosts' listings, p2p_lobby.c: the brokers' player-hosted games and the
master servers' dedicated servers, master_list.c). Joining one joins its
invite (as Direct Link's PASTE LINK); once its host is reached its game is
among the client's, and an A press (posted) joins it, or shows its lobby if
under way.

What it shows of them is the network.browser_* settings': the top-left tab
(ALL GAMES, DEDICATED, P2P GAMES), the Filters screen (CE's, which
Network Setup's SERVER BROWSER sets too) and the column titles, which sort
the rows (again: the other way). A game of another network version is
shown greyed, NEWER or OLDER, and is not joined. The line below the rows
says where the chosen game was listed and, while it is joined, how the join
goes (p2p_join_progress); the error panel covers the list when nothing can
be listed */

#define LOBBY_BROWSER_GAMES 256
/* milliseconds: a host not reached in this long is given up (p2p's own
wait is longer) */
#define LOBBY_BROWSER_JOIN_TIMEOUT 30000
/* how long the browser looks before it says it found none, and shows a
join that failed; and before it says nothing answers */
#define LOBBY_BROWSER_LOOK_TIME 6000
#define LOBBY_BROWSER_FAILED_TIME 8000
/* seconds: a clock off by more is wrong (p2p_lobby.c's retained window; the
Switch's updater says it too, host_update.c) */
#define LOBBY_BROWSER_CLOCK_TOLERANCE 600
#define LOBBY_BROWSER_UNREACHABLE_TIME 15000
/* the line below the rows: two rows' worth */
#define TICKER_TEXT_LENGTH (ROW_TEXT_LENGTH * 2)
/* between the parts of the line below the rows */
#define TICKER_SEPARATOR L" \x00B7 "

/* the browser's words (tools/port_settings.py's BROWSER_WORDS, in order) */
#define BROWSER_WORDS "pc\\main_menu\\multiplayer_type_select\\join_game\\browser_words"
enum
{
	_browser_word_of,
	_browser_word_players,
	_browser_word_player,
	_browser_word_under_way,
	_browser_word_full,
	_browser_word_dedicated_by,
	_browser_word_listed_on,
	_browser_word_master,
	_browser_word_masters,
	_browser_word_dedicated_server,
	_browser_word_player_hosted,
	_browser_word_password,
	_browser_word_newer_build,
	_browser_word_older_build,
	_browser_word_asking,
	_browser_word_answered,
	_browser_word_connected,
	_browser_word_no_answer,
	_browser_word_no_route,
	_browser_word_refused_full,
	_browser_word_refused_closed,
	_browser_word_refused_banned,
	_browser_word_refused_password,
	_browser_word_refused_version,
	_browser_word_refused_token,
	_browser_word_strict_nat,
	_browser_word_online_off,
	_browser_word_lobby_off,
	_browser_word_looking,
	_browser_word_newer_games,
	_browser_word_none_found,
	_browser_word_none_match,
	_browser_word_masters_down,
	_browser_word_dedicated,
	_browser_word_joining,
	_browser_word_newer,
	_browser_word_older,
	_browser_word_failed,
	_browser_word_live,
	_browser_word_closed,
	_browser_word_error_online_off,
	_browser_word_error_unreachable,
	_browser_word_refused_name,
};

/* the words p2p_join_progress gives a join that failed, and the browser's
for each */
static struct
{
	char const *reason;
	short word;
} const join_failures[] =
{
	{ "no_answer", _browser_word_no_answer },
	{ "no_route", _browser_word_no_route },
	{ "full", _browser_word_refused_full },
	{ "closed", _browser_word_refused_closed },
	{ "banned", _browser_word_refused_banned },
	{ "name", _browser_word_refused_name },
	{ "password", _browser_word_refused_password },
	{ "version", _browser_word_refused_version },
	{ "token", _browser_word_refused_token },
};

/* the column titles (their widgets' names) and the order each sorts by
(NONE: none; the dedicated servers' icon's is the default order) */
static struct
{
	char const *name;
	short order;
} const sort_headers[] =
{
	{ "op_sort_header_server_name", P2P_LOBBY_SORT_NAME },
	{ "op_sort_header_map", P2P_LOBBY_SORT_MAP },
	{ "op_sort_header_type", P2P_LOBBY_SORT_TYPE },
	{ "op_sort_header_players", P2P_LOBBY_SORT_PLAYERS },
	{ "op_sort_header_locked", P2P_LOBBY_SORT_LOCKED },
	{ "op_sort_header_dedicated", P2P_LOBBY_SORT_SERVERS },
	{ "op_sort_header_classic", NONE },
	{ "op_sort_header_ping", NONE },
};

static struct
{
	struct p2p_listing games[LOBBY_BROWSER_GAMES];
	/* the games shown (of those found), the first on the rows, the one last
	focused (Join's) */
	short count, found, first, chosen;
	/* whether it has shown a game (the first has the focus) */
	boolean shown;
	unsigned long begin_time;
	/* the order (network.browser_sort's); the source the tab showed */
	short order;
	boolean reverse;
	short source;
	/* the error panel was shown since the browser opened */
	boolean error_shown;
	/* a game being joined: its host, its name, since when, the controller
	that asked; ready: its game reached (the A press posted) */
	boolean joining, ready;
	unsigned char identifier[6];
	char name[P2P_LISTING_NAME_SIZE + 1];
	unsigned long join_time;
	short controller;
	/* whether the game joined is a dedicated server's (the lobby's line) */
	boolean joining_dedicated;
	/* when the press was posted (again if it was lost) */
	unsigned long ready_time;
	/* the press was posted: the host's game may yet refuse the join, which
	the browser says when it opens again */
	boolean refusal_watched;
	/* the last that failed, when, and why (a _browser_word_*) */
	char failed_name[P2P_LISTING_NAME_SIZE + 1];
	unsigned long failed_time;
	short failed_word;
	/* a game with a password chosen: the password screen's (its own copy:
	the rows' are read again each frame), and a join it began, which the
	browser takes up again as it comes back */
	struct p2p_listing password_game;
	boolean password_joined;
} lobby_browser;

/* whether the game this machine joined last is a dedicated server's: its
advertisement's flag, or its listing's (the lobby's line, lobby_update) */
static boolean lobby_joined_dedicated;

static void lobby_dedicated_set(struct advertised_game *game, boolean listed)
{
	void *client = global_network_game_client_get();

	lobby_joined_dedicated = listed ||
		(game && client && network_game_client_advertised_game_dedicated(client, game));
}

/* one of the browser's words, of up to length characters */
static void browser_word(short word, wchar_t *text, short length)
{
	words_get(BROWSER_WORDS, word, text, length);
}

/* the word appended to the text (of size characters) */
static void text_append(wchar_t *text, short size, wchar_t const *word)
{
	size_t length = ustrlen(text);

	if ((short)length < size - 1)
		usnprintf(text + length, size - 1 - length, L"%s", word);
	text[size - 1] = 0;
}

static void text_append_word(wchar_t *text, short size, short word)
{
	wchar_t piece[TICKER_TEXT_LENGTH];

	browser_word(word, piece, NUMBEROF(piece));
	text_append(text, size, piece);
}

/* the row of the browser's chosen game, NONE if it is not on the rows */
static short browser_chosen_row(void)
{
	boolean server_browser = multiplayer.mode == _multiplayer_mode_server_browser;
	short chosen = server_browser ? lobby_browser.chosen : multiplayer.game_chosen;
	short row = server_browser ? (short)(chosen - lobby_browser.first) : chosen;

	return chosen < (server_browser ? lobby_browser.count : multiplayer.game_count) && row >= 0 &&
		row < BROWSER_ROWS ? row : NONE;
}

/* "3h 12 min", "25 min" */
static void duration_text(long seconds, wchar_t *text, short size)
{
	long minutes = (seconds + 30) / 60;

	if (minutes >= 48 * 60)
		usnprintf(text, size - 1, L"%ld days", (minutes + 12 * 60) / (24 * 60));
	else if (minutes >= 60)
		usnprintf(text, size - 1, L"%ldh %ld min", minutes / 60, minutes % 60);
	else
		usnprintf(text, size - 1, L"%ld min", minutes);
	text[size - 1] = 0;
}

/* how far this machine's clock is off, if the platform learned it and it is
wrong (p2p_lobby_clock_offset): positive if it is behind */
static boolean lobby_browser_clock_wrong(long *offset)
{
	return p2p_lobby_clock_offset(offset) &&
		(*offset > LOBBY_BROWSER_CLOCK_TOLERANCE || *offset < -LOBBY_BROWSER_CLOCK_TOLERANCE);
}

/* whether the player has been told the clock is wrong: once a run (the
dialog closing opens the browser again, which would tell them again) */
static boolean lobby_browser_clock_warned = FALSE;

/* the word of a join's failure (p2p_join_progress's reason), NONE if none */
static short join_failure_word(char const *reason)
{
	short index;

	for (index = 0; index < NUMBEROF(join_failures); index++)
	{
		if (!strcmp(reason, join_failures[index].reason))
			return join_failures[index].word;
	}
	return NONE;
}

static void lobby_browser_begin(void)
{
	long offset;
	int reverse;

	/* (a clock that is wrong sees no game: the player told, the first time
	the browser opens) */
	if (!lobby_browser_clock_warned && lobby_browser_clock_wrong(&offset))
	{
		static wchar_t text[256];
		wchar_t duration[32];

		duration_text(offset < 0 ? -offset : offset, duration, NUMBEROF(duration));
		lobby_browser_clock_warned = TRUE;
		usnprintf(text, NUMBEROF(text) - 1, L"This console's clock is %s %s, no games can be joined. Set the "
			L"console's date and time in the settings.", duration, offset < 0 ? L"ahead" : L"behind");
		text[NUMBEROF(text) - 1] = 0;
		platform_log("menus: the server browser opened with the clock %ld s %s", offset < 0 ? -offset : offset,
			offset < 0 ? "ahead" : "behind");
		display_error_text_deferred(text, NONE);
	}
	lobby_browser.count = 0;
	lobby_browser.found = 0;
	lobby_browser.first = 0;
	lobby_browser.chosen = 0;
	lobby_browser.shown = FALSE;
	lobby_browser.error_shown = FALSE;
	lobby_browser.source = NONE;
	lobby_browser.order = (short)p2p_lobby_sort_parse(config_string("network.browser_sort"), &reverse);
	lobby_browser.reverse = reverse != 0;
	/* (going on with the join the password screen began) */
	lobby_browser.joining = lobby_browser.password_joined;
	lobby_browser.ready = FALSE;
	lobby_browser.password_joined = FALSE;
	lobby_browser.begin_time = system_milliseconds();
	/* (the host's game refused the join posted last time: why, now) */
	if (lobby_browser.refusal_watched)
	{
		char reason[16];
		int stage;
		short word;

		lobby_browser.refusal_watched = FALSE;
		p2p_join_progress(&stage, reason, sizeof(reason));
		word = stage == P2P_JOIN_FAILED ? join_failure_word(reason) : NONE;
		if (word != NONE)
		{
			csmemcpy(lobby_browser.failed_name, lobby_browser.name, sizeof(lobby_browser.failed_name));
			lobby_browser.failed_word = word;
			lobby_browser.failed_time = lobby_browser.begin_time ? lobby_browser.begin_time : 1;
		}
	}
	p2p_lobby_browse(TRUE);
}

/* "gamespy screen dispose" */
static void lobby_browser_end(void)
{
	lobby_browser.joining = lobby_browser.ready = FALSE;
	p2p_lobby_browse(FALSE);
}

static void text_to_wide(char const *text, wchar_t *wide, short size)
{
	short index;

	for (index = 0; text[index] && index < size - 1; index++)
		wide[index] = (wchar_t)(unsigned char)text[index];
	wide[index] = 0;
}

/* a master server's region label as shown, in capitals (EU, NA-E) */
static void region_to_wide(char const *region, wchar_t *wide, short size)
{
	short index;

	text_to_wide(region, wide, size);
	for (index = 0; wide[index]; index++)
	{
		if (wide[index] >= 'a' && wide[index] <= 'z')
			wide[index] = (wchar_t)(wide[index] - 'a' + 'A');
	}
}

/* the games whose names are valid, as a host keeps the names of the
machines and players that join it (network_game_server_clean_name): each
name cleaned (player_name_clean), and a game whose name has nothing left
that names it left out; returns how many are left */
static short lobby_browser_valid_games(struct p2p_listing *games, short count)
{
	short read;
	short written = 0;

	for (read = 0; read < count; read++)
	{
		wchar_t name[P2P_LISTING_NAME_SIZE + 1];
		short index;

		text_to_wide(games[read].name, name, NUMBEROF(name));
		if (!player_name_clean(name, NUMBEROF(name)))
			continue;
		/* (ASCII still: the listing's names are) */
		for (index = 0; name[index]; index++)
			games[read].name[index] = (char)name[index];
		games[read].name[index] = 0;
		if (written != read)
			games[written] = games[read];
		written++;
	}
	return written;
}

/* what a game's map is to this machine (p2p_lobby_filter): a Custom
Edition map it has not can be downloaded as it joins, if map downloads are
on (its host shares it, as hosts do unless they turned it off) */
static int lobby_browser_map_state(char const *map)
{
	char const *const *names;
	short index;
	short count;

	if (custom_edition_level_name(map))
	{
		if (custom_edition_maps_display_index(map) != NONE)
			return P2P_LOBBY_MAP_INSTALLED;
		return config_boolean("network.map_download") ? P2P_LOBBY_MAP_DOWNLOADABLE : P2P_LOBBY_MAP_MISSING;
	}
	count = xbox_multiplayer_map_count(ui_widget_port_multiplayer_maps(&names, NULL));
	for (index = 0; index < count; index++)
	{
		if (!_stricmp(scenario_name(names[index]), scenario_name(map)))
			return P2P_LOBBY_MAP_INSTALLED;
	}
	return campaign_level_of(map) != NONE ? P2P_LOBBY_MAP_INSTALLED : P2P_LOBBY_MAP_MISSING;
}

/* the settings' choices of games shown (the tab's source: source) */
static void lobby_browser_choices(struct p2p_lobby_choices *choices, short source)
{
	static char const *const engines[] = { "any", "ctf", "slayer", "oddball", "king", "race" };
	char const *maps = config_string("network.browser_maps");
	char const *engine = config_string("network.browser_gametype");
	short index;

	choices->source = source;
	choices->show_empty = config_boolean("network.browser_show_empty");
	choices->show_full = config_boolean("network.browser_show_full");
	choices->engine_type = 0;
	for (index = 0; index < NUMBEROF(engines); index++)
	{
		if (!strcmp(engine, engines[index]))
			choices->engine_type = index;
	}
	choices->maps = !strcmp(maps, "no") ? P2P_LOBBY_MAPS_NO :
		!strcmp(maps, "downloadable") ? P2P_LOBBY_MAPS_DOWNLOADABLE : P2P_LOBBY_MAPS_YES;
}

/* the top-left tab: the source of the games shown (network.browser_source,
written as it changes); under network.listings = "masters" the dedicated
servers alone, under "brokers" the games players host alone (the tab and
its setting kept as they were) */
static short lobby_browser_source(struct widget_instance *list)
{
	struct widget_instance *spinner = named(list, "browser_mode_spinner", 0);
	struct pc_menu_setting *setting = spinner ? pc_menu_setting_get(spinner->definition_tag_index) : NULL;
	short source;

	if (!spinner || !setting)
		return P2P_LOBBY_SOURCE_ALL;
	/* (CE's box behind the tab, whose highlight showed it had the focus, is
	gone: its outline clashed with the screen's own. The tab is dimmed
	instead, its words and arrows, but while it has the focus) */
	spinner->alpha_modifier = list->focused_child == spinner->parent ? 1.0f : TAB_UNFOCUSED_ALPHA;
	if (master_list_policy() == _master_listings_masters)
		spinner->parameters.list.selected_index = P2P_LOBBY_SOURCE_DEDICATED;
	else if (master_list_policy() == _master_listings_brokers)
		spinner->parameters.list.selected_index = P2P_LOBBY_SOURCE_PLAYERS;
	source = (short)PIN(spinner->parameters.list.selected_index, 0, setting->value_count - 1);
	if (lobby_browser.source != NONE && source != lobby_browser.source &&
		master_list_policy() == _master_listings_both)
	{
		config_write("network.browser_source", setting->values[source]);
		/* (a list of other games: from its top) */
		lobby_browser.first = 0;
		lobby_browser.chosen = 0;
	}
	lobby_browser.source = source;
	return source;
}

/* the game being joined, once its host is reached (the client's game from
it), else NULL */
static struct advertised_game *lobby_browser_joined_game(void)
{
	void *client = global_network_game_client_get();
	struct advertised_game *games = client ? network_game_client_get_available_games(client) : NULL;
	short index;

	for (index = 0; games && index < MAXIMUM_ADVERTISED_GAMES; index++)
	{
		if (network_game_client_advertised_game_is_valid(&games[index]) &&
			!memcmp(games[index].xnaddr + 2, lobby_browser.identifier, sizeof(lobby_browser.identifier)))
		{
			return &games[index];
		}
	}
	return NULL;
}

/* the focus on a game's row (0 to BROWSER_ROWS - 1) */
static void lobby_browser_focus_row(struct widget_instance *list, short row)
{
	struct widget_instance *child;
	short child_index = 0;
	char name[16];

	sprintf(name, "server_item_%d", row + 1);
	for (child = list->child; child; child = child->next, child_index++)
	{
		if (!strcmp(child->name, name))
		{
			list->focused_child = child;
			list->parameters.list.selected_index = child_index;
			return;
		}
	}
}

/* the column titles' arrows: the one the list is sorted by shows the way
(up: A to Z, or the fewest first), in white (its bitmaps' second frame: the
first, blue, is lost on the title's blue) */
static void lobby_browser_sort_arrows(struct widget_instance *list)
{
	short index;

	for (index = 0; index < NUMBEROF(sort_headers); index++)
	{
		struct widget_instance *header = named(list, sort_headers[index].name, 0);
		struct widget_instance *arrows = named(header, "header_sort_arrows", 0);
		short order = sort_headers[index].order;
		boolean sorted = order != NONE && order == lobby_browser.order;
		/* (names, maps and types first go A to Z; players and the others
		the most first) */
		boolean up = (order == P2P_LOBBY_SORT_NAME || order == P2P_LOBBY_SORT_MAP || order == P2P_LOBBY_SORT_TYPE) !=
			lobby_browser.reverse;
		struct widget_instance *up_arrow = named(arrows, "header_sort_up_arrow", 0);
		struct widget_instance *down_arrow = named(arrows, "header_sort_down_arrow", 0);

		visible_set(arrows, sorted);
		visible_set(up_arrow, up);
		visible_set(down_arrow, !up);
		if (up_arrow)
			up_arrow->animation.current_frame_index = 1;
		if (down_arrow)
			down_arrow->animation.current_frame_index = 1;
	}
}

/* the list and the title, hidden under a panel (their backgrounds let
them through) */
static void browser_list_show(struct widget_instance *screen, boolean shown)
{
	visible_set(named(screen, "join_game_items_list", 0), shown);
	visible_set(named(screen, "header_join_game", 0), shown);
}

/* the error panel over the list, with its words (NONE: hidden, the focus
back on the list) */
static void lobby_browser_error(struct widget_instance *screen, short word)
{
	struct widget_instance *panel = named(screen, "gamespy_error_fullscreen", 0);
	struct widget_instance *list = named(screen, "join_game_items_list", 0);

	if (!panel)
		return;
	if (word != NONE)
	{
		wchar_t text[TICKER_TEXT_LENGTH];
		long tag_index = tag_loaded('ustr', BROWSER_WORDS);
		wchar_t const *string = tag_index != NONE ? unicode_string_list_get_string(tag_index, word) : NULL;

		/* (its line breaks kept: the panel's text is a box) */
		ustrncpy(text, string ? string : L"", NUMBEROF(text) - 1);
		text[NUMBEROF(text) - 1] = 0;
		text_set_length(named(panel, "error_words", 0), text, TICKER_TEXT_LENGTH);
		visible_set(named(panel, "error_text_box", 0), FALSE);
		lobby_browser.error_shown = TRUE;
		ui_play_audio_feedback_sound(SOUND_ERROR);
	}
	panel->visible = word != NONE;
	browser_list_show(screen, word == NONE);
	if (word != NONE)
		screen->focused_child = panel;
	else if (list && screen->focused_child == panel)
		screen->focused_child = list;
}

/* the Filters screen over the list (filters_show) or not: shown, it shows
the settings; its OK writes those changed ("gamespy update filter
settings"), and CANCEL or B leaves them */
static boolean setting_shown_load(struct widget_instance *spinner, struct pc_menu_setting *setting)
{
	(void)setting;
	return setting_load(spinner);
}

static void filters_show(struct widget_instance *screen, boolean show, boolean save)
{
	struct widget_instance *filters = named(screen, "filters_screen", 0);
	struct widget_instance *list = named(screen, "join_game_items_list", 0);

	if (!filters)
		return;
	if (show)
		settings_each(filters, setting_shown_load);
	else if (save && filters->visible)
		settings_each(filters, setting_changed_save);
	filters->visible = show;
	browser_list_show(screen, !show);
	if (show)
		screen->focused_child = filters;
	else if (list && screen->focused_child == filters)
		screen->focused_child = list;
}

/* "gamespy back handler" (B) and "gamespy dismiss error/filters": a panel
over the server browser closed, if one is shown; FALSE if none is */
static boolean browser_panel_close(struct widget_instance *widget)
{
	struct widget_instance *screen = screen_of(widget);
	struct widget_instance *filters = named(screen, "filters_screen", 0);
	struct widget_instance *panel = named(screen, "gamespy_error_fullscreen", 0);

	if (filters && filters->visible)
	{
		filters_show(screen, FALSE, FALSE);
		return TRUE;
	}
	if (panel && panel->visible)
	{
		lobby_browser_error(screen, NONE);
		return TRUE;
	}
	return FALSE;
}

/* Filters' help: its row's, for the value shown (the PC version's
cap_filter_options: empty and full servers' NO and YES, the game type's,
and missing maps' NO, YES and the port's IF DOWNLOADABLE) */
static void filters_help(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	static struct
	{
		char const *row;
		short first;
		boolean by_value;
	} const helps[] =
	{
		{ "op_empty_servers", 0, TRUE },
		{ "op_full_servers", 2, TRUE },
		{ "op_game_type", 5, FALSE },
		{ "op_installed_maps", 9, TRUE },
	};
	struct widget_instance *row = list->focused_child;
	short index;

	if (strcmp(list->name, "filters_options_menu"))
		return;
	for (index = 0; row && description && index < NUMBEROF(helps); index++)
	{
		struct widget_instance *spinner;

		if (strcmp(row->name, helps[index].row))
			continue;
		for (spinner = row->child; spinner && spinner->type != 2 /* spinner */; spinner = spinner->next)
			;
		description->parameters.text_box.string_list_index = (short)(helps[index].first +
			(helps[index].by_value && spinner ? spinner->parameters.list.selected_index : 0));
	}
}

/* "gamespy select header": the list sorted by the column chosen; the one
it is sorted by, the other way (network.browser_sort) */
static boolean lobby_browser_sort_choose(struct widget_instance *header)
{
	char text[32];
	short index;

	for (index = 0; index < NUMBEROF(sort_headers); index++)
	{
		if (strcmp(header->name, sort_headers[index].name))
			continue;
		if (sort_headers[index].order == NONE)
			return TRUE;
		lobby_browser.reverse = sort_headers[index].order == lobby_browser.order ? !lobby_browser.reverse : FALSE;
		lobby_browser.order = sort_headers[index].order;
		p2p_lobby_sort_text(lobby_browser.order, lobby_browser.reverse, text, sizeof(text));
		config_write("network.browser_sort", text);
		ui_play_audio_feedback_sound(SOUND_CURSOR);
		return TRUE;
	}
	return TRUE;
}

/* the line below the rows of the chosen game: its players, map, type and
region, and where it was listed (or what it needs) */
static void lobby_browser_game_lines(struct p2p_listing const *game, wchar_t *players, wchar_t *rules, short size)
{
	wchar_t word[TICKER_TEXT_LENGTH], map[ROW_TEXT_LENGTH];

	usnprintf(players, size - 1, L"%d ", game->player_count);
	text_append_word(players, size, _browser_word_of);
	usnprintf(word, NUMBEROF(word) - 1, L" %d ", game->maximum_player_count);
	text_append(players, size, word);
	text_append_word(players, size, game->maximum_player_count == 1 ? _browser_word_player : _browser_word_players);
	map_display_name(game->map, map);
	text_append(players, size, TICKER_SEPARATOR);
	text_append(players, size, map);
	text_append(players, size, TICKER_SEPARATOR);
	text_append(players, size, game_kind_name(game->engine_type, game->kind, FALSE));
	if (game->region[0])
	{
		region_to_wide(game->region, word, NUMBEROF(word));
		text_append(players, size, TICKER_SEPARATOR);
		text_append(players, size, word);
	}
	if (!game->open || game->in_progress)
	{
		text_append(players, size, TICKER_SEPARATOR);
		text_append_word(players, size, !game->open ? _browser_word_full : _browser_word_under_way);
	}
	rules[0] = 0;
	if (game->network_version != HALO_PORT_NETWORK_VERSION)
	{
		text_append_word(rules, size, game->network_version > HALO_PORT_NETWORK_VERSION ?
			_browser_word_newer_build : _browser_word_older_build);
		return;
	}
	if (game->dedicated && game->master)
	{
		/* (the publisher's name, which only this line shows: an operator
		cannot pass for a server by its name) */
		text_append_word(rules, size, game->publisher[0] ? _browser_word_dedicated_by : _browser_word_dedicated_server);
		if (game->publisher[0])
		{
			text_to_wide(game->publisher, word, NUMBEROF(word));
			text_append(rules, size, L" ");
			text_append(rules, size, word);
		}
		text_append(rules, size, L", ");
		text_append_word(rules, size, _browser_word_listed_on);
		usnprintf(word, NUMBEROF(word) - 1, L" %d ", MAX(1, game->master_count));
		text_append(rules, size, word);
		text_append_word(rules, size, game->master_count > 1 ? _browser_word_masters : _browser_word_master);
	}
	else
		text_append_word(rules, size, game->dedicated ? _browser_word_dedicated_server : _browser_word_player_hosted);
	if (game->locked)
	{
		text_append(rules, size, TICKER_SEPARATOR);
		text_append_word(rules, size, _browser_word_password);
	}
}

/* "gamespy screen update" for the server browser: the games shown, the
rows (scrolled on at their ends), the line below them, the counts over the
titles, the titles' arrows; a join going on; the error panel */
static void lobby_browser_update(struct widget_instance *list)
{
	struct widget_instance *stats = list->parameters.list.extended_description;
	struct widget_instance *screen = screen_of(list);
	struct widget_instance *row;
	short focused = browser_row_index(list->focused_child);
	wchar_t text[TICKER_TEXT_LENGTH], rules[TICKER_TEXT_LENGTH];
	unsigned long now = system_milliseconds();
	struct p2p_lobby_choices choices;
	int join_stage = P2P_JOIN_NONE;
	int masters, masters_answered, masters_asked;
	boolean masters_down;
	short chosen;

	lobby_browser.found = lobby_browser_valid_games(lobby_browser.games,
		(short)p2p_lobby_games(lobby_browser.games, LOBBY_BROWSER_GAMES));
	lobby_browser_choices(&choices, lobby_browser_source(list));
	lobby_browser.count = (short)p2p_lobby_filter(lobby_browser.games, lobby_browser.found, &choices,
		lobby_browser_map_state);
	p2p_lobby_sort(lobby_browser.games, lobby_browser.count, lobby_browser.order, lobby_browser.reverse);
	if (focused == BROWSER_ROWS - 1 && lobby_browser.first + BROWSER_ROWS < lobby_browser.count && list_scrolls_at_end())
	{
		lobby_browser.first++;
		lobby_browser_focus_row(list, --focused);
	}
	else if (focused == 0 && lobby_browser.first > 0 && list_scrolls_at_end())
	{
		lobby_browser.first--;
		lobby_browser_focus_row(list, ++focused);
	}
	lobby_browser.first = (short)PIN(lobby_browser.first, 0, MAX(0, lobby_browser.count - BROWSER_ROWS));
	/* (the first game found takes the focus from the buttons, which had it
	while there were none) */
	if (lobby_browser.count && !lobby_browser.shown)
	{
		struct widget_instance *first_row = named(list, "server_item_1", 0);

		lobby_browser.shown = TRUE;
		if (first_row)
		{
			first_row->visible = TRUE;
			lobby_browser_focus_row(list, 0);
			focused = 0;
			lobby_browser.chosen = lobby_browser.first;
		}
	}
	if (focused != NONE && !ui_widget_port_hover_focused())
		lobby_browser.chosen = (short)(lobby_browser.first + focused);
	chosen = lobby_browser.chosen;
	for (row = list->child; row; row = row->next)
	{
		short index = browser_row_index(row);
		short place;
		struct p2p_listing const *game;
		boolean other_version;

		if (index == NONE)
			continue;
		place = index;
		index = (short)(index + lobby_browser.first);
		row->visible = index < lobby_browser.count;
		if (index >= lobby_browser.count)
			continue;
		game = &lobby_browser.games[index];
		other_version = game->network_version != HALO_PORT_NETWORK_VERSION;
		text_to_wide(game->name, text, ROW_TEXT_LENGTH);
		text_set(named(row, "server_item_server_name", 0), text);
		map_display_name(game->map, text);
		if (game->randomized)
			randomizer_prefix_name(text);
		text_set(named(row, "server_item_map", 0), text);
		text_set(named(row, "server_item_type", 0), game_kind_name(game->engine_type, game->kind, TRUE));
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%d/%d", game->player_count, game->maximum_player_count);
		text_set(named(row, "server_item_players", 0), text);
		/* (its status: JOINING, NEWER or OLDER, in yellow; its host's ping,
		once probed (p2p_lobby.c), else FAILED, CLOSED, LIVE; a master
		server's, which is not probed, its region) */
		browser_row_pings[place] = game->failed || other_version ? -1 : game->ping;
		browser_row_greyed[place] = other_version;
		browser_row_marked[place] = TRUE;
		if (lobby_browser.joining && !memcmp(game->identifier, lobby_browser.identifier, 6))
			browser_word(_browser_word_joining, text, ROW_TEXT_LENGTH);
		else if (other_version)
		{
			browser_word(game->network_version > HALO_PORT_NETWORK_VERSION ? _browser_word_newer : _browser_word_older,
				text, ROW_TEXT_LENGTH);
		}
		else
		{
			browser_row_marked[place] = FALSE;
			if (game->failed)
				browser_word(_browser_word_failed, text, ROW_TEXT_LENGTH);
			else if (game->ping >= 0)
				usnprintf(text, ROW_TEXT_LENGTH - 1, L"%d", game->ping);
			else if (game->master && game->region[0])
				region_to_wide(game->region, text, ROW_TEXT_LENGTH);
			else if (!game->open)
				browser_word(_browser_word_closed, text, ROW_TEXT_LENGTH);
			else if (game->in_progress)
				browser_word(_browser_word_live, text, ROW_TEXT_LENGTH);
			else
				usnprintf(text, ROW_TEXT_LENGTH - 1, L"-");
		}
		text_set(named(row, "server_item_ping", 0), text);
		/* (the lock: a game with a password; the server: a dedicated
		server's (CE's own icon). Their bitmaps' first frame is empty, the
		PC version's for none; the next is the icon, in the menus' blue) */
		{
			struct widget_instance *lock = named(row, "server_item_locked", 0);
			struct widget_instance *dedicated = named(row, "server_item_dedicated", 0);

			visible_set(lock, game->locked);
			if (lock)
				lock->animation.current_frame_index = 1;
			visible_set(dedicated, game->dedicated);
			if (dedicated)
				dedicated->animation.current_frame_index = 1;
		}
		visible_set(named(row, "server_item_classic", 0), FALSE);
	}
	/* joining: once the host is reached, its game joined (by an A press, on
	its row); given up after a while, or once p2p says it failed */
	if (lobby_browser.ready && now - lobby_browser.ready_time > 3000 &&
		now - lobby_browser.join_time <= LOBBY_BROWSER_JOIN_TIMEOUT)
	{
		lobby_browser.ready_time = now;
		ui_widget_port_post_button(lobby_browser.controller, BUTTON_A);
	}
	if (lobby_browser.joining)
	{
		char reason[16];

		p2p_join_progress(&join_stage, reason, sizeof(reason));
		if (lobby_browser.ready)
		{
			/* (its press posted: waiting for it, or for the timeout) */
			if (now - lobby_browser.join_time > LOBBY_BROWSER_JOIN_TIMEOUT)
				lobby_browser.joining = lobby_browser.ready = FALSE;
		}
		else if (lobby_browser_joined_game())
		{
			short index;

			lobby_browser.ready = TRUE;
			lobby_browser.refusal_watched = TRUE;
			for (index = 0; index < lobby_browser.count; index++)
			{
				if (!memcmp(lobby_browser.games[index].identifier, lobby_browser.identifier, 6) &&
					index >= lobby_browser.first && index < lobby_browser.first + BROWSER_ROWS)
				{
					lobby_browser_focus_row(list, (short)(index - lobby_browser.first));
				}
			}
			lobby_browser.ready_time = now;
			ui_widget_port_post_button(lobby_browser.controller, BUTTON_A);
		}
		else if (now - lobby_browser.join_time > LOBBY_BROWSER_JOIN_TIMEOUT || join_stage == P2P_JOIN_FAILED)
		{
			/* (why: p2p's word, else the stage it got to. A strict NAT is
			what stops a host that answered: the router's UPnP, or a
			forwarded network.tunnel_port, lets it through) */
			short word = join_stage == P2P_JOIN_FAILED ? join_failure_word(reason) : NONE;

			if (word == NONE)
				word = join_stage == P2P_JOIN_ANSWERED ? _browser_word_no_route : _browser_word_no_answer;
			if (word == _browser_word_no_route && p2p_nat_strict())
				word = _browser_word_strict_nat;
			p2p_lobby_mark_failed(lobby_browser.identifier);
			csmemcpy(lobby_browser.failed_name, lobby_browser.name, sizeof(lobby_browser.failed_name));
			lobby_browser.failed_time = now ? now : 1;
			lobby_browser.failed_word = word;
			lobby_browser.joining = FALSE;
			platform_log("menus: could not reach the server browser's game %s", lobby_browser.name);
			ui_play_audio_feedback_sound(SOUND_ERROR);
		}
	}
	/* the master servers: none answered while the brokers do is told on the
	line; nothing answering at all, and internet play off, on the panel */
	if (!lobby_browser.error_shown && !config_boolean("network.online"))
		lobby_browser_error(screen, _browser_word_error_online_off);
	master_list_counts(&masters, &masters_answered, &masters_asked);
	masters_down = master_list_policy() != _master_listings_brokers && masters > 0 && masters_asked >= masters &&
		!masters_answered;
	if (!lobby_browser.error_shown && !lobby_browser.found && config_boolean("network.online") &&
		config_boolean("network.public_lobby") && now - lobby_browser.begin_time > LOBBY_BROWSER_UNREACHABLE_TIME)
	{
		boolean brokers = master_list_policy() != _master_listings_masters && p2p_lobby_brokers_reached();
		boolean masters_reached = master_list_policy() != _master_listings_brokers && masters > 0 && !masters_down;

		if (!brokers && !masters_reached)
			lobby_browser_error(screen, _browser_word_error_unreachable);
	}
	/* the line below the rows: the chosen game's (or the list's state), and
	where it was listed, or how its join goes */
	{
		wchar_t name[P2P_LISTING_NAME_SIZE + 1];
		int newer, wrong_clock;
		int hidden = p2p_lobby_hidden_games(&newer, &wrong_clock);

		text[0] = rules[0] = 0;
		if (!config_boolean("network.online"))
			browser_word(_browser_word_online_off, text, NUMBEROF(text));
		else if (!config_boolean("network.public_lobby"))
			browser_word(_browser_word_lobby_off, text, NUMBEROF(text));
		else if (!lobby_browser.count)
		{
			/* (an old build, or a wrong clock, sees none of the games: it
			says why; the line below counts them) */
			long offset;

			if (now - lobby_browser.begin_time < LOBBY_BROWSER_LOOK_TIME)
				browser_word(_browser_word_looking, text, NUMBEROF(text));
			else if (lobby_browser_clock_wrong(&offset))
			{
				wchar_t duration[32];

				duration_text(offset < 0 ? -offset : offset, duration, NUMBEROF(duration));
				usnprintf(text, NUMBEROF(text) - 1, L"The console's clock is %s %s: set it to see the games",
					duration, offset < 0 ? L"ahead" : L"behind");
			}
			else if (newer)
			{
				browser_word(_browser_word_newer_games, text, NUMBEROF(text));
				usnprintf(name, NUMBEROF(name) - 1, L" %d", newer);
				text_append(text, NUMBEROF(text), name);
			}
			else if (hidden)
				usnprintf(text, NUMBEROF(text) - 1, L"No public games this version can join");
			else
				browser_word(lobby_browser.found ? _browser_word_none_match : _browser_word_none_found, text, NUMBEROF(text));
		}
		else if (chosen < lobby_browser.count)
			lobby_browser_game_lines(&lobby_browser.games[chosen], text, rules, NUMBEROF(rules));
		if (lobby_browser.joining)
		{
			/* (its stage, and for how long: p2p_join_progress) */
			short word = join_stage == P2P_JOIN_CONNECTED || lobby_browser.ready ? _browser_word_connected :
				join_stage == P2P_JOIN_ANSWERED ? _browser_word_answered : _browser_word_asking;

			text_to_wide(lobby_browser.name, name, NUMBEROF(name));
			browser_word(word, rules, NUMBEROF(rules));
			usnprintf(text, NUMBEROF(text) - 1, L" %lu s", (now - lobby_browser.join_time) / 1000);
			text_append(rules, NUMBEROF(rules), text);
			text_set_length(named(list, "ticker_rules_info", 0), rules, TICKER_TEXT_LENGTH);
			/* (the game joined's own line, wherever the focus is) */
			text[0] = 0;
			for (chosen = 0; chosen < lobby_browser.count; chosen++)
			{
				if (!memcmp(lobby_browser.games[chosen].identifier, lobby_browser.identifier, 6))
				{
					lobby_browser_game_lines(&lobby_browser.games[chosen], text, rules, NUMBEROF(rules));
					break;
				}
			}
			if (!text[0])
				ustrncpy(text, name, NUMBEROF(text) - 1);
			chosen = lobby_browser.chosen;
			text[NUMBEROF(text) - 1] = 0;
			text_set_length(named(list, "ticker_player_info", 0), text, TICKER_TEXT_LENGTH);
		}
		else
		{
			if (lobby_browser.failed_time && now - lobby_browser.failed_time < LOBBY_BROWSER_FAILED_TIME)
			{
				text_to_wide(lobby_browser.failed_name, text, NUMBEROF(text));
				browser_word(lobby_browser.failed_word, rules, NUMBEROF(rules));
			}
			else if (masters_down && (!lobby_browser.count ||
				(chosen < lobby_browser.count && !lobby_browser.games[chosen].dedicated)))
			{
				browser_word(_browser_word_masters_down, rules, NUMBEROF(rules));
			}
			/* (the games heard that are not shown, and why) */
			else if (!lobby_browser.count && hidden && config_boolean("network.online") &&
				config_boolean("network.public_lobby"))
			{
				if (!newer || !wrong_clock)
				{
					usnprintf(rules, NUMBEROF(rules) - 1, L"%d not shown: %s", hidden,
						newer ? L"on a newer or test version" : L"their clock or this console's is wrong");
				}
				else
				{
					usnprintf(rules, NUMBEROF(rules) - 1, L"%d not shown: %d on a newer or test version, %d with a "
						L"wrong clock", hidden, newer, wrong_clock);
				}
				rules[NUMBEROF(rules) - 1] = 0;
			}
			text_set_length(named(list, "ticker_player_info", 0), text, TICKER_TEXT_LENGTH);
			text_set_length(named(list, "ticker_rules_info", 0), rules, TICKER_TEXT_LENGTH);
		}
	}
	/* the counts over the titles: the players beside the page (the tab holds
	the source), the servers and how many are dedicated */
	if (stats)
	{
		wchar_t label[ROW_TEXT_LENGTH], word[ROW_TEXT_LENGTH];
		long players = 0;
		short index, dedicated = 0;
		boolean one_page = lobby_browser.count <= BROWSER_ROWS;

		for (index = 0; index < lobby_browser.count; index++)
		{
			players += lobby_browser.games[index].player_count;
			dedicated += lobby_browser.games[index].dedicated ? 1 : 0;
		}
		/* (the page only when there are more than one, and the dedicated
		servers' count when every game is listed on one: the tab is not wide
		enough for all of them. The players as far in from the panel's left
		edge, 301, as the servers are from its right, 630 (their place is
		CE's); the page centred, at 466, between them) */
		if (named(stats, "player_count_label", 0))
			named(stats, "player_count_label", 0)->horizontal_offset = 312;
		if (named(stats, "page_count_label", 0))
			named(stats, "page_count_label", 0)->horizontal_offset = 466 - 170 / 2;
		visible_set(named(stats, "page_count_label", 0), !one_page);
		string_get(JOIN_GAME_LABELS, _join_game_label_players, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s %ld", label, players);
		text_set(named(stats, "player_count_label", 0), text);
		string_get(JOIN_GAME_LABELS, _join_game_label_page, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s %d/%d", label, chosen / BROWSER_ROWS + 1,
			MAX(1, (lobby_browser.count + BROWSER_ROWS - 1) / BROWSER_ROWS));
		text_set(named(stats, "page_count_label", 0), text);
		string_get(JOIN_GAME_LABELS, _join_game_label_servers, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s %d", label, lobby_browser.count);
		if (dedicated && one_page && lobby_browser.source == P2P_LOBBY_SOURCE_ALL)
		{
			browser_word(_browser_word_dedicated, word, NUMBEROF(word));
			usnprintf(label, ROW_TEXT_LENGTH - 1, L" (%d %s)", dedicated, word);
			text_append(text, ROW_TEXT_LENGTH, label);
		}
		text_set(named(stats, "server_count_label", 0), text);
	}
	lobby_browser_sort_arrows(list);
	browser_focus(list, browser_chosen_row());
}

/* the game's host reached by its invite (joined, once it is reached:
lobby_browser_update) */
static boolean lobby_browser_join(struct p2p_listing const *game, short controller)
{
	/* (a public game: the text chat is off in it, whatever its host says:
	network_chat.c) */
	if (!p2p_join_listing(game))
		return FALSE;
	csmemcpy(lobby_browser.identifier, game->identifier, sizeof(lobby_browser.identifier));
	csmemcpy(lobby_browser.name, game->name, sizeof(lobby_browser.name));
	lobby_browser.joining = TRUE;
	lobby_browser.ready = FALSE;
	lobby_browser.refusal_watched = FALSE;
	lobby_browser.failed_time = 0;
	lobby_browser.joining_dedicated = game->dedicated != 0;
	lobby_browser.join_time = system_milliseconds();
	lobby_browser.controller = controller;
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

/* the server browser's rows and buttons: Refresh asks the hosts and the
master servers again; Filters shows its screen; a row (or Join) joins its
game's invite, a game with a password once its screen has it, one of
another network version not at all (the player is told why); the A press
posted once its host is reached joins its game (or shows its lobby, if
under way) */
static boolean lobby_browser_select(struct widget_instance *widget, short controller, short row,
	boolean *widget_deleted)
{
	struct p2p_listing const *game;
	short index;

	/* (the press posted: whatever has the focus) */
	if (lobby_browser.ready)
	{
		struct advertised_game *found = lobby_browser_joined_game();

		lobby_browser.joining = lobby_browser.ready = FALSE;
		if (!found)
			return campaign_fail();
		lobby_dedicated_set(found, lobby_browser.joining_dedicated);
		if (advertised_in_progress(found))
		{
			/* (player 1 joining it: others join them there) */
			if (!multiplayer_player(controller))
				return FALSE;
			csmemcpy(multiplayer.preview_key_id, found->key_id, sizeof(multiplayer.preview_key_id));
			csmemcpy(multiplayer.preview_xnaddr, found->xnaddr, sizeof(multiplayer.preview_xnaddr));
			return ui_widget_port_open(widget, PREVIEW_NAME, widget_deleted);
		}
		if (!found->open)
			return campaign_fail();
		if (!multiplayer_player(controller))
			return FALSE;
		return ui_widget_port_join(widget, found, LOBBY_NAME, widget_deleted);
	}
	if (strstr(widget->name, "button_refresh"))
	{
		p2p_lobby_refresh();
		lobby_browser.begin_time = system_milliseconds();
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}
	if (strstr(widget->name, "button_filters"))
	{
		filters_show(screen_of(widget), TRUE, FALSE);
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}
	if (row == NONE && !strstr(widget->name, "button_join"))
		return TRUE;
	if (lobby_browser.joining)
		return TRUE;
	index = row != NONE ? (short)(lobby_browser.first + row) : lobby_browser.chosen;
	if (index >= lobby_browser.count)
		return campaign_fail();
	game = &lobby_browser.games[index];
	if (game->network_version != HALO_PORT_NETWORK_VERSION)
	{
		network_game_client_tell_version(game->network_version, TRUE);
		return campaign_fail();
	}
	if (!game->open || !config_boolean("network.online"))
		return campaign_fail();
	/* (a game with a password: its screen asks for it, then joins) */
	if (game->locked)
	{
		lobby_browser.password_game = *game;
		lobby_browser.controller = controller;
		return ui_widget_port_open(widget, PASSWORD_NAME, widget_deleted);
	}
	return lobby_browser_join(game, controller) || campaign_fail();
}

/* ---- the server browser's password screen (a game with a password
chosen: lobby_browser_select, tools/port_settings.py): its password typed at
once, shown as stars; JOIN GAME, or Enter, joins the game with it if it is
the game's (the browser, back again, goes on with the join), else its help
says it is not and the typing begins again */

/* its help's lines (tools/port_settings.py's) */
enum
{
	_password_help_ask = 1,
	_password_help_wrong,
	_password_help_failed,
};

static struct
{
	char text[PASSWORD_LENGTH + 1];
	short help;
} password_screen;

static void password_screen_done(char const *text)
{
	snprintf(password_screen.text, sizeof(password_screen.text), "%s", text);
}

/* "port password init" */
static boolean password_screen_initialize(struct widget_instance *list)
{
	struct widget_instance *row = named(list, "op_password", 0);

	password_screen.text[0] = 0;
	password_screen.help = _password_help_ask;
	if (row)
		text_field_begin_masked(row, "", PASSWORD_LENGTH, password_screen_done);
	return TRUE;
}

/* "port password update" */
static void password_screen_update(struct widget_instance *list)
{
	struct widget_instance *help = list->parameters.list.extended_description;
	char stars[PASSWORD_LENGTH + 1];
	short index;

	for (index = 0; password_screen.text[index] && index < PASSWORD_LENGTH; index++)
		stars[index] = '*';
	stars[index] = 0;
	text_field_show(named(list, "password_value", 0), stars, text_field_editing(named(list, "op_password", 0)));
	if (help)
		help->parameters.text_box.string_list_index = password_screen.help;
}

/* "port password join" (JOIN GAME, and Enter: password_screen_edit) */
static boolean password_screen_join(struct widget_instance *widget, boolean *widget_deleted)
{
	struct p2p_listing game = lobby_browser.password_game;
	struct widget_instance *row = named(screen_of(widget), "op_password", 0);

	if (text_field_editing(NULL))
		text_field_end(TRUE);
	if (!p2p_listing_unlock(&game, password_screen.text))
	{
		password_screen.help = _password_help_wrong;
		password_screen.text[0] = 0;
		if (row)
			text_field_begin_masked(row, "", PASSWORD_LENGTH, password_screen_done);
		return campaign_fail();
	}
	if (!lobby_browser_join(&game, lobby_browser.controller))
	{
		password_screen.help = _password_help_failed;
		return campaign_fail();
	}
	password_screen.text[0] = 0;
	lobby_browser.password_joined = TRUE;
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* "port password back" (B, Escape): back to the browser at once, though the
password is being typed */
static boolean password_screen_back(struct widget_instance *widget, boolean *widget_deleted)
{
	if (text_field_editing(NULL))
		text_field_end(FALSE);
	password_screen.text[0] = 0;
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* "port password edit" (the field's row's A: the typing begins; Enter, its
end, joins) */
static boolean password_screen_edit(struct widget_instance *row, boolean *widget_deleted)
{
	if (text_field_editing(row))
		return password_screen_join(row, widget_deleted);
	text_field_begin_masked(row, password_screen.text, PASSWORD_LENGTH, password_screen_done);
	return TRUE;
}

/* "gamespy screen update": the games' rows (name, map, gametype, players),
the one chosen's line, the counts over the titles (the PC version's: its
players, page, servers) */
static void browser_update(struct widget_instance *list)
{
#ifdef LINK_PAGE
	/* (the link page while Direct Link's page is up: link_page.c) */
	if (multiplayer.mode == _multiplayer_mode_direct_link)
		link_page_wanted();
#endif
	struct widget_instance *row;
	struct widget_instance *stats = list->parameters.list.extended_description;
	short focused = browser_row_index(list->focused_child);

	if (multiplayer.mode == _multiplayer_mode_server_browser)
	{
		lobby_browser_update(list);
		return;
	}
	browser_games_read();
	if (focused != NONE && focused < multiplayer.game_count && !ui_widget_port_hover_focused())
		multiplayer.game_chosen = focused;
	for (row = list->child; row; row = row->next)
	{
		short index = browser_row_index(row);
		struct advertised_game *game;
		wchar_t text[ROW_TEXT_LENGTH];
		int ping;

		if (index == NONE)
			continue;
		row->visible = index < multiplayer.game_count;
		if (index >= multiplayer.game_count)
			continue;
		game = multiplayer.games[index];
		ustrncpy(text, game->game_name, NUMBEROF(game->game_name) - 1);
		text[NUMBEROF(game->game_name) - 1] = 0;
		text_set(named(row, "server_item_server_name", 0), text);
		game_map_name(game, text);
		text_set(named(row, "server_item_map", 0), text);
		text_set(named(row, "server_item_type", 0),
			game_kind_name(game->engine_type, advertised_game_kind(game), TRUE));
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%d/%d", game->player_count, game->maximum_player_count);
		text_set(named(row, "server_item_players", 0), text);
		/* (a Direct Link game's host: the tunnel's ping) */
		ping = multiplayer.mode == _multiplayer_mode_direct_link ? p2p_peer_ping(game->xnaddr + 2) : -1;
		browser_row_pings[index] = (short)ping;
		browser_row_marked[index] = browser_row_greyed[index] = FALSE;
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%d", ping);
		text_set(named(row, "server_item_ping", 0), ping >= 0 ? text : advertised_in_progress(game) ? L"LIVE" : L"");
		visible_set(named(row, "server_item_locked", 0), FALSE);
		visible_set(named(row, "server_item_dedicated", 0), FALSE);
		visible_set(named(row, "server_item_classic", 0), FALSE);
	}
	{
		wchar_t text[ROW_TEXT_LENGTH * 2];

		if (multiplayer.mode == _multiplayer_mode_server_browser)
			text[0] = 0;
#ifdef LINK_PAGE
		/* (Direct Link's way in: the link page, on the local network, and its QR code above this line) */
		else if (multiplayer.mode == _multiplayer_mode_direct_link && !multiplayer.game_count)
		{
			if (link_page_address()[0])
			{
				wchar_t address[64];
				short index;

				for (index = 0; index < NUMBEROF(address) - 1 && link_page_address()[index]; index++)
					address[index] = (wchar_t)link_page_address()[index];
				address[index] = 0;
				usnprintf(text, NUMBEROF(text) - 1, L"Scan with your phone, or open %s",
					address);
			}
			else if (!config_boolean("network.link_page"))
				usnprintf(text, NUMBEROF(text) - 1, L"%s", L"The link page is off (network.link_page in config.toml)");
#ifdef HALO_PS5
			else
				usnprintf(text, NUMBEROF(text) - 1, L"%s", L"Connect the PS5 to a network to send it an invite link");
#else
			else
				usnprintf(text, NUMBEROF(text) - 1, L"%s", L"Connect the Switch to a network to send it an invite link");
#endif
		}
#endif
		else if (!multiplayer.game_count)
		{
			usnprintf(text, NUMBEROF(text) - 1, L"%s", multiplayer.mode == _multiplayer_mode_direct_link ?
				L"Copy an invite link and PASTE LINK, or accept a Discord invite" :
				L"Looking for games on your LAN...");
		}
		else
		{
			struct advertised_game *game = multiplayer.games[multiplayer.game_chosen];

			usnprintf(text, NUMBEROF(text) - 1, L"%d players of %d, on %d machines%s", game->player_count,
				game->maximum_player_count, game->machine_count, advertised_in_progress(game) ? L": under way" : L"");
		}
		text[NUMBEROF(text) - 1] = 0;
		text_set_length(named(list, "ticker_player_info", 0), text, TICKER_TEXT_LENGTH);
		text_set_length(named(list, "ticker_rules_info", 0), L"", TICKER_TEXT_LENGTH);
	}
	if (stats)
	{
		wchar_t label[ROW_TEXT_LENGTH], text[ROW_TEXT_LENGTH];
		long players = 0;
		short index;

		for (index = 0; index < multiplayer.game_count; index++)
			players += multiplayer.games[index]->player_count;
		string_get(JOIN_GAME_LABELS, _join_game_label_players, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s %ld", label, players);
		text_set(named(stats, "player_count_label", 0), text);
		/* (one page: the list holds every game found) */
		string_get(JOIN_GAME_LABELS, _join_game_label_page, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s 1/1", label);
		text_set(named(stats, "page_count_label", 0), text);
		string_get(JOIN_GAME_LABELS, _join_game_label_servers, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s %d", label, multiplayer.game_count);
		text_set(named(stats, "server_count_label", 0), text);
	}
	browser_focus(list, browser_chosen_row());
}

/* "direct ip connect go" (Direct Link's PASTE LINK): the invite
link on the clipboard reached, its game then in the list */
static boolean direct_link_from_clipboard(void)
{
	char text[TEXT_FIELD_LENGTH];
	char *link = text;
	size_t length;

	if (!platform_clipboard_get(text, sizeof(text)))
		text[0] = 0;
	while (*link == ' ' || *link == '\t' || *link == '\r' || *link == '\n')
		link++;
	for (length = strlen(link); length && (link[length - 1] == ' ' || link[length - 1] == '\t' ||
		link[length - 1] == '\r' || link[length - 1] == '\n'); length--)
		link[length - 1] = 0;
	if (!*link || !p2p_join_invite(link))
	{
		platform_log("menus: the clipboard has no invite link");
		return campaign_fail();
	}
	return TRUE;
}

/* joining the game chosen: player 1 in, then the lobby */
static boolean browser_join(struct widget_instance *widget, short controller, boolean *widget_deleted)
{
	if (multiplayer.game_chosen >= multiplayer.game_count)
		return campaign_fail();
	if (!multiplayer_player(controller))
		return FALSE;
	lobby_dedicated_set(multiplayer.games[multiplayer.game_chosen], FALSE);
	return ui_widget_port_join(widget, multiplayer.games[multiplayer.game_chosen], LOBBY_NAME, widget_deleted);
}

/* "gamespy select item" (a row: chooses its game, or joins it once chosen)
and "gamespy select button" (Join) */
static boolean browser_select(struct widget_instance *widget, struct event_record *event, short controller,
	boolean *widget_deleted)
{
	short row = browser_row_index(widget);

	/* (a row not chosen, clicked after the pointer passed over it: chosen,
	as Custom Edition's; a click on it chosen, a double click, A or JOIN
	joins its game. Not the A press a join posts: lobby_browser_select) */
	if (row != NONE && row != browser_chosen_row() && !lobby_browser.ready)
	{
		if (multiplayer.mode == _multiplayer_mode_server_browser)
			lobby_browser.chosen = (short)(lobby_browser.first + row);
		else
			multiplayer.game_chosen = row;
		ui_play_audio_feedback_sound(SOUND_CURSOR);
		return TRUE;
	}
	if (multiplayer.mode == _multiplayer_mode_server_browser)
		return lobby_browser_select(widget, controller, row, widget_deleted);
	if (row != NONE)
		multiplayer.game_chosen = row;
	if (row != NONE || strstr(widget->name, "button_join"))
	{
		/* (a game under way: its lobby first, then JOIN GAME) */
		if (multiplayer.game_chosen < multiplayer.game_count &&
			advertised_in_progress(multiplayer.games[multiplayer.game_chosen]))
		{
			/* (player 1 joining it: others join them there) */
			if (!multiplayer_player(controller))
				return FALSE;
			csmemcpy(multiplayer.preview_key_id, multiplayer.games[multiplayer.game_chosen]->key_id,
				sizeof(multiplayer.preview_key_id));
			csmemcpy(multiplayer.preview_xnaddr, multiplayer.games[multiplayer.game_chosen]->xnaddr,
				sizeof(multiplayer.preview_xnaddr));
			return ui_widget_port_open(widget, PREVIEW_NAME, widget_deleted);
		}
		return browser_join(widget, controller, widget_deleted);
	}
	return TRUE;
}

/* "player profile save changes" (Settings' OK, the profile being edited):
saved if it has changes, as the Xbox's (the saving screen follows); if it
has none (the settings' own screens write theirs to config.toml as their OK
is chosen), editing ends and the previous screen comes back, as CANCEL
(the Xbox's called that a failure, and closed every screen) */
static boolean profile_save_changes(struct widget_instance *widget, boolean *widget_deleted)
{
	if (player_ui_edit_profile_is_dirty())
	{
		if (player_ui_save_profile())
			return TRUE;
		platform_log("menus: could not save the profile's changes");
		return campaign_fail();
	}
	player_ui_end_editing_profile();
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* "port profile settings save" (Gamepads' OK in a single-player campaign:
menu_tags.c's pause_settings_patch): the profile saved at once, not on
Settings' OK, so that the campaign's next save of the player's profile
keeps it; saving makes it the player's own (player_ui_save_profile), and it
is edited again from what was saved, for Settings to go on with */
static boolean profile_settings_save(struct widget_instance *widget)
{
	long index = player_ui_get_edit_profile_index();

	settings_each(screen_of(widget), setting_changed_save);
	if (!player_ui_get_edit_player_profile() || !player_ui_edit_profile_is_dirty())
		return TRUE;
	if (!player_ui_save_profile())
	{
		platform_log("menus: could not save the profile's changes");
		return campaign_fail();
	}
	player_ui_begin_editing_profile(index);
	return TRUE;
}

/* "port pause end game" (the in-game pause menu's END GAME, the host's:
menu_tags.c's pause_patch): the game ends as its time limit would, its
players staying for the next (the carnage report, then the host's PICK GAME) */
static boolean pause_end_game(void)
{
	if (!global_network_game_server_get() || !game_engine_running())
		return campaign_fail();
	game_engine_end_game();
	return TRUE;
}

/* "port coop friendly fire" (network co-op's pause menu's FRIENDLY FIRE,
menu_tags.c): switches this machine's own friendly fire, from now on */
static boolean coop_friendly_fire_change(void)
{
	char const *choice = config_boolean("network.coop_own_friendly_fire") ? "false" : "true";

	if (!config_write("network.coop_own_friendly_fire", choice))
		platform_log("menus: could not write network.coop_own_friendly_fire to config.toml");
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

/* "port coop friendly fire text": the button's text, ON or OFF */
static void coop_friendly_fire_text(struct widget_instance *button)
{
	button->parameters.text_box.string_list_index = config_boolean("network.coop_own_friendly_fire") ? 0 : 1;
}

/* ---- the lobby: the game's players (up to the port's 128), its map and
gametype, the countdown */

static struct network_player *lobby_players[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static short lobby_player_count;

/* split screen: up to 4 players on this machine. In the lobby, a controller
not playing presses START to join ("port lobby join"), then chooses a
profile (its screen, "port lobby player choose"); ADD PLAYER ("port lobby
add player") gives one gamepad its own controller first (as it shares
player 1's: pc_menu_split_players). A player's B leaves the game alone ("port
lobby leave"; the machine's last leaves it). In game, a player's pause menu's
QUIT is theirs (the Xbox's "mp game player quit") */
static struct
{
	/* ADD PLAYER chosen: one gamepad is the new player's */
	boolean adding;
	/* the controller choosing its profile, NONE if none */
	short controller;
} lobby_join = { FALSE, NONE };

/* the client's player of the controller on this machine, NULL if none */
static struct network_player *lobby_local_player(short controller)
{
	void *client = global_network_game_client_get();
	struct network_game *game = client ? network_game_client_get_game(client) : NULL;
	short machine_index = network_game_client_get_local_machine_index();
	short index;

	for (index = 0; game && machine_index != NONE && index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
	{
		struct network_player *player = &game->players[index];

		if (network_player_is_valid(player) && (short)player->machine_index == machine_index &&
			(short)player->controller_index == controller)
		{
			return player;
		}
	}
	return NULL;
}

/* whether the controller plays (or is to: joined, not yet added) */
static boolean lobby_controller_playing(short controller)
{
	return player_ui_local_player_wants_to_play_multiplayer(controller) || lobby_local_player(controller);
}

/* this machine's players in the network game (or to be) */
static short lobby_local_player_count(void)
{
	short controller, count = 0;

	if (!global_network_game_client_get())
		return 0;
	for (controller = 0; controller < MAXIMUM_LOCAL_PLAYERS; controller++)
		count += lobby_controller_playing(controller) ? 1 : 0;
	return count;
}

/* (xinput_sdl.c) whether this machine has, or is adding, a second player: co-op,
or split screen in a network game (a player who quit keeps their part of the
screen until the game ends). Then one gamepad is its own controller, not
sharing player 1's with the keyboard */
unsigned char pc_menu_split_players(void)
{
	return player_spawn_count >= 2 || lobby_join.adding || lobby_join.controller != NONE ||
		lobby_local_player_count() >= 2 || players_port_local_player_count() >= 2;
}

/* the lobby's widget that has the focus (the one a press goes to) */
static struct widget_instance *focused_leaf(struct widget_instance *widget)
{
	while (widget->parent)
		widget = widget->parent;
	while (widget->focused_child)
		widget = widget->focused_child;
	return widget;
}

/* "port lobby open" (the lobby made, or come back to): no player being added */
static boolean lobby_join_reset(void)
{
	lobby_join.adding = FALSE;
	lobby_join.controller = NONE;
	return TRUE;
}

/* "port lobby add player" (ADD PLAYER): the next START of another controller
joins (one gamepad leaves the keyboard's controller for its own) */
static boolean lobby_add_player(void)
{
	if (lobby_local_player_count() >= MAXIMUM_LOCAL_PLAYERS)
		return campaign_fail();
	lobby_join.adding = TRUE;
	return TRUE;
}

/* "port lobby join" (START): a controller not playing joins, choosing its
profile next (FALSE: no profile screen); a player's START is the focused
button's */
static boolean lobby_join_start(struct widget_instance *widget, short controller, boolean *widget_deleted)
{
	void *client = global_network_game_client_get();
	struct network_game *game = client ? network_game_client_get_game(client) : NULL;
	short state_data;

	if (lobby_controller_playing(controller))
	{
		ui_widget_port_dispatch_event(focused_leaf(widget), BUTTON_START, controller, widget_deleted);
		return FALSE;
	}
	if (!game || network_game_client_get_state(client, &state_data) != _client_state_pregame)
		return campaign_fail();
	if (lobby_player_count >= game->maximum_players)
	{
		display_error_text_deferred(L"The game is full.", NONE);
		return campaign_fail();
	}
	lobby_join.controller = controller;
	return TRUE;
}

/* "port lobby leave" (B): a player leaves the game, the machine's last
leaving it (TRUE: back from the lobby); a controller not playing cancels
ADD PLAYER */
static boolean lobby_leave(struct widget_instance *widget, struct event_record *event, short controller,
	boolean *widget_deleted)
{
	if (!lobby_controller_playing(controller))
	{
		lobby_join_reset();
		return FALSE;
	}
	return ui_widget_port_unjoin_player(widget, event, widget_deleted);
}

/* "port lobby player list initialize": on the joining controller's profile,
else the first no other player of this machine has */
static boolean lobby_player_list_initialize(struct widget_instance *list)
{
	long mine = lobby_join.controller != NONE ? player_ui_get_active_player_profile_index(lobby_join.controller) : NONE;
	short index, controller;

	profile_list_read(TRUE);
	profile_list.chosen = 0;
	for (index = profile_list.count - 1; index >= 0; index--)
	{
		boolean taken = FALSE;

		for (controller = 0; controller < MAXIMUM_LOCAL_PLAYERS; controller++)
		{
			taken |= controller != lobby_join.controller && lobby_controller_playing(controller) &&
				player_ui_get_active_player_profile_index(controller) == profile_list.indices[index];
		}
		if (!taken)
			profile_list.chosen = index;
	}
	for (index = 0; index < profile_list.count && mine != NONE; index++)
	{
		if (profile_list.indices[index] == mine)
			profile_list.chosen = index;
	}
	profile_list.first = (short)PIN(profile_list.chosen - PROFILE_ROWS / 2, 0,
		MAX(0, profile_list.count + 1 - PROFILE_ROWS));
	focus_row(list, (short)(profile_list.chosen - profile_list.first));
	return TRUE;
}

/* "port lobby player choose": the joining controller's player on the profile
chosen (the lobby's "net splitscreen prejoin players" adds them) */
static boolean lobby_player_choose(void)
{
	short controller = lobby_join.controller;

	if (controller == NONE || profile_list.chosen >= profile_list.count ||
		!ui_widget_port_multiplayer_player(controller, profile_list.indices[profile_list.chosen]))
	{
		return campaign_fail();
	}
	return TRUE;
}

/* the lobby's line under its players: how another player joins */
static void lobby_join_help(struct widget_instance *list)
{
	struct widget_instance *help = named(screen_of(list), "lobby_join_help", 0);
	wchar_t const *text = L"";
	short controller;

	if (lobby_join.adding)
	{
		text = L"New player: press START.";
	}
	else if (lobby_local_player_count() < MAXIMUM_LOCAL_PLAYERS)
	{
		for (controller = 0; controller < MAXIMUM_LOCAL_PLAYERS; controller++)
		{
			if (input_has_gamepad(controller) && !lobby_controller_playing(controller))
				text = L"Another controller: START joins.";
		}
	}
	text_set(help, text);
}

static void lobby_row_text(short row, wchar_t *text)
{
	struct network_player *player = lobby_players[multiplayer.lobby_first + row];
	struct network_game *game = network_game_client_get_game(global_network_game_client_get());
	wchar_t name[NUMBEROF(player->name) + 1];

	ustrncpy(name, player->name, NUMBEROF(player->name));
	name[NUMBEROF(player->name)] = 0;
	if (game && game->variant.universal_variant.teams)
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s  (%s)", name, player->team_index ? L"BLUE" : L"RED");
	else
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s", name);
	/* (this machine's players, when it has more than one: their controllers) */
	if (lobby_local_player_count() >= 2 && (short)player->machine_index == network_game_client_get_local_machine_index())
	{
		size_t length = ustrlen(text);

		usnprintf(text + length, ROW_TEXT_LENGTH - 1 - length, L"  [P%d]", player->controller_index + 1);
	}
	text[ROW_TEXT_LENGTH - 1] = 0;
}

/* the lobby's panel's map: its picture and name */
static void lobby_map_show(struct widget_instance *description, char const *map_name)
{
	char const *const *names;
	short count = xbox_multiplayer_map_count(ui_widget_port_multiplayer_maps(&names, NULL)), map = 19, index;
	short level = campaign_level_of(map_name);
	struct widget_instance *widget;

	for (index = 0; index < count; index++)
	{
		if (!_stricmp(names[index], map_name))
			map = index;
	}
	/* (a Custom Edition map's picture and name, multiplayer or campaign, by
	its display index, if this machine has it: custom_edition_maps.c) */
	if (custom_edition_level_name(map_name) && custom_edition_maps_display_index(map_name) != NONE)
		map = custom_edition_maps_display_index(map_name);
	/* (a network co-op game's level: its picture and name, sp_levels' and
	map_list's, in the place of the map's) */
	visible_set(named(description, "lobby_map_pic", 0), level == NONE);
	visible_set(named(description, "lobby_map_name", 0), level == NONE);
	visible_set(named(description, "replay_level_right_pic", 0), level != NONE);
	visible_set(named(description, "replay_level_right_name", 0), level != NONE);
	if ((widget = named(description, "lobby_map_pic", 0)) != NULL)
		widget->animation.current_frame_index = map;
	if ((widget = named(description, "lobby_map_name", 0)) != NULL)
		widget->parameters.text_box.string_list_index = map;
	if (level == NONE)
		return;
	if ((widget = named(description, "replay_level_right_pic", 0)) != NULL)
		widget->animation.current_frame_index = level;
	if ((widget = named(description, "replay_level_right_name", 0)) != NULL)
		widget->parameters.text_box.string_list_index = level;
}

/* the lobby's rows' texts as last updated, and their players' machines,
for their speaker icons (menu_functions_text_box_drawn); and when */
static struct
{
	struct widget_instance *texts[LOBBY_ROWS];
	long machines[LOBBY_ROWS];
	short count;
	unsigned long updated_at;
} lobby_icons;

#ifdef LINK_PAGE
/* The QR codes (link_page.c's encoder): Direct Link's of the link page's
address, in the games' list while it has none, centred over it and drawn
from where the ticker line under the list is (UI units: the list's rows from
y 102 to 357, the ticker's text from x 100 at y 402); and Server Setup's of
the game's invite link, on its own screen (invite_copy), over its line of
text. The Switch's and the PS5's. Each picture is made again when its text changes: whole
texels a module (and the white margin a QR code wants, four modules), in a
256 by 256 texture */
enum
{
	LINK_QR_TEXTURE = 256,
	LINK_QR_MARGIN = 4,
	_link_qr_address = 0,
	_link_qr_invite,
	NUMBER_OF_LINK_QRS
};

static struct
{
	struct bitmap_data *bitmap;
	char text[128];
	/* the texels the code fills */
	short texels;
} link_qrs[NUMBER_OF_LINK_QRS];

static boolean link_qr_make(short which, char const *text)
{
	long modules = which == _link_qr_address ? link_page_qr_size() : link_page_text_qr(text);
	long side = modules + 2 * LINK_QR_MARGIN;
	long scale = modules ? LINK_QR_TEXTURE / side : 0;
	unsigned long *pixels;
	long x, y;

	if (scale < 1)
		return FALSE;
	if (!link_qrs[which].bitmap)
	{
		link_qrs[which].bitmap = bitmap_2d_new(LINK_QR_TEXTURE, LINK_QR_TEXTURE, 0, 11 /* a8r8g8b8 */);
		if (link_qrs[which].bitmap && !rasterizer_bitmap_new(link_qrs[which].bitmap))
		{
			bitmap_delete(link_qrs[which].bitmap);
			link_qrs[which].bitmap = NULL;
		}
		if (!link_qrs[which].bitmap)
			return FALSE;
	}
	pixels = (unsigned long *)link_qrs[which].bitmap->base_address;
	for (y = 0; y < LINK_QR_TEXTURE; y++)
	{
		for (x = 0; x < LINK_QR_TEXTURE; x++)
		{
			long module_x = x / scale - LINK_QR_MARGIN;
			long module_y = y / scale - LINK_QR_MARGIN;
			boolean inside = x < side * scale && y < side * scale;
			boolean dark = inside && module_x >= 0 && module_y >= 0 && module_x < modules && module_y < modules &&
				(which == _link_qr_address ? link_page_qr_dark((int)module_x, (int)module_y) :
					link_page_text_qr_dark((int)module_x, (int)module_y));

			pixels[y * LINK_QR_TEXTURE + x] = !inside ? 0 : dark ? 0xFF000000UL : 0xFFFFFFFFUL;
		}
	}
	rasterizer_bitmap_changed(link_qrs[which].bitmap);
	link_qrs[which].texels = (short)(side * scale);
	csstrncpy(link_qrs[which].text, text, sizeof(link_qrs[which].text) - 1);
	return TRUE;
}

/* the code of text, side UI units square, centred at (centre_x, centre_y) */
static void link_qr_draw_at(short which, char const *text, short centre_x, short centre_y, short side)
{
	rectangle2d square;
	rectangle2d source;

	if (!text[0])
		return;
	if ((!link_qrs[which].bitmap || strcmp(link_qrs[which].text, text)) && !link_qr_make(which, text))
		return;
	square.x0 = (short)(centre_x - side / 2);
	square.y0 = (short)(centre_y - side / 2);
	square.x1 = (short)(square.x0 + side);
	square.y1 = (short)(square.y0 + side);
	source.x0 = source.y0 = 0;
	source.x1 = source.y1 = link_qrs[which].texels;
	draw_bitmap_in_rect(link_qrs[which].bitmap, &square, &source, NULL, 0xFFFFFFFFUL, NULL, TRUE);
}

/* (Direct Link's) over the empty list, from the ticker line */
static void link_qr_draw(rectangle2d const *ticker)
{
	real scale = (real)(ticker->y1 - ticker->y0) / 18.0f;

	if (multiplayer.mode != _multiplayer_mode_direct_link || multiplayer.game_count)
		return;
	link_qr_draw_at(_link_qr_address, link_page_address(), (short)(ticker->x0 + (320 - 100) * scale),
		(short)(ticker->y0 - (402 - (102 + 357) / 2) * scale), (short)(200 * scale));
}

/* (the invite's screen) over its line of text (UI units: the text from x 40
at y 410, 560 wide; the code between the title, to y 60, and it) */
static void invite_qr_draw(rectangle2d const *line)
{
	real scale = (real)(line->x1 - line->x0) / 560.0f;
	char link[TEXT_FIELD_LENGTH];

	if (!p2p_invite_link(link, sizeof(link)))
		return;
	link_qr_draw_at(_link_qr_invite, link, (short)(line->x0 + (320 - 40) * scale),
		(short)(line->y0 - (410 - (60 + 400) / 2) * scale), (short)(320 * scale));
}
#endif

/* ---------- the main menu's news

This fork's releases' notes, from GitHub (community.c's community_news_...:
asked for once the main menu shows; until they came, or when they could not
be had, the main menu's hint is not shown and its X opens nothing), in a
window the main menu's X opens (the west face button: Y on a Switch, Square
on a PS5; tools/port_settings.py's _news_screen). It opens on the newest
release, and left and right go to the newer and the older ones. A release's
name is its title, in two rows when it has a " - " (its title, then its
description; each in the small font when the large one's is too wide), its
day and build under it, and its notes are wrapped to the window's lines,
measured in the font they are drawn in once one has been drawn
(menu_functions_text_box_drawn), and shown NEWS_ROWS lines at a time from
the top line, which up and down move; under the panel, this build's number
and whether a newer one is out. B, Back or X close it. */

#define NEWS_NAME "pc\\main_menu\\news\\news_screen"
/* the window's rows (port_settings.py's NEWS_ROWS), the most wrapped lines
kept, and the longest */
#define NEWS_ROWS 12
#define NEWS_MAXIMUM_LINES 512
#define NEWS_LINE_LENGTH 128
/* the characters a line holds until its font is known */
#define NEWS_LINE_CHARACTERS 80

static struct
{
	/* the wrapped lines, and the font, width and release they were wrapped
	to (NONE before one was drawn) */
	char lines[NEWS_MAXIMUM_LINES][NEWS_LINE_LENGTH];
	long count;
	long wrapped_font;
	short wrapped_width;
	short wrapped_release;
	/* the rows' font and width as last drawn, and the title's (the large
	one's: NONE before it was drawn) */
	long font;
	short width;
	long title_font;
	short title_width;
	/* the release shown (0 the newest), and its top line shown */
	short release;
	long first;
} news_screen = { { { 0 } }, 0, NONE, 0, NONE, NONE, 0, NONE, 0, 0, 0 };

/* whether `count` characters of text fit across a text box of the font and
width (unknown, NONE: as many as a row holds) */
static boolean news_text_fits(long font, short width, char const *text, long count)
{
	static real_argb_color const white = { 1.0f, 1.0f, 1.0f, 1.0f };
	wchar_t wide[NEWS_LINE_LENGTH];
	rectangle2d bounds, text_bounds, cursor;
	long index;

	if (count >= NEWS_LINE_LENGTH)
		return FALSE;
	if (font == NONE || width <= 0)
		return count <= NEWS_LINE_CHARACTERS;
	for (index = 0; index < count; index++)
		wide[index] = (wchar_t)(unsigned char)text[index];
	wide[count] = 0;
	bounds.x0 = 0;
	bounds.y0 = 0;
	bounds.x1 = 0x3FFF;
	bounds.y1 = 480;
	draw_string_set_draw_mode(font, NONE, 0, 0, &white);
	draw_unicode_string_compute_bounds(&bounds, wide, &text_bounds, &cursor);
	return MAX(text_bounds.x1, cursor.x0) - bounds.x0 <= width - MOD_TEXT_MARGIN;
}

/* whether `count` characters of text fit across a row */
static boolean news_fits(char const *text, long count)
{
	return news_text_fits(news_screen.font, news_screen.width, text, count);
}

static void news_add_line(char const *text, long count)
{
	if (news_screen.count >= NEWS_MAXIMUM_LINES)
		return;
	count = MIN(count, NEWS_LINE_LENGTH - 1);
	memcpy(news_screen.lines[news_screen.count], text, (size_t)count);
	news_screen.lines[news_screen.count][count] = 0;
	news_screen.count++;
}

/* text's lines added, wrapped to the rows: each a paragraph wrapped at its
spaces, a list item's ("- ...") lines after its first indented under its
text */
static void news_add_text(char const *text)
{
	char const *end = text + strlen(text);
	char const *line_end;

	for (; text < end; text = line_end + (line_end < end))
	{
		char const *paragraph = text;
		char indent[3] = "";
		long indent_length = 0;

		line_end = memchr(text, '\n', (size_t)(end - text));
		if (!line_end)
			line_end = end;
		if (line_end == text)
		{
			news_add_line("", 0);
			continue;
		}
		while (paragraph < line_end)
		{
			char line[NEWS_LINE_LENGTH];
			long room = NEWS_LINE_LENGTH - 1 - indent_length;
			long length = MIN((long)(line_end - paragraph), room);
			long cut = length;

			memcpy(line, indent, (size_t)indent_length);
			memcpy(line + indent_length, paragraph, (size_t)length);
			/* (as much as fits, cut at a space when there is one) */
			while (cut > 1 && !news_fits(line, indent_length + cut))
			{
				long space = cut - 1;

				while (space > 0 && paragraph[space] != ' ')
					space--;
				cut = space > 0 ? space : cut - 1;
			}
			news_add_line(line, indent_length + cut);
			if (!indent_length && paragraph == text && paragraph[0] == '-' && paragraph[1] == ' ')
			{
				strcpy(indent, "  ");
				indent_length = 2;
			}
			paragraph += cut;
			while (paragraph < line_end && *paragraph == ' ')
				paragraph++;
		}
	}
}

/* the release shown's notes, wrapped */
static void news_wrap(void)
{
	short release = news_screen.release;

	news_screen.count = 0;
	news_screen.wrapped_font = news_screen.font;
	news_screen.wrapped_width = news_screen.width;
	news_screen.wrapped_release = release;
	news_add_text(community_news_text(release)[0] ? community_news_text(release) : "No notes for this release.");
}

/* the top line, kept within the lines */
static void news_scroll(long by)
{
	long last = MAX(0, news_screen.count - NEWS_ROWS);

	news_screen.first = PIN(news_screen.first + by, 0, last);
}

/* another release (by: -1 the newer, 1 the older), from its top */
static boolean news_turn(short by)
{
	short release = (short)PIN(news_screen.release + by, 0, MAX(0, community_news_count() - 1));

	if (release == news_screen.release)
		return FALSE;
	news_screen.release = release;
	news_screen.first = 0;
	news_wrap();
	return TRUE;
}

/* "port news open" (the main menu's X): the window, once there is news */
static boolean news_open(struct widget_instance *widget, boolean *widget_deleted)
{
	if (!community_news_count())
		return FALSE;
	return ui_widget_port_open(widget, NEWS_NAME, widget_deleted);
}

/* "port news hint update" (the main menu's list): the hint that X opens the
news, while there is news */
static void news_hint_update(struct widget_instance *list)
{
	struct widget_instance *screen = screen_of(list);
	boolean shown = community_news_count() > 0;

	visible_set(named(screen, "news_hint", 0), shown);
	visible_set(named(screen, "x_butn_news", 0), shown);
}

/* "port news init" */
static boolean news_initialize(void)
{
	news_screen.release = 0;
	news_screen.first = 0;
	news_wrap();
	return TRUE;
}

/* an ASCII text into a text box */
static void news_text_set(struct widget_instance *text_box, char const *text)
{
	wchar_t wide[NEWS_LINE_LENGTH];
	long index;

	for (index = 0; text[index] && index < NEWS_LINE_LENGTH - 1; index++)
		wide[index] = (wchar_t)(unsigned char)text[index];
	wide[index] = 0;
	text_set(text_box, wide);
}

/* a row of the title (row: "news_title", "news_title_2"), in the large font,
else (too wide) the small one */
static void news_title_row_set(struct widget_instance *screen, char const *row, char const *text)
{
	boolean large = news_text_fits(news_screen.title_font, news_screen.title_width, text, (long)strlen(text));
	char small[32];

	snprintf(small, sizeof(small), "%s_small", row);
	news_text_set(named(screen, row, 0), large ? text : "");
	news_text_set(named(screen, small, 0), large ? "" : text);
}

/* the release's name ("Side Release V0.1.11" over "Update with upstream"),
its day and build ("Released 2026-10-27 - Build 111 (latest)"), and this
build's number and whether a newer one is out */
static void news_heading_update(struct widget_instance *screen)
{
	short release = news_screen.release;
	char const *name = community_news_name(release);
	char const *dash = strstr(name, " - ");
	long build = community_news_build(release);
	long current = community_build_number();
	long latest = community_news_latest_build();
	char text[160];

	if (dash)
	{
		snprintf(text, sizeof(text), "%.*s", (int)(dash - name), name);
		news_title_row_set(screen, "news_title", text);
		news_title_row_set(screen, "news_title_2", dash + 3);
	}
	else
	{
		news_title_row_set(screen, "news_title", "");
		news_title_row_set(screen, "news_title_2", name);
	}
	if (build != NONE)
	{
		snprintf(text, sizeof(text), "Released %s - Build %ld%s", community_news_date(release), build,
			build == latest ? " (latest)" : "");
	}
	else
		snprintf(text, sizeof(text), "Released %s", community_news_date(release));
	news_text_set(named(screen, "news_subtitle", 0), text);
	if (current <= 0)
		snprintf(text, sizeof(text), "Development build");
	else if (latest == NONE)
		snprintf(text, sizeof(text), "Current build %ld", current);
	else
		snprintf(text, sizeof(text), "Current build %ld, %s", current, current >= latest ? "up to date" : "update needed");
	news_text_set(named(screen, "news_build", 0), text);
}

/* "port news update": wrapped again once the rows' font is known, then the
rows, the title and the footer from the top line */
static void news_update(struct widget_instance *screen)
{
	wchar_t text[NEWS_LINE_LENGTH];
	long row;

	if (news_screen.font != news_screen.wrapped_font || news_screen.width != news_screen.wrapped_width ||
		news_screen.release != news_screen.wrapped_release)
	{
		news_wrap();
		news_scroll(0);
	}
	for (row = 0; row < NEWS_ROWS; row++)
	{
		struct widget_instance *line = named(screen, "news_line", row);
		long index = news_screen.first + row;
		long character;

		if (!line)
			continue;
		text[0] = 0;
		if (index < news_screen.count)
		{
			for (character = 0; news_screen.lines[index][character] && character < NEWS_LINE_LENGTH - 1; character++)
				text[character] = (wchar_t)(unsigned char)news_screen.lines[index][character];
			text[character] = 0;
		}
		text_set_length(line, text, NEWS_LINE_LENGTH);
	}
	news_heading_update(screen);
	/* (the scroll arrows and "Scroll" while there is more than the panel
	holds; the release arrows and which release it is of how many, while
	there is more than one, each arrow while there is one that way) */
	{
		boolean scrolls = news_screen.count > NEWS_ROWS;
		short count = community_news_count();
		char page[32];

		visible_set(named(screen, "dpad_up_butn_news", 0), scrolls);
		visible_set(named(screen, "dpad_down_butn_news", 0), scrolls);
		visible_set(named(screen, "news_scroll_text", 0), scrolls);
		visible_set(named(screen, "dpad_left_butn_news", 0), news_screen.release > 0);
		visible_set(named(screen, "dpad_right_butn_news", 0), news_screen.release < count - 1);
		visible_set(named(screen, "news_page_text", 0), count > 1);
		snprintf(page, sizeof(page), "%d of %d", news_screen.release + 1, count);
		for (row = 0; page[row] && row < (long)NUMBEROF(text) - 1; row++)
			text[row] = (wchar_t)page[row];
		text[row] = 0;
		text_set(named(screen, "news_page_text", 0), text);
	}
}

/* "port news close" (B, Back, X): back to the main menu */
static boolean news_close(struct widget_instance *widget, boolean *widget_deleted)
{
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* ui_widget.c's: a text box drawn, at the bounds its text was drawn in
(its font and justification still set): a lobby row's player talking in
voice chat (or muted) has a speaker just left of their name
(port/linux/game/network_voice.c) */
void menu_functions_text_box_drawn(struct widget_instance *widget, rectangle2d const *bounds)
{
	short row;

#ifdef LINK_PAGE
	if (widget->name && !strcmp(widget->name, "ticker_player_info"))
		link_qr_draw(bounds);
	if (widget->name && !strcmp(widget->name, "invite_qr_text"))
		invite_qr_draw(bounds);
#endif
	/* (the news's rows: the font and width its notes are wrapped to; and
	its title's, which a release's name is measured in) */
	if (widget->name && !strcmp(widget->name, "news_line"))
	{
		news_screen.font = draw_string_get_font();
		news_screen.width = (short)(bounds->x1 - bounds->x0);
		return;
	}
	if (widget->name && (!strcmp(widget->name, "news_title") || !strcmp(widget->name, "news_title_2")))
	{
		news_screen.title_font = draw_string_get_font();
		news_screen.title_width = (short)(bounds->x1 - bounds->x0);
		return;
	}
	/* (the Mods screen's texts: the fonts and widths their lines are
	measured in, mods_text_fits) */
	for (row = 0; widget->name && row < NUMBER_OF_MOD_TEXT_BOXES; row++)
	{
		if (!strcmp(widget->name, mod_text_box_names[row]) || !strcmp(widget->name, tag_text_box_names[row]))
		{
			mods_screen.fonts[row] = draw_string_get_font();
			mods_screen.widths[row] = (short)(bounds->x1 - bounds->x0);
			return;
		}
	}

	/* (as the lobby last updated them: the rows' widgets go with it) */
	if (!lobby_icons.count || system_milliseconds() - lobby_icons.updated_at > 250)
		return;
	for (row = 0; row < lobby_icons.count; row++)
	{
		long machine_index = lobby_icons.machines[row];
		rectangle2d icon;
		short size;

		if (lobby_icons.texts[row] != widget)
			continue;
		if (!network_voice_machine_speaking(machine_index) && !network_voice_machine_muted(machine_index))
			return;
		size = (short)MIN(14, bounds->y1 - bounds->y0);
		icon.x1 = (short)(bounds->x1 - 4);
		icon.y0 = (short)((bounds->y0 + bounds->y1 - size) / 2);
		/* (the name's ink: the icon just left of it, centred on its
		capitals) */
		if (widget->parameters.text_box.text && widget->parameters.text_box.text[0])
		{
			rectangle2d text;
			rectangle2d cursor;

			draw_unicode_string_compute_bounds(bounds, widget->parameters.text_box.text, &text, &cursor);
			if (text.x0 - size - 6 >= bounds->x0)
				icon.x1 = (short)(text.x0 - 6);
			icon.y0 = (short)(draw_unicode_string_capital_middle(bounds, widget->parameters.text_box.text) - size / 2);
		}
		icon.x0 = (short)(icon.x1 - size);
		icon.y1 = (short)(icon.y0 + size);
		network_voice_draw_icon(&icon, network_voice_machine_muted(machine_index), 1.0f);
		return;
	}
}

/* the lobby's words (tools/port_settings.py's lobby_words) */
#define LOBBY_WORDS "pc\\main_menu\\multiplayer_type_select\\lobby\\lobby_words"
enum
{
	_lobby_word_dedicated,
};

/* "port lobby update" */
static void lobby_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	void *client = global_network_game_client_get();
	struct network_game *game = client ? network_game_client_get_game(client) : NULL;
	short state_data, state = client ? network_game_client_get_state(client, &state_data) : NONE;
	wchar_t text[LOBBY_TEXT_LENGTH];
	short index;

	lobby_player_count = 0;
	for (index = 0; game && state >= _client_state_pregame && index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
	{
		if (network_player_is_valid(&game->players[index]))
			lobby_players[lobby_player_count++] = &game->players[index];
	}
	list_scroll(list, &multiplayer.lobby_first, lobby_player_count, LOBBY_ROWS);
	if (multiplayer.lobby_first > MAX(0, lobby_player_count - LOBBY_ROWS))
		multiplayer.lobby_first = (short)MAX(0, lobby_player_count - LOBBY_ROWS);
	rows_update(list, (short)MIN(lobby_player_count, LOBBY_ROWS), lobby_row_text);
	/* (the rows' players, for their speaker icons) */
	{
		struct widget_instance *row = list->child;

		lobby_icons.count = 0;
		for (index = 0; row && index < MIN(lobby_player_count, LOBBY_ROWS); row = row->next, index++)
		{
			if (strncmp(row->name, "list_item_", 10))
				break;
			lobby_icons.texts[index] = named(row, "list_item_text", 0);
			lobby_icons.machines[index] = lobby_players[multiplayer.lobby_first + index]->machine_index;
			lobby_icons.count = (short)(index + 1);
		}
		lobby_icons.updated_at = system_milliseconds();
	}
	lobby_join_help(list);
	visible_set(named(list, "lobby_button_team", 0), game && game->variant.universal_variant.teams);
	/* (the buttons' focus, off Switch Team when it is hidden) */
	focus_off_hidden(named(list, "lobby_button_bar", 0));
	visible_set(named(description, "lobby_right_item", 0), game != NULL && state >= _client_state_pregame);
	if (!game || state < _client_state_pregame)
	{
		profile_name_show(description);
		return;
	}
	lobby_map_show(description, game->map.name);
	{
		short seconds = network_game_client_get_seconds_to_game_start(client);
		char link[TEXT_FIELD_LENGTH];
		wchar_t gametype[NUMBEROF(game->variant.human_readable_game_description) + 1];
		/* (an OpenCE gametype's kind: game_modes.c) */
		wchar_t const *kind = game_modes_variant_kind_name(&game->variant, &game->variant_options);
		/* (a dedicated server's game: its name first, and it waits for
		players while its director holds the countdown) */
		boolean dedicated = lobby_joined_dedicated && !global_network_game_server_get();
		wchar_t name[NUMBEROF(game->name) + ROW_TEXT_LENGTH];

		name[0] = 0;
		if (dedicated)
		{
			wchar_t word[ROW_TEXT_LENGTH];

			ustrncpy(name, game->name, NUMBEROF(game->name));
			name[NUMBEROF(game->name)] = 0;
			words_get(LOBBY_WORDS, _lobby_word_dedicated, word, NUMBEROF(word));
			text_append(name, NUMBEROF(name), TICKER_SEPARATOR);
			text_append(name, NUMBEROF(name), word);
			text_append(name, NUMBEROF(name), L"\r\n");
		}
		ustrncpy(gametype, game->variant.human_readable_game_description, NUMBEROF(gametype) - 1);
		gametype[NUMBEROF(gametype) - 1] = 0;
		usnprintf(text, NUMBEROF(text) - 1, L"%s%s\r\n%s\r\n%d of %d players\r\n\r\n%s", name, gametype,
			kind ? kind : engine_names[PIN(game->variant.game_engine_index, 0, 5)], lobby_player_count,
			game->maximum_players,
			seconds > 0 ? L"Starting in:" : (game->machine_count < 2 && lobby_player_count < 2) || dedicated ?
				L"Waiting for players" : L"");
		text[NUMBEROF(text) - 1] = 0;
		if (seconds > 0)
		{
			size_t length = ustrlen(text);

			usnprintf(text + length, NUMBEROF(text) - 1 - length, L" %d", seconds);
		}
		if (global_network_game_server_get() && p2p_invite_link(link, sizeof(link)))
		{
			size_t length = ustrlen(text);

			usnprintf(text + length, NUMBEROF(text) - 1 - length, L"\r\n\r\nInvite link copied:\r\npaste it to friends");
		}
		text_set_length(named(description, "lobby_game_data", 0), text, LOBBY_TEXT_LENGTH);
	}
	profile_name_show(description);
}

/* ---- an in-progress game's lobby, before joining it (Direct Link and
LAN's rows of games under way): what its advertisement tells (no players'
names: they come with joining), JOIN GAME. Split screen players join here
as in the lobby (lobby_join), for the game starts at once for a machine
that joins it: player 1 as it opens, the others with START */

/* the previewed game, found again among the client's (NULL: gone) */
static struct advertised_game *preview_game(void)
{
	void *client = global_network_game_client_get();
	struct advertised_game *games = client ? network_game_client_get_available_games(client) : NULL;
	short index;

	for (index = 0; games && index < MAXIMUM_ADVERTISED_GAMES; index++)
	{
		if (network_game_client_advertised_game_is_valid(&games[index]) &&
			!csmemcmp(games[index].key_id, multiplayer.preview_key_id, sizeof(multiplayer.preview_key_id)) &&
			!csmemcmp(games[index].xnaddr, multiplayer.preview_xnaddr, sizeof(multiplayer.preview_xnaddr)))
		{
			return &games[index];
		}
	}
	return NULL;
}

/* the preview's status's end: the players joining from here, when there
are more than one (split screen) */
static void preview_players_text(wchar_t *text, short size)
{
	short controller;
	size_t length;

	if (lobby_local_player_count() < 2)
		return;
	length = ustrlen(text);
	usnprintf(text + length, size - 1 - length, L"\r\n\r\nJoining from here:");
	for (controller = 0; controller < MAXIMUM_LOCAL_PLAYERS; controller++)
	{
		struct player_profile profile;
		wchar_t name[NUMBEROF(profile.player_name) + 1];

		if (!player_ui_local_player_wants_to_play_multiplayer(controller))
			continue;
		player_ui_get_active_player_profile(controller, &profile);
		ustrncpy(name, profile.player_name, NUMBEROF(profile.player_name));
		name[NUMBEROF(profile.player_name)] = 0;
		text[size - 1] = 0;
		length = ustrlen(text);
		usnprintf(text + length, size - 1 - length, L"\r\n%s  [P%d]", name, controller + 1);
	}
	text[size - 1] = 0;
}

/* "port lobby preview update" */
static void preview_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct advertised_game *game = preview_game();
	boolean valid = game != NULL;
	wchar_t text[LOBBY_TEXT_LENGTH], name[NUMBEROF(game->game_name) + 1];

	visible_set(named(description, "lobby_right_item", 0), valid);
	if (valid)
	{
		lobby_map_show(description, game->map_name);
		usnprintf(text, NUMBEROF(text) - 1, L"%s\r\n%d of %d players\r\non %d machines",
			game_kind_name(game->engine_type, advertised_game_kind(game), FALSE), game->player_count,
			game->maximum_player_count, game->machine_count);
		text[NUMBEROF(text) - 1] = 0;
		text_set_length(named(description, "lobby_game_data", 0), text, LOBBY_TEXT_LENGTH);
		ustrncpy(name, game->game_name, NUMBEROF(game->game_name));
		name[NUMBEROF(game->game_name)] = 0;
		/* (the text box does not wrap: lines of up to 28 characters) */
		usnprintf(text, NUMBEROF(text) - 1, L"%s\r\n\r\nThis game is under way.\r\n\r\n%s", name,
			game->open ? L"JOIN GAME joins it now;\r\nits players show then." :
			L"It cannot be joined now:\r\nit is loading, over or full.");
		text[NUMBEROF(text) - 1] = 0;
		preview_players_text(text, NUMBEROF(text));
	}
	else
		usnprintf(text, NUMBEROF(text) - 1, L"The game is gone.");
	text[NUMBEROF(text) - 1] = 0;
	text_set_length(named(list, "preview_status", 0), text, LOBBY_TEXT_LENGTH);
	lobby_join_help(list);
	profile_name_show(description);
}

/* "port lobby preview add" (START): as the lobby's ("port lobby join"), a
controller not joining chooses its profile, to join with the others; a
joining player's START is the focused button's */
static boolean preview_add(struct widget_instance *widget, short controller, boolean *widget_deleted)
{
	struct advertised_game *game = preview_game();
	short count = lobby_local_player_count();

	if (lobby_controller_playing(controller))
	{
		ui_widget_port_dispatch_event(focused_leaf(widget), BUTTON_START, controller, widget_deleted);
		return FALSE;
	}
	if (!game || !game->open || count >= MAXIMUM_LOCAL_PLAYERS)
		return campaign_fail();
	if (game->player_count + count >= game->maximum_player_count)
	{
		display_error_text_deferred(L"The game is full.", NONE);
		return campaign_fail();
	}
	lobby_join.controller = controller;
	return TRUE;
}

/* "port lobby preview leave" (B): a joining player stays out, the last of
them backing out (TRUE); a controller not joining cancels ADD PLAYER, else
backs out for them all (the game is not joined yet: none is kept here) */
static boolean preview_leave(short controller)
{
	short index;

	if (!lobby_controller_playing(controller))
	{
		if (lobby_join.adding)
		{
			lobby_join_reset();
			return FALSE;
		}
		for (index = 0; index < MAXIMUM_LOCAL_PLAYERS; index++)
			player_ui_local_player_left_multiplayer_game(index);
		return TRUE;
	}
	player_ui_local_player_left_multiplayer_game(controller);
	return lobby_local_player_count() == 0;
}

/* "port lobby preview join": with the players joining from here (player 1,
as the preview opened, and those added), whom the lobby asks the host for
all at once ("net splitscreen prejoin players"): it starts the machine in
the game once it has them all */
static boolean preview_join(struct widget_instance *widget, short controller, boolean *widget_deleted)
{
	struct advertised_game *game = preview_game();

	if (!game || !game->open)
		return campaign_fail();
	if (!lobby_local_player_count() && !multiplayer_player(controller))
		return FALSE;
	lobby_dedicated_set(game, lobby_joined_dedicated && p2p_joined_listed_host());
	return ui_widget_port_join(widget, game, LOBBY_NAME, widget_deleted);
}

/* ---- the gametype editor: Edit Gametypes' list (the built-in gametypes
and those saved), the gametype being edited (player_ui's) or Server Setup's
(a copy of the game's gametype), its options' screens: each option a
spinner, found by its name (the PC version's screens put them in another
order than the Xbox's, whose functions of the same names walk them by
place) */

boolean ui_widget_port_gametype_edit_begin(long profile_index);
boolean ui_widget_port_gametype_save(struct widget_instance *widget, boolean *widget_deleted);
boolean ui_widget_port_game_variant_set(struct game_variant *variant, struct game_variant_options const *options);
boolean ui_widget_port_gametype_delete(long profile_index);

#define GAMETYPE_EDIT_ROWS 11

static struct
{
	/* the list's */
	long gametypes[MAXIMUM_GAMETYPES];
	short count, first, chosen;
	boolean stale;
	long deleting;
	/* Server Setup's copy of the game's gametype, edited in place of
	player_ui's (setup) */
	boolean setup;
	struct game_variant setup_variant;
	struct game_variant_options setup_options;
	/* the vehicle screen's side shown */
	short vehicle_side;
} gametype_edit = { { 0 }, 0, 0, 0, TRUE, NONE, FALSE };

static struct game_variant *edit_variant(void)
{
	return gametype_edit.setup ? &gametype_edit.setup_variant : player_ui_get_edit_playlist_profile();
}

static struct game_variant_options *edit_options(void)
{
	return gametype_edit.setup ? &gametype_edit.setup_options : player_ui_get_edit_playlist_options();
}

enum
{
	_option_long,		/* a long of the variant */
	_option_byte,		/* a byte of the variant */
	_option_flag,		/* a bit of the variant's flags (argument), set by value 1 */
	_option_health,		/* the variant's health, tenths */
	_option_short,		/* a short of the options */
	_option_option_byte,	/* a byte of the options */
	_option_radar,		/* the options' radar players, and the variant's flag */
	_option_setting		/* the host's own (config.toml): Server Setup's copy only */
};

struct gametype_option
{
	char const *spinner;
	short kind;
	short offset;
	unsigned long argument;
	short count;
	long values[16];
	/* (_option_setting: the setting, and its values' text) */
	char const *setting;
	char const *setting_values[16];
};

#define VARIANT_FIELD(field) (short)offsetof(struct game_variant, field)
#define OPTIONS_FIELD(field) (short)offsetof(struct game_variant_options, field)
#define TIME_LIMITS { 0, 10, 15, 20, 25, 30, 45 }

static struct gametype_option const gametype_options[] =
{
	/* OPENCE OPTIONS (OpenCE's gametypes: game_modes.c) */
	{ "oce_mode_spinner", _option_option_byte, OPTIONS_FIELD(mode), 0, 4,
		{ _game_mode_none, _game_mode_swat, _game_mode_infection, _game_mode_gun_game } },
	{ "oce_hardcore_spinner", _option_option_byte, OPTIONS_FIELD(hardcore), 0, 2, { FALSE, TRUE } },
	{ "inf_zombies_spinner", _option_option_byte, OPTIONS_FIELD(infection_initial_zombies), 0, 6, { 1, 2, 3, 4, 8, 16 } },
	{ "inf_speed_spinner", _option_option_byte, OPTIONS_FIELD(infection_zombie_speed), 0, 6,
		{ 100, 110, 125, 150, 175, 200 } },
	{ "inf_damage_spinner", _option_option_byte, OPTIONS_FIELD(infection_skull_damage), 0, 4, { 100, 150, 200, 250 } },
	{ "inf_respawn_spinner", _option_option_byte, OPTIONS_FIELD(infection_respawn_time), 0, 6, { 1, 3, 5, 10, 15, 30 } },
	{ "inf_countdown_spinner", _option_option_byte, OPTIONS_FIELD(infection_countdown), 0, 5, { 0, 5, 10, 15, 30 } },
	{ "inf_shields_spinner", _option_option_byte, OPTIONS_FIELD(infection_zombie_shields), 0, 2, { FALSE, TRUE } },
	{ "gg_order_spinner", _option_option_byte, OPTIONS_FIELD(gun_game_order), 0, 3,
		{ _gun_game_order_strongest_first, _gun_game_order_weakest_first, _gun_game_order_random } },
	{ "gg_melee_spinner", _option_option_byte, OPTIONS_FIELD(gun_game_melee_demotes), 0, 2, { FALSE, TRUE } },
	/* player options */
	{ "number_of_lives_spinner", _option_long, VARIANT_FIELD(universal_variant.lives), 0, 4, { 0, 1, 3, 5 } },
	{ "maximum_health_spinner", _option_health, 0, 0, 6, { 5, 10, 15, 20, 30, 40 } },
	{ "shields_spinner", _option_flag, 0, FLAG(_game_variant_no_shields_bit), 2, { 0, 1 } },
	{ "respawn_time_spinner", _option_long, VARIANT_FIELD(universal_variant.respawn_time), 0, 4, { 0, 150, 300, 450 } },
	{ "respawn_time_growth_spinner", _option_long, VARIANT_FIELD(universal_variant.respawn_time_growth), 0, 4,
		{ 0, 150, 300, 450 } },
	{ "odd_man_out_spinner", _option_byte, VARIANT_FIELD(universal_variant.odd_man_out), 0, 2, { 1, 0 } },
	{ "invisible_players_spinner", _option_flag, 0, FLAG(_game_variant_always_invisible_bit), 2, { 1, 0 } },
	{ "suicide_penalty_spinner", _option_long, VARIANT_FIELD(universal_variant.suicide_penalty), 0, 4,
		{ 0, 150, 300, 450 } },
	/* item options (weapon sets: the PC's list, then the Xbox's NO GRENADES) */
	{ "item_options_infinite_grenades_spinner", _option_flag, 0, FLAG(_game_variant_infinite_grenades_bit), 2,
		{ 1, 0 } },
	{ "item_options_weapon_set_spinner", _option_long, VARIANT_FIELD(universal_variant.weapon_set), 0, 14,
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 10 } },
	{ "item_options_starting_equipment_spinner", _option_flag, 0, FLAG(_game_variant_generic_starting_equipment_bit),
		2, { 0, 1 } },
	{ "map_weapons_spinner", _option_option_byte, OPTIONS_FIELD(no_map_weapons), 0, 2, { 0, 1 } },
	/* (the loadout: the weapon set's, or each player's two weapons) */
	{ "loadout_spinner", _option_option_byte, OPTIONS_FIELD(loadout), 0, 2, { _loadout_category, _loadout_custom } },
	{ "primary_weapon_spinner", _option_option_byte, OPTIONS_FIELD(primary_weapon), 0, NUMBER_OF_LOADOUT_WEAPONS,
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 } },
	{ "secondary_weapon_spinner", _option_option_byte, OPTIONS_FIELD(secondary_weapon), 0, NUMBER_OF_LOADOUT_WEAPONS,
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 } },
	/* indicator options */
	{ "indicator_options_radar display_spinner", _option_long, VARIANT_FIELD(universal_variant.goal_radar), 0, 3,
		{ 0, 1, 2 } },
	{ "indicator_options_players_on_radar_spinner", _option_radar, OPTIONS_FIELD(radar_players), 0, 3,
		{ _radar_players_all, _radar_players_friends, _radar_players_none } },
	{ "indicator_options_friends_on_screen_spinner", _option_flag, 0, FLAG(_game_variant_allow_friendly_navpoints_bit),
		2, { 1, 0 } },
	/* capture the flag */
	{ "assault_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.ctf.assault), 0, 2, { 1, 0 } },
	{ "single_flag_spinner", _option_long, VARIANT_FIELD(game_engine_variant.ctf.single_flag_time), 0, 6,
		{ 0, 1800, 3600, 5400, 9000, 18000 } },
	{ "flag_must_reset_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.ctf.flag_must_reset), 0, 2,
		{ 1, 0 } },
	{ "flag_at_home_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.ctf.flag_at_home_to_score), 0, 2,
		{ 1, 0 } },
	{ "captures_to_win_spinner", _option_long, VARIANT_FIELD(universal_variant.score_to_win), 0, 5,
		{ 1, 3, 5, 10, 15 } },
	{ "time_limit_spinner", _option_short, OPTIONS_FIELD(time_limit), 0, 7, TIME_LIMITS },
	/* king of the hill */
	{ "koth_moving_hill_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.king.moving_hill), 0, 2, { 1, 0 } },
	{ "koth_score_to_win_spinner", _option_long, VARIANT_FIELD(universal_variant.score_to_win), 0, 5,
		{ 1, 2, 5, 10, 15 } },
	{ "koth_team_play_spinner", _option_byte, VARIANT_FIELD(universal_variant.teams), 0, 2, { 1, 0 } },
	/* oddball */
	{ "trait_with_ball_spinner", _option_long, VARIANT_FIELD(game_engine_variant.oddball.trait_with_ball), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "trait_without_ball_spinner", _option_long, VARIANT_FIELD(game_engine_variant.oddball.trait_without_ball), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "speed_with_ball_spinner", _option_long, VARIANT_FIELD(game_engine_variant.oddball.speed_with_ball), 0, 3,
		{ 1, 0, 2 } },
	{ "ball_type_spinner", _option_long, VARIANT_FIELD(game_engine_variant.oddball.oddball_ball_type), 0, 3,
		{ 0, 1, 2 } },
	{ "random_start_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.oddball.random_start), 0, 2, { 1, 0 } },
	{ "ball_spawn_count_spinner", _option_long, VARIANT_FIELD(game_engine_variant.oddball.ball_spawn_count), 0, 16,
		{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 } },
	{ "score_to_win_spinner", _option_long, VARIANT_FIELD(universal_variant.score_to_win), 0, 5, { 1, 2, 5, 10, 15 } },
	/* (Slayer's, Oddball's and Race's) */
	{ "team_play_spinner", _option_byte, VARIANT_FIELD(universal_variant.teams), 0, 2, { 1, 0 } },
	/* race */
	{ "team_scoring_spinner", _option_long, VARIANT_FIELD(game_engine_variant.race.team_scoring), 0, 3, { 0, 1, 2 } },
	{ "race_type_spinner", _option_long, VARIANT_FIELD(game_engine_variant.race.race_type), 0, 3, { 0, 1, 2 } },
	{ "laps_to_win_spinner", _option_long, VARIANT_FIELD(universal_variant.score_to_win), 0, 6,
		{ 1, 3, 5, 10, 15, 25 } },
	/* slayer (the PC's rows' names and labels are crossed: kill_penalty's
	row is labelled KILL IN ORDER, kill_in_order's KILL PENALTY; each sets
	what its label says) */
	{ "death_bonus_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.slayer.no_death_bonus), 0, 2, { 0, 1 } },
	{ "kill_penalty_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.slayer.kill_in_order), 0, 2, { 1, 0 } },
	{ "kill_in_order_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.slayer.no_kill_penalty), 0, 2,
		{ 0, 1 } },
	/* (up to 500, for big games: tools/port_settings.py's STRING_INSERTS) */
	{ "kills_to_win_spinner", _option_long, VARIANT_FIELD(universal_variant.score_to_win), 0, 11,
		{ 5, 10, 15, 25, 50, 75, 100, 150, 200, 250, 500 } },
	/* team options */
	{ "friendly_fire_spinner", _option_short, OPTIONS_FIELD(friendly_fire), 0, 4,
		{ _friendly_fire_off, _friendly_fire_on, _friendly_fire_shields_only, _friendly_fire_explosives_only } },
	{ "friendly_fire_penalty_spinner", _option_short, OPTIONS_FIELD(friendly_fire_penalty), 0, 4, { 0, 5, 10, 15 } },
	{ "autobalance_spinner", _option_option_byte, OPTIONS_FIELD(auto_team_balance), 0, 2, { 0, 1 } },
	/* (the host's voice chat, below them: port/linux/game/network_voice.c;
	tools/port_settings.py's TEAMPLAY_ROWS) */
	{ "voice_mode_spinner", _option_setting, 0, 0, 5, { 0 }, "network.voice_mode",
		{ "off", "team_proximity", "team_enemy_proximity", "team_global", "team_global_enemy_proximity" } },
	{ "voice_lobby_spinner", _option_setting, 0, 0, 2, { 0 }, "network.voice_lobby", { "true", "false" } },
	{ "voice_kbps_spinner", _option_setting, 0, 0, 7, { 0 }, "network.voice_kbps",
		{ "8", "12", "16", "24", "32", "48", "64" } },
	{ "voice_proximity_spinner", _option_setting, 0, 0, 6, { 0 }, "network.voice_proximity",
		{ "5", "10", "15", "20", "30", "50" } },
	/* (and the players' votes to kick: port/linux/game/network_votekick.c) */
	{ "votekick_spinner", _option_setting, 0, 0, 2, { 0 }, "network.votekick", { "true", "false" } },
	/* (and the text chat: port/linux/game/network_chat.c) */
	{ "text_chat_spinner", _option_setting, 0, 0, 3, { 0 }, "network.text_chat", { "on", "links", "off" } },
	{ "aim_assist_spinner", _option_setting, 0, 0, 2, { 0 }, "network.block_mouse_aim_assist", { "false", "true" } },
	/* (the shield effects every machine draws: network_shield_effects.c) */
	{ "shield_effects_spinner", _option_setting, 0, 0, 7, { 0 }, "network.shield_effects",
		{ "stock", "glow", "light_show", "overshield", "glow_overshield", "light_show_overshield", "players" } },
	/* co-op's options (Server Setup's CO-OP OPTIONS: tools/port_settings.py's
	COOP_SETUP_SCREENS; coop_enemies.c) */
	{ "coop_friendly_fire_spinner", _option_setting, 0, 0, 4, { 0 }, "network.coop_friendly_fire",
		{ "off", "on", "shields_only", "explosives_only" } },
	{ "coop_extra_enemies_spinner", _option_setting, 0, 0, 3, { 0 }, "network.coop_enemies_mode",
		{ "per_player", "multiplier", "none" } },
	{ "coop_extra_pickups_spinner", _option_setting, 0, 0, 3, { 0 }, "network.coop_pickups",
		{ "per_player", "multiplier", "none" } },
	{ "coop_extra_vehicles_spinner", _option_setting, 0, 0, 3, { 0 }, "network.coop_vehicles",
		{ "per_player", "multiplier", "none" } },
	{ "coop_enemies_per_player_spinner", _option_setting, 0, 0, 5, { 0 }, "network.coop_enemies",
		{ "25", "50", "100", "150", "200" } },
	{ "coop_enemies_multiplier_spinner", _option_setting, 0, 0, 5, { 0 }, "network.coop_enemies_multiplier",
		{ "2", "4", "8", "16", "32" } },
	{ "coop_player_collisions_spinner", _option_setting, 0, 0, 2, { 0 }, "network.coop_player_collisions",
		{ "true", "false" } },
	/* (and the campaign's score, as Settings > Gameplay has it, and how it is kept) */
	{ "coop_campaign_score_spinner", _option_setting, 0, 0, 2, { 0 }, "game.campaign_score",
		{ "true", "false" } },
	{ "coop_score_type_spinner", _option_setting, 0, 0, 2, { 0 }, "network.coop_score_type",
		{ "competitive", "cooperative" } },
	{ "coop_ally_friendly_fire_spinner", _option_setting, 0, 0, 2, { 0 }, "network.coop_ally_friendly_fire",
		{ "true", "false" } },
	/* map sharing's (Server Setup's MAP SHARING: tools/port_settings.py's
	MAP_SHARE_SETUP; map_share.c) */
	{ "map_share_spinner", _option_setting, 0, 0, 2, { 0 }, "network.map_share", { "true", "false" } },
	{ "map_share_upload_spinner", _option_setting, 0, 0, 8, { 0 }, "network.map_share_upload",
		{ "256", "512", "1024", "2048", "4096", "8192", "16384", "20480" } },
	/* the bots' (Server Setup's BOTS, a LOCAL game's: tools/port_settings.py's
	BOTS_SETUP; bots.c, which the lobby's bots follow as they change) */
	{ "bots_count_spinner", _option_setting, 0, 0, 14, { 0 }, "bots.count",
		{ "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "15" } },
	{ "bots_skill_spinner", _option_setting, 0, 0, 7, { 0 }, "bots.skill",
		{ "distributed", "recruit", "easy", "normal", "heroic", "veteran", "legendary" } },
	{ "bots_teams_spinner", _option_setting, 0, 0, 2, { 0 }, "bots.teams", { "even", "against" } },
	/* vehicle options (the side's set and counts: vehicles_update) */
	{ "vehicles_respawn_spinner", _option_short, OPTIONS_FIELD(vehicle_respawn_time), 0, 7,
		{ 0, 30, 60, 90, 120, 180, 300 } },
};

/* TEXT CHAT's (network.text_chat) OFF, which it shows while the game is
listed in the server browser, and its host cannot change: a listed game has
no text chat whatever the setting (network_chat.c), which is kept for a
private game or a LAN one */
#define TEXT_CHAT_SPINNER "text_chat_spinner"
#define TEXT_CHAT_OFF 2

static boolean text_chat_locked(struct gametype_option const *option)
{
	return !strcmp(option->spinner, TEXT_CHAT_SPINNER) && p2p_hosting_listed();
}

/* an option of the host's own setting: the value shown for it (the one
set, else the nearest) */
static short gametype_option_setting_index(struct gametype_option const *option)
{
	struct pc_menu_setting setting;
	char text[64];
	short index;

	csmemset(&setting, 0, sizeof(setting));
	setting.setting = option->setting;
	setting.value_count = option->count;
	for (index = 0; index < option->count; index++)
		setting.values[index] = option->setting_values[index];
	if (!setting_text(option->setting, text, sizeof(text), FALSE))
		return 0;
	return setting_value_index(&setting, text);
}

/* (an instance's name is its definition's cut to 31 characters:
"item_options_infinite_grenades_") */
#define WIDGET_NAME_LENGTH 31

static struct gametype_option const *gametype_option_named(char const *spinner)
{
	short index;

	for (index = 0; index < NUMBEROF(gametype_options); index++)
	{
		if (!strncmp(gametype_options[index].spinner, spinner, WIDGET_NAME_LENGTH))
			return &gametype_options[index];
	}
	return NULL;
}

static long gametype_option_value(struct gametype_option const *option, struct game_variant *variant,
	struct game_variant_options *options)
{
	byte *v = (byte *)variant;
	byte *o = (byte *)options;

	switch (option->kind)
	{
	case _option_long: return *(long *)(v + option->offset);
	case _option_byte: return v[option->offset] != 0;
	case _option_flag: return (variant->universal_variant.flags & option->argument) != 0;
	case _option_health: return (long)(variant->universal_variant.health * 10.0f + 0.5f);
	case _option_short: return *(short *)(o + option->offset);
	case _option_option_byte: return o[option->offset];
	case _option_radar: return options->radar_players;
	}
	return 0;
}

static void gametype_option_value_set(struct gametype_option const *option, long value, struct game_variant *variant,
	struct game_variant_options *options)
{
	byte *v = (byte *)variant;
	byte *o = (byte *)options;

	switch (option->kind)
	{
	case _option_long: *(long *)(v + option->offset) = value; break;
	case _option_byte: v[option->offset] = (byte)value; break;
	case _option_flag:
		if (value)
			variant->universal_variant.flags |= option->argument;
		else
			variant->universal_variant.flags &= ~option->argument;
		break;
	case _option_health: variant->universal_variant.health = (real)value / 10.0f; break;
	case _option_short: *(short *)(o + option->offset) = (short)value; break;
	case _option_option_byte: o[option->offset] = (byte)value; break;
	case _option_radar:
		/* (and the Xbox's flag: other players on the tracker or not) */
		options->radar_players = (byte)value;
		if (value == _radar_players_none)
			variant->universal_variant.flags &= ~FLAG(_game_variant_draw_object_in_motion_sensor_bit);
		else
			variant->universal_variant.flags |= FLAG(_game_variant_draw_object_in_motion_sensor_bit);
		break;
	}
}

/* the value's place in the option's list: its own, else the nearest */
static short gametype_option_index(struct gametype_option const *option, long value)
{
	short index, best = 0;
	long best_distance = 0x7FFFFFFF;

	for (index = 0; index < option->count; index++)
	{
		long distance = option->values[index] > value ? option->values[index] - value : value - option->values[index];

		if (distance < best_distance)
		{
			best_distance = distance;
			best = index;
		}
	}
	return best;
}

/* each of the screen's option rows' spinner, with the option */
static void gametype_options_each(struct widget_instance *list, boolean save)
{
	struct game_variant *variant = edit_variant();
	struct game_variant_options *options = edit_options();
	struct widget_instance *row;

	if (!variant || !options)
		return;
	for (row = list->child; row; row = row->next)
	{
		struct widget_instance *spinner;

		for (spinner = row->child; spinner; spinner = spinner->next)
		{
			/* (a spinner: its label's name, cut to 31 characters, can be
			the same as its own) */
			struct gametype_option const *option = spinner->type == 2 ? gametype_option_named(spinner->name) : NULL;

			if (!option)
				continue;
			/* (the host's own settings: Server Setup's alone, their rows
			hidden in Edit Gametypes', which edits a gametype for any game;
			written on OK, as the gametype's are kept) */
			if (option->kind == _option_setting)
			{
				short index = (short)PIN(spinner->parameters.list.selected_index, 0, option->count - 1);

				visible_set(row, gametype_edit.setup);
				if (!gametype_edit.setup)
					continue;
				if (!save)
				{
					spinner->parameters.list.selected_index = text_chat_locked(option) ? TEXT_CHAT_OFF :
						gametype_option_setting_index(option);
				}
				else if (!text_chat_locked(option) && index != gametype_option_setting_index(option) &&
					!setting_write(option->setting, option->setting_values[index]))
				{
					platform_log("menus: could not set %s", option->setting);
				}
				continue;
			}
			if (save)
			{
				short index = (short)PIN(spinner->parameters.list.selected_index, 0, option->count - 1);

				gametype_option_value_set(option, option->values[index], variant, options);
			}
			else
				spinner->parameters.list.selected_index =
					gametype_option_index(option, gametype_option_value(option, variant, options));
		}
	}
}

/* each of a screen's rows of the host's own settings (_option_setting:
co-op's options' screens, tools/port_settings.py's _setup_option_screen),
shown, or (save) set */
static void setting_options_each(struct widget_instance *list, boolean save)
{
	struct widget_instance *row;

	for (row = list->child; row; row = row->next)
	{
		struct widget_instance *spinner;

		for (spinner = row->child; spinner; spinner = spinner->next)
		{
			struct gametype_option const *option = spinner->type == 2 ? gametype_option_named(spinner->name) : NULL;
			short index;

			if (!option || option->kind != _option_setting)
				continue;
			index = (short)PIN(spinner->parameters.list.selected_index, 0, option->count - 1);
			if (!save)
			{
				spinner->parameters.list.selected_index = text_chat_locked(option) ? TEXT_CHAT_OFF :
					gametype_option_setting_index(option);
			}
			else if (!text_chat_locked(option) && index != gametype_option_setting_index(option) &&
				!setting_write(option->setting, option->setting_values[index]))
			{
				platform_log("menus: could not set %s", option->setting);
			}
		}
	}
}

/* the options' list of a button of its bar (OK's): the one above it whose
rows are its options */
static struct widget_instance *gametype_options_list(struct widget_instance *widget)
{
	struct widget_instance *list;

	for (list = widget; list; list = list->parent)
	{
		struct widget_instance *child;

		for (child = list->child; child; child = child->next)
		{
			if (!strncmp(child->name, "op_", 3))
				return list;
		}
	}
	return NULL;
}

/* ---- the vehicle options: a side's (red's, blue's) set and counts, on
the spinners while its side is shown */

static char const *const vehicle_spinners[NUMBER_OF_VARIANT_VEHICLES] =
{
	"warthog_spinner", "ghost_spinner", "scorpion_spinner", "rwarthog_spinner", "banshee_spinner", "cgturret_spinner"
};
/* (the presets' spinner: the vehicle sets 0 to 7, then PC and CUSTOM) */
#define VEHICLE_PRESET_PC 8
#define VEHICLE_PRESET_CUSTOM 9

static struct widget_instance *vehicle_spinner(struct widget_instance *list, char const *name)
{
	struct widget_instance *row;

	for (row = list->child; row; row = row->next)
	{
		struct widget_instance *spinner = named(row, name, 0);

		if (spinner)
			return spinner;
	}
	return NULL;
}

static void vehicles_show(struct widget_instance *list)
{
	struct game_variant_options *options = edit_options();
	struct widget_instance *spinner;
	short side = gametype_edit.vehicle_side, index;

	if (!options)
		return;
	if ((spinner = vehicle_spinner(list, "team_spinner")) != NULL)
		spinner->parameters.list.selected_index = side;
	if ((spinner = vehicle_spinner(list, "vehicle_presets_spinner")) != NULL)
		spinner->parameters.list.selected_index = options->vehicle_set[side] == VARIANT_VEHICLE_SET_CUSTOM ?
			VEHICLE_PRESET_CUSTOM : options->vehicle_set[side] == VARIANT_VEHICLE_SET_PC ?
			VEHICLE_PRESET_PC : (short)MIN(options->vehicle_set[side], VEHICLE_PRESET_PC - 1);
	for (index = 0; index < NUMBER_OF_VARIANT_VEHICLES; index++)
	{
		if ((spinner = vehicle_spinner(list, vehicle_spinners[index])) != NULL)
			spinner->parameters.list.selected_index =
				(short)MIN(options->vehicle_counts[side][index], MAXIMUM_VARIANT_VEHICLE_COUNT);
	}
}

/* the side shown's spinners kept (and the Xbox's vehicle set: red's, if it
is one of the Xbox's) */
static void vehicles_keep(struct widget_instance *list)
{
	struct game_variant *variant = edit_variant();
	struct game_variant_options *options = edit_options();
	struct widget_instance *spinner;
	short side = gametype_edit.vehicle_side, index;

	if (!options || !variant)
		return;
	if ((spinner = vehicle_spinner(list, "vehicle_presets_spinner")) != NULL)
		options->vehicle_set[side] = spinner->parameters.list.selected_index >= VEHICLE_PRESET_CUSTOM ?
			VARIANT_VEHICLE_SET_CUSTOM : spinner->parameters.list.selected_index == VEHICLE_PRESET_PC ?
			VARIANT_VEHICLE_SET_PC : (byte)spinner->parameters.list.selected_index;
	for (index = 0; index < NUMBER_OF_VARIANT_VEHICLES; index++)
	{
		if ((spinner = vehicle_spinner(list, vehicle_spinners[index])) != NULL)
			options->vehicle_counts[side][index] =
				(byte)PIN(spinner->parameters.list.selected_index, 0, MAXIMUM_VARIANT_VEHICLE_COUNT);
	}
	variant->universal_variant.vehicle_set = options->vehicle_set[0] <= 4 ? options->vehicle_set[0] : 0;
}

/* "mp prof vehicles update": the side changed (the one left kept, the other
shown); a count changed makes the side's set CUSTOM */
static void vehicles_update(struct widget_instance *list)
{
	struct game_variant_options *options = edit_options();
	struct widget_instance *team = vehicle_spinner(list, "team_spinner");
	struct widget_instance *preset = vehicle_spinner(list, "vehicle_presets_spinner");
	short index;

	if (!options)
		return;
	if (team && team->parameters.list.selected_index != gametype_edit.vehicle_side)
	{
		vehicles_keep(list);
		gametype_edit.vehicle_side = (short)PIN(team->parameters.list.selected_index, 0, 1);
		vehicles_show(list);
		return;
	}
	for (index = 0; preset && index < NUMBER_OF_VARIANT_VEHICLES; index++)
	{
		struct widget_instance *spinner = vehicle_spinner(list, vehicle_spinners[index]);

		if (spinner && spinner->parameters.list.selected_index !=
			options->vehicle_counts[gametype_edit.vehicle_side][index])
		{
			options->vehicle_counts[gametype_edit.vehicle_side][index] = (byte)spinner->parameters.list.selected_index;
			preset->parameters.list.selected_index = VEHICLE_PRESET_CUSTOM;
		}
	}
}

/* "mp profile init X" (the list's creation): its spinners from the gametype */
static boolean gametype_options_init(struct widget_instance *list)
{
	if (!edit_variant())
		return campaign_fail();
	gametype_options_each(list, FALSE);
	if (named(list, "op_team", 0))
		vehicles_show(list);
	return TRUE;
}

/* "mp profile set X" (OK): the gametype from its spinners */
static boolean gametype_options_save(struct widget_instance *widget)
{
	struct widget_instance *list = gametype_options_list(widget);

	if (!list || !edit_variant())
		return campaign_fail();
	if (named(list, "op_team", 0))
		vehicles_keep(list);
	gametype_options_each(list, TRUE);
	/* (OPENCE OPTIONS: Infection is played on teams, Gun Game free for all:
	game_modes_variant_mode) */
	if (named(list, "oce_mode_spinner", 0) && edit_variant()->game_engine_index == game_engine_slayer)
	{
		if (edit_options()->mode == _game_mode_infection)
			edit_variant()->universal_variant.teams = TRUE;
		else if (edit_options()->mode == _game_mode_gun_game)
			edit_variant()->universal_variant.teams = FALSE;
	}
	return TRUE;
}

/* ---- Server Setup's options: a copy of the game's gametype (the one chosen
before), edited by the editor's screens, the game's on START GAME */

static void gametype_setup_begin(void)
{
	if (gametype_edit.setup)
		return;
	if (!player_ui_game_variant_specified(&gametype_edit.setup_variant))
		return;
	gametype_edit.setup_options = *player_ui_get_game_variant_options();
	gametype_edit.setup = TRUE;
	gametype_edit.vehicle_side = 0;
}

static void gametype_setup_end(void)
{
	gametype_edit.setup = FALSE;
}

static boolean gametype_setup_apply(void)
{
	boolean applied = !gametype_edit.setup ||
		ui_widget_port_game_variant_set(&gametype_edit.setup_variant, &gametype_edit.setup_options);

	gametype_edit.setup = FALSE;
	return applied;
}

/* the game type row's: the gametype's name and type */
static void gametype_setup_type(wchar_t *text)
{
	wchar_t name[NUMBEROF(gametype_edit.setup_variant.human_readable_game_description) + 1];
	wchar_t const *kind;

	text[0] = 0;
	if (!gametype_edit.setup)
		return;
	ustrncpy(name, gametype_edit.setup_variant.human_readable_game_description, NUMBEROF(name) - 1);
	name[NUMBEROF(name) - 1] = 0;
	kind = game_modes_variant_kind_name(&gametype_edit.setup_variant, &gametype_edit.setup_options);
	usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s (%s%s)", name,
		kind ? kind : engine_names[PIN(gametype_edit.setup_variant.game_engine_index, 0, 5)],
		gametype_edit.setup_variant.universal_variant.teams ? L", TEAMS" : L"");
	text[ROW_TEXT_LENGTH - 1] = 0;
}

/* "port setup edit" (a Server Setup option's row): its screen edits the copy */
static boolean gametype_setup_edit(void)
{
	gametype_setup_begin();
	return gametype_edit.setup || campaign_fail();
}

/* ---- Edit Gametypes' list */

static void gametype_edit_read(void)
{
	short last;

	gametype_edit.count = ui_widget_port_gametypes(gametype_edit.gametypes, MAXIMUM_GAMETYPES, &last);
	gametype_edit.chosen = (short)MIN(gametype_edit.chosen, MAX(0, gametype_edit.count - 1));
	gametype_edit.stale = FALSE;
	gametype_shown_forget();
}

/* "mp profiles list initialize" (Edit Gametypes'): on the gametype used last */
static boolean gametype_edit_list_initialize(struct widget_instance *list)
{
	short last;

	gametype_edit.setup = FALSE;
	gametype_edit.count = ui_widget_port_gametypes(gametype_edit.gametypes, MAXIMUM_GAMETYPES, &last);
	gametype_edit.chosen = last;
	gametype_edit.first = (short)MAX(0, MIN(last - GAMETYPE_EDIT_ROWS / 2, gametype_edit.count - GAMETYPE_EDIT_ROWS));
	gametype_edit.stale = FALSE;
	gametype_shown_forget();
	focus_row(list, (short)(gametype_edit.chosen - gametype_edit.first));
	return TRUE;
}

static void gametype_edit_row_text(short row, wchar_t *text)
{
	gametype_display_name(gametype_edit.gametypes[gametype_edit.first + row], text);
}

/* "gt edit list update": the rows (scrolled), the gametype chosen's
picture, name and rules */
static void gametype_edit_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *widget;
	short focused;

	if (gametype_edit.stale)
		gametype_edit_read();
	focused = list_scroll(list, &gametype_edit.first, gametype_edit.count, GAMETYPE_EDIT_ROWS);
	if (focused != NONE)
		gametype_edit.chosen = (short)MAX(0, MIN(focused, gametype_edit.count - 1));
	rows_update(list, (short)MIN(gametype_edit.count, GAMETYPE_EDIT_ROWS), gametype_edit_row_text);
	visible_set(named(description, "gametype_right_item", 0), gametype_edit.count > 0);
	if (gametype_edit.chosen < gametype_edit.count)
	{
		long profile_index = gametype_edit.gametypes[gametype_edit.chosen];
		struct game_variant const *variant = gametype_shown_get(profile_index);
		wchar_t text[ROW_TEXT_LENGTH * 2];

		visible_set(named(description, "locked_gametype_icon", 0),
			((unsigned long)profile_index & PLAYLIST_READ_ONLY_BIT) != 0);
		gametype_display_name(profile_index, text);
		text_set(named(description, "gametype_right_name", 0), text);
		if (variant)
		{
			long engine = PIN(variant->game_engine_index, 0, 5);

			if ((widget = named(description, "gametype_right_pic", 0)) != NULL)
				widget->animation.current_frame_index = (short)engine;
			gametype_rules_text(profile_index, text, NUMBEROF(text));
			text_set(named(description, "gametype_right_data", 0), text);
		}
	}
	profile_name_show(description);
}

/* "port playlist edit desc" (after the PC version's "playlist settings menu
update desc"): the gametype editor's OPENCE OPTIONS, its eighth row, has its
own words (the list's ninth) over GAME OPTIONS' picture, and the buttons
after it SAVE CHANGES' */
#define PLAYLIST_EDIT_OPENCE_ROW 7
#define PLAYLIST_EDIT_OPENCE_WORDS 8
#define PLAYLIST_EDIT_GAME_OPTIONS_PICTURE 1

static void playlist_edit_description(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	short index = list->focused_child ? ui_widget_port_list_index(list) : NONE;

	if (!description || !description->child || !description->child->next || index < PLAYLIST_EDIT_OPENCE_ROW)
		return;
	description->child->parameters.text_box.string_list_index =
		index == PLAYLIST_EDIT_OPENCE_ROW ? PLAYLIST_EDIT_OPENCE_WORDS : (short)(index - 1);
	description->child->next->animation.current_frame_index =
		index == PLAYLIST_EDIT_OPENCE_ROW ? PLAYLIST_EDIT_GAME_OPTIONS_PICTURE : (short)(index - 1);
}

/* "mp profile begin editing" (the list's custom activation: OK, a row's A) */
static boolean gametype_edit_begin(void)
{
	if (gametype_edit.chosen >= gametype_edit.count)
		return campaign_fail();
	gametype_edit.setup = FALSE;
	gametype_edit.vehicle_side = 0;
	return ui_widget_port_gametype_edit_begin(gametype_edit.gametypes[gametype_edit.chosen]);
}

/* "request del playlist profile" (X, DELETE): not a built-in one */
static boolean gametype_delete_request(void)
{
	long profile_index = gametype_edit.chosen < gametype_edit.count ? gametype_edit.gametypes[gametype_edit.chosen] :
		NONE;

	if (profile_index == NONE || ((unsigned long)profile_index & PLAYLIST_READ_ONLY_BIT))
		return campaign_fail();
	gametype_edit.deleting = profile_index;
	return TRUE;
}

/* "final del playlist profile" */
static boolean gametype_delete_final(void)
{
	boolean deleted = gametype_edit.deleting != NONE && ui_widget_port_gametype_delete(gametype_edit.deleting);

	gametype_edit.deleting = NONE;
	gametype_edit.stale = TRUE;
	return deleted;
}

/* ---- the gametype's screen and the gametype type's */

/* "get edit game settings name": the gametype's name */
static void gametype_edit_name(struct widget_instance *text_box)
{
	struct game_variant *variant = edit_variant();
	wchar_t name[NUMBEROF(variant->human_readable_game_description) + 1];

	if (!variant)
		return;
	ustrncpy(name, variant->human_readable_game_description, NUMBEROF(name) - 1);
	name[NUMBEROF(name) - 1] = 0;
	text_set(text_box, name);
}

/* "game settings lists text update": the help of the option with the
focus, for its value (the description's strings are each option's values'
helps in turn), as the Xbox's, which stops the game on the buttons' row */
static void gametype_option_help(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *row;
	short index = 0;

	/* (Item Options: the weapon set's row for a category loadout, the two
	weapons' for a custom one) */
	if (named(list, "loadout_spinner", 0))
	{
		boolean custom = named(list, "loadout_spinner", 0)->parameters.list.selected_index == _loadout_custom;

		visible_set(named(list, "op_weapon_set", 0), !custom);
		visible_set(named(list, "op_primary_weapon", 0), custom);
		visible_set(named(list, "op_secondary_weapon", 0), custom);
	}
	/* (OPENCE OPTIONS: the mode's own rows, Infection's or Gun Game's, below
	MODE and HARDCORE) */
	if (named(list, "oce_mode_spinner", 0))
	{
		short mode = named(list, "oce_mode_spinner", 0)->parameters.list.selected_index;
		short place = 73 + 2 * 30;

		for (row = list->child; row; row = row->next)
		{
			boolean infection = !strncmp(row->name, "op_inf_", 7);

			if (!infection && strncmp(row->name, "op_gg_", 6))
				continue;
			row->visible = infection ? mode == _game_mode_infection : mode == _game_mode_gun_game;
			if (row->visible)
			{
				row->vertical_offset = place;
				place += 30;
			}
		}
	}
	/* (TEXT CHAT held at OFF while the game is listed: text_chat_locked) */
	if (named(list, TEXT_CHAT_SPINNER, 0) && p2p_hosting_listed())
		named(list, TEXT_CHAT_SPINNER, 0)->parameters.list.selected_index = TEXT_CHAT_OFF;
	/* CO-OP OPTIONS: enemies, pickups and vehicles share the amount rows.
	Mixed modes may need both rows at once. */
	if (named(list, "coop_extra_enemies_spinner", 0))
	{
		short mode = named(list, "coop_extra_enemies_spinner", 0)->parameters.list.selected_index;

		struct widget_instance *pickups = named(list, "coop_extra_pickups_spinner", 0);
		struct widget_instance *vehicles = named(list, "coop_extra_vehicles_spinner", 0);
		boolean per_player = mode == _cooperative_enemies_per_player ||
			(pickups && pickups->parameters.list.selected_index == _cooperative_enemies_per_player) ||
			(vehicles && vehicles->parameters.list.selected_index == _cooperative_enemies_per_player);
		boolean multiplier = mode == _cooperative_enemies_multiplier ||
			(pickups && pickups->parameters.list.selected_index == _cooperative_enemies_multiplier) ||
			(vehicles && vehicles->parameters.list.selected_index == _cooperative_enemies_multiplier);

		visible_set(named(list, "op_coop_enemies_per_player", 0), per_player);
		visible_set(named(list, "op_coop_enemies_multiplier", 0), multiplier);

		/* Hidden amounts must not reserve a blank row. Offsets are relative
		to the parent: moving only the row carries its labels and spinners
		with it when widget_instance_render_recursive draws and notes targets. */
		{
			short y = 73;

			for (row = list->child; row; row = row->next)
				if (row->visible && !strncmp(row->name, "op_coop_", 8))
				{
					row->vertical_offset = y;
					y += 30;
				}
		}
	}
	/* (the server browser's filters, hidden: their helps are fewer than
	their values, filters_help) */
	if (!description || !list->focused_child || !strncmp(list->name, "filters", 7))
		return;
	for (row = list->child; row; row = row->next)
	{
		struct widget_instance *spinner;

		for (spinner = row->child; spinner && spinner->type != 2 /* spinner */; spinner = spinner->next)
			;
		if (row == list->focused_child)
		{
			if (spinner)
				description->parameters.text_box.string_list_index = (short)(index + spinner->parameters.list.selected_index);
			return;
		}
		if (spinner)
			index += spinner->parameters.list.number_of_items;
	}
}

static char const *const engine_items[] =
{
	/* (the type list's rows, in order: game_engine_ctf and so on) */
	"gametype_select_ctf_item", "gametype_select_koth_item", "gametype_select_slayer_item",
	"gametype_select_oddball_item", "gametype_select_race_item"
};
static long const engine_of_item[] = { 1, 4, 2, 3, 5 };

/* "mp profile init game engine": the gametype's type focused */
static boolean gametype_engine_init(struct widget_instance *list)
{
	struct game_variant *variant = edit_variant();
	short index;

	if (!variant)
		return campaign_fail();
	for (index = 0; index < NUMBEROF(engine_items); index++)
	{
		if (engine_of_item[index] == variant->game_engine_index)
			focus_row(list, index);
	}
	return TRUE;
}

/* "mp profile set game engine" (a type's A): another type clears the type's
own rules (as the Xbox's) */
static boolean gametype_engine_set(struct widget_instance *item)
{
	struct game_variant *variant = edit_variant();
	short index;

	if (!variant)
		return campaign_fail();
	for (index = 0; index < NUMBEROF(engine_items); index++)
	{
		if (!strcmp(item->name, engine_items[index]))
		{
			if (variant->game_engine_index != engine_of_item[index])
				csmemset(&variant->game_engine_variant, 0, sizeof(variant->game_engine_variant));
			variant->game_engine_index = engine_of_item[index];
			return TRUE;
		}
	}
	return campaign_fail();
}

/* "mp edit profile set rule text": the gametype's type (its string in
ui\multiplayer_game_text, as the Xbox's: capture the flag 3, slayer 4,
oddball 5, king of the hill 6, race 7, unknown 8) */
static void gametype_engine_name(struct widget_instance *text_box)
{
	static short const strings[] = { 8, 3, 4, 5, 6, 7 };
	struct game_variant *variant = edit_variant();

	if (variant)
		text_box->parameters.text_box.string_list_index = strings[PIN(variant->game_engine_index, 0, 5)];
}

/* ---------- the renderer, under the version number */

/* the renderer the game draws with, and the driver it was given if it is not the system's own, as a line for the main menu
(a second, the driver's, after a '\r'); each renderer says (d3d8_gl.c, port/android/guest/d3d8_vk.c). A renderer that does
not leaves the line empty. */
__attribute__((weak)) char const *d3d8_renderer_description(void)
{
	return NULL;
}

static void renderer_text_update(struct widget_instance *text_box)
{
	char const *description = d3d8_renderer_description();
	wchar_t text[ROW_TEXT_LENGTH];

	if (!description || !ascii_to_wide(description, text, sizeof(text)))
		return;
	text_set(text_box, text);
}

/* ---------- public code */

boolean pc_menu_event_function_invoke(
	struct widget_instance *widget,
	struct event_record *event,
	long function_index,
	boolean *widget_deleted)
{
	short controller = event_controller(widget, event);

	switch (function_index)
	{
	case _pc_menu_function_quit_game:
		platform_request_quit();
		return TRUE;
	case _pc_menu_function_setting_load:
		return setting_load(widget);
	case _pc_menu_function_setting_save:
		return setting_save(widget);
	}
	{
		char const *name = pc_menu_function_name(function_index);

		if (!name)
			return FALSE;
		if (!strcmp(name, "main menu quit game"))
		{
			platform_request_quit();
		}
		else if (!strcmp(name, "profile set edit begin"))
		{
			return profile_edit_begin(widget, event, widget_deleted);
		}
		else if (!strcmp(name, "new campaign decision"))
		{
			return new_profile_decision(widget, event, widget_deleted);
		}
		/* (the press posted is the controller's that chose the button: a
		split screen player's LEAVE is theirs) */
		else if (!strcmp(name, "mouse emit accept event"))
		{
			event_manager_post_button(controller, BUTTON_A);
		}
		else if (!strcmp(name, "mouse emit back event"))
		{
			event_manager_post_button(controller, BUTTON_B);
		}
		else if (!strcmp(name, "mouse emit x event"))
		{
			event_manager_post_button(controller, BUTTON_X);
		}
		else if (!strcmp(name, "emit custom activation event"))
		{
			struct widget_instance *top = widget;
			boolean deleted;

			while (top->parent)
				top = top->parent;
			ui_widget_port_dispatch_event(top, EVENT_CUSTOM_ACTIVATION, event_controller(widget, event), &deleted);
			if (deleted)
				*widget_deleted = TRUE;
		}
		else if (!strcmp(name, "mp type set mode"))
		{
			lan_mode = strstr(widget->name, "_lan_") != NULL;
			multiplayer_mode_set(widget);
			/* (Create's: the map list only for a game made, "join controller
			to mp game" before; it fails with the game's port in use, by
			another copy of the game) */
			if ((multiplayer.mode == _multiplayer_mode_host_internet ||
				multiplayer.mode == _multiplayer_mode_host_lan ||
				multiplayer.mode == _multiplayer_mode_host_local) && !global_network_game_server_get())
			{
				platform_log("menus: the game could not be made (is its port in use?)");
				return campaign_fail();
			}
		}
		else if (!strcmp(name, "gamespy screen init"))
		{
			return browser_initialize(widget, event, widget_deleted);
		}
		else if (!strcmp(name, "gamespy select item") || !strcmp(name, "gamespy select button"))
		{
			return browser_select(widget, event, controller, widget_deleted);
		}
		else if (!strcmp(name, "gamespy back handler") && text_field_editing(NULL))
		{
			text_field_end(FALSE);
		}
		/* (the server browser's panels: B, or their own buttons, close them
		over the list) */
		else if ((!strcmp(name, "gamespy back handler") || !strcmp(name, "gamespy dismiss error") ||
			!strcmp(name, "gamespy dismiss filters")) && browser_panel_close(widget))
		{
			ui_play_audio_feedback_sound(SOUND_BACK);
		}
		else if (!strcmp(name, "gamespy select header"))
		{
			return lobby_browser_sort_choose(widget);
		}
		else if (!strcmp(name, "gamespy update filter settings"))
		{
			filters_show(screen_of(widget), FALSE, TRUE);
			ui_play_audio_feedback_sound(SOUND_FORWARD);
		}
		else if (!strcmp(name, "port setup edit"))
		{
			return gametype_setup_edit();
		}
		/* (co-op's options' screens: their settings shown, and OK's) */
		else if (!strcmp(name, "port setup options init"))
		{
			setting_options_each(widget, FALSE);
		}
		else if (!strcmp(name, "port setup options save"))
		{
			struct widget_instance *list = gametype_options_list(widget);

			if (list)
				setting_options_each(list, TRUE);
		}
		else if (!strcmp(name, "mp profile save changes"))
		{
			gametype_shown_forget();
			return ui_widget_port_gametype_save(widget, widget_deleted);
		}
		else if (!strcmp(name, "mp profile begin editing"))
		{
			return gametype_edit_begin();
		}
		else if (!strcmp(name, "request del playlist profile"))
		{
			return gametype_delete_request();
		}
		else if (!strcmp(name, "final del playlist profile"))
		{
			return gametype_delete_final();
		}
		else if (!strcmp(name, "mp profile init game engine"))
		{
			return gametype_engine_init(widget);
		}
		else if (!strcmp(name, "mp profile set game engine"))
		{
			return gametype_engine_set(widget);
		}
		else if (!strncmp(name, "mp profile init ", 16) || !strcmp(name, "mp prof init teamplay options") ||
			!strcmp(name, "mp prof init vehicle options"))
		{
			return gametype_options_init(widget);
		}
		else if ((!strncmp(name, "mp profile set ", 15) && strcmp(name, "mp profile set for game")) ||
			!strcmp(name, "mp prof save teamplay options") || !strcmp(name, "mp prof save vehicle options"))
		{
			return gametype_options_save(widget);
		}
		else if (!strcmp(name, "port lobby preview join"))
		{
			return preview_join(widget, controller, widget_deleted);
		}
		else if (!strcmp(name, "gamespy screen dispose"))
		{
			lobby_browser_end();
		}
		else if (!strcmp(name, "port pause end game"))
		{
			return pause_end_game();
		}
		else if (!strcmp(name, "port coop friendly fire"))
		{
			return coop_friendly_fire_change();
		}
		else if (!strcmp(name, "player profile save changes"))
		{
			return profile_save_changes(widget, widget_deleted);
		}
		else if (!strcmp(name, "port profile settings save"))
		{
			return profile_settings_save(widget);
		}
		else if (!strcmp(name, "direct ip connect go"))
		{
			return direct_link_from_clipboard();
		}
		else if (!strcmp(name, "join controller to mp game"))
		{
			return multiplayer_host(widget, event, controller, widget_deleted);
		}
		else if (!strcmp(name, "mp level list initialize"))
		{
			return map_list_initialize(widget);
		}
		else if (!strcmp(name, "mp level select"))
		{
			return map_list_choose(widget, widget_deleted);
		}
		else if (!strcmp(name, "port map list back"))
		{
			return map_list_back(widget, widget_deleted);
		}
		else if (!strcmp(name, "mp profiles list initialize"))
		{
			if (!strcmp(widget->name, "playlist_select_list"))
				return gametype_edit_list_initialize(widget);
			return gametype_list_initialize(widget);
		}
		else if (!strcmp(name, "mp profile set for game"))
		{
			return gametype_choose(widget, widget_deleted);
		}
		else if (!strcmp(name, "mp tag profiles init"))
		{
			return tag_profiles_initialize(widget);
		}
		else if (!strcmp(name, "mp tag profile choose"))
		{
			return tag_profile_choose(widget, widget_deleted);
		}
		else if (!strcmp(name, "server settings init"))
		{
			return server_settings_initialize(widget);
		}
		else if (!strcmp(name, "ss edit server name"))
		{
			return server_name_edit(widget);
		}
		else if (!strcmp(name, "ss edit server password"))
		{
			return server_password_edit(widget);
		}
		else if (!strcmp(name, "port password init"))
		{
			return password_screen_initialize(widget);
		}
		else if (!strcmp(name, "port leaderboard init"))
		{
			return leaderboard_initialize();
		}
		else if (!strcmp(name, "port leaderboard view"))
		{
			return leaderboard_view();
		}
		else if (!strcmp(name, "port daily today"))
		{
			return daily_today();
		}
		else if (!strcmp(name, "port daily init"))
		{
			return daily_initialize();
		}
		else if (!strcmp(name, "port daily play"))
		{
			return daily_play(controller);
		}
		else if (!strcmp(name, "port daily choose"))
		{
			return daily_choose(widget);
		}
		else if (!strcmp(name, "port daily leaderboard init"))
		{
			return daily_leaderboard_initialize();
		}
		else if (!strcmp(name, "port history init"))
		{
			return daily_history_initialize();
		}
		else if (!strcmp(name, "port daily back"))
		{
			return daily_back(widget_deleted);
		}
		else if (!strcmp(name, "port daily random"))
		{
			return daily_random(controller);
		}
		else if (!strcmp(name, "port daily try default"))
		{
			return daily_connection_try_default(widget, widget_deleted);
		}
		else if (!strcmp(name, "port daily connection edit"))
		{
			return daily_connection_edit(widget, widget_deleted);
		}
		else if (!strcmp(name, "port daily go offline"))
		{
			return daily_connection_offline(widget, widget_deleted);
		}
		else if (!strcmp(name, "port daily retry"))
		{
			return daily_connection_back(widget, widget_deleted);
		}
		else if (!strcmp(name, "port daily settings init"))
		{
			return daily_settings_initialize();
		}
		else if (!strcmp(name, "port daily server edit"))
		{
			return daily_server_edit(widget);
		}
		else if (!strcmp(name, "port field back"))
		{
			return field_back(widget, widget_deleted);
		}
		else if (!strcmp(name, "port custom maps init"))
		{
			return custom_maps_folder_initialize();
		}
		else if (!strcmp(name, "port custom maps pick"))
		{
			return custom_maps_folder_pick();
		}
		else if (!strcmp(name, "port password edit"))
		{
			return password_screen_edit(widget, widget_deleted);
		}
		else if (!strcmp(name, "port password join"))
		{
			return password_screen_join(widget, widget_deleted);
		}
		else if (!strcmp(name, "port password back"))
		{
			return password_screen_back(widget, widget_deleted);
		}
		else if (!strcmp(name, "ss copy invite"))
		{
			return invite_copy(widget, widget_deleted);
		}
		else if (!strcmp(name, "ss invite qr back"))
		{
			return invite_qr_back(widget, widget_deleted);
		}
		/* (the main menu's news) */
		else if (!strcmp(name, "port news init"))
		{
			return news_initialize();
		}
		else if (!strcmp(name, "port news close"))
		{
			return news_close(widget, widget_deleted);
		}
		else if (!strcmp(name, "port news open"))
		{
			return news_open(widget, widget_deleted);
		}
		else if (!strcmp(name, "port news up") || !strcmp(name, "port news down"))
		{
			news_scroll(strstr(name, "up") ? -1 : 1);
			return TRUE;
		}
		else if (!strcmp(name, "port news newer") || !strcmp(name, "port news older"))
		{
			return news_turn(strstr(name, "newer") ? -1 : 1);
		}
		else if (!strcmp(name, "ss start game"))
		{
			return server_start();
		}
		else if (!strcmp(name, "ss randomizer") || !strcmp(name, "ss skulls"))
		{
			return server_campaign_screen(widget, !strcmp(name, "ss skulls"), widget_deleted);
		}
		else if (!strcmp(name, "ss tag profile change"))
		{
			return server_settings_tag_profile_change(widget, widget_deleted);
		}
		else if (!strcmp(name, "single prev cl item activated"))
		{
			item_activated(widget, event_controller(widget, event), widget_deleted);
		}
		else if (!strcmp(name, "port settings save"))
		{
			settings_each(screen_of(widget), setting_changed_save);
			daily_settings_save();
			custom_maps_folder_save();
			platform_display_apply();
		}
		else if (!strcmp(name, "port settings defaults"))
		{
			settings_each(screen_of(widget), setting_default_show);
			if (named(screen_of(widget), "op_server_address", 0))
				daily_settings_defaults();
			if (named(screen_of(widget), "op_custom_maps_folder", 0))
				custom_maps_folder_defaults();
		}
		else if (!strcmp(name, "port mods init") || !strcmp(name, "port mods close"))
		{
			return mods_screen_reset();
		}
		else if (!strcmp(name, "port mods toggle"))
		{
			return mods_toggle(widget);
		}
		else if (!strcmp(name, "port mods move up") || !strcmp(name, "port mods move down"))
		{
			return mods_move(widget, (short)(!strcmp(name, "port mods move up") ? -1 : 1));
		}
		else if (!strcmp(name, "port mods save"))
		{
			return mods_save();
		}
		else if (!strcmp(name, "port tags init") || !strcmp(name, "port tags close"))
		{
			return tags_screen_reset(!strcmp(name, "port tags init"));
		}
		else if (!strcmp(name, "port tags choose"))
		{
			return tags_choose(widget);
		}
		else if (!strcmp(name, "port tags left") || !strcmp(name, "port tags right"))
		{
			return tags_step(widget, (short)(!strcmp(name, "port tags left") ? -1 : 1));
		}
		else if (!strcmp(name, "port tags back"))
		{
			return tags_back(widget, widget_deleted);
		}
		else if (!strcmp(name, "port tags new profile"))
		{
			return tags_new_profile();
		}
		else if (!strcmp(name, "port tags profile button"))
		{
			return tags_profile_button(widget);
		}
		else if (!strcmp(name, "controls screen init") || !strcmp(name, "controls screen defaults"))
		{
			return controls_load(!strcmp(name, "controls screen defaults"));
		}
		else if (!strcmp(name, "controls screen change set"))
		{
			return controls_save();
		}
		else if (!strcmp(name, "controls begin binding"))
		{
			return control_capture_begin(widget);
		}
		else if (!strcmp(name, "player profile list initialize"))
		{
			return profile_list_initialize(widget);
		}
		else if (!strcmp(name, "profile manager select"))
		{
			return profile_choose(controller);
		}
		else if (!strcmp(name, "port lobby open"))
		{
			return lobby_join_reset();
		}
		else if (!strcmp(name, "port lobby add player"))
		{
			return lobby_add_player();
		}
		else if (!strcmp(name, "port lobby join"))
		{
			return lobby_join_start(widget, controller, widget_deleted);
		}
		else if (!strcmp(name, "port lobby leave"))
		{
			return lobby_leave(widget, event, controller, widget_deleted);
		}
		else if (!strcmp(name, "port lobby player list initialize"))
		{
			return lobby_player_list_initialize(widget);
		}
		else if (!strcmp(name, "port lobby player choose"))
		{
			return lobby_player_choose();
		}
		else if (!strcmp(name, "port lobby preview add"))
		{
			return preview_add(widget, controller, widget_deleted);
		}
		else if (!strcmp(name, "port lobby preview leave"))
		{
			return preview_leave(controller);
		}
		else if (!strcmp(name, "port coop begin"))
		{
			return coop_begin(controller);
		}
		else if (!strcmp(name, "port coop player 2 list initialize"))
		{
			return coop_player2_list_initialize(widget);
		}
		else if (!strcmp(name, "port coop player 2"))
		{
			return coop_player2_choose(controller);
		}
		else if (!strcmp(name, "request del player profile"))
		{
			return profile_delete_request();
		}
		else if (!strcmp(name, "final del player profile"))
		{
			return profile_delete();
		}
		else if (!strcmp(name, "color picker menu initialize"))
		{
			return color_list_initialize(widget);
		}
		else if (!strcmp(name, "color picker select color"))
		{
			return color_choose();
		}
		else if (!strcmp(name, "controls binding slot"))
		{
			controls_screen.slot = (short)!controls_screen.slot;
		}
		else if (!strcmp(name, "campaign menu init"))
		{
			return campaign_menu_initialize(widget, controller);
		}
		else if (!strcmp(name, "campaign menu continue"))
		{
			return campaign_continue(controller);
		}
		else if (!strcmp(name, "initialize sp level list solo"))
		{
			return level_list_initialize(widget, controller);
		}
		else if (!strcmp(name, "solo level set map"))
		{
			return level_choose(widget, controller);
		}
		else if (!strcmp(name, "difficulty item select"))
		{
			return difficulty_start(widget, sibling_index(widget), controller, widget_deleted);
		}
		else if (!strcmp(name, "set difficulty"))
		{
			short difficulty = PIN(difficulty_shown(widget), 0, 3);

			main_set_difficulty(difficulty);
			/* (with the randomizer's or the skulls' screen to open first, as
			a difficulty chosen opens them: the OK button's next handler,
			"main menu switch to solo game", would start the game without
			them. Its screen opened, or the game started, that handler is
			not run: the chain stops as for a widget deleted) */
			if ((config_boolean("game.randomizer") && campaign_level_of(main_get_map_name()) != NONE) ||
				config_boolean("game.skulls"))
			{
				boolean result = difficulty_start(widget, difficulty, controller, widget_deleted);

				*widget_deleted = TRUE;
				return result;
			}
		}
		else if (!strcmp(name, "load game menu init"))
		{
			return saved_game_list_initialize(widget, controller);
		}
		else if (!strcmp(name, "load game menu activated"))
		{
			return saved_game_continue(controller);
		}
		else if (!strcmp(name, "load game menu delete request"))
		{
			return saved_game_delete_request();
		}
		else if (!strcmp(name, "load game menu delete finish"))
		{
			return saved_game_delete(controller);
		}
		else if (!strcmp(name, "randomizer profile save"))
		{
			return randomizer_profile_save_event(widget);
		}
		else if (!strcmp(name, "randomizer profile load"))
		{
			return randomizer_profile_load_event(widget, widget_deleted);
		}
		else if (!strcmp(name, "randomizer profiles init"))
		{
			randomizer_menu.delete_mode = FALSE;
			randomizer_menu.deleting = NONE;
		}
		else if (!strcmp(name, "randomizer profiles delete mode"))
		{
			return randomizer_profiles_delete_mode();
		}
		else if (!strcmp(name, "randomizer start"))
		{
			return randomizer_ok(widget, widget_deleted);
		}
		else if (!strcmp(name, "randomizer custom left") || !strcmp(name, "randomizer custom right"))
		{
			return randomizer_custom_step(widget, (short)(!strcmp(name, "randomizer custom left") ? -1 : 1));
		}
		else if (!strcmp(name, "randomizer tag profile choose"))
		{
			return randomizer_tag_profile_choose(widget, widget_deleted);
		}
		else if (!strcmp(name, "skull toggle"))
		{
			return skull_toggle(widget);
		}
		else if (!strcmp(name, "skulls toggle all"))
		{
			/* (any of those had on: all off; none on: all those had on) */
			struct skull_set unlocked = skulls_unlocked_set();
			struct skull_set on = skull_set_intersection(skulls_menu_set, unlocked);

			skulls_menu_set = skull_set_is_empty(&on) ? unlocked : skull_set_none();
			ui_play_audio_feedback_sound(skull_set_is_empty(&on) ? SOUND_FORWARD : SOUND_BACK);
		}
		else if (!strcmp(name, "skulls start"))
		{
			return skulls_start(widget, widget_deleted);
		}
		else if (!strcmp(name, "skulls random roll"))
		{
			return skulls_random_roll(widget, widget_deleted);
		}
		else if (!strcmp(name, "skulls reset progress"))
		{
			skulls_reset_progress(widget);
		}
		else if (!strcmp(name, "skulls unlock all"))
		{
			return skulls_unlock_choose(widget, FALSE, FALSE, TRUE, widget_deleted);
		}
		else if (!strcmp(name, "skulls unlock play"))
		{
			return skulls_question_open("pc\\main_menu\\difficulty_select\\skulls\\death_screen", TRUE,
				widget_deleted);
		}
		else if (!strcmp(name, "skulls unlock pickup"))
		{
			return skulls_unlock_choose(widget, TRUE, TRUE, TRUE, widget_deleted);
		}
		else if (!strcmp(name, "skulls death lose"))
		{
			return skulls_unlock_choose(widget, TRUE, FALSE, TRUE, widget_deleted);
		}
		else if (!strcmp(name, "skulls death keep"))
		{
			return skulls_unlock_choose(widget, TRUE, FALSE, FALSE, widget_deleted);
		}
		else if (!strcmp(name, "skulls death back"))
		{
			return skulls_question_open("pc\\main_menu\\difficulty_select\\skulls\\unlock_screen", FALSE,
				widget_deleted);
		}
		else if (!strcmp(name, "skulls up"))
		{
			return skulls_move(widget, 0, -1);
		}
		else if (!strcmp(name, "skulls down"))
		{
			return skulls_move(widget, 0, 1);
		}
		else if (!strcmp(name, "skulls left"))
		{
			return skulls_move(widget, -1, 0);
		}
		else if (!strcmp(name, "skulls right"))
		{
			return skulls_move(widget, 1, 0);
		}
		else if (!strcmp(name, "randomizer profile delete"))
		{
			return randomizer_profile_delete_event(widget);
		}
		else if (!strcmp(name, "controls back handler") || !strcmp(name, "gamespy back handler") ||
			!strcmp(name, "gamespy dismiss error") || !strcmp(name, "gamespy dismiss filters"))
		{
			ui_widget_port_go_back(widget);
			*widget_deleted = TRUE;
		}
		return TRUE;
	}
}

void pc_menu_game_data_function_invoke(
	struct widget_instance *widget,
	long function)
{
	char const *name = pc_menu_game_data_input_name(function);

	if (!name)
		return;
	/* (a text field whose screen has gone: let go of) */
	if (text_field.row && system_milliseconds() - text_field_shown_time > 500)
		text_field_end(FALSE);
	if (!strcmp(name, "solo map list update"))
		level_list_update(widget);
	else if (!strcmp(name, "mp map list update"))
		map_list_update(widget);
	else if (!strcmp(name, "gt select list update"))
		gametype_list_update(widget);
	else if (!strcmp(name, "server settings update"))
		server_settings_update(widget);
	else if (!strcmp(name, "port news update"))
		news_update(widget);
	else if (!strcmp(name, "port news hint update"))
		news_hint_update(widget);
	else if (!strcmp(name, "port password update"))
		password_screen_update(widget);
	else if (!strcmp(name, "port leaderboard update"))
		leaderboard_update(widget);
	else if (!strcmp(name, "port daily update"))
		daily_update(widget);
	else if (!strcmp(name, "port history update"))
		daily_history_update(widget);
	else if (!strcmp(name, "port daily connection update"))
		daily_connection_update(widget);
	else if (!strcmp(name, "port daily settings update"))
		daily_settings_update(widget);
	else if (!strcmp(name, "port custom maps update"))
		custom_maps_folder_update(widget);
	else if (!strcmp(name, "gamespy screen update"))
		browser_update(widget);
	else if (!strcmp(name, "gt edit list update"))
		gametype_edit_list_update(widget);
	else if (!strcmp(name, "get edit game settings name"))
		gametype_edit_name(widget);
	else if (!strcmp(name, "game settings lists text update"))
	{
		gametype_option_help(widget);
		filters_help(widget);
	}
	else if (!strcmp(name, "mp edit profile set rule text"))
		gametype_engine_name(widget);
	else if (!strcmp(name, "mp prof vehicles update"))
		vehicles_update(widget);
	else if (!strcmp(name, "port lobby preview update"))
		preview_update(widget);
	else if (!strcmp(name, "port lobby update"))
		lobby_update(widget);
	else if (!strcmp(name, "port settings help"))
	{
		video_rows_show(widget);
		vk_driver_row_show(widget);
		hud_button_style_row_show(widget);
		master_servers_show(widget);
		settings_help(widget);
	}
	else if (!strcmp(name, "controls update menu"))
		controls_update(widget);
	else if (!strcmp(name, "port mods update"))
		mods_update(widget);
	else if (!strcmp(name, "port tags update"))
		tags_update(widget);
	else if (!strcmp(name, "color picker update"))
		color_list_update(widget);
	else if (!strcmp(name, "3wide player profile list update"))
		profile_list_update(widget);
	else if (!strcmp(name, "load game list update"))
		saved_game_list_update(widget);
	else if (!strcmp(name, "port gamepad layout preview"))
		profile_gamepad_layout(widget);
	else if (!strcmp(name, "port renderer textbox"))
		renderer_text_update(widget);
	else if (!strcmp(name, "randomizer profiles update"))
		randomizer_profiles_update(widget);
	else if (!strcmp(name, "randomizer screen update"))
		randomizer_screen_update(widget);
	else if (!strcmp(name, "randomizer tag profiles update"))
		randomizer_tag_profiles_update(widget);
	else if (!strcmp(name, "mp tag profiles update"))
		tag_profiles_update(widget);
	else if (!strcmp(name, "skulls screen update"))
		skulls_screen_update(widget);
	else if (!strcmp(name, "gameplay screen update"))
		gameplay_screen_update(widget);
	else if (!strcmp(name, "port coop friendly fire text"))
		coop_friendly_fire_text(widget);
	else if (!strcmp(name, "port playlist edit desc"))
		playlist_edit_description(widget);
}
