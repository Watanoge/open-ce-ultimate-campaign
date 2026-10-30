"""The port's settings screens, which tools/ce_menus.py writes in place of the
PC version's (port/assets/menus/ce): laid out as its are (rows of a label and
a spinner over its row backgrounds, a line of help for the row chosen,
Defaults, OK and Cancel), with what this port has to set instead of what the
PC version had (no connection speed or texture quality: a window or the full
screen, the frame rate, the keyboard's own controls).

A row's spinner sets one of config.toml's settings (setting=, values=), or of
the profile being edited ("profile.<field>": the controller's settings), by
port/linux/game/menu_functions.c: "port setting load" shows its value,
"port settings save" (OK) writes those changed and applies them, "port
settings defaults" shows the defaults. Controls Setup binds the keyboard and
mouse's controls, by the functions the PC version names for it.
"""

import re
import sys
from pathlib import Path
from xml.sax.saxutils import quoteattr

PE = "main_menu/settings_select/player_setup/player_profile_edit"
# the mods' screen, and its rows (_mods); the main menu's items
# (ce/main_menu.xml), MAIN_MENU_ITEM_SPACING apart (the PC version's 36
# leaves no room below Quit for a seventh): Campaign and Multiplayer, THEATER
# below it (_theater), the PC version's Profiles and Settings, MODS in
# Credits' place (Credits is left out: tools/ce_menus.py's LEFT_OUT), and
# Quit (their CHILD_OFFSETS); the list's order is its up and down's
MODS = "main_menu/mods"
MOD_ROWS = 11
MAIN_MENU_ITEM_TOP = 247
MAIN_MENU_ITEM_SPACING = 32
MAIN_MENU_ITEMS = ["load_camp", "multiplayer", "theater", "profile", "settings", "mods", "quit_game"]


def main_menu_item_place(item: str) -> tuple:
    """where the main menu's item is (Campaign's 2 units right of the rest,
    as the PC version has it)"""
    return (194 if item == "load_camp" else 192,
            MAIN_MENU_ITEM_TOP + MAIN_MENU_ITEM_SPACING * MAIN_MENU_ITEMS.index(item))


MODS_ITEM_PLACE = main_menu_item_place("mods")
THEATER_ITEM_PLACE = main_menu_item_place("theater")
QUIT_ITEM_PLACE = main_menu_item_place("quit_game")
# the Theater (port/linux/REPLAY.md): the replays a host records, its rows
# (_theater)
THEATER = "main_menu/theater"
# the daily challenge's screens, and Campaign's DAILY CHALLENGE (_daily),
# above Back, which goes a row down (wider than the other items, about the
# same middle; menu_functions.c's campaign_menu_initialize takes it away,
# and Back up again, with game.daily_challenge off)
DAILY = "main_menu/daily"
DAILY_ITEM = "main_menu/campaign_select/menu_item_daily"
# (the map lists' DAILY CHALLENGES: a challenge's skulls' icons over its
# level's picture, two rows of five at most, at its top: _daily)
DAILY_LEVEL_SKULL = "main_menu/daily/level_skull_icon"
DAILY_LEVEL_SKULLS = 10
DAILY_LEVEL_SKULL_CHILDREN = [f'<child widget="{DAILY_LEVEL_SKULL}" x="{421 + (index % 5) * 27}" y="{89 + (index // 5) * 26}"/>'
                              for index in range(DAILY_LEVEL_SKULLS)]
DAILY_ITEM_PLACE = (160, 391)
CAMPAIGN_BACK_ITEM_PLACE = (192, 427)
# the profile settings' rows, in order: the PC version's nine and Gameplay,
# second to last, all in the nine's room (78 to About's top, 342) so the
# last one's 32-unit highlight stays beside the description panel. Each is
# moved from its tag's top (the second number; Gameplay's is ours) by its
# list's child offset (tools/ce_menus.py's CHILD_OFFSETS)
PROFILE_ROWS = [("name_profile_item", 78), ("controller_setup_profile_item", 111),
                ("gamepads_profile_item", 144), ("mouse_settings_profile_item", 177),
                ("audio_settings_profile_item", 210), ("video_settings_profile_item", 243),
                ("network_settings_profile_item", 277), ("color_profile_item", 277),
                ("gameplay_profile_item", None), ("about_item", 310)]
PROFILE_ROWS_TOP = 78
PROFILE_ROWS_LAST_TOP = 342


def profile_row_top(row: str) -> int:
    """where a profile settings row is: the rows evenly spaced"""
    index = [name for name, _ in PROFILE_ROWS].index(row)
    return PROFILE_ROWS_TOP + round(index * (PROFILE_ROWS_LAST_TOP - PROFILE_ROWS_TOP) / (len(PROFILE_ROWS) - 1))


def profile_row_offset(row: str) -> int:
    """a PC row's child offset in the list, to its place"""
    return profile_row_top(row) - dict(PROFILE_ROWS)[row]


YES_NO = [("YES", "true"), ("NO", "false")]
ON_OFF = [("ON", "true"), ("OFF", "false")]
# the multiplayer armor colors, in the profile's order (player_profile.c)
SHIELD_COLORS = ["white", "black", "red", "blue", "gray", "yellow", "green", "pink", "purple", "cyan", "cobalt",
                 "orange", "teal", "sage", "brown", "tan", "maroon", "salmon"]
XBOX_BUTTON_STYLES = [("XBOX CLASSIC", "classic"), ("XBOX GEMS", "gems"),
                     ("XBOX DARK GEMS", "menu_gems"), ("XBOX MODERN", "xbox_modern")]
PS_BUTTON_STYLES = [("PS CLASSIC", "ps_classic"), ("PS GEMS", "ps_gems"),
                   ("PS DARK GEMS", "ps_dark_gems"), ("PS MODERN", "ps_modern")]
SWITCH_BUTTON_STYLES = [("SWITCH CLASSIC", "switch_classic"), ("SWITCH GEMS", "switch_gems"),
                       ("SWITCH MODERN", "switch_modern")]
SENSITIVITIES = [(f"{value:g}", f"{value:g}") for value in (0.1, 0.15, 0.25, 0.4, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.5, 3, 4)]
VOLUMES = [(str(step), f"{step / 10:g}") for step in range(11)]
# an HDR display's paper white and peak in nits (display.hdr_paper_white,
# display.hdr_peak_brightness), in steps fine enough for HDR Calibration's
# patterns (and BT.2408's reference white, 203); Extra Effects' rows have the
# same, so they show what it chose
PAPER_WHITES = [("AUTO", "0")] + [(str(nits), str(nits)) for nits in sorted({*range(80, 200, 10), 200, 203, *range(220, 401, 20), 250})]
PEAK_BRIGHTNESSES = [("AUTO", "0")] + [(str(nits), str(nits)) for nits in
                                       [*range(250, 600, 25), *range(600, 1200, 50), *range(1200, 2001, 100),
                                        2500, 3000, 4000, 5000, 6000, 8000, 10000]]

# each screen: its folder below PE, its screen's widget (the name the profile
# menu opens), its header (widget, bitmap), the row spacing, and its rows:
# (label, setting, [(shown, value)], help, platform[, key]): platform is None
# for every platform, else the platforms the row is for, separated by spaces
# ("desktop", "android" (the Android app), "switch", "ps5", "web": the menus'
# platform attribute, port/linux/src/menu_files.c); key names the row's widgets where
# two rows set one setting (one for each platform), else the setting's name does
SCREENS = {
    "video_settings": {
        "screen": "video_settings_screen",
        "header": ("header_profile_video_settings", f"{PE}/video_settings/header_profile_video_settings"),
        # a 28px step (the other screens' 30 would take the desktop's ten
        # rows into the help: Graphics' is 27). each platform packs its own
        # rows, so a row hidden on this machine does not leave a gap
        "spacing": 28,
        # (below the desktop's last row, 353: its two lines clear of the
        # buttons, 414)
        "help_top": 358, "help_height": 54,
        "platform_places": True,
        # (rows in the place of the row before them: Window Size in
        # Resolution's, port/linux/game/menu_functions.c showing the one the
        # display mode chosen uses; Android's anti-aliasing in the desktop's.
        # Android's backend and Vulkan driver take places of their own:
        # platform_places packs each platform's rows)
        "same_place": ["display.window_size", "anti_aliasing_android"],
        # lines of text among the rows, in a place no row of theirs uses:
        # (text, platform, place), None the place after the platform's last
        # row. Below Android's rows, wherever they end (place 6 was below
        # them until Maximum Resolution and Extra Effects came)
        "notes": [("The graphics backend and driver change when the game restarts.", "android", None)],
        "rows": [
            ("DISPLAY MODE:", "display.mode",
             [("FULLSCREEN", "fullscreen"), ("BORDERLESS", "borderless"), ("WINDOWED", "windowed")],
             "Fullscreen takes the display; borderless covers it\nwith a window. F11 switches to the window.",
             "desktop"),
            # (read by the Android app as the game starts, which loads the game
            # image built with that renderer: port/android/host/host_main.c)
            ("GRAPHICS BACKEND:", "display.renderer", [("VULKAN", "vulkan"), ("OPENGL", "gl")],
             "How the game draws: Vulkan, or OpenGL ES. Takes\neffect the next time the game starts.",
             "android"),
            # (port/linux/game/menu_tags.c adds the display's resolutions)
            ("RESOLUTION:", "display.resolution", [("NATIVE", "native")],
             "What fullscreen and borderless draw at. Fullscreen\nsets the display to it; borderless scales it.",
             "desktop"),
            # (port/linux/game/menu_tags.c puts the sizes that fit the desktop
            # in place of this one)
            ("WINDOW SIZE:", "display.window_size", [("1280 x 960", "1280x960")],
             "The window's size: 4:3, then 16:10, 16:9 and 21:9\n(its edges can also be dragged).", "desktop"),
            # (port/linux/game/menu_functions.c shows it while the backend
            # shown is Vulkan; a driver archive's name left there by hand is
            # kept unless another is chosen: port/android/host/host_vk_driver.c)
            ("VULKAN DRIVER:", "display.vk_driver", [("STOCK", ""), ("TURNIP", "auto")],
             "The phone's own driver, or Turnip on an Adreno\nGPU. Takes effect the next time the game starts.",
             "android"),
            # (the phone's own is MAXIMUM RESOLUTION below: Android keeps
            # display.resolution_scaling "original", whose lines it sets)
            ("RESOLUTION SCALING:", "display.resolution_scaling",
             [("NATIVE", "native"), ("75%", "75%"), ("50%", "50%"), ("25%", "25%"), ("ORIGINAL", "original")],
             "Native draws at the resolution, 75% to 25% that\npart each way; Original, the Xbox's 480 lines.", "desktop web"),
            # (the PS5's own, first: what the game draws at, no more than the
            # display's mode, port/ps5/host/host_main.c; the Switch's row
            # below has the same setting with its own values)
            ("MAXIMUM RESOLUTION:", "display.resolution_limit",
             [("2160P (4K)", "2160"), ("1440P (2K)", "1440"), ("1080P (FHD)", "1080")],
             "The most lines the game draws at, no more than\nthe display shows. Changes at once.", "ps5",
             "resolution_limit_ps5"),
            # (the phone's: sdl_platform.c's platform_resolution_scale)
            ("MAXIMUM RESOLUTION:", "display.resolution_limit",
             [("NATIVE", "native"), ("2160P (4K)", "2160"), ("1440P (2K)", "1440"), ("1080P (FHD)", "1080"),
              ("720P", "720"), ("480P", "480")],
             "The most lines the game draws at, no more than\nthe screen has. More are sharper, and slower.", "android",
             "resolution_limit_android"),
            ("V-SYNC:", "display.vsync", ON_OFF,
             "Wait for the display between frames, so that the\npicture never tears.", None),
            ("FRAME RATE LIMIT:", "display.max_fps",
             [("AUTO", "0"), ("30", "30"), ("60", "60"), ("120", "120"), ("144", "144"), ("165", "165"),
              ("240", "240"), ("NONE", "-1")],
             "With V-Sync off, the most frames a second. Auto:\ntwice the display's refresh rate.", "desktop web"),
            ("FRAME RATE LIMIT:", "display.max_fps",
             [("AUTO", "0"), ("30", "30"), ("60", "60"), ("120", "120"), ("NONE", "-1")],
             "With V-Sync off, the most frames a second. Auto:\ntwice the display's refresh rate.", "android",
             "max_fps_android"),
            ("SMOOTH MOTION:", "display.interpolation", ON_OFF,
             "Draw a frame for every display refresh, blending\nbetween the game's 30 ticks a second.", None),
            ("INSTANT AIM:", "display.direct_camera", ON_OFF,
             "In first person, turn the view the moment the\nmouse moves, not up to two ticks later.", "desktop web"),
            # (the deko3d renderer's screen scale: port/switch/guest/d3d8_dk.c)
            ("RESOLUTION LIMIT:", "display.resolution_limit",
             [("480P", "480"), ("720P", "720"), ("900P", "900"), ("1080P", "1080")],
             "What the game draws at. Shown at 720p in handheld\nand 1080p docked; handheld draws 720p at most.", "switch"),
            # (port/switch/guest/d3d8_dk.c's dynamic_resolution)
            ("DYNAMIC RESOLUTION:", "display.dynamic_resolution", ON_OFF,
             "Draw one step lower while the frame rate would\nfall under 55, and back up once it fits again.", "switch"),
            # (Graphics', with the HUD's rows: Video Setup's own has no room
            # left above its help on the desktop. The game's own counter, main.c's main_framerate_render, which
            # display.show_fps turns on: port/linux/game/frame_rate_display.c)
            ("SHOW FRAMES:", "display.show_fps", ON_OFF,
             "Draw the frame rate at the bottom right of a game,\ncounted over half a second.", None),
            ("HIGH-RES HUD:", "display.high_res_hud", ON_OFF,
             "Draw the HUD from the high-res redraws; off\ndraws the game's own pictures.", None),
            ("HIGH-RES TEXT:", "display.high_res_text", ON_OFF,
             "Draw text and titles with high-res fonts; off\ndraws the game's own.", None),
            ("ANTI-ALIASING:", "display.anti_aliasing",
             [("OFF", "off"), ("FXAA", "fxaa"), ("SMAA", "smaa"), ("SSAA 2X", "ssaa2x"), ("MSAA 2X", "msaa2x"),
              ("MSAA 4X", "msaa4x"), ("MSAA 8X", "msaa8x")],
             "Smooth jagged edges, which the Xbox did not. FXAA\nand SMAA are cheap; SSAA and MSAA are sharper.",
             "desktop"),
            ("ANTI-ALIASING:", "display.anti_aliasing",
             [("OFF", "off"), ("FXAA", "fxaa"), ("MSAA 2X", "msaa2x"), ("MSAA 4X", "msaa4x")],
             "Smooth jagged edges, which the Xbox did not. FXAA\nis cheap; MSAA is sharper.",
             "android web ps5", "anti_aliasing_android"),
            ("TEXTURE FILTERING:", "display.anisotropic_filtering",
             [("OFF", "0"), ("2X", "2"), ("4X", "4"), ("8X", "8"), ("16X", "16")],
             "Keep floors and walls sharp at a grazing angle,\nwhich the Xbox did not. Off: as the Xbox.",
             "desktop android web"),
            ("SHADOW RESOLUTION:", "display.shadow_resolution",
             [("128", "128"), ("256", "256"), ("512", "512"), ("1024", "1024")],
             "The size objects' shadows are drawn at: 128 as on\nthe Xbox; larger for smoother, as soft, edges.",
             "desktop android web ps5"),
            # (neither is the Switch's: its deko3d renderer draws the Xbox's
            # shadow maps and lights each vertex, port/switch/guest/d3d8_dk.c)
            ("PER-PIXEL LIGHTING:", "display.per_pixel_lighting", ON_OFF,
             "Light models for each pixel, without the facets\nof the Xbox's lighting for each vertex.",
             "desktop android web ps5"),
            # (the OpenGL renderer's: the deko3d and Vulkan ones do not draw
            # its fade up the body, d3d8_gl_set_height_fade)
            ("OVERSHIELD:", "display.overshield_effect", ON_OFF,
             "The overshield rises up the body and stays lit,\nin its own color. Online: the host's choice.",
             "desktop android web"),
            ("MODEL DETAIL:", "display.model_detail", [("STOCK", "stock"), ("NATIVE", "native"), ("HIGHEST", "highest")],
             "Stock picks models' detail by the Xbox's 480 lines;\nnative by this display's; highest, always the best.", None),
            ("SHIELD GLOW:", "display.shield_glow", ON_OFF,
             "Shields light their surroundings as they flare.\nOnline: the host's choice.", None),
            ("GLOW INTENSITY:", "display.shield_glow_intensity", [("DEFAULT", "default"), ("LIGHT SHOW", "light_show")],
             "Light Show: three times as far, at full from a\nthird of the flare. Online: the host's choice.", None),
            # (the OpenGL renderer's alone, as texture filtering: the deko3d
            # and Vulkan ones draw the Xbox's range)
            ("HDR:", "display.hdr", ON_OFF,
             "Let explosions, plasma and the sun shine brighter\nthan white, then fit them to the screen.",
             "desktop android web"),
            ("HDR DISPLAY:", "display.hdr_output", ON_OFF,
             "On an HDR display (Windows HDR on), show that\nlight truly brighter. From the next start.", "desktop"),
            ("TONE CURVE:", "display.hdr_tone_mapping",
             [("GT7", "gt7"), ("NEUTRAL", "neutral"), ("ACES", "aces"), ("REINHARD", "reinhard")],
             "GT7: bright light turns white, as on film. Neutral:\nas drawn. ACES: more contrast. Reinhard: softer.", None),
            ("PAPER WHITE:", "display.hdr_paper_white", PAPER_WHITES,
             "With HDR Display, how bright white is, in nits.\nAuto: Windows' SDR content brightness.", "desktop"),
            ("PEAK BRIGHTNESS:", "display.hdr_peak_brightness", PEAK_BRIGHTNESSES,
             "With HDR Display, your display's brightest, in\nnits. Auto: Windows' HDR calibration.", "desktop"),
        ],
    },
    "mouse_settings": {
        "screen": "mouse_settings_screen",
        "header": ("header_profile_mouse_settings", f"{PE}/mouse_settings/header_profile_mouse_settings"),
        "spacing": 30,
        "rows": [
            ("HORIZONTAL SENSITIVITY:", "input.mouse_sensitivity", SENSITIVITIES,
             "How fast the view turns side to side for the\nmouse's movement.", None),
            ("VERTICAL SENSITIVITY:", "input.mouse_vertical_sensitivity", [("SAME", "0")] + SENSITIVITIES,
             "How fast the view turns up and down; Same\nturns it as fast as side to side.", None),
            ("INVERT VERTICAL AXIS:", "input.invert_mouse", YES_NO,
             "Moving the mouse forward looks down.", None),
            ("AIM ASSIST:", "input.mouse_aim_assist", ON_OFF,
             "Slow and drag the view along with a target while\naiming with the mouse, as with a controller.", None),
            # (port/linux/src/sdl_platform.c's menu_cursor_size; last, as the
            # other platforms have no row in its place)
            ("MENU CURSOR:", "display.menu_cursor",
             [("OFF", "off"), ("SMALL", "small"), ("MEDIUM", "medium"), ("LARGE", "large"),
              ("EXTRA LARGE", "extra_large")],
             "The menus' blue arrow for the mouse, and its size;\nOff shows the system's pointer.", "desktop"),
        ],
    },
    "audio_settings": {
        "screen": "audio_settings_screen",
        "header": ("header_profile_audio_settings", f"{PE}/audio_settings/header_profile_audio_settings"),
        "spacing": 30,
        # (the devices, desktop only, first: Android's rows from the top)
        "platform_places": True,
        "rows": [
            # (port/linux/game/menu_tags.c adds the devices SDL finds)
            ("OUTPUT DEVICE:", "audio.output_device", [("SYSTEM DEFAULT", "default")],
             "Where the game's sound and the voices play.", "desktop"),
            ("INPUT DEVICE:", "audio.input_device", [("SYSTEM DEFAULT", "default")],
             "The microphone voice chat listens to.", "desktop"),
            ("MASTER VOLUME:", "audio.volume", VOLUMES, "The volume of everything.", None),
            ("MUSIC VOLUME:", "audio.music_volume", VOLUMES, "The music's volume.", None),
            ("EFFECTS VOLUME:", "audio.effects_volume", VOLUMES,
             "The volume of every other sound: effects and\nspeech.", None),
            ("REVERB:", "audio.reverb", ON_OFF,
             "Echo sounds as the place you are in does, and\nmuffle those behind walls, as the Xbox did.", None),
            ("SOUND:", "audio.enabled", ON_OFF,
             "Play sound at all; from the next time the game\nstarts.", None),
            # voice chat: this machine's own (port/linux/game/network_voice.c)
            ("VOICE CHAT:", "audio.voice_chat",
             [("PUSH TO TALK", "push_to_talk"), ("OPEN MIC", "open_mic"), ("OFF", "off")],
             "Talk in network games: while PUSH TO TALK is held\n(Controls), whenever you speak, or never.", None),
            ("VOICE VOLUME:", "audio.voice_volume", VOLUMES, "The other players' voices.", None),
        ],
    },
    "network_setup": {
        "screen": "network_settings_screen",
        "header": ("header_profile_network_settings", f"{PE}/network_setup/header_profile_network_settings"),
        "spacing": 30,
        # (the router, the clipboard and the updates, which the web build
        # has not: the other rows from the top there)
        "platform_places": True,
        "rows": [
            ("INTERNET PLAY:", "network.online", ON_OFF,
             "Host and join games over the internet by invite\nlinks; off keeps to the local network.",
             "desktop android switch web ps5"),
            ("UPNP PORT FORWARDING:", "network.allow_upnp", ON_OFF,
             "Let internet play ask the router to forward its\nport, for networks that stop connections.",
             "desktop android switch ps5"),
            # (neither of these two is the PS5's: nothing outside a title
            # can put text on its clipboard, and a title is updated by
            # installing it again, port/ps5/host/host_update_none.c)
            ("JOIN FROM CLIPBOARD:", "network.join_from_clipboard", ON_OFF,
             "Join the game of an invite link copied before\nswitching to the game.", "desktop android switch"),
            ("CHECK FOR UPDATES:", "update.auto", ON_OFF,
             "Look for a new version when the game starts.", "desktop android switch"),
            ("PLAYER NAMES:", "display.player_names",
             [("ALL", "all"), ("ALLIES", "allies"), ("ENEMIES", "enemies"), ("NONE", "none")],
             "In multiplayer, whose names are drawn above their\nheads.", None),
            ("PLAYER NAME SIZE:", "display.player_name_scale",
             [(f"{value:g}x", f"{value:g}") for value in (0.5, 0.75, 1, 1.25, 1.5, 2)],
             "How large the players' names are drawn.", None),
            ("SCOREBOARD LAYOUT:", "display.scoreboard_team_layout", [("TEAMS", "teams"), ("BY SCORE", "score")],
             "A team game's scoreboard: a column for each team,\nor every player in order of score.", None),
            ("SCOREBOARD PANEL:", "display.scoreboard_background", ON_OFF,
             "Draw a panel behind the scoreboard, for clearer\ntext.", None),
        ],
    },
    "gamepad_setup": {
        "screen": "gamepad_setup_screen",
        "header": ("header_profile_controller_setting", f"{PE}/controller_edit/header_profile_controller_setting"),
        "spacing": 27,
        "rows": [
            ("LOOK SENSITIVITY:", "profile.look_sensitivity", [(str(step), str(step)) for step in range(1, 11)],
             "How fast the right stick turns the view.", None),
            ("INVERT LOOK:", "profile.invert_look", YES_NO, "Pushing the right stick up looks down.", None),
            ("INVERT FLIGHT:", "profile.flight_inversion", YES_NO,
             "Flying vehicles pitch as aircraft do: pushing up\nflies down.", None),
            ("AUTO-CENTER LOOK:", "profile.autocenter", YES_NO,
             "Level the view while walking.", None),
            ("BUTTON LAYOUT:", "profile.button_preset",
             [("DEFAULT", "0"), ("SWAP TRIGGERS", "1"), ("SWAP A, L TRIGGER", "2"), ("SWAP B, L TRIGGER", "3"),
              ("SWAP B, R STICK", "4"),
              # (the port's: the later games' layouts as Halo: CE's controls
              # go, source/interface/player_ui.c; upstream's Fishsticks and
              # Recon after this fork's, whose numbers saved profiles keep)
              ("BOXER", "5"), ("GREEN THUMB", "6"), ("BUMPER JUMPER", "7"), ("RECLAIMER", "8"),
              ("FISHSTICKS", "9"), ("RECON", "10")],
             "Which of the controller's buttons does what (not\nthe keyboard's: Controls Setup sets those).", None),
            ("STICK LAYOUT:", "profile.joystick_preset",
             [("DEFAULT", "0"), ("SOUTHPAW", "1"), ("LEGACY", "2"), ("LEGACY SOUTHPAW", "3")],
             "Which stick moves and which looks.", None),
            ("VIBRATION:", "profile.vibration", ON_OFF, "Rumble the controller.", None),
            ("IN-GAME HELP:", "profile.ingame_help", ON_OFF, "Show the game's hints about its controls.", None),
        ],
        "categories": [("BUTTON REMAPPING", "controller_remapping",
                        "Custom button layouts for P1-P4.\nUse / Reload stays combined."),
                       ("BUTTON APPEARANCE", "controller_appearance",
                        "Controller artwork and bumper style.\nRequires High-Res HUD in Graphics.")],
    },
    "controller_appearance": {
        "screen": "controller_appearance_screen", "spacing": 30,
        "header": ("header_profile_controller_setting", f"{PE}/controller_edit/header_profile_controller_setting"),
        "same_place": ["hud_button_style_xbox", "hud_button_style_ps", "hud_button_style_switch"],
        "rows": [
            ("AUTO BUTTON DETECTION:", "input.auto_button_detection", ON_OFF,
             "Show styles for your controller's family.\nRequires High-Res HUD in Graphics.", None),
            ("BUTTON STYLE:", "display.hud_button_style", XBOX_BUTTON_STYLES + PS_BUTTON_STYLES + SWITCH_BUTTON_STYLES,
             "Manual gameplay button artwork, kept when auto\ndetection is on. Requires High-Res HUD.", None),
            ("BUTTON STYLE:", "display.hud_button_style_xbox", XBOX_BUTTON_STYLES,
             "Preferred Xbox artwork for automatic detection.\nRequires High-Res HUD in Graphics.", None),
            ("BUTTON STYLE:", "display.hud_button_style_ps", PS_BUTTON_STYLES,
             "Preferred PlayStation artwork for auto detection.\nRequires High-Res HUD in Graphics.", None),
            ("BUTTON STYLE:", "display.hud_button_style_switch", SWITCH_BUTTON_STYLES,
             "Preferred Switch artwork for automatic detection.\nRequires High-Res HUD in Graphics.", None),
            ("BUMPER STYLE:", "display.hud_bumper_style",
             [("ORIGINAL XBOX", "original"), ("MODERN XBOX", "modern")],
             "Original: black/white buttons. Modern: LB/RB.\nKept when automatic detection is on.", None),
        ],
    },
    "controller_remapping": {
        "screen": "controller_remapping_screen", "spacing": 30,
        "dynamic_help": True, "help_top": 320, "help_height": 90,
        "header": ("header_profile_controller_setting", f"{PE}/controller_edit/header_profile_controller_setting"),
        "rows": [],
    },
}

# Gyro Aim is its own screen, opened from Gamepads: the controller's gyroscope
# on the Switch and the PS5, the phone's on Android. The other platforms do not show the
# row, and it takes the place after the last row, so they have no gap.
_gamepads = SCREENS["gamepad_setup"]
_gamepads["categories"] = [
    ("GYRO AIM:", "gamepad_setup/gyro",
     "Turn the view with the gyroscope,\nas well as the stick.", "android switch ps5"),
]
SCREENS["gamepad_setup/gyro"] = {
    "screen": "gyro_aim_screen", "header": _gamepads["header"], "spacing": 30,
    "rows": [
        ("GYRO AIM:", "input.gyro_aim", ON_OFF,
         "Turn the view with the gyroscope.", None),
        ("ONLY ON ZOOM:", "input.gyro_zoom_only", YES_NO,
         "The gyroscope turns the view only while\nzoomed in.", None),
        ("HORIZONTAL SENSITIVITY:", "input.gyro_sensitivity_horizontal",
         [(f"{value:g}", f"{value:g}") for value in
          (0.25, 0.5, 0.75, 1, 1.25, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8, 9, 10)],
         "How far the view turns left and right.\n1 is the sensor's own turn.", None),
        ("VERTICAL SENSITIVITY:", "input.gyro_sensitivity_vertical",
         [(f"{value:g}", f"{value:g}") for value in
          (0.25, 0.5, 0.75, 1, 1.25, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8, 9, 10)],
         "How far the view turns up and down.\n1 is the sensor's own turn.", None),
        ("INVERT HORIZONTAL:", "input.gyro_invert_horizontal", YES_NO,
         "Turning it right looks left.", None),
        ("INVERT VERTICAL:", "input.gyro_invert_vertical", YES_NO,
         "Tilting it up looks down.", None),
    ],
    # The other controller rows are profile fields, written when the profile
    # is saved. These are config.toml, and B (and Cancel) only went back, so
    # a change was kept for this run and gone after quitting.
    "save_on_leave": True,
}

# Video Setup's categories open ordinary settings screens. Each has the same
# pending edits, Defaults, OK and Cancel as the other settings screens.
_video = SCREENS["video_settings"]
_graphics = {"display.show_fps", "display.high_res_hud",
             "display.high_res_text", "display.anti_aliasing",
             "display.anisotropic_filtering", "display.shadow_resolution", "display.per_pixel_lighting",
             "display.model_detail"}
# (the visual modifiers: a screen of their own, Extra Effects)
_extra_effects = {"display.shield_glow", "display.shield_glow_intensity",
                  "display.overshield_effect", "display.hdr", "display.hdr_output", "display.hdr_tone_mapping",
                  "display.hdr_paper_white", "display.hdr_peak_brightness"}
# (each platform's rows packed: the Switch has none of the renderer's rows
# between the text and the model detail, which left it gaps; the two
# anti-aliasing rows land in the same place, each its platforms' next)
SCREENS["video_settings/graphics"] = {
    "screen": "graphics_settings_screen", "header": _video["header"], "spacing": 27,
    "platform_places": True,
    "rows": [row for row in _video["rows"] if row[1] in _graphics],
}
SCREENS["video_settings/extra_effects"] = {
    "screen": "extra_effects_settings_screen", "header": _video["header"], "spacing": 30,
    "platform_places": True,
    "rows": [row for row in _video["rows"] if row[1] in _extra_effects],
    # (shown only with an HDR display: menu_functions.c's
    # hdr_calibration_row_show)
    "categories": [
        ("HDR CALIBRATION:", "video_settings/hdr_calibration",
         "Set Peak Brightness and Paper White by eye,\nwith patterns brighter than white.", "desktop"),
    ],
}
# HDR Calibration: the peak, then paper white, each row's pattern drawn by
# the renderer between the rows and the help, at the values shown, which the
# display takes while the screen is open (menu_functions.c's "port hdr
# calibration", d3d8_gl.c's hdr_calibration_draw)
SCREENS["video_settings/hdr_calibration"] = {
    "screen": "hdr_calibration_settings_screen", "header": _video["header"], "spacing": 30,
    "inputs": ["port hdr calibration"],
    "rows": [
        ("PEAK BRIGHTNESS:", "display.hdr_peak_brightness", PEAK_BRIGHTNESSES,
         "Raise it until the ring just disappears into\nthe square: the brightest your display shows.", None),
        ("PAPER WHITE:", "display.hdr_paper_white", PAPER_WHITES,
         "How bright white is (the menus, the HUD, the\ngrey bars), in nits. Choose what looks right.", None),
    ],
}
_video["rows"] = [row for row in _video["rows"] if row[1] not in _graphics | _extra_effects]
_video["categories"] = [
    ("GRAPHICS:", "video_settings/graphics", "The HUD, text, anti-aliasing, shadows,\nlighting and HDR."),
    ("FOV AND VIEWMODELS:", "video_settings/fov_viewmodels",
     "The field of view and the first-person\nweapon. Their defaults keep the stock view."),
    ("PERFORMANCE:", "video_settings/performance",
     "How fast the console runs the game: a steadier\nframe rate, or a cooler console.", "switch"),
    ("EXTRA EFFECTS:", "video_settings/extra_effects",
     "Visual modifiers the stock game does not\nhave, such as the shield glow."),
]
# the Switch's clocks (port/switch/host/host_sdl2.c)
SCREENS["video_settings/performance"] = {
    "screen": "performance_settings_screen", "header": _video["header"], "spacing": 30,
    "rows": [
        ("CPU CLOCK:", "performance.cpu_clock", [("STOCK", "stock"), ("1 GHZ", "1020"), ("1.7 GHZ", "1785")],
         "1.7 GHz is the fastest Nintendo's games use.\n1 GHz and stock run cooler, and the battery lasts.", None),
    ],
}
SCREENS["video_settings/fov_viewmodels"] = {
    "screen": "fov_viewmodel_settings_screen", "header": _video["header"], "spacing": 30,
    "rows": [
        ("FOV:", "display.fov", [("DEFAULT", "0")] + [(str(n), str(n)) for n in range(80, 151, 5)],
         "On foot, horizontal at 16:9, in degrees.\nDefault keeps the stock view.", None),
        ("VIEWMODEL FOV:", "display.viewmodel_fov", [("DEFAULT", "0")] + [(str(n), str(n)) for n in range(80, 151, 5)],
         "The weapon and hands, horizontal at 16:9.\nDefault keeps the weapon's stock view.", None),
        ("VIEWMODELS:", "display.viewmodel_visible", ON_OFF,
         "Draw first-person weapons, hands and attached\nvisuals. Gameplay and sound continue when off.", None),
        ("PASSENGER THIRD PERSON:", "display.passenger_third_person", ON_OFF,
         "Third-person Warthog and Scorpion passengers in\nthe campaign and co-op. Weapon zoom is disabled.", None),
        ("ARM SHIELDS:", "display.viewmodel_shield", ON_OFF,
         "The energy shield's flare on your first-person\narms too, as on your body.", None),
        ("LEGS:", "display.first_person_legs", ON_OFF,
         "Your legs in first person, seen looking down,\nmoving as you run. Your body as others see it.", None),
        ("SHIELD COLOR:", "display.shield_color", [("DEFAULT", "default")] + [(c.upper(), c) for c in SHIELD_COLORS],
         "Your shield's flare in an armor color, whatever\nyour armor. Only you see it. Default: its own.", None),
    ],
}

# Audio Setup's sound processing (port/linux/src/dsound_sdl.c): the reverb
# and the resampler, a screen of their own (a category, as Video Setup's);
# the devices and the volumes stay on Audio Setup
_audio = SCREENS["audio_settings"]
_processing = {"audio.reverb"}
SCREENS["audio_settings/sound_processing"] = {
    "screen": "sound_processing_settings_screen", "header": _audio["header"], "spacing": 30,
    "rows": [row for row in _audio["rows"] if row[1] in _processing] + [
        ("RESAMPLER:", "audio.resampler", [("SINC", "sinc"), ("LINEAR", "linear")],
         "How sounds are converted to the output's rate:\nsinc is clean; linear is brighter, with aliasing.",
         None),
    ],
}
_audio["rows"] = [row for row in _audio["rows"] if row[1] not in _processing]

# (voice chat stays on Audio Setup; the text chat's rows are the multiplayer
# HUD's, on Network Setup)
_audio["categories"] = [
    ("SOUND PROCESSING:", "audio_settings/sound_processing",
     "The reverb, and how sounds are converted to the\noutput's rate."),
]

# Network Setup's map sharing (port/linux/game/map_share.c): this machine's
# part in another's game, a screen of its own (a category, as Video Setup's)
SEED_UPLOADS = (64, 128, 256, 512, 1024, 2048, 4096, 8192, 10240)
_network = SCREENS["network_setup"]
# (and the multiplayer HUD's rows, and streamer mode, screens of their own:
# Network Setup has room for nine places)
_hud_settings = ("display.player_names", "display.player_name_scale", "display.scoreboard_team_layout",
                 "display.scoreboard_background")
_hud_rows = [row for row in _network["rows"] if row[1] in _hud_settings]
_network["rows"] = [row for row in _network["rows"] if row[1] not in _hud_settings]
_network["categories"] = [
    ("MAP SHARING:", "network_setup/map_sharing",
     "Downloading Custom Edition maps you do not have,\nand helping send them to other players.",
     "desktop android ps5"),
    ("MULTIPLAYER HUD:", "network_setup/multiplayer_hud",
     "Players' names above their heads, the\nscoreboard, the text chat and the match clock."),
    ("STREAMER MODE:", "network_setup/streamer_mode",
     "Hide offensive words in names and text from\nother players, maps and mods, as stars."),
    ("SERVER BROWSER:", "network_setup/server_browser",
     "What the Server Browser lists and its order: the\nsame settings as its Filters screen."),
    ("DISCORD ACCOUNT:", "network_setup/discord_account",
     "Link a Discord account, which lets you join\nservers that block VPNs while you use one."),
]
SCREENS["network_setup/map_sharing"] = {
    "screen": "map_sharing_settings_screen", "header": _network["header"], "spacing": 30,
    "rows": [
        ("DOWNLOAD MAPS:", "network.map_download", ON_OFF,
         "Joining a game on a Custom Edition map you do not\nhave: download it from the players (if shared).",
         None),
        ("HELP SEND MAPS:", "network.map_seed", ON_OFF,
         "In another's game, send its map to players who\njoin without it, as a torrent's peers do.", None),
        ("UPLOAD LIMIT:", "network.map_seed_upload",
         [(f"{kb} KB/S" if kb < 1024 else f"{kb // 1024} MB/S", str(kb)) for kb in SEED_UPLOADS],
         "The most you send of maps to other players, so\nthat the game keeps the rest of your upload.", None),
    ],
}

# Gameplay, a screen of its own from Settings (a row of the profile menu):
# its own rows, then its categories, each a screen of its own
_GAMEPLAY_HEADER = ("header_gameplay", f"{PE}/gameplay/header_gameplay")
SCREENS["gameplay"] = {
    "screen": "gameplay_settings_screen",
    "header": _GAMEPLAY_HEADER,
    "spacing": 30,
    "rows": [
        ("TERMINAL OUTPUT:", "game.terminal_output",
         [("NONE", "none"), ("MENUS ONLY", "menus"), ("ALWAYS", "always")],
         "The game's messages at the bottom left: never,\nin the menus only, or in games too.",
         None),
        ("CAMPAIGN PROGRESSION:", "game.campaign_progression",
         [("DEFAULT", "default"), ("ALL UNLOCKED", "all_unlocked")],
         "Default: finish a level to unlock the next.\nAll unlocked: every campaign level can be played.",
         None),
    ],
    # (the phone's: game.custom_maps.path, a folder chosen with Android's
    # folder picker and shown in the row, written with OK:
    # menu_functions.c's "port custom maps" functions)
    "fields": [
        ("CUSTOM MAPS FOLDER:", "port custom maps pick", "custom_maps_folder",
         "Where Custom Edition maps are read from and\ndownloaded to. A chooses another folder.", "android"),
    ],
    "field_label_width": 290,
    "inputs": ["port custom maps update"],
    "handlers": ['<on event="created" run="port custom maps init"/>'],
    "categories": [
        ("RAGDOLLS", "gameplay/ragdolls", "How the bodies of players and the AI fall."),
        ("RANDOMIZER", "gameplay/randomizer",
         "The randomizer: a campaign level's enemies\nand weapons swapped."),
        ("SKULLS", "gameplay/skulls", "The Master Chief Collection's skulls, and\nhow they are found."),
        ("SCORING", "gameplay/scoring", "The campaign's score, its time and par times,\nand medals, in multiplayer too."),
        ("COMMUNITY", "gameplay/community",
         "Online leaderboards, the daily challenge, and\nthe community server."),
        ("TAG INJECTOR", "gameplay/tag_injector",
         "Weapons and AI of Custom Edition maps, in\nprofiles the randomizer can add."),
    ],
}
SCREENS["gameplay/ragdolls"] = {
    "screen": "ragdolls_settings_screen", "header": _GAMEPLAY_HEADER, "spacing": 30,
    "rows": [
        ("PLAYER RAGDOLLS:", "game.ragdoll_players",
         [("NONE", "none"), ("WOODY", "woody"), ("HALO 2", "halo2")],
         "How a player's body falls:\nthe animation, a loose ragdoll, or Halo 2's.",
         None),
        ("AI RAGDOLLS:", "game.ragdoll_ai",
         [("NONE", "none"), ("WOODY", "woody"), ("HALO 2", "halo2")],
         "How an AI's body falls:\nthe animation, a loose ragdoll, or Halo 2's.",
         None),
    ],
}
# (the campaign's scoring: port/linux/game/campaign_score.c, the score and
# time at the bottom right, the skulls' icons above them, and the
# scoreboard's scores; campaign_medals.c, with it, medals worth points)
SCREENS["gameplay/scoring"] = {
    "screen": "scoring_settings_screen", "header": _GAMEPLAY_HEADER, "spacing": 30,
    "rows": [
        ("SCORING:", "game.campaign_score", ON_OFF,
         "Score each kill in the campaign: your score and\ntime at the bottom right, everyone's on tab.",
         None),
        ("CO-OP SCORING:", "network.coop_score_type", [("INDIVIDUAL", "competitive"), ("TEAM", "cooperative")],
         "Co-op, split screen or online: each player's own\nscore, or the team's together. The host's counts.",
         None),
        ("MEDAL POPUPS:", "game.campaign_medals", ON_OFF,
         "Show the medals you win at the center left as\nyou win them. Medals always count with scoring.",
         None),
        # (campaign_score.c's campaign_score_palette_set; CUSTOM COLOR shown
        # with HUD COLOR's CUSTOM, the rows below it closing its gap:
        # menu_functions.c's gameplay_screen_update)
        ("HUD COLOR:", "game.score_hud_color",
         [("DEFAULT", "default"), ("SPARTAN", "spartan"), ("CUSTOM", "custom")],
         "The score panel's color: its own blue, your\nSpartan's (or team's), or one you choose.",
         None),
        ("CUSTOM COLOR:", "game.score_hud_custom_color", [(c.upper(), c) for c in SHIELD_COLORS],
         "The score panel's color with HUD COLOR's\nCUSTOM, whatever your Spartan's is.",
         None),
        # (multiplayer's: campaign_score.c's multiplayer panel and medals,
        # this machine's alone; MEDALS and SCORE HUD shown with MULTIPLAYER
        # on: menu_functions.c's gameplay_screen_update)
        ("MULTIPLAYER:", "game.multiplayer_scoring", ON_OFF,
         "Medals and a score panel in multiplayer games,\nyours alone. Nothing is sent to other players.",
         None),
        ("MEDALS:", "game.multiplayer_medals", ON_OFF,
         "Multiplayer: the campaign's medals for your\nkills, as you win them and the last few.",
         None),
        ("SCORE HUD:", "game.multiplayer_score_hud", ON_OFF,
         "Multiplayer: your kills and deaths, and the\ngame's score, at the bottom right.",
         None),
    ],
    "inputs": ["gameplay screen update"],
}
# (Community: the daily challenge and the online leaderboards, port/linux/
# game/community.c; SERVER ADDRESS shown with SERVER CUSTOM, typed in its row
# and written with OK: menu_functions.c's "port daily settings" functions)
SCREENS["gameplay/community"] = {
    "screen": "community_settings_screen", "header": _GAMEPLAY_HEADER, "spacing": 30,
    "rows": [
        ("DAILY CHALLENGE:", "game.daily_challenge", ON_OFF,
         "Campaign's DAILY CHALLENGE: a level a day, its\nenemies and skulls the same for everyone.",
         None),
        ("ONLINE:", "community.enabled", ON_OFF,
         "Enable online features like campaign mission and\ndaily challenge global leaderboards.",
         None),
        ("SHARE LEVEL SCORES:", "community.upload_scores", ON_OFF,
         "Online: each campaign level you win with scoring\non goes to its online leaderboard.",
         None),
        ("SERVER:", "community.server_mode", [("DEFAULT", "default"), ("CUSTOM", "custom")],
         "Default: the OpenCE server. Custom: another\ncommunity server, at the address below.",
         None),
    ],
    "fields": [
        ("SERVER ADDRESS:", "port daily server edit", "server_address",
         "The custom server's address, https://... A types\nit, Enter keeps it; OK writes it."),
    ],
    "inputs": ["port daily settings update"],
    "handlers": ['<on event="created" run="port daily settings init"/>'],
}
SCREENS["gameplay/randomizer"] = {
    "screen": "randomizer_settings_screen", "header": _GAMEPLAY_HEADER, "spacing": 30,
    "rows": [
        ("RANDOMIZER:", "game.randomizer",
         [("OFF", "false"), ("ON", "true")],
         "On: after a campaign level's difficulty, a screen\nsets how that level's enemies and weapons change.",
         None),
        ("GO CRAZY:", "game.randomizer_crazy", [("OFF", "false"), ("ON", "true")],
         "On: the choices below. Off: none of them, and\none flood infection pod in three is swapped.",
         None),
        # (Go Crazy's parts, shown with it on: menu_functions.c's
        # gameplay_screen_update)
        ("ANY NPC, ANY WEAPON:", "game.randomizer_crazy_weapons", [("OFF", "false"), ("ON", "true")],
         "Any NPC can hold any weapon, but may not\nknow how to use it.",
         None),
        ("ENGINEERS & SENTINELS:", "game.randomizer_crazy_enemies", [("OFF", "false"), ("ON", "true")],
         "The unused NPCs join the enemy pools.",
         None),
        ("FLAMETHROWER:", "game.randomizer_crazy_flamethrower", [("OFF", "false"), ("ON", "true")],
         "The multiplayer flamethrower can be chosen as\na weapon type and joins the mixed pools.",
         None, "randomizer_crazy_flame"),
        ("FLOOD PODS SWAPPED:", "game.randomizer_crazy_pods",
         [(f"{percent}%", str(percent)) for percent in range(10, 101, 10)],
         "How many flood infection pods become other\nenemies. The rest are left out.",
         None),
    ],
    "inputs": ["gameplay screen update"],
}
SCREENS["gameplay/skulls"] = {
    "screen": "skulls_settings_screen", "header": _GAMEPLAY_HEADER, "spacing": 30,
    "rows": [
        ("SKULLS:", "game.skulls",
         [("OFF", "false"), ("ON", "true")],
         "On: after a campaign level's difficulty, a screen\nturns the Master Chief Collection's skulls on.",
         None),
        # (shown with SKULLS on, once UNLOCK BY PLAYING was chosen:
        # menu_functions.c's gameplay_screen_update)
        ("ALL SKULLS:", "game.skulls_all_unlocked",
         [("OFF", "false"), ("ON", "true")],
         "On: every skull can be picked, and none drop.\nOff: enemies drop the skulls you haven't found.",
         None),
        # (shown with ALL SKULLS off: menu_functions.c's gameplay_screen_update)
        ("INSTANT UNLOCK:", "game.skulls_instant_unlock",
         [("OFF", "false"), ("ON", "true")],
         "On: a skull you pick up is unlocked at once.\nOff: finish the level to unlock it.",
         None),
        # (shown with INSTANT UNLOCK off too: menu_functions.c's
        # gameplay_screen_update)
        ("LOSE ON DEATH:", "game.skulls_lose_on_death",
         [("OFF", "false"), ("ON", "true")],
         "On: dying loses the skulls you picked up and\nhave not unlocked yet. Off: you keep them.",
         None),
        # (shown with ALL SKULLS off: menu_functions.c's gameplay_screen_update.
        # The host's in co-op, whose enemies drop its locked skulls)
        ("SKULLS PER LEVEL:", "game.skulls_drop_count",
         [("BY DIFFICULTY", "0")] + [(str(count), str(count)) for count in range(1, 9)],
         "How many locked skulls a level's enemies drop.\nBy difficulty: 1 on Easy to 4 on Legendary.",
         None),
        ("DROP CHANCE:", "game.skulls_drop_chance",
         [("1/4X", "25"), ("1/2X", "50"), ("1X", "100"), ("2X", "200"), ("4X", "400")],
         "How likely a kill drops a skull. The first\nskull of a level drops at the first kill.",
         None),
    ],
    # (a row that runs a function: A once asks, A again resets. Its help's
    # next two strings are the question and the done, which
    # menu_functions.c's gameplay_screen_update shows; it is the last row)
    "actions": [
        ("RESET SKULL PROGRESS", "skulls reset progress", "reset_progress",
         "Forget every skull found, and ask again how\nthe skulls are had.",
         ["Press A again to forget every skull found.\nThis cannot be undone.",
          "Skull progress reset. Every skull is to be\nfound again."]),
    ],
    "inputs": ["gameplay screen update"],
}
# Network Setup's server browser (menu_functions.c's lobby_browser_update): where
# its games come from (network.listings, master_list.c), the Filters screen's
# settings and its order; the master servers that answered, shown, not set
# (a row with no values: its value's text, menu_functions.c's
# master_servers_show)
SCREENS["network_setup/server_browser"] = {
    "screen": "server_browser_settings_screen", "header": _network["header"], "spacing": 30,
    "rows": [
        ("LISTS:", "network.listings",
         [("ALL GAMES", "both"), ("DEDICATED", "masters"), ("P2P GAMES", "brokers")],
         "The master servers' dedicated servers, the games\nplayers host (through the brokers), or all of them.", None),
        ("SHOW EMPTY SERVERS:", "network.browser_show_empty", YES_NO, "List games no one is playing yet.", None),
        ("SHOW FULL SERVERS:", "network.browser_show_full", YES_NO, "List games with no room left.", None),
        ("SHOW MISSING MAPS:", "network.browser_maps",
         [("NO", "no"), ("YES", "yes"), ("IF DOWNLOADABLE", "downloadable")],
         "List games on maps you do not have: IF\nDOWNLOADABLE, those Map Sharing can download.", None),
        # (a column title chosen again writes the order reversed, "-players": the row shows its order's word,
        # menu_functions.c's setting_value_index, and writes it again only if another is chosen)
        ("SORT BY:", "network.browser_sort",
         [("DEDICATED FIRST", "servers"), ("PLAYERS", "players"), ("NAME", "name"), ("MAP", "map"),
          ("GAME TYPE", "type"), ("LOCKED FIRST", "locked")],
         "The list's order. Its column titles sort it too;\nchoosing one again reverses it.", None),
        ("MASTER SERVERS:", "network.masters_file", None,
         "The master servers (masters.txt) that answered\nwhen the Server Browser last asked.", None,
         "master_servers"),
    ],
}
# (the read-only row's words, its screen's words: menu_functions.c's
# master_servers_show)
SCREENS["network_setup/server_browser"]["words"] = ["OF", "NONE LISTED", "NOT ASKED YET", "OFF"]

# Network Setup's Discord account (port/linux/src/player_link.c): the account
# linked at the master servers (masters.txt's, up to 4), which a dedicated
# server that blocks VPNs lets in from one when it is listed by the master the
# account is linked with. A row a master: its state; LINK WITH MASTER n opens
# that master's page in the browser (on OK; the master a refusing server needs
# is chosen already), UNLINK ALL forgets the account (link.action is no
# setting of config.toml's: menu_functions.c's setting_write does it). The
# rows' text: menu_functions.c's discord_account_show, its words
SCREENS["network_setup/discord_account"] = {
    "screen": "discord_account_settings_screen", "header": _network["header"], "spacing": 30,
    "wide_values": ["link_master_1", "link_master_2", "link_master_3", "link_master_4"],
    "rows": [
        ("DISCORD ACCOUNT:", "network.link_share", None,
         "The Discord account linked to this game, and with\nhow many of the master servers.", None,
         "discord_account"),
        *[(f"MASTER {n}:", "network.masters_file", None,
           "A master server (masters.txt) and the account\nlinked with it: a server it lists lets it in.", None,
           f"link_master_{n}") for n in range(1, 5)],
        ("LINK:", "link.action",
         [("NO CHANGE", "none")] + [(f"WITH MASTER {n}", f"link_{n}") for n in range(1, 5)] +
         [("UNLINK ALL", "unlink")],
         "WITH MASTER n opens that master's page in the\nbrowser (on OK). UNLINK ALL forgets the account.", None,
         "link_action"),
        ("SHOW TO SERVERS:", "network.link_share",
         [("WHEN ASKED", "when_needed"), ("ALWAYS", "always"), ("NEVER", "never")],
         "Which servers see your Discord account: WHEN\nASKED, those that block VPNs, joined from one.", None),
    ],
    "words": ["NOT LINKED", "LINKED AS", "WAITING: CODE", "REVOKED", "EXPIRED:", "OF", "A SERVER NEEDS IT", "NONE"],
}

# Network Setup's multiplayer HUD: its rows, moved from Network Setup, and the
# text chat's (port/linux/game/network_chat.c)
SCREENS["network_setup/multiplayer_hud"] = {
    "screen": "multiplayer_hud_settings_screen", "header": _network["header"], "spacing": 30,
    "rows": _hud_rows + [
        ("TEXT CHAT:", "display.text_chat", [("SHOW", "true"), ("HIDE", "false")],
         "SHOW the text chat, or HIDE it: no lines, and\nits keys and D-pad Up open nothing.", None),
        ("CHAT TEXT:", "display.chat_style", [("COLORS", "colors"), ("WHITE", "white")],
         "The text chat's colors: COLORS, names in their\nteam's color; WHITE, as Custom Edition.", None),
        ("MATCH TIMER:", "display.match_timer", ON_OFF,
         "Draw a clock at the bottom right in multiplayer,\ncounting up from the start of the game.", None),
    ],
}

# Network Setup's streamer mode (port/linux/game/text_filter.c): words hidden
# in what other players, maps and mods show, each group from a severity of
# Surge AI's on (port/assets/text_filter)
STREAMER_LEVELS = [("OFF", "off"), ("SEVERE", "severe"), ("STRONG AND WORSE", "strong"), ("MILD AND WORSE", "mild")]
SCREENS["network_setup/streamer_mode"] = {
    "screen": "streamer_mode_settings_screen", "header": _network["header"], "spacing": 30,
    "rows": [
        ("STREAMER MODE:", "streamer.enabled", ON_OFF,
         "Hide offensive words in names and text from\nother players, maps and mods, as stars.", None),
        ("CENSOR SLURS:", "streamer.slurs", STREAMER_LEVELS,
         "Slurs about race, sexuality, gender and\ndisability.", None),
        ("CENSOR SWEARING:", "streamer.swearing", STREAMER_LEVELS,
         "Swearing and sexual words (STRONG AND WORSE\nleaves the mildest showing).", None),
        ("CENSOR INSULTS:", "streamer.insults", STREAMER_LEVELS,
         "Name-calling and insults that are not slurs.", None),
    ],
}

# Network Setup in a game (the pause menu's SETTINGS: port/linux/game/
# menu_tags.c's pause_network_patch): its pages alone, without the rows that
# would drop the game (internet play) or are for the main menu (the router,
# the clipboard, the updates)
SCREENS["network_setup/in_game"] = {
    "screen": "in_game_network_settings_screen", "header": _network["header"], "spacing": 30,
    "platform_places": True, "rows": [],
    "categories": [category for category in _network["categories"] if category[1] != "network_setup/server_browser"],
}

# Controls Setup: the keyboard and mouse's actions, in groups (the order of
# port/linux/game/menu_functions.c's table of them)
CONTROL_GROUPS = ["MOVEMENT", "WEAPONS", "ACTIONS", "COMMUNICATION"]
CONTROL_ROWS = 7

# the settings whose rows have the wider spinner (a device's name)
WIDE_SETTINGS = {"audio.output_device", "audio.input_device", "streamer.slurs", "streamer.swearing",
                 "streamer.insults", "network.listings", "network.browser_maps", "network.browser_sort"}

# Teamplay Options' rows of the port's (_teamplay_options_extras): the
# gametype's FORCE TEAM SPAWNS, then the host's voice chat and vote kicks;
# each row's key, its label, its choices' words (menu_functions.c's
# gametype_options give their values) and their helps
TEAMPLAY_EDIT = "main_menu/settings_select/multiplayer_setup/teamplay_options_edit"
VOICE_QUALITIES = (8, 12, 16, 24, 32, 48, 64)
# (the rows' step: closer than the PC's 30px, so that all of Server Setup's
# fit above the help text, which moves down to TEAMPLAY_HELP_TOP)
TEAMPLAY_STEP = 26
TEAMPLAY_HELP_TOP = 351
# (game_engine.c's game_engine_force_team_spawns: the gametype's own, so
# shown in Edit Gametypes' copy too, and not on Server Setup's VOICE OPTIONS)
TEAMPLAY_GAMETYPE_ROWS = [
    ("force_team_spawns", "FORCE TEAM SPAWNS:", ["OFF", "ON"], [
        "Players will spawn at any spawn point the\\ngametype allows.",
        "Players will only spawn at their own team's\\nCapture the Flag spawn points.",
    ]),
]
TEAMPLAY_ROWS = [
    ("voice_mode", "VOICE CHAT:", ["OFF", "TEAM NEAR", "ANYONE NEAR", "TEAM", "TEAM, ENEMIES NEAR"], [
        "No voice chat during the game.",
        "Players hear their teammates who are near them.",
        "Players hear everyone who is near them.",
        "Players hear all their teammates, wherever they\\nare.",
        "Players hear all their teammates, and the enemies\\nwho are near them.",
    ]),
    ("voice_lobby", "LOBBY VOICE CHAT:", ["ON", "OFF"], [
        "Everyone hears everyone in the lobby, before and\\nafter the game.",
        "No voice chat in the lobby.",
    ]),
    ("voice_kbps", "VOICE QUALITY:", [f"{kbps} KBPS" for kbps in VOICE_QUALITIES],
     [f"Voices at {kbps} kilobits a second, in the lobby\\nand in the game." for kbps in VOICE_QUALITIES]),
    ("voice_proximity", "VOICE NEAR DISTANCE:", ["15 M", "30 M", "45 M", "60 M", "90 M", "150 M"],
     [f"Players {metres} metres apart or less are near\\nfor voice chat." for metres in (15, 30, 45, 60, 90, 150)]),
    # (network_votekick.c)
    ("votekick", "VOTE KICK:", ["ON", "OFF"], [
        "Players can vote to kick a player, from the\\nscoreboard.",
        "Players cannot vote to kick anyone.",
    ]),
    # (network_chat.c: one row for the chat and its links, as Teamplay
    # Options has room for one more)
    ("text_chat", "TEXT CHAT:", ["ON", "WITH LINKS", "OFF"], [
        "Players can type to each other; lines with links\\nare refused. Off in a public internet game.",
        "Players can type to each other, links too. Off\\nin a public internet game.",
        "No text chat in the game.",
    ]),
]
# (Teamplay Options' rows of the port's, in order)
TEAMPLAY_EXTRA_ROWS = TEAMPLAY_GAMETYPE_ROWS + TEAMPLAY_ROWS

# the profile menu's words for what its items now open
STRING_OVERRIDES = {
    f"{PE}/profile_edit_descriptions": [
        "Rename this profile.\\n\\n\\nProfile:",
        "Choose the keys and mouse\\nbuttons for each action.\\n\\nProfile:",
        "Set this profile's controller\\nsensitivity and layouts.\\n\\nProfile:",
        "Adjust the mouse's sensitivity\\nand aiming.\\n\\nProfile:",
        "Adjust the volume of the music\\nand of everything else.\\n\\nProfile:",
        "Choose a window or the full\\nscreen, the frame rate and more.\\n\\nProfile:",
        "Internet play, updates and the\\nmultiplayer HUD.\\n\\nProfile:",
        "Change the current profile's\\nfree-for-all multiplayer color.\\n\\nProfile:",
        "How players and the AI fall:\\nthe animation, or a ragdoll.\\n\\nProfile:",
        "Halo: Combat Evolved, the Xbox\\ngame, on this computer.\\n\\nProfile:",
        "How players and the AI fall,\\nthe randomizer and skulls.\\n\\nProfile:",
    ],
}


def attributes(pairs: list) -> str:
    return "".join(f" {key}={quoteattr(str(value))}" for key, value in pairs if value is not None)


def _widget(name: str, pairs: list, inner: list) -> list:
    return [f"\t<widget{attributes([('name', name)] + pairs)}>", *[f"\t\t{line}" for line in inner], "\t</widget>"]


def _strings(name: str, texts: list) -> list:
    return [f"\t<strings{attributes([('name', name)])}>",
            *[f"\t\t<string{attributes([('text', text)])}/>" for text in texts], "\t</strings>"]


def _button(name: str, caption: int, handlers: list) -> list:
    pairs = [("type", "text"), ("width", 128), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
             ("string_list", "strings/common_button_captions"), ("string_index", caption), ("font", "ui\\small_ui"),
             ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)]
    return _widget(name, pairs, handlers + ['<on event="left_mouse" run="mouse emit accept event"/>'])


def _screen(folder: str, spec: dict, rows: list, list_inputs: list, list_handlers: list, extra: list) -> list:
    """a screen: its header, the list of rows (with the help line as its
    description), and the buttons"""
    base = folder if folder.startswith("main_menu/") else f"{PE}/{folder}"
    header, header_bitmap = spec["header"]
    lines = []
    # (save_on_leave: B and Cancel write the screen, as OK does, then go back)
    leave = ['<on event="b" run="port settings save" back="true"/>',
             '<on event="back" run="port settings save" back="true"/>'] if spec.get("save_on_leave") else \
        ['<on event="b" back="true"/>', '<on event="back" back="true"/>']
    lines += _widget(f"{base}/{spec['screen']}",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child pause_game"),
                      ("bitmap", "bitmaps/gradient")],
                     [*leave,
                      f'<child{attributes([("widget", f"{base}/{header}")])}/>',
                      f'<child{attributes([("widget", f"{base}/options_menu")])}/>',
                      *[f'<child{attributes([("widget", f"{base}/note_{index}"), ("x", 54), ("y", 73 + place * spec["spacing"]), ("platform", platform)])}/>'
                        for index, (_, platform, place) in enumerate(spec.get("notes", []))]])
    lines += _widget(f"{base}/{header}", [("controller", 1), ("left", 35), ("top", 11), ("width", 605), ("height", 59),
                                          ("bitmap", header_bitmap)], [])
    # (a note is text alone, which the focus never lands on)
    for index, (text, platform, _) in enumerate(spec.get("notes", [])):
        lines += _widget(f"{base}/note_{index}", [("type", "text"), ("width", 512), ("height", 24), ("text", text),
                                                  ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                                  ("text_y", 5), ("text_flags", "no_focus_test"),
                                                  ("platform", platform)], [])
    lines += _widget(f"{base}/help", [("type", "text"), ("controller", 1), ("left", 68),
                                      ("top", spec.get("help_top", 350)), ("width", 482),
                                      ("height", spec.get("help_height", 60)), ("string_list", None if spec.get("dynamic_help") else f"{base}/help_strings"),
                                      ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    children = [f'<data input="{name}"/>' for name in list_inputs] + list_handlers
    # (each row: its widget, platform, and the place it is in, else the next)
    for index, (row, platform, *place) in enumerate(rows):
        y = 73 + (place[0] if place else index) * spec["spacing"]
        children.append(f'<child{attributes([("widget", row), ("x", 54), ("y", y), ("platform", platform)])}/>')
    children.append(f'<child{attributes([("widget", f"{base}/button_bar"), ("y", 414)])}/>')
    lines += _widget(f"{base}/options_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/help")], children)
    lines += _widget(f"{base}/button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child{attributes([("widget", f"{base}/button_defaults"), ("y", 1)])}/>',
                      f'<child{attributes([("widget", f"{base}/button_ok"), ("x", 380), ("y", 1)])}/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += extra
    return lines


def _placed(row: str, platform, members: list, places: dict) -> list:
    """a row's children (row, platform, place): one for its platforms where
    they have it in the same place, else one for each place, with the
    platforms whose place it is"""
    groups = {}
    for name in members:
        groups.setdefault(places[name], []).append(name)
    if len(groups) == 1:
        return [(row, platform, places[members[0]])]
    return [(row, " ".join(names) if len(names) < len(places) else None, place) for place, names in groups.items()]


def _setting_screen(folder: str, spec: dict) -> list:
    base = f"{PE}/{folder}"
    rows, extra = [], []
    place = -1
    # (platform_places: each platform's rows in places of their own, with
    # no gap where the others' are; a row in one place for all its
    # platforms, else a child for each place. The Switch's are its own: a row
    # for "android" is the Android app's, port/linux/src/menu_files.c)
    places = {"desktop": -1, "android": -1, "switch": -1, "ps5": -1, "web": -1}
    for index, (label, setting, choices, _, platform, *named) in enumerate(spec["rows"]):
        key = named[0] if named else setting.split(".", 1)[1]
        row = f"{base}/op_{key}"
        shares = setting in spec.get("same_place", ()) or key in spec.get("same_place", ())
        if spec.get("platform_places"):
            members = platform.split() if platform else list(places)
            for name in members:
                if not shares:
                    places[name] += 1
            rows += _placed(row, platform, members, places)
        else:
            if setting not in spec.get("same_place", ()) and key not in spec.get("same_place", ()):
                place += 1
            rows.append((row, platform, place))
        # (a row with no values shows its value, set by the game: its text; a
        # wide one, a short label and a long value in the small font)
        if choices is None:
            wide_value = key in spec.get("wide_values", ())
            label_width, value_x, value_width = (130, 130, 382) if wide_value else (300, 286, 212)
            extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                   ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF"), ("platform", platform)],
                             [f'<child{attributes([("widget", f"{base}/{key}_label")])}/>',
                              f'<child{attributes([("widget", f"{base}/{key}_value"), ("x", value_x), ("y", 1)])}/>'])
            extra += _widget(f"{base}/{key}_label",
                             [("type", "text"), ("controller", 1), ("width", label_width), ("height", 22),
                              ("string_list", f"{base}/labels"), ("string_index", index), ("font", "ui\\large_ui"),
                              ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
            extra += _widget(f"{base}/{key}_value",
                             [("type", "text"), ("controller", 1), ("width", value_width), ("height", 22),
                              ("font", "ui\\small_ui" if wide_value else "ui\\large_ui"), ("color", "#FF2896FF"),
                              ("align", "left" if wide_value else "center"), ("text_y", 5 if wide_value else 1),
                              ("text_flags", "no_focus_test")], [])
            continue
        # (a device's name wants the wider spinner Server Setup's co-op rows
        # have: WIDE_SETTINGS)
        wide = setting in WIDE_SETTINGS
        bumper_style = setting == "display.hud_bumper_style"
        controller_style = setting.startswith("display.hud_button_style")
        # Dynamic captions retain their help index without a string list that
        # would overwrite the player number during the engine's text update.
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF"), ("platform", platform)],
                         [f'<child{attributes([("widget", f"{base}/{key}_label")])}/>',
                          f'<child{attributes([("widget", f"{base}/{key}_spinner"), ("x", 240 if controller_style or bumper_style else 286 if wide else 320), ("y", 1)])}/>'])
        extra += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                          ("string_list", None if controller_style else f"{base}/labels"),
                          ("string_index", index), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        extra += _widget(f"{base}/{key}_spinner",
                         [("type", "spinner"), ("left", None if wide else 3), ("top", 2), ("width", 252 if controller_style or bumper_style else 206 if wide else 147),
                          ("height", 20), ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("strings", "|".join(shown for shown, _ in choices)), ("setting", setting),
                          ("values", "|".join(value for _, value in choices)), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4 if wide else 1),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7" if wide else "7 -6 19 0"),
                          ("footer_bounds", "7 254 19 260" if controller_style or bumper_style else "7 208 19 214" if wide else "7 150 19 156")],
                         ['<on event="created" run="port setting load"/>'] +
                         (['<on event="deleted" run="port setting save"/>'] if spec.get("save_on_leave") else []))
    # (each category's row: its label, the screen it opens, its help, and the
    # platforms it is for, as a row's: every one's when it names none)
    for index, (label, category_folder, _, *category_platform) in enumerate(spec.get("categories", ())):
        key = category_folder.rsplit("/", 1)[-1]
        row = f"{base}/op_{key}"
        target = f"{PE}/{category_folder}/{(SCREENS.get(category_folder) or CUSTOM_SCREENS[category_folder])['screen']}"
        platform = category_platform[0] if category_platform else None
        if spec.get("platform_places"):
            members = platform.split() if platform else list(places)
            for name in members:
                places[name] += 1
            rows += _placed(row, platform, members, places)
        else:
            rows.append((row, platform, place + 1 + index))
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF"), ("platform", platform)],
                         [f'<on event="a" open="{target}"/>', f'<on event="start" open="{target}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>'])
        extra += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", 512), ("height", 22),
                          ("string_list", f"{base}/labels"), ("string_index", len(spec["rows"]) + index),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    # (each action's row: its label, the function A runs, and its help, then
    # the helps that function shows)
    categories = len(spec.get("categories", ()))
    for index, (label, function, key, _, _) in enumerate(spec.get("actions", ())):
        row = f"{base}/op_{key}"
        rows.append((row, None, place + 1 + categories + index))
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<on event="a" run="{function}"/>', f'<on event="start" run="{function}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>'])
        extra += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", 512), ("height", 22),
                          ("string_list", f"{base}/labels"), ("string_index", len(spec["rows"]) + categories + index),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    # (each field's row: its label, the function A runs (the typing begins,
    # or ends), its key (its value's widget is <key>_value, which the screen's
    # inputs show), its help and the platforms it is for, as a row's; its
    # label as wide as the screen's field_label_width, its value the rest)
    actions = len(spec.get("actions", ()))
    label_width = spec.get("field_label_width", 200)
    for index, (label, function, key, _, *field_platform) in enumerate(spec.get("fields", ())):
        row = f"{base}/op_{key}"
        platform = field_platform[0] if field_platform else None
        rows.append((row, platform, place + 1 + categories + actions + index))
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF"), ("platform", platform)],
                         [f'<on event="a" run="{function}"/>', f'<on event="start" run="{function}"/>',
                          # (B ends the typing, else leaves as Cancel does)
                          '<on event="b" run="port field back"/>', '<on event="back" run="port field back"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>',
                          f'<child widget="{base}/{key}_value" x="{label_width}" y="1"/>'])
        extra += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", label_width), ("height", 22),
                          ("string_list", f"{base}/labels"),
                          ("string_index", len(spec["rows"]) + categories + actions + index),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        extra += _widget(f"{base}/{key}_value",
                         [("type", "text"), ("controller", 1), ("width", 506 - label_width), ("height", 22),
                          ("font", "ui\\small_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 5), ("text_flags", "no_focus_test")],
                         [])
    extra += _button(f"{base}/button_defaults", 3, ['<on event="a" run="port settings defaults"/>',
                                                    '<on event="start" run="port settings defaults"/>'])
    extra += _button(f"{base}/button_ok", 1, ['<on event="a" run="port settings save" back="true"/>',
                                              '<on event="start" run="port settings save" back="true"/>'])
    extra += _strings(f"{base}/labels", [label for label, *_ in spec["rows"]] +
                      [label for label, *_ in spec.get("categories", ())] +
                      [label for label, *_ in spec.get("actions", ())] +
                      [label for label, *_ in spec.get("fields", ())])
    if spec.get("words"):
        extra += _strings(f"{base}/words", spec["words"])
    # (the help of the row whose label is string n is n + 1: the buttons' is 0)
    extra += _strings(f"{base}/help_strings",
                      [""] + [help_text.replace("\n", "\\n") for _, _, _, help_text, *_ in spec["rows"]] +
                      [help_text.replace("\n", "\\n") for _, _, help_text, *_ in spec.get("categories", ())] +
                      [text.replace("\n", "\\n") for *_, help_text, more in spec.get("actions", ())
                       for text in [help_text, *more]] +
                      [help_text.replace("\n", "\\n") for _, _, _, help_text, *_ in spec.get("fields", ())])
    # (a note's place None: the one after its platform's last row)
    if spec.get("notes"):
        spec = dict(spec, notes=[(text, platform, places[platform] + 1 if place is None else place)
                                 for text, platform, place in spec["notes"]])
    return _screen(folder, spec, rows, ["port settings help", *spec.get("inputs", ())], spec.get("handlers", []),
                   extra)


def _controls_screen() -> list:
    folder = "controls_setup"
    base = f"{PE}/{folder}"
    spec = {"screen": "controls_settings_screen", "spacing": 30,
            "header": ("header_profile_controls_settings", f"{base}/header_profile_controls_settings")}
    rows, extra = [(f"{base}/op_group", None)], []
    extra += _widget(f"{base}/op_group", [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                          ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<child{attributes([("widget", f"{base}/group_label")])}/>',
                      f'<child{attributes([("widget", f"{base}/group_spinner"), ("x", 200), ("y", 1)])}/>'])
    extra += _widget(f"{base}/group_label",
                     [("type", "text"), ("controller", 1), ("width", 200), ("height", 22), ("text", "SHOW:"),
                      ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/group_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", 297), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("strings", "|".join(CONTROL_GROUPS)), ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
                      ("align", "center"), ("text_y", 1), ("header_bitmap", "bitmaps/arrow_sm_left"),
                      ("footer_bitmap", "bitmaps/arrow_sm_right"), ("header_bounds", "7 -6 19 0"),
                      ("footer_bounds", "7 300 19 306")], [])
    for index in range(CONTROL_ROWS):
        row = f"{base}/op_command_{index + 1}"
        rows.append((row, None))
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         ['<on event="a" run="controls begin binding"/>',
                          '<on event="start" run="controls begin binding"/>',
                          '<on event="left right" run="controls binding slot"/>',
                          f'<child{attributes([("widget", f"{base}/command_label")])}/>',
                          f'<child{attributes([("widget", f"{base}/command_binding"), ("x", 200)])}/>',
                          f'<child{attributes([("widget", f"{base}/command_binding"), ("x", 356)])}/>'])
    extra += _widget(f"{base}/command_label",
                     [("type", "text"), ("controller", 1), ("width", 200), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/command_binding",
                     [("type", "text"), ("controller", 1), ("width", 152), ("height", 22), ("font", "ui\\small_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 5), ("text_flags", "no_focus_test")],
                     [])
    extra += _button(f"{base}/button_defaults", 3, ['<on event="a" run="controls screen defaults"/>',
                                                    '<on event="start" run="controls screen defaults"/>'])
    extra += _button(f"{base}/button_ok", 1, ['<on event="a" run="controls screen change set" back="true"/>',
                                              '<on event="start" run="controls screen change set" back="true"/>'])
    extra += _strings(f"{base}/help_strings", [
        "Choose what to show, with left and right.",
        "Enter sets the key chosen; left and right choose\\nwhich of two. Then press the new key or button.",
        "Press a key, a mouse button or turn the wheel...\\nEscape or a controller's B cancels, Delete clears it.",
    ])
    lines = _screen(folder, spec, rows, ["controls update menu"], ['<on event="created" run="controls screen init"/>'],
                    extra)
    # (B cancels a key's setting first)
    return [line.replace('<on event="b" back="true"/>', '<on event="b" run="controls back handler"/>')
            .replace('<on event="back" back="true"/>', '<on event="back" run="controls back handler"/>')
            for line in lines]


def _controller_screen() -> list:
    folder = "controller_remapping"
    base = f"{PE}/{folder}"
    rows, extra = [], []
    for key, label, choices in [("player", "PLAYER:", "P1|P2|P3|P4"),
                                ("group", "SHOW:", "MOVEMENT|COMBAT|GENERAL|COMMUNICATION|DEBUG")]:
        row = f"{base}/op_{key}"
        rows.append((row, None))
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{base}/{key}_label"/>', f'<child widget="{base}/pad_{key}" x="200" y="1"/>'])
        extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 200),
                         ("height", 22), ("text", label), ("font", "ui\\large_ui"),
                         ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        extra += _widget(f"{base}/pad_{key}", [("type", "spinner"), ("left", 3), ("top", 2),
                         ("width", 297), ("height", 20), ("strings", choices),
                         ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                         ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                         ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                         ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 300 19 306")], [])
    for index in range(6):
        row = f"{base}/op_pad_{index + 1}"
        rows.append((row, None))
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         ['<on event="a start" run="port controller bind"/>',
                          '<on event="x" run="port controller clear"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          '<on event="left" run="port controller left"/>',
                          '<on event="right" run="port controller right"/>',
                          f'<child widget="{base}/pad_action"/>',
                          f'<child widget="{base}/pad_binding" x="220"/>'])
    extra += _widget(f"{base}/pad_action", [("type", "text"), ("controller", 1), ("width", 220),
                     ("height", 22), ("font", "ui\\small_ui"), ("color", "#FF2896FF"),
                     ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/pad_binding", [("type", "text"), ("controller", 1), ("width", 280),
                     ("height", 22), ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
                     ("align", "center"), ("text_y", 4), ("text_flags", "no_focus_test")], [])
    extra += _button(f"{base}/button_defaults", 3, ['<on event="a start" run="port controller defaults"/>'])
    extra += _button(f"{base}/button_ok", 1, ['<on event="a start" run="port controller save" back="true"/>'])
    lines = _screen(folder, SCREENS[folder], rows, ["port controller update"],
                   ['<on event="created" run="port controller init"/>',
                    '<on event="deleted" run="port controller close"/>'], extra)

    return [line.replace('<on event="b" back="true"/>', '<on event="b" run="port controller cancel" back="true"/>')
            .replace('<on event="back" back="true"/>', '<on event="back" run="port controller cancel" back="true"/>')
            for line in lines]


def _gameplay_profile_item() -> list:
    """Settings' Gameplay row, before About: the screen is this folder's"""
    name = f"{PE}/gameplay_profile_item"
    return _widget(name, [("type", "text"), ("left", 51), ("top", profile_row_top("gameplay_profile_item")), ("width", 232), ("height", 32),
                          ("bitmap", "bitmaps/list_item_bkd"), ("text", "GAMEPLAY"), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                   [f'<on event="a" open="{PE}/gameplay/gameplay_settings_screen"/>',
                    f'<on event="start" open="{PE}/gameplay/gameplay_settings_screen"/>',
                    '<on event="left_mouse" run="mouse emit accept event"/>'])


def _randomizer_spinner(base: str, key: str, index: int, setting: str, choices: list) -> list:
    """one row of the randomizer screen: its label and the setting it shows.
    Every row's spinner is as wide as the widest choice needs, so the arrows
    of all of them line up, OFF/ON's too"""
    width, x, text_y = 236, 256, 4
    bounds = ("7 -13 19 -7", f"7 {width + 2} 19 {width + 8}")
    return _widget(f"{base}/op_{key}", [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                        ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                   [f'<child widget="{base}/{key}_label"/>',
                    f'<child{attributes([("widget", f"{base}/{key}_spinner"), ("x", x), ("y", 1)])}/>']) + \
        _widget(f"{base}/{key}_label",
                [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                 ("string_list", f"{base}/labels"), ("string_index", index), ("font", "ui\\large_ui"),
                 ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], []) + \
        _widget(f"{base}/{key}_spinner",
                [("type", "spinner"), ("left", 3), ("top", 2), ("width", width), ("height", 20),
                 ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                 ("strings", "|".join(shown for shown, _ in choices)), ("setting", setting),
                 ("values", "|".join(value for _, value in choices)), ("font", "ui\\large_ui"),
                 ("color", "#FF2896FF"), ("align", "center"), ("text_y", text_y),
                 ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                 ("header_bounds", bounds[0]), ("footer_bounds", bounds[1])],
                ['<on event="created" run="port setting load"/>'])


OFF_ON = [("OFF", "false"), ("ON", "true")]
RANDOMIZER_WEAPONS = [("PISTOL", "pistol"), ("ASSAULT RIFLE", "assault_rifle"), ("SHOTGUN", "shotgun"),
                      ("SNIPER RIFLE", "sniper_rifle"), ("ROCKETS", "rocket_launcher"),
                      ("FLAMETHROWER", "flamethrower"), ("PLASMA PISTOL", "plasma_pistol"),
                      ("PLASMA RIFLE", "plasma_rifle"), ("NEEDLER", "needler")]
# (the choices of the Tag Injector's profile: menu_functions.c's
# randomizer_screen_update passes over them unless a profile is played, and
# one of its tags of their kind is on)
TAG_INJECTOR_CHOICE = ("TAG INJECTOR", "tag_injector")
RANDOMIZER_WEAPON_POOLS = [("ANY", "any"), ("COVENANT", "covenant"), ("HUMAN", "human"),
                           ("PRECISION", "precision"), ("AUTOMATIC", "automatic"), ("POWER", "power"),
                           ("ONE TYPE", "one"), TAG_INJECTOR_CHOICE]
# the randomizer screen's rows, top to bottom. menu_functions.c's
# randomizer_rows_show shows a row only while what it depends on is chosen
# (a pool's type with ONE TYPE, a rank with its type, a custom type with
# TAG INJECTOR) and closes the gaps. A row of no choices (None) is a custom
# type's: the Tag Injector profile's, which menu_functions.c shows and steps
# through (_randomizer_value_row)
RANDOMIZER_ROWS = [
    ("enemies", "ENEMIES:", "game.randomizer_enemies", OFF_ON,
     "On: a squad's actors are taken from the pool."),
    ("allies", "ALLIES TOO:", "game.randomizer_allies", OFF_ON,
     "On: the marines beside you are swapped too,\\nonce out of any vehicle. Off: they stay."),
    ("pool", "ENEMY POOL:", "game.randomizer_pool",
     [("ANY", "any"), ("COVENANT", "covenant"), ("FLOOD", "flood"), ("HUMAN", "human"), ("ONE TYPE", "one"),
      TAG_INJECTOR_CHOICE],
     "Which actors a squad may become. Tag Injector:\\nthe profile's AI alone."),
    ("type", "ENEMY TYPE:", "game.randomizer_type",
     [("GRUNT", "grunt"), ("ELITE", "elite"), ("JACKAL", "jackal"), ("HUNTER", "hunter"),
      ("COMBAT FORM", "flood_combat"), ("CARRIER FORM", "flood_carrier"), ("MARINE", "marine"),
      ("ENGINEER", "engineer"), ("SENTINEL", "sentinel"), TAG_INJECTOR_CHOICE],
     "The only actor a squad may become."),
    ("custom_type", "CUSTOM TYPE:", "game.randomizer_custom_type", None,
     "The Tag Injector profile's AI of this type alone.\\nLeft and right choose another."),
    ("grunt_rank", "RANK:", "game.randomizer_grunt_rank",
     [("ANY", "any"), ("MINOR", "minor"), ("MAJOR", "major"), ("SPEC OPS", "specops")],
     "Which grunts."),
    ("elite_rank", "RANK:", "game.randomizer_elite_rank",
     [("ANY", "any"), ("MINOR", "minor"), ("MAJOR", "major"), ("SPEC OPS", "specops"), ("ZEALOT", "zealot"),
      ("STEALTH", "stealth")],
     "Which elites. Zealots are the gold commanders."),
    ("jackal_rank", "RANK:", "game.randomizer_jackal_rank",
     [("ANY", "any"), ("MINOR", "minor"), ("MAJOR", "major")], "Which jackals."),
    ("hunter_rank", "RANK:", "game.randomizer_hunter_rank",
     [("ANY", "any"), ("MINOR", "minor"), ("MAJOR", "major")], "Which hunters."),
    ("flood_combat_rank", "FORM OF:", "game.randomizer_flood_combat_rank",
     [("ANY", "any"), ("HUMAN", "human"), ("ELITE", "elite")], "Which combat forms."),
    ("pickups", "WEAPON PICKUPS:", "game.randomizer_pickups", OFF_ON,
     "On: a weapon lying in the level is swapped.\\nOne a script names stays."),
    ("pickup_pool", "PICKUP POOL:", "game.randomizer_pickup_pool", RANDOMIZER_WEAPON_POOLS,
     "Which weapons a pickup may become. A weapon\\nthe level lacks comes from another level."),
    ("pickup_type", "PICKUP TYPE:", "game.randomizer_pickup_type", RANDOMIZER_WEAPONS + [TAG_INJECTOR_CHOICE],
     "The only weapon a pickup may become."),
    ("pickup_custom", "CUSTOM TYPE:", "game.randomizer_pickup_custom", None,
     "The Tag Injector profile's weapons of this type\\nalone. Left and right choose another."),
    ("enemy_weapons", "ENEMY WEAPONS:", "game.randomizer_enemy_weapons", OFF_ON,
     "On: an actor's weapon is swapped for one it\\ncan hold; one that can hold none keeps its own."),
    ("weapon_pool", "WEAPON POOL:", "game.randomizer_weapon_pool", RANDOMIZER_WEAPON_POOLS,
     "Which weapons an actor may be given. An actor\\nthat can hold none of them keeps its own."),
    ("weapon_type", "WEAPON TYPE:", "game.randomizer_weapon_type",
     RANDOMIZER_WEAPONS + [("FUEL ROD", "fuel_rod"), ("ENERGY SWORD", "energy_sword"), TAG_INJECTOR_CHOICE],
     "Only enemies that can hold it get it; others\\nkeep theirs (Go Crazy lends them the hands)."),
    ("weapon_custom", "CUSTOM TYPE:", "game.randomizer_weapon_custom", None,
     "The Tag Injector profile's weapons of this type\\nalone, for those that can hold them."),
]


def _randomizer_value_row(base: str, key: str, index: int) -> list:
    """a custom type's row of the randomizer screen: its label, and the value
    menu_functions.c shows (the Tag Injector profile's custom types, which no
    spinner's choices could be: they are the profile's), where a spinner's
    is; left, right and A step through them ("randomizer custom left" and
    "right")"""
    return _widget(f"{base}/op_{key}", [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                        ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                   ['<on event="left" run="randomizer custom left"/>',
                    '<on event="stick_left" run="randomizer custom left"/>',
                    '<on event="right" run="randomizer custom right"/>',
                    '<on event="stick_right" run="randomizer custom right"/>',
                    '<on event="a" run="randomizer custom right"/>',
                    '<on event="left_mouse" run="mouse emit accept event"/>',
                    f'<child widget="{base}/{key}_label"/>',
                    f'<child{attributes([("widget", f"{base}/{key}_value"), ("x", 259), ("y", 3)])}/>']) + \
        _widget(f"{base}/{key}_label",
                [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                 ("string_list", f"{base}/labels"), ("string_index", index), ("font", "ui\\large_ui"),
                 ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], []) + \
        _widget(f"{base}/{key}_value",
                # (as tall as the label: the large font's line is drawn only
                # when its baseline is above the box's bottom, and 20 less
                # text_y's 4 is too short for it)
                [("type", "text"), ("controller", 1), ("width", 236), ("height", 22), ("font", "ui\\large_ui"),
                 ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4), ("text_flags", "no_focus_test")], [])


def _randomizer_screen() -> list:
    """the screen difficulty opens when the randomizer is on: what is
    swapped and from which pools, and the buttons: Save (a setup under a
    name), Load (the profiles' screen), OK, which starts the level"""
    base = "main_menu/difficulty_select/randomizer"
    lines = _widget(f"{base}/randomizer_screen",
                    [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                     ("bitmap", "bitmaps/gradient")],
                    ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                     f'<child widget="{base}/header_randomizer"/>',
                     f'<child widget="{base}/options_menu"/>'])
    lines += _widget(f"{base}/header_randomizer",
                     [("controller", 1), ("left", 35), ("top", 11), ("width", 605), ("height", 59),
                      ("bitmap", f"{base}/header_randomizer")], [])
    lines += _widget(f"{base}/help",
                     [("type", "text"), ("controller", 1), ("left", 68), ("top", 388), ("width", 482), ("height", 24),
                      ("string_list", f"{base}/help_strings"), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF")], [])
    children = ['<data input="port settings help"/>', '<data input="randomizer screen update"/>']
    extra = []
    labels = []
    for index, (key, label, setting, choices, _) in enumerate(RANDOMIZER_ROWS):
        # (each where it is with every row shown; the screen moves them up)
        children.append(f'<child{attributes([("widget", f"{base}/op_{key}"), ("x", 54), ("y", 70 + index * 26)])}/>')
        if choices is None:
            extra += _randomizer_value_row(base, key, index)
        else:
            extra += _randomizer_spinner(base, key, index, setting, choices)
        labels.append(label)
    children.append(f'<child{attributes([("widget", f"{base}/button_bar"), ("y", 414)])}/>')
    lines += _widget(f"{base}/options_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/help")], children)
    lines += _widget(f"{base}/button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child{attributes([("widget", f"{base}/button_save"), ("y", 1)])}/>',
                      f'<child{attributes([("widget", f"{base}/button_load"), ("x", 130), ("y", 1)])}/>',
                      f'<child{attributes([("widget", f"{base}/button_ok"), ("x", 380), ("y", 1)])}/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += extra
    lines += _widget(f"{base}/button_save",
                     [("type", "text"), ("width", 128), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
                      ("text", "SAVE"), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                      ("text_y", 2)],
                     ['<on event="a" run="randomizer profile save"/>',
                      '<on event="start" run="randomizer profile save"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/button_load",
                     [("type", "text"), ("width", 128), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
                      ("text", "LOAD"), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                      ("text_y", 2)],
                     [f'<on event="a" open="{base}/profiles_screen"/>',
                      f'<on event="start" open="{base}/profiles_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _button(f"{base}/button_ok", 1, ['<on event="a" run="randomizer start"/>',
                                              '<on event="start" run="randomizer start"/>'])
    lines += _strings(f"{base}/labels", labels)
    lines += _strings(f"{base}/help_strings",
                      ["Save keeps this setup under a name.\\nLoad chooses one saved before."] +
                      [help_text for *_, help_text in RANDOMIZER_ROWS])
    lines += _randomizer_profiles_screen(base)
    lines += _randomizer_tag_profiles_screen(base)
    return lines


# the skulls, in port/linux/game/skulls.c's order (halo_skulls.h's enum
# skull): each one's key in game.skulls_active and its name on the screen.
# Their helps are skulls.c's, which the screen shows
SKULLS = [
    ("anger", "ANGER"), ("armistice", "ARMISTICE"), ("assassins", "ASSASSINS"), ("bandanna", "BANDANNA"),
    ("black_eye", "BLACK EYE"), ("blind", "BLIND"), ("boom", "BOOM"), ("catch", "CATCH"),
    ("confetti_party", "CONFETTI PARTY"), ("efficient", "EFFICIENT"), ("envy", "ENVY"), ("eye_patch", "EYE PATCH"),
    ("famine", "FAMINE"), ("floor_is_lava", "FLOOR IS LAVA"), ("fog", "FOG"), ("foreign", "FOREIGN"),
    ("ghost", "GHOST"), ("give_and_take", "GIVE & TAKE"), ("grunt_birthday_party", "GRUNT BIRTHDAY"),
    ("grunt_funeral", "GRUNT FUNERAL"), ("iron", "IRON"), ("johnny_ammo", "JOHNNY AMMO"), ("leadhead", "LEADHEAD"),
    ("m_rating", "M RATING"), ("malfunction", "MALFUNCTION"), ("mythic", "MYTHIC"),
    ("need_glasses", "NEED GLASSES?"), ("outbreak", "OUTBREAK"), ("pinata", "PINATA"), ("protected", "PROTECTED"),
    ("recession", "RECESSION"), ("rip_and_tear", "RIP & TEAR"), ("scarab", "SCARAB"), ("sputnik", "SPUTNIK"),
    ("streaking", "STREAKING"), ("swarm", "SWARM"), ("thats_just_wrong", "JUST... WRONG"),
    ("thunderstorm", "THUNDERSTORM"), ("tough_luck", "TOUGH LUCK"), ("woe_is_me", "WOE IS ME"),
]
SKULL_COLUMNS = 4
# a skull's place in the grid: its bar (port/assets/skulls/option, as
# option_bkds_sm is, four to a row across the whole screen, end to end)
# and the rows' pitch
SKULL_LEFT = 0
SKULL_WIDTH = 160
SKULL_PITCH_X = 160
SKULL_PITCH_Y = 29
SKULL_TOP = 74
# the grid's moves, which menu_functions.c's "skulls up" (and so on) make:
# the screen's list steps none itself
SKULL_MOVES = [('up', 'skulls up'), ('stick_up', 'skulls up'), ('down', 'skulls down'),
               ('stick_down', 'skulls down'), ('left', 'skulls left'), ('stick_left', 'skulls left'),
               ('right', 'skulls right'), ('stick_right', 'skulls right')]


def _skulls_screen() -> list:
    """the screen difficulty opens (after the randomizer's) when the skulls
    are on: the skulls in two columns, A one to turn it on or off
    (menu_functions.c's "skull toggle", which "skulls screen update" ticks,
    with the chosen one's help), and the buttons: Toggle All, Random (the
    screen of how many to turn on at random), OK, which starts the level, and
    Cancel. One list of them all, the skulls row by row and
    then the buttons, which menu_functions.c moves the focus through as a
    grid: a list of a row each would keep each row's item lit"""
    base = "main_menu/difficulty_select/skulls"
    moves = [f'<on event="{event}" run="{function}"/>' for event, function in SKULL_MOVES]
    lines = _widget(f"{base}/skulls_screen",
                    [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                     ("bitmap", "bitmaps/gradient")],
                    ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                     f'<child widget="{base}/header_skulls"/>',
                     f'<child widget="{base}/options_menu"/>'])
    lines += _widget(f"{base}/header_skulls",
                     [("controller", 1), ("left", 35), ("top", 11), ("width", 605), ("height", 59),
                      ("bitmap", f"{base}/header_skulls")], [])
    lines += _widget(f"{base}/help",
                     [("type", "text"), ("controller", 1), ("left", 68), ("top", 372), ("width", 504), ("height", 36),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF")], [])
    children = ['<data input="skulls screen update"/>']
    extra = []
    # (no text of their own: menu_functions.c sets each its name, ticked or
    # not, which a string list's text would overwrite)
    for index, (key, _) in enumerate(SKULLS):
        row, column = divmod(index, SKULL_COLUMNS)
        children.append(f'<child{attributes([("widget", f"{base}/skull_{key}"), ("x", SKULL_LEFT + column * SKULL_PITCH_X), ("y", SKULL_TOP + row * SKULL_PITCH_Y)])}/>')
        extra += _widget(f"{base}/skull_{key}",
                         [("type", "text"), ("width", SKULL_WIDTH), ("height", 28), ("bitmap", "skulls/option"),
                          ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("text_x", 6), ("text_y", 7)],
                         ['<on event="a" run="skull toggle"/>', '<on event="start" run="skull toggle"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>', *moves])
    for name, x in (("button_toggle_all", 0), ("button_random", 130), ("button_ok", 380), ("button_cancel", 510)):
        children.append(f'<child{attributes([("widget", f"{base}/{name}"), ("x", x), ("y", 415)])}/>')
    lines += _widget(f"{base}/options_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child"), ("description", f"{base}/help")], children)
    lines += extra
    lines += _widget(f"{base}/button_toggle_all",
                     [("type", "text"), ("width", 128), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
                      ("text", "TOGGLE ALL"), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                      ("text_y", 2)],
                     ['<on event="a" run="skulls toggle all"/>',
                      '<on event="start" run="skulls toggle all"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>', *moves])
    lines += _text_button(f"{base}/button_random", "RANDOM", [f'<on event="a" open="{base}/random_screen"/>',
                                                              f'<on event="start" open="{base}/random_screen"/>',
                                                              *moves])
    lines += _button(f"{base}/button_ok", 1, ['<on event="a" run="skulls start"/>',
                                              '<on event="start" run="skulls start"/>', *moves])
    lines += _skulls_unlock_screen(base)
    lines += _skulls_random_screen(base)
    # (Cancel as shell.xml's common_button_cancel, with the grid's moves)
    lines += _widget(f"{base}/button_cancel",
                     [("type", "text"), ("width", 128), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
                      ("string_list", "strings/common_button_captions"), ("string_index", 2),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="mouse emit back event"/>',
                      '<on event="start" run="mouse emit back event"/>',
                      '<on event="left_mouse" run="mouse emit back event"/>', *moves])
    return lines


# the question the skulls screen asks before it opens the first time
# (menu_functions.c's "skulls unlock all", "skulls unlock play" and
# "skulls unlock pickup")
SKULLS_UNLOCK_TEXT = [
    "How should the skulls be unlocked?",
    "",
    "ALL UNLOCKED: every skull can be turned on right away.",
    "",
    "UNLOCK VIA GAMEPLAY: enemies carry the skulls. Pick one",
    "up to enable its effect, then finish the level to",
    "unlock it.",
    "",
    "UNLOCK ON PICKUP: enemies carry the skulls. Pick one up",
    "to unlock it.",
]
SKULLS_UNLOCK_CHOICES = [("unlock_all", "ALL UNLOCKED", "skulls unlock all"),
                         ("unlock_play", "UNLOCK VIA GAMEPLAY", "skulls unlock play"),
                         ("unlock_pickup", "UNLOCK ON PICKUP", "skulls unlock pickup")]
# ... and UNLOCK VIA GAMEPLAY's, after it ("skulls death lose", "skulls
# death keep"; B back to the first: "skulls death back")
SKULLS_DEATH_TEXT = [
    "Should dying lose the skulls you picked up?",
    "",
    "LOSE THEM ON DEATH: a skull you picked up is lost if",
    "you die or return to a checkpoint before you finish",
    "the level. It drops again in another level.",
    "",
    "KEEP THEM: a skull you picked up stays with you until",
    "you finish the level, however often you die.",
]
SKULLS_DEATH_CHOICES = [("death_lose", "LOSE THEM ON DEATH", "skulls death lose"),
                        ("death_keep", "KEEP THEM", "skulls death keep")]


def _skulls_question_screen(base: str, name: str, text: list, choices: list, back: str = None) -> list:
    """a question of how the skulls are had: its text and a row of each
    answer, which menu_functions.c runs (the last opening the skulls
    screen); B and Cancel back, or run the function back"""
    leave = [f'<on event="b" run="{back}"/>', f'<on event="back" run="{back}"/>'] if back else \
        ['<on event="b" back="true"/>', '<on event="back" back="true"/>']
    lines = _widget(f"{base}/{name}_screen",
                    [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                     ("bitmap", "bitmaps/gradient")],
                    [*leave,
                     f'<child widget="{base}/header_skulls"/>',
                     f'<child widget="{base}/{name}_text"/>',
                     f'<child widget="{base}/{name}_menu"/>'])
    lines += _widget(f"{base}/{name}_text",
                     [("type", "text"), ("controller", 1), ("left", 40), ("top", 84), ("width", 560), ("height", 196),
                      ("text", "\\n".join(text)), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF")], [])
    children = []
    for index, (row, _, _) in enumerate(choices):
        children.append(f'<child{attributes([("widget", f"{base}/{row}"), ("x", 64), ("y", 290 + index * 32)])}/>')
    children.append('<child widget="common_button_cancel" x="510" y="415"/>')
    lines += _widget(f"{base}/{name}_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children")], children)
    for row, label, function in choices:
        lines += _widget(f"{base}/{row}",
                         [("type", "text"), ("width", 512), ("height", 28), ("bitmap", "bitmaps/option_bkds"),
                          ("text", label), ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                          ("text_y", 4)],
                         [f'<on event="a" run="{function}"/>', f'<on event="start" run="{function}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>'])
    return lines


# the skulls screen's RANDOM: the fewest and the most skulls it turns on
# (menu_functions.c's "skulls random roll": a number between them of the
# skulls had, at random, and the rest off)
SKULLS_RANDOM_ROWS = [("random_min", "FEWEST SKULLS:", "game.skulls_random_min"),
                      ("random_max", "MOST SKULLS:", "game.skulls_random_max")]


def _skulls_random_screen(base: str) -> list:
    """RANDOM's screen, over the skulls screen: what it does, a spinner each
    of the fewest and the most skulls, and the buttons: Roll, which turns the
    skulls on and goes back to the skulls screen, and Cancel"""
    lines = _widget(f"{base}/random_screen",
                    [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                     ("bitmap", "bitmaps/gradient")],
                    ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                     f'<child widget="{base}/header_skulls"/>',
                     f'<child widget="{base}/random_text"/>',
                     f'<child widget="{base}/random_menu"/>'])
    lines += _widget(f"{base}/random_text",
                     [("type", "text"), ("controller", 1), ("left", 68), ("top", 84), ("width", 504), ("height", 80),
                      ("text", "How many skulls should RANDOM turn on?\\n\\nROLL turns on a number between these two, "
                               "each skull\\nat random among those you have, and the rest off."),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF")], [])
    choices = [(str(count), str(count)) for count in range(len(SKULLS) + 1)]
    children = []
    extra = []
    for index, (key, _, setting) in enumerate(SKULLS_RANDOM_ROWS):
        children.append(f'<child{attributes([("widget", f"{base}/op_{key}"), ("x", 64), ("y", 190 + index * 32)])}/>')
        extra += _randomizer_spinner(base, key, index, setting, choices)
    children.append(f'<child{attributes([("widget", f"{base}/random_button_bar"), ("y", 414)])}/>')
    lines += _widget(f"{base}/random_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children")], children)
    lines += _widget(f"{base}/random_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child{attributes([("widget", f"{base}/button_roll"), ("x", 380), ("y", 1)])}/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += extra
    lines += _text_button(f"{base}/button_roll", "ROLL", ['<on event="a" run="skulls random roll"/>',
                                                         '<on event="start" run="skulls random roll"/>'])
    lines += _strings(f"{base}/labels", [label for _, label, _ in SKULLS_RANDOM_ROWS])
    return lines


def _skulls_unlock_screen(base: str) -> list:
    """the questions of how the skulls are had: every one now, or those found
    playing, and then whether a death loses those picked up; menu_functions.c
    then opens the skulls screen"""
    return _skulls_question_screen(base, "unlock", SKULLS_UNLOCK_TEXT, SKULLS_UNLOCK_CHOICES) + \
        _skulls_question_screen(base, "death", SKULLS_DEATH_TEXT, SKULLS_DEATH_CHOICES, back="skulls death back")


def _randomizer_profiles_screen(base: str) -> list:
    """Load's screen: the setups saved, A one to use it (and back: the code
    goes back itself, which it does not while DELETE is on), and the buttons:
    DELETE (the code's words: DELETE, or DONE while it is on, A on a setup
    then marking it and A again deleting it, as X twice does), and Cancel"""
    lines = _widget(f"{base}/profiles_screen",
                    [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                     ("bitmap", "bitmaps/gradient")],
                    ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                     f'<child widget="{base}/header_randomizer"/>',
                     f'<child widget="{base}/profiles_menu"/>'])
    lines += _widget(f"{base}/profiles_help",
                     [("type", "text"), ("controller", 1), ("left", 68), ("top", 388), ("width", 482), ("height", 24),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF")], [])
    children = ['<data input="randomizer profiles update"/>', '<on event="created" run="randomizer profiles init"/>']
    extra = []
    for index in range(8):
        children.append(f'<child{attributes([("widget", f"{base}/profile_{index}"), ("x", 54), ("y", 80 + index * 32)])}/>')
        extra += _widget(f"{base}/profile_{index}",
                         [("type", "text"), ("width", 512), ("height", 28), ("bitmap", "bitmaps/option_bkds"),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)],
                         ['<on event="a" run="randomizer profile load"/>',
                          '<on event="start" run="randomizer profile load"/>',
                          '<on event="x" run="randomizer profile delete"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>'])
    children.append(f'<child{attributes([("widget", f"{base}/profiles_button_bar"), ("y", 414)])}/>')
    lines += _widget(f"{base}/profiles_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/profiles_help")], children)
    lines += _widget(f"{base}/profiles_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{base}/profiles_button_delete" y="1"/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += extra
    lines += _text_button(f"{base}/profiles_button_delete", None,
                          ['<on event="a" run="randomizer profiles delete mode"/>',
                           '<on event="start" run="randomizer profiles delete mode"/>'])
    return lines


# the screen before the randomizer's (menu_functions.c's difficulty_start),
# with the Tag Injector on and a profile: which profile's tags
# the randomizer plays, or none (game.randomizer_tag_profile). Its rows a
# list at a time ("randomizer tag profiles update" moves it on at its ends)
RANDOMIZER_TAG_PROFILE_ROWS = 9


def _randomizer_tag_profiles_screen(base: str) -> list:
    """the profiles of tags, NONE first: A one to play it, then the
    randomizer's screen ("randomizer tag profile choose"); B back to the
    difficulty"""
    lines = _widget(f"{base}/tag_profile_screen",
                    [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                     ("bitmap", "bitmaps/gradient")],
                    ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                     f'<child widget="{base}/header_randomizer"/>',
                     f'<child widget="{base}/tag_profile_title"/>',
                     f'<child widget="{base}/tag_profile_menu"/>'])
    lines += _widget(f"{base}/tag_profile_title",
                     [("type", "text"), ("controller", 1), ("left", 68), ("top", 74), ("width", 504), ("height", 22),
                      ("text", "TAG INJECTOR PROFILE"), ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/tag_profile_help",
                     [("type", "text"), ("controller", 1), ("left", 68), ("top", 388), ("width", 482), ("height", 24),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF")], [])
    children = ['<data input="randomizer tag profiles update"/>']
    extra = []
    for index in range(RANDOMIZER_TAG_PROFILE_ROWS):
        children.append(f'<child{attributes([("widget", f"{base}/tag_profile_{index}"), ("x", 54), ("y", 100 + index * 31)])}/>')
        extra += _widget(f"{base}/tag_profile_{index}",
                         [("type", "text"), ("width", 512), ("height", 28), ("bitmap", "bitmaps/option_bkds"),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)],
                         ['<on event="a" run="randomizer tag profile choose"/>',
                          '<on event="start" run="randomizer tag profile choose"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>'])
    children.append(f'<child{attributes([("widget", f"{base}/tag_profile_button_bar"), ("y", 414)])}/>')
    lines += _widget(f"{base}/tag_profile_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/tag_profile_help")], children)
    lines += _widget(f"{base}/tag_profile_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += extra
    return lines


# the Tag Injector (port/linux/game/tag_injector.c, docs/tag_injector.md):
# Gameplay's TAG INJECTOR, a screen laid out as the mods' is (_mods), its rows
# a list at a time. The first: TAG INJECTION (on or off), TAG PROFILES and
# MAPS. TAG PROFILES lists the profiles, each with its size; A on one lists
# its tags (where each came from, and its size), left and right turning one
# on or off, A opening its settings (on, faction, type or class, custom type,
# taking it out). MAPS: the profile tags are added to, then CUSTOM
# SINGLEPLAYER and CUSTOM MULTIPLAYER, each its folders of custom_maps (and
# its loose maps), a folder its maps; a map's kinds of tags; a kind's tags,
# each added to the profile or taken out by A. B (and BACK) goes back up a
# list; the button bar has nothing in the first list, NEW PROFILE (a name
# asked for) in TAG PROFILES, DUPLICATE PROFILE (a name asked for the copy)
# and DELETE PROFILE (twice) in a profile's lists, and NEW PROFILE and VIEW
# <name> PROFILE (the profile MAPS adds to shown; B goes back) in MAPS' lists
# (menu_functions.c's "port tags" functions)
TAG_INJECTOR = f"{PE}/gameplay/tag_injector"
TAG_INJECTOR_ROWS = MOD_ROWS
# the preview of a tag's model under its details (port/linux/game/tag_preview.c,
# whose picture is of its shape), and under it, at the frame's foot, what the
# tag needs to work: left, top, width and height
TAG_PREVIEW_BOUNDS = (412, 256, 200, 80)
TAG_NEEDS_BOUNDS = (412, 338, 200, 64)
# the screens of a category that are no settings screen (_setting_screen),
# made by a function of their own
CUSTOM_SCREENS = {"gameplay/tag_injector": {"screen": "tag_injector_screen"}}


def _tag_injector_screen() -> list:
    """Gameplay's TAG INJECTOR: on the left two thirds the rows of the list
    shown, a row each with its name and state (ON, a profile's size, a tag's,
    a setting's value, a folder's maps, a kind's tags, ADDED); on the right
    third, in the lobby's frame, the row chosen's details, the word of the
    last thing done above them"""
    base = TAG_INJECTOR
    back = ['<on event="b" run="port tags back"/>', '<on event="back" run="port tags back"/>']
    lines = _widget(f"{base}/tag_injector_screen", [("width", 640), ("height", 480),
                                                   ("flags", "pass_unhandled_to_focused_child pause_game"),
                                                   ("bitmap", "bitmaps/gradient")],
                    [*back, '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                     f'<child widget="{base}/tags_list"/>', f'<child widget="{base}/header_gameplay"/>'])
    lines += _widget(f"{base}/header_gameplay", [("controller", 1), ("left", 35), ("top", 11), ("width", 605),
                                                ("height", 59), ("bitmap", _GAMEPLAY_HEADER[1])], [])
    rows = [f'<child widget="{base}/op_tag_{index + 1}" x="20" y="{73 + 30 * index}"/>'
            for index in range(TAG_INJECTOR_ROWS)]
    lines += _widget(f"{base}/tags_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                           ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                           ("description", f"{base}/tag_details")],
                     ['<data input="port tags update"/>', '<on event="created" run="port tags init"/>',
                      '<on event="deleted" run="port tags close"/>', *rows,
                      f'<child widget="{base}/button_bar" y="414"/>'])
    for index in range(TAG_INJECTOR_ROWS):
        lines += _widget(f"{base}/op_tag_{index + 1}",
                         [("width", 390), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("color", "#FF2896FF")],
                         ['<on event="a" run="port tags choose"/>', '<on event="start" run="port tags choose"/>',
                          '<on event="left" run="port tags left"/>', '<on event="right" run="port tags right"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/tag_name" x="12"/>',
                          f'<child widget="{base}/tag_state" x="250"/>'])
    lines += _widget(f"{base}/tag_name",
                     [("type", "text"), ("controller", 1), ("width", 238), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_y", 4)], [])
    lines += _widget(f"{base}/tag_state",
                     [("type", "text"), ("controller", 1), ("width", 134), ("height", 22), ("font", "ui\\small_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 6), ("text_flags", "no_focus_test")],
                     [])
    # (in the lobby's frame, sel_list_desc_bkd's: the row chosen's name, the
    # rest, and under it a map's or a profile's tag's model, turning (blank,
    # its picture the code's), and what the tag needs to work)
    lines += _widget(f"{base}/tag_details", [("width", 640), ("height", 480)],
                     [f'<child widget="{base}/tag_details_name"/>', f'<child widget="{base}/tag_details_text"/>',
                      f'<child widget="{base}/tag_preview"/>', f'<child widget="{base}/tag_needs"/>'])
    lines += _widget(f"{base}/tag_details_name",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 84), ("width", 200), ("height", 44),
                      ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/tag_details_text",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 130), ("width", 200),
                      ("height", 272), ("font", "ui\\small_ui"), ("color", "#FF2896FF")], [])
    left, top, width, height = TAG_PREVIEW_BOUNDS
    lines += _widget(f"{base}/tag_preview", [("controller", 1), ("left", left), ("top", top), ("width", width),
                                            ("height", height), ("bitmap", "blank")], [])
    left, top, width, height = TAG_NEEDS_BOUNDS
    lines += _widget(f"{base}/tag_needs",
                     [("type", "text"), ("controller", 1), ("left", left), ("top", top), ("width", width),
                      ("height", height), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{base}/button_new" y="1"/>',
                      f'<child widget="{base}/button_profile" x="164" y="1"/>',
                      f'<child widget="{base}/button_back" x="510" y="1"/>'])
    # (their words, and whether they are there, the code's: NEW PROFILE or
    # DUPLICATE PROFILE, DELETE PROFILE or VIEW <name> PROFILE)
    lines += _text_button(f"{base}/button_new", None, ['<on event="a" run="port tags new profile"/>',
                                                       '<on event="start" run="port tags new profile"/>'],
                          width=160)
    lines += _text_button(f"{base}/button_profile", None,
                          ['<on event="a" run="port tags profile button"/>',
                           '<on event="start" run="port tags profile button"/>'], width=250)
    lines += _text_button(f"{base}/button_back", "BACK", ['<on event="a" run="port tags back"/>',
                                                         '<on event="start" run="port tags back"/>'])
    return lines


def settings_files() -> dict:
    """the files of the port's screens, by their names in ce/"""
    files = {}
    for folder, spec in SCREENS.items():
        lines = _controller_screen() if folder == "controller_remapping" else _setting_screen(folder, spec)
        if folder == "gameplay":
            lines += _gameplay_profile_item()
        files[f"{PE}/{folder}".replace("/", ".") + ".xml"] = lines
    files[TAG_INJECTOR.replace("/", ".") + ".xml"] = _tag_injector_screen()
    files[f"{PE}/controls_setup".replace("/", ".") + ".xml"] = _controls_screen()
    files["main_menu.difficulty_select.randomizer.xml"] = _randomizer_screen()
    files["main_menu.difficulty_select.skulls.xml"] = _skulls_screen()
    return {name: ['<?xml version="1.0" encoding="UTF-8"?>',
                   "<!-- The port's settings screen, in the PC version's style (tools/port_settings.py) -->",
                   "<menus>", *lines, "</menus>", ""] for name, lines in files.items()}


def replaced(folder: str) -> bool:
    """whether the PC version's folder of widgets is replaced by ours (and
    those below it: the controls' advanced screen, the video test)"""
    return any(folder == path or folder.startswith(f"{path}/")
               for path in [*(f"{PE}/{name}" for name in [*SCREENS, "controls_setup"]), *REPLACED_FOLDERS])


# ---------- multiplayer (PLAN.md section 11)

MT = "main_menu/multiplayer_type_select"

# Multiplayer's rows, in order: the PC version's, Create Game's LOCAL
# (_create_local) and CO-OP CAMPAIGN (_coop), 29 apart (not the PC version's
# 33) so that CO-OP still ends above the divider and Edit Gametypes
# (tools/ce_menus.py's CHILD_OFFSETS), as the profile settings' ten rows fit
# in the nine's room. JOIN GAME and CREATE GAME are labels, 32 to the left
MULTIPLAYER_ROWS = ["multiplayer_type_join_item", "multiplayer_type_join_internet_item",
                    "multiplayer_type_join_lan_item", "multiplayer_type_join_direct_item",
                    "multiplayer_type_create_item", "multiplayer_type_create_internet_item",
                    "multiplayer_type_create_lan_item", "multiplayer_type_create_local_item",
                    "multiplayer_type_coop_item"]
MULTIPLAYER_LABEL_ROWS = {"multiplayer_type_join_item", "multiplayer_type_create_item"}


def multiplayer_row_place(row: str) -> tuple:
    """a Multiplayer row's place in its list (x, y)"""
    return (-32 if row in MULTIPLAYER_LABEL_ROWS else 0, 78 + 29 * MULTIPLAYER_ROWS.index(row))


# strings added to the PC version's lists: (at, strings) of each list. Slayer's
# kills to win goes up to 500, for big games (the Xbox editor's are
# source/interface/ui_widget.c's kills_to_win_extra_strings): the values,
# and their helps after the five of its own (its helps are the rows' values'
# in turn: menu_functions.c's gametype_option_help)
SLAYER_EDIT = "main_menu/settings_select/multiplayer_setup/playlist_edit/slayer_edit"
PLAYER_EDIT = "main_menu/settings_select/multiplayer_setup/player_options_edit"
# (its rows, the PC version's, in order: ce_menus.py moves them 27 apart)
PLAYER_OPTIONS_ROWS = ['number_of_lives', 'maximum_health', 'shields', 'respawn_time', 'respawn_time_growth', 'odd_man_out', 'invisible_players', 'suicide_penalty']
# the vehicle sets' PC set (game_engine.h's VARIANT_VEHICLE_SET_PC), between
# GUN TURRETS and CUSTOM (menu_functions.c's VEHICLE_PRESET_PC), and its help
# among the sets' (its rows' values' in turn: gametype_option_help)
VEHICLE_EDIT = "main_menu/settings_select/multiplayer_setup/vehicle_options_edit"
AIM_ASSIST_HELP = [
    "Players keep their current mouse aim assist setting.",
    "Disable mouse aim assist for mouse/keyboard players.\\nController and touch aim assist are unchanged.",
]
# the host's shield effects, for every player of a network game
# (network_shield_effects.c): their words, then their helps
SHIELD_EFFECTS = ['STOCK', 'GLOW', 'LIGHT SHOW', 'OVERSHIELD', 'GLOW+OVERSHIELD', 'SHOW+OVERSHIELD',
                  "PLAYERS' OWN"]
SHIELD_EFFECTS_HELP = [
    "Everyone sees the stock game's shields and\\novershield, whatever their own settings.",
    "Everyone's shields light their surroundings as\\nthey flare.",
    "Everyone's shields light their surroundings as\\nthey flare, three times as far (Light Show).",
    "For everyone, the overshield rises up the body\\nand stays lit, in its own color.",
    "For everyone, shields light their surroundings,\\nand the overshield rises and stays lit.",
    "For everyone, Light Show's shield glow, and the\\novershield rising and staying lit.",
    "Not synced: each player sees the shield effects\\nof their own Video Setup settings.",
]

# OpenCE's gametypes' options (port/linux/game/game_modes.c), a screen of the
# gametype editor's (menu_functions.c's "mp profile init opence options"): the
# mode and Hardcore, then the mode's own rows, which menu_functions.c's
# gametype_option_help shows (op_inf_ for Infection and Floodfection, op_gg_
# for Gun Game and Tag Gun Game) below them; a row's values are menu_functions.c's gametype_options'
PLAYLIST_EDIT = "main_menu/settings_select/multiplayer_setup/playlist_edit"
OPENCE_EDIT = "main_menu/settings_select/multiplayer_setup/opence_options_edit"
OPENCE_ROWS = [
    ("oce_mode", "MODE:", ["NONE", "SWAT", "INFECTION", "GUN GAME", "FLOODFECTION", "TAG GUN GAME"], [
        "The game type's own rules.",
        "No shields, and a shot to the head kills. Slayer only.",
        "Humans killed join the zombies, who win when\\nno human is left. Team Slayer only.",
        "Each kill gives the next weapon; a kill with the\\nlast wins. Free for all Slayer only.",
        "Infection with green zombies, every map under\\nthe Flood's fog. Team Slayer only.",
        "Gun Game with weapons drawn each game, a Tag\\nInjector profile's too. Free for all Slayer only."]),
    ("oce_hardcore", "HARDCORE:", ["OFF", "ON"], [
        "The motion sensor and aim assist as the game\\ntype has them.",
        "No motion sensor, no aim assist, and a kill\\nrestores the killer's health and shields."]),
    ("inf_zombies", "FIRST ZOMBIES:", ["1", "2", "3", "4", "8", "16"],
     ["The zombies chosen when the round starts\\n(a human is always left)."] * 6),
    ("inf_speed", "ZOMBIE SPEED:", ["100%", "110%", "125%", "150%", "175%", "200%"],
     ["How fast zombies move, of a human's speed."] * 6),
    ("inf_damage", "SKULL DAMAGE:", ["100%", "150%", "200%", "250%"],
     ["The damage of a zombie's skull's melee."] * 4),
    ("inf_respawn", "ZOMBIE RESPAWN:", ["1 SECOND", "3 SECONDS", "5 SECONDS", "10 SECONDS", "15 SECONDS",
                                        "30 SECONDS"],
     ["How long a dead zombie waits to respawn."] * 6),
    ("inf_countdown", "COUNTDOWN:", ["NONE", "5 SECONDS", "10 SECONDS", "15 SECONDS", "30 SECONDS"],
     ["How long the round waits, once two players are\\nalive, before the first zombies are chosen."] * 5),
    ("inf_shields", "ZOMBIE SHIELDS:", ["OFF", "ON"],
     ["Zombies have no shields.", "Zombies have shields, as humans do."]),
    ("gg_order", "WEAPON ORDER:", ["STRONGEST FIRST", "WEAKEST FIRST", "RANDOM"], [
        "From the rocket launcher to the pistol.", "From the pistol to the rocket launcher.",
        "In an order the host draws for each game."]),
    ("gg_melee", "MELEE DEMOTES:", ["OFF", "ON"], [
        "A melee kill takes nothing from the victim.",
        "A melee kill takes the victim back a weapon."]),
]
OPENCE_ITEM = f"{PLAYLIST_EDIT}/playlist_edit_opence_item"

STRING_INSERTS = {
    # (Filters' SHOW MISSING MAPS' third value's help, after the PC version's:
    # menu_functions.c's filters_help)
    f"{MT}/join_game/filters/cap_filter_options": [(11, [
        "Show servers on maps you do not have if their\\nhost shares them: they download as you join."])],
    # (the gametype editor's OPENCE OPTIONS' words, after the PC version's:
    # menu_functions.c's playlist_edit_description)
    f"{PLAYLIST_EDIT}/playlist_edit_list_extended_dsc_text": [(8, [
        "Choose the rules of OpenCE's\\ngametypes: SWAT, Infection,\\nGun Game and Hardcore.\\nThis gametype:"])],
    f"{PLAYER_EDIT}/player_options_labels": [(8, ["AIM ASSIST:", "SHIELD EFFECTS:"])],
    f"{PLAYER_EDIT}/cap_player_options": [(28, AIM_ASSIST_HELP + SHIELD_EFFECTS_HELP)],
    # (Teamplay Options' voice chat and vote kick rows, after its own:
    # TEAMPLAY_ROWS)
    f"{TEAMPLAY_EDIT}/teamplay_options_labels": [(3, [label for _, label, *_ in TEAMPLAY_EXTRA_ROWS])],
    f"{TEAMPLAY_EDIT}/cap_teamplay_options": [(10, [text for *_, helps in TEAMPLAY_EXTRA_ROWS for text in helps])],
    f"{SLAYER_EDIT}/var_kills_to_win": [(5, ["75", "100", "150", "200", "250", "500"])],
    f"{VEHICLE_EDIT}/var_vehicle_set": [(8, ["PC"])],
    f"{VEHICLE_EDIT}/cap_vehicle_options": [(17, [
        "Every vehicle the map places will spawn, as in\\nHalo PC.",
    ])],
    f"{SLAYER_EDIT}/cap_slayer": [(11, [
        "Seventy-five kills to win. Settle in for a long\\nfight.",
        "A hundred kills to win. Made for big games.",
        "A hundred and fifty kills to win. Only a crowded\\nserver gets there.",
        "Two hundred kills to win. Bring friends. Lots of\\nthem.",
        "Two hundred and fifty kills to win.",
        "Five hundred kills to win. You'll be here a while.",
    ])],
}


# a custom loadout's weapons (game_engine.h's _loadout_weapon_*)
LOADOUT_WEAPONS = ["NONE", "RANDOM", "ASSAULT RIFLE", "PISTOL", "SHOTGUN", "SNIPER RIFLE", "ROCKET LAUNCHER",
                   "PLASMA PISTOL", "PLASMA RIFLE", "NEEDLER"]


def LOADOUT_HELPS(slot: str) -> list:
    """the helps of a custom loadout's weapon's values"""
    helps = [f"No {slot} weapon.", f"A random {slot} weapon, of those the map has,\\neach time you spawn."]
    for weapon in LOADOUT_WEAPONS[2:]:
        helps.append(f"Everyone spawns with a {weapon.lower()} as their {slot}\\nweapon (where the map has one).")
    return helps


STRING_OVERRIDES.update({
    f"{MT}/multiplayer_options": ["JOIN GAME", "CREATE GAME", "INTERNET", "LAN", "DIRECT LINK", "EDIT GAMETYPES",
                                  "SERVER BROWSER", "CO-OP CAMPAIGN", "LOCAL"],
    f"{MT}/multiplayer_option_descriptions": [
        "Browse the games on the\\nInternet.",
        "Join a multiplayer game on\\nyour LAN.",
        "Join a game by its invite link,\\nor one a Discord invite\\nreached.",
        "Host a game on the Internet:\\nplayers join by its invite\\nlink or Discord.",
        "Host a game on your LAN\\nonly.",
        "Play on this computer alone:\\nsplit screen, and bots in\\nSlayer games.",
        "Play the campaign with a\\nfriend in split screen:\\nplayer 2 on a gamepad.",
        "Set all the attributes for your \\nmultiplayer gametypes and\\nkeep them for future use.",
    ],
    # (and the port's OPENCE: OpenCE's gametypes, port/linux/game/game_modes.c;
    # and TAG INJECTION: this fork's takes on them, Floodfection and Tag Gun Game)
    "main_menu/gametype_select/var_gametype_banks": ["STANDARD", "OPENCE", "CUSTOM", "TAG INJECTION"],
    # (the PC's weapon sets, then the Xbox's NO GRENADES: menu_functions.c's
    # gametype_options maps them to the engine's)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/item_options_labels": ["INFINITE GRENADES:", "WEAPON SET:", "VEHICLE SET:", "STARTING EQUIPMENT:",
                                  "VEHICLE RESPAWN TIME:", "LOADOUT:", "PRIMARY WEAPON:", "SECONDARY WEAPON:",
                                  "MAP WEAPONS:"],
    # (the helps of each option's values in turn: menu_functions.c's
    # gametype_option_help)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/cap_item_options": [
        "Everyone has a limitless supply of grenades, which\\nmakes for loud, explosive and occasionally messy fun.",
        "Each player can only carry up to 4 grenades of each\\ntype at any given time.",
        "The map will contain whatever weapons the designers\\nplaced on it.",
        "All the weapons on the map will be replaced by\\npistols.",
        "All the weapons on the map will be replaced by\\nassault rifles and plasma rifles.",
        "All the weapons on the map will be replaced by\\nplasma weapons.",
        "The guns on the map will be replaced by weapons\\nwith sniper scopes.",
        "Sniper rifles and pistols will not appear on the map.",
        "All the weapons on the map will be replaced by\\nrocket launchers.",
        "All the weapons on the map will be replaced by\\nshotguns.",
        "All the weapons on the map are only effective at\\nshort ranges.",
        "There are no weapons on the map that were made in\\nCovenant sweat shops.",
        "Only weapons made by the Covenant will appear\\nin the map.",
        "The flamethrower and fuel rod gun will not appear\\nin the map.",
        "Only the most devastating and dangerous of weapons\\nwill appear in the map.",
        "The map's weapons, without any grenades.",
        "You will start the game with the weapon set that the \\nmap designers custom tailored for it.",
        "Use the generic starting weapons across all maps.",
        "The map's weapons appear where its designers\\nplaced them.",
        "No weapons appear on the map: with a loadout of\\nNONE and NONE, melee and grenades only.",
        "The weapons follow the weapon set above.",
        "Everyone starts with the primary and secondary\\nweapons below; the map's weapons are its own.",
        *LOADOUT_HELPS("primary"),
        *LOADOUT_HELPS("secondary"),
    ],
    "main_menu/settings_select/multiplayer_setup/item_options_edit/var_weapon_set": [
        "NORMAL", "PISTOLS", "RIFLES", "PLASMA WEAPONS", "SNIPER", "NO SNIPING", "ROCKET LAUNCHERS", "SHOTGUNS",
        "SHORT RANGE", "HUMAN", "COVENANT", "CLASSIC", "HEAVY WEAPONS", "NO GRENADES"],
})

# which of STRING_OVERRIDES' strings say what one of the PC version's own
# says: its index in the PC version's list, or None (the port's own). A map
# in another language than English has those in its language (map=,
# menu_tags.c's map_strings); the inserted strings (STRING_INSERTS) are the
# port's own, the rest of their lists the PC version's
OVERRIDE_ORIGINALS = {
    f"{PE}/profile_edit_descriptions": [0, None, None, None, None, None, None, 7, None],
    f"{MT}/multiplayer_options": [0, 1, 2, 3, None, 5, None, None, None],
    f"{MT}/multiplayer_option_descriptions": [0, 1, None, None, 4, None, None, 5],
    "main_menu/gametype_select/var_gametype_banks": [0, None, 2, None],
    "main_menu/settings_select/multiplayer_setup/item_options_edit/item_options_labels": [0, 1, 2, 3, 4] + [None] * 4,
    # (the first's note that infinite grenades work only with 4 players or
    # fewer is left out: not so here)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/cap_item_options":
        [None] + list(range(1, 15)) + [None, 15, 16] + [None] * 24,
    "main_menu/settings_select/multiplayer_setup/item_options_edit/var_weapon_set": list(range(13)) + [None],
}

# New Game's level details' lines of the level's records, the profile's best
# score and time (menu_functions.c's level_description, campaign_score.c's
# records), under the level's words, where the difficulties' large icons were;
# and those icons, the difficulties the level was won on (Normal, Heroic,
# Legendary), small badges over the picture's top right corner instead
LEVEL_RECORD = "main_menu/solo_level_select/replay_level_right_record"
# (the level's words, at this percentage of their font, so that the
# records' three lines fit beneath them: four lines of words end at 326)
LEVEL_DATA_TEXT_SCALE = 85
LEVEL_BADGE = "main_menu/solo_level_select/difficulty_badge"
LEVEL_BADGE_SIZE = (22, 24)
# (along the picture's bottom, centred: it is 140 wide from 419, 114 high
# from 87)
LEVEL_BADGE_LEFTS = (454, 478, 502)
# (the icons' art has a few units of room at its foot: the badges' boxes
# reach past the picture's, their icons 3 units from its bottom)
LEVEL_BADGE_TOP = 180
# (the difficulties' icons, ui.map's difficulty_options_small, easy to
# legendary, scaled to the badges' size: a widget draws its picture texel for
# texel, so its own size would only crop the large icons; tools/ce_menus.py
# writes the bitmap)
LEVEL_BADGE_BITMAP = "main_menu/solo_level_select/difficulty_badges"
LEVEL_BADGE_SOURCE = "ui\\shell\\main_menu\\difficulty_select\\difficulty_options_small"
LEVEL_BADGE_FRAMES = 4
# (and behind them, shown with any, a gradient up from the picture's bottom,
# a little higher than they are, so that they stand out of it: the menus'
# panel blue, port/assets/records/badge_gradient_blue.png)
LEVEL_BADGE_PLATE = "main_menu/solo_level_select/difficulty_badges_plate"
LEVEL_BADGE_PLATE_BITMAP = "records/badge_gradient"
LEVEL_BADGE_PLATE_PNG = "badge_gradient_blue.png"
LEVEL_BADGE_PLATE_PLACE = (419, 161, 140, 40)
# New Game's LEADERBOARD (its button bar's, at the left, with the campaign's
# levels shown): the level shown's records, every profile's of this machine,
# best score first (menu_functions.c's leaderboard_*, campaign_score.c's
# records)
LEADERBOARD = "main_menu/solo_level_select/leaderboard"
LEADERBOARD_BUTTON = "main_menu/solo_level_select/button_leaderboard"
LEADERBOARD_ROWS = 10
# (its columns: the widgets' name, heading, left, width, alignment, and
# whether in the large font; each the profile's best of its own, whichever
# run's: the most skulls on, the most medals, the most points, the
# shortest time)
LEADERBOARD_COLUMNS = [
    ("name", "PROFILE", 40, 190, None, True),
    ("skulls", "SKULLS", 236, 80, "right", True),
    ("medals", "MEDALS", 322, 84, "right", True),
    ("score", "SCORE", 412, 96, "right", True),
    ("time", "TIME", 514, 86, "right", True),
]

# the map lists' first row: SINGLEPLAYER, MULTIPLAYER, CUSTOM SINGLEPLAYER or
# CUSTOM MULTIPLAYER maps (_map_kind)
MAP_KIND_CHOOSER = "main_menu/new_select/list_item_0_map_kind"

# the main menu's news: this fork's releases' notes, from GitHub (community.c),
# in a window the main menu's X opens once they came (WIDGET_PATCHES), its
# lines filled, scrolled and turned by menu_functions.c's news screen ("port
# news ...")
NEWS = "main_menu/news"
NEWS_ROWS = 12
# the window's panel (bitmaps/controller_border, drawn a texel a unit from
# its widget's corner: the panel is its texels 49 to 556 across and 104 to
# 349 down), placed in the middle of the screen
NEWS_PANEL = (17, 14)


# changes to the PC version's widgets (by our names): attributes set, all
# their handlers replaced, children added or removed, game data inputs added
WIDGET_PATCHES = {
    # The host returns here after a round with no menu history. Back must
    # reopen the main menu when it closes the last screen, as the lobby does.
    f"{MT}/connected/connected_map_select_wrapper": {"set": {
        "flags": "pass_unhandled_to_focused_child pause_game main_menu_if_no_history tag_controller_index force_handle_mouse",
    }},
    # (the main menu's THEATER, after Multiplayer, and MODS, before Quit:
    # _theater, _mods)
    "main_menu/main_menu_select_list": {"insert_before": {
        "main_menu/main_menu_item_profile": [
            f'<child widget="main_menu/main_menu_item_theater" x="{THEATER_ITEM_PLACE[0]}" y="{THEATER_ITEM_PLACE[1]}"/>'],
        "main_menu/main_menu_item_quit_game": [
            f'<child widget="main_menu/main_menu_item_mods" x="{MODS_ITEM_PLACE[0]}" y="{MODS_ITEM_PLACE[1]}"/>']},
        # (the main menu's X - the west face button: Y on a Switch, Square on
        # a PS5 - opens the news, the releases' notes: _news_screen; on the
        # main menu alone, once they came, and its hint shown only then)
        "inputs": ["port news hint update"],
        "handlers": ['<on event="down"/>', '<on event="x" run="port news open"/>']},
    # (and the hint that X opens it, over the build number: a tap or a click
    # on it presses X, its picture a key's icon to the pointer, x_butn_news)
    "main_menu/main_menu": {"insert_before": {"main_menu/build_number": [
        f'<child widget="{NEWS}/x_butn_news" x="450" y="380"/>',
        f'<child widget="{NEWS}/news_hint" x="450" y="380"/>']}},
    # (Campaign's DAILY CHALLENGE, before Back: _daily)
    "main_menu/campaign_select/campaign_menu_select_list": {"insert_before": {
        "main_menu/campaign_select/menu_item_back": [
            f'<child widget="{DAILY_ITEM}" x="{DAILY_ITEM_PLACE[0]}" y="{DAILY_ITEM_PLACE[1]}"/>']}},
    f"{PLAYER_EDIT}/player_options_menu": {"insert_before": {
        f"{PLAYER_EDIT}/player_button_bar": [
            f'<child widget="{PLAYER_EDIT}/op_aim_assist" x="54" y="289"/>',
            f'<child widget="{PLAYER_EDIT}/op_shield_effects" x="54" y="316"/>']}},
    # (its rows 27 apart, not 30, so that ten fit above the help, which keeps
    # the three lines some have: ce_menus.py's CHILD_OFFSETS)
    f"{PLAYER_EDIT}/player_options_help": {"set": {"top": 345, "height": 60}},
    # (the gametype editor's OPENCE OPTIONS, after its own rows: _opence_options)
    f"{PLAYLIST_EDIT}/edit_playlist_select_list": {
        "insert_before": {f"{PLAYLIST_EDIT}/playlist_edit_button_bar": [f'<child widget="{OPENCE_ITEM}"/>']},
        "inputs": ["port playlist edit desc"]},
    # (the profile settings' picture: on Gamepad Setup's row, the profile's
    # button settings, BITMAP_FRAMES; menu_functions.c's
    # profile_gamepad_layout)
    f"{PE}/profile_edit_extended_desc_pic": {"inputs": ["port gamepad layout preview"]},
    # (Gameplay, before About: its screen is the gameplay file's; the rows'
    # places, PROFILE_ROWS)
    f"{PE}/profile_edit_select_list": {"insert_before": {
        f"{PE}/about_item": [f'<child widget="{PE}/gameplay_profile_item"/>'],
    }},
    # (straight to their screens: no "checking for updates" dialog, which
    # asked the PC version's servers)
    f"{MT}/multiplayer_type_join_internet_item": {"set": {"string_index": 6}, "handlers": [
        f'<on event="a" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        f'<on event="start" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    f"{MT}/multiplayer_type_create_internet_item": {"handlers": [
        '<on event="a" run="join controller to mp game"/>',
        f'<on event="a" run="mp type set mode" open="{MT}/connected/connected_map_select_wrapper"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    f"{MT}/multiplayer_type_join_direct_item": {"handlers": [
        f'<on event="a" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        f'<on event="start" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    # (Create Game's LOCAL, then Co-op, after Create Game's: _create_local,
    # _coop)
    f"{MT}/multiplayer_type_select_list": {"insert_before": {
        f"{MT}/multiplayer_type_gametypes_item": [
            f'<child widget="{MT}/{row}" y="{multiplayer_row_place(row)[1]}"/>'
            for row in ["multiplayer_type_create_local_item", "multiplayer_type_coop_item"]],
    }},
    # (the Map screen's and New Game's lists: SINGLEPLAYER or MULTIPLAYER
    # maps, the first row's chooser's (_map_kind); B on the Map screen goes
    # from a co-op level's difficulties back to the level, else out of the
    # screen: menu_functions.c's map list)
    f"{MT}/mp_map_select/mp_map_select_list_2": {"swap": {"main_menu/new_select/list_item_0": MAP_KIND_CHOOSER},
                                                 "handlers": [
        '<on event="created" run="mp level list initialize"/>',
        '<on event="deleted" run="mp level list dispose"/>',
        f'<on event="custom_activation" run="mp level select" open="{MT}/connected/gametype_select_screen_wrapper"/>',
        '<on event="b" run="port map list back"/>',
        '<on event="back" run="port map list back"/>',
    ]},
    "main_menu/solo_level_select/solo_level_select_list": {"swap": {"main_menu/new_select/list_item_0": MAP_KIND_CHOOSER}},
    # (each description shows the other kind's maps too: a campaign level's
    # picture, name and words on the Map screen, a multiplayer map's on New
    # Game's)
    f"{MT}/mp_map_select/mp_map_right_item": {"children": [
        '<child widget="main_menu/solo_level_select/replay_level_right_name"/>',
        '<child widget="main_menu/solo_level_select/replay_level_right_pic"/>',
        '<child widget="main_menu/solo_level_select/replay_level_right_data"/>',
        # (a daily challenge's difficulty badge and skulls over its picture,
        # as New Game's: _daily)
        f'<child widget="{LEVEL_BADGE_PLATE}" x="{LEVEL_BADGE_PLATE_PLACE[0]}" y="{LEVEL_BADGE_PLATE_PLACE[1]}"/>',
        *[f'<child widget="{LEVEL_BADGE}" x="{x}" y="{LEVEL_BADGE_TOP}"/>' for x in LEVEL_BADGE_LEFTS],
        *DAILY_LEVEL_SKULL_CHILDREN,
    ]},
    # (and the level's records, its best score and time, where the large
    # difficulty icons were, which are small badges over the picture's
    # corner instead: _level_records)
    "main_menu/solo_level_select/replay_level_right_item": {"remove": ["main_menu/solo_level_select/difficulty_indicator"],
                                                            "children": [
        f'<child widget="{MT}/mp_map_select/mp_map_right_name"/>',
        f'<child widget="{MT}/mp_map_select/mp_map_right_pic"/>',
        f'<child widget="{MT}/mp_map_select/mp_map_right_data"/>',
        f'<child widget="{LEVEL_BADGE_PLATE}" x="{LEVEL_BADGE_PLATE_PLACE[0]}" y="{LEVEL_BADGE_PLATE_PLACE[1]}"/>',
        *[f'<child widget="{LEVEL_BADGE}" x="{x}" y="{LEVEL_BADGE_TOP}"/>' for x in LEVEL_BADGE_LEFTS],
        f'<child widget="{LEVEL_RECORD}"/>',
        *DAILY_LEVEL_SKULL_CHILDREN,
    ]},
    # (the level's words a little smaller, for the records' three lines
    # beneath them: _level_records)
    "main_menu/solo_level_select/replay_level_right_data": {"set": {"text_scale": LEVEL_DATA_TEXT_SCALE}},
    "main_menu/solo_level_select/solo_level_select_button_bar": {"children": [
        f'<child widget="{LEADERBOARD_BUTTON}" x="24" y="1"/>',
    ]},
    f"{MT}/join_game/header_join_game": {"children": [
        f'<child widget="{MT}/join_game/header_server_browser"/>',
        f'<child widget="{MT}/join_game/header_direct_link"/>',
    ]},
    # (the small font's line does not fit below their 4 units of offset)
    f"{MT}/join_game/ticker_player_info": {"set": {"text_y": 1}},
    # (the server browser's top-left tab: which games it lists, not Internet
    # or LAN, which are menu items of their own here; wider for its words, in
    # the small font of the counts beside it, and moved left to stay centred
    # in its tab: ce_menus.py's CHILD_OFFSETS)
    # (the tab behind it without CE's rounded box: the screen's border
    # picture already outlines the tab, and the two outlines clashed)
    f"{MT}/join_game/op_browser_mode": {"set": {"bitmap": None}},
    f"{MT}/join_game/browser_mode_spinner": {"set": {
        "string_list": None, "strings": "ALL GAMES|DEDICATED|P2P GAMES", "setting": "network.browser_source",
        "values": "all|dedicated|players", "width": 117, "footer_bounds": "7 120 19 126", "font": "ui\\small_ui",
        "text_y": 3}},
    # (the server browser's error panel: its words in a box of their own in
    # the error box's place, under its title and button; menu_functions.c's
    # lobby_browser_error hides the error box)
    f"{MT}/join_game/gamespy_error_fullscreen": {"insert_before": {"error/error_modal": [
        f'<child widget="{MT}/join_game/error_words" x="192" y="176"/>']}},
    # (wider to the left, for "SERVERS: 8 (5 dedicated)": ce_menus.py's
    # CHILD_OFFSETS keeps its right edge)
    f"{MT}/join_game/server_count_label": {"set": {"width": 210}},
    # (the page, when there is more than one: centred in the counts' panel,
    # between the players and the servers, whatever their numbers' widths;
    # menu_functions.c's lobby_browser_update places it)
    f"{MT}/join_game/page_count_label": {"set": {"align": "center"}},
    # (Filters' rows set the network.browser_* settings, as Network Setup's
    # SERVER BROWSER does: menu_functions.c's filters_show and
    # "gamespy update filter settings". SHOW MISSING MAPS has a third value,
    # those a host shares: Map Sharing downloads them)
    f"{MT}/join_game/filters/empty_servers_spinner": {"set": {
        "string_list": None, "strings": "NO|YES", "setting": "network.browser_show_empty", "values": "false|true"}},
    f"{MT}/join_game/filters/full_servers_spinner": {"set": {
        "string_list": None, "strings": "NO|YES", "setting": "network.browser_show_full", "values": "false|true"}},
    f"{MT}/join_game/filters/game_type_spinner": {"set": {
        "string_list": None, "strings": "ANY|CTF|SLAYER|ODDBALL|KING|RACE", "setting": "network.browser_gametype",
        "values": "any|ctf|slayer|oddball|king|race"}},
    f"{MT}/join_game/filters/installed_maps_spinner": {"set": {
        "string_list": None, "strings": "NO|YES|IF DOWNLOADABLE", "setting": "network.browser_maps",
        "values": "no|yes|downloadable"}},
    f"{MT}/join_game/ticker_rules_info": {"set": {"text_y": 1}},
    # (Item Options' loadout rows, over its buttons)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/item_options_menu": {"insert_before": {
        "main_menu/settings_select/multiplayer_setup/item_options_edit/item_button_bar": [
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_map_weapons" x="54" y="163"/>',
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_loadout" x="54" y="193"/>',
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_primary_weapon" x="54" y="223"/>',
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_secondary_weapon" x="54" y="253"/>',
        ]}},
    # (Teamplay Options' voice chat and vote kick rows, over its buttons:
    # Server Setup's only, _teamplay_options_extras)
    f"{TEAMPLAY_EDIT}/teamplay_options_menu": {"insert_before": {
        f"{TEAMPLAY_EDIT}/teamplay_button_bar": [
            f'<child widget="{TEAMPLAY_EDIT}/op_{key}" x="54" y="{163 + TEAMPLAY_STEP * index}"/>'
            for index, (key, *_) in enumerate(TEAMPLAY_EXTRA_ROWS)]}},
    f"{TEAMPLAY_EDIT}/teamplay_options_help": {"set": {"top": TEAMPLAY_HELP_TOP,
                                                       "height": 414 - TEAMPLAY_HELP_TOP}},
    # (the PC's Vehicles row's Start opened Item Options)
    "main_menu/settings_select/multiplayer_setup/playlist_edit/playlist_edit_vehicles_list_item": {"handlers": [
        '<on event="a" open="main_menu/settings_select/multiplayer_setup/vehicle_options_edit/vehicle_options_screen"/>',
        '<on event="start" open="main_menu/settings_select/multiplayer_setup/vehicle_options_edit/vehicle_options_screen"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    # (Direct Link's clipboard button in the place of the PC version's
    # Refresh, and Refresh in Get List's: the Server Browser's only, which
    # asks the public games' hosts again; "swap" puts another widget in a
    # child's place)
    f"{MT}/join_game/join_game_button_bar": {"swap": {
        f"{MT}/join_game/join_game_button_refresh": f"{MT}/join_game/button_clipboard",
        f"{MT}/join_game/join_game_button_update": f"{MT}/join_game/join_game_button_refresh",
    }},
}

# frames added after a PC bitmap's (by our names): (the Xbox map's bitmap,
# its frame, the size drawn at, where in the widget). The profile settings'
# picture gets the Xbox's Controller Setup's pictures of its five button
# settings (its first four frames are the thumbstick settings'), which show
# 512 by 235 of their 512 by 512: drawn at 279 wide, in the middle of the
# picture's 279 by 202
XBOX_CONTROLLER_PICTURES = f"ui\\shell\\{PE.replace('/', chr(92))}\\controller_edit\\config_controller"
BITMAP_FRAMES = {
    f"{PE}/profile_options": [(XBOX_CONTROLLER_PICTURES, 4 + preset, 279, 279, 0, 34) for preset in range(5)],
}

# the titles this port has that the PC version has not, set as its headers
# are (port/assets/menus/port_svg): bitmap name, text
TITLES = {
    "main_menu/mods/header_mods": "MODS",
    "main_menu/theater/header_theater": "THEATER",
    f"{MT}/join_game/header_server_browser": "SERVER BROWSER",
    f"{MT}/join_game/header_direct_link": "DIRECT LINK",
    f"{MT}/join_game/header_password": "PASSWORD",
    f"{MT}/lobby/header_lobby": "GAME LOBBY",
    f"{MT}/coop/header_player_2": "PLAYER 2 PROFILE",
    f"{MT}/lobby/header_add_player": "ADD PLAYER",
    f"{PE}/gameplay/header_gameplay": "GAMEPLAY",
    "main_menu/difficulty_select/randomizer/header_randomizer": "RANDOMIZER",
    "main_menu/difficulty_select/skulls/header_skulls": "SKULLS",
}


def title_backdrop(width: float) -> str:
    """a header's dark backdrop, as the PC headers' redraws have it, for text
    width units wide (the text is set over it in OpenCE: ce_menus.py)"""
    return "\n".join([
        '<svg xmlns="http://www.w3.org/2000/svg" width="512" height="64" viewBox="0 0 512 64">',
        "  <defs>",
        '    <filter id="soft" filterUnits="userSpaceOnUse" x="-60" y="-60" width="632" height="184">',
        '      <feGaussianBlur stdDeviation="14.23 13.06"/>', "    </filter>",
        '    <mask id="cut" maskUnits="userSpaceOnUse" x="0" y="0" width="512" height="64">',
        '      <rect width="512" height="59" fill="#fff"/>', "    </mask>", "  </defs>",
        '  <g mask="url(#cut)">',
        f'    <rect x="29.08" y="28.05" width="{width + 28:.2f}" height="40.95" fill="#021931" fill-opacity="0.893" '
        'filter="url(#soft)"/>',
        "  </g>", "</svg>", ""])


# the in-game pause menu's box, taller for the buttons this port adds
# (menu_tags.c's pause_patch: SETTINGS, and the host's END GAME): the
# pausebox5 redraw's rounded box (ui-svg-handmade/shell/bitmaps), drawn to
# hold each number of buttons, one frame each (the Xbox's 2-button box is
# 159 units high, and each button 35 more)
PAUSE_BOX_BUTTONS = [3, 4]
# the campaign pause box, tall enough for five rows (SETTINGS added) or six
# (network co-op's FRIENDLY FIRE too) at the PC version's pause menu's pitch
# (30) with the prompts still under the last row: the stock box is 159, and
# those rows need 36 and 66 more (menu_tags.c's pause_campaign_patch); the
# mission objectives' box beside it grows with it
PAUSE_BOX_EXTRA_HEIGHTS = [195, 225]


def pause_box_height(buttons: int) -> int:
    return 159 + (buttons - 2) * 35


def pause_box_svg(piece: str, height: int, header: bool = False) -> str:
    """a piece of the pause box (left cap, centre strip, right cap; 9-slice:
    the strip tiles across), height units high in its 256-high texture"""
    width = 4 if piece == "center" else 16
    lines = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="256" viewBox="0 0 {width} 256">']
    fill = "#0a2649"
    body_clip = ""
    if not header:
        # Keep the translucent footer clear of the body: blending two
        # 75%-opaque fills would make it nearly opaque and tint it blue.
        lines += [f'  <defs><clipPath id="body-area"><rect x="-16" y="0" width="48" height="{height - 28}"/></clipPath></defs>']
        body_clip = ' clip-path="url(#body-area)"'
    if height in PAUSE_BOX_EXTRA_HEIGHTS or height in map(pause_box_height, PAUSE_BOX_BUTTONS):
        # The stock campaign body fades from blue to dark navy at 75% opacity.
        # Scale it within the body, excluding the objective header or key footer.
        top, bottom = (29, height - 2) if header else (2, height - 28)
        lines += [f'  <defs><linearGradient id="body" gradientUnits="userSpaceOnUse" x1="0" y1="{top}" x2="0" y2="{bottom}">',
                  '    <stop offset="0" stop-color="#0a2649"/>',
                  '    <stop offset="1" stop-color="#050d1c"/>',
                  '  </linearGradient></defs>']
        fill = "url(#body)"
    if piece == "center":
        lines += [f'  <rect x="0" y="2" width="4" height="{height - 4}" fill="{fill}" fill-opacity="0.75"{body_clip}/>',
                  '  <rect x="0" y="0" width="4" height="2" fill="#2896ff"/>',
                  f'  <rect x="0" y="{height - 2}" width="4" height="2" fill="#2896ff"/>']
    else:
        bottom, corner = height - 1, height - 9.5
        cap = [f'    <path d="M9.5,1 H17 V{bottom} H9.5 A8.5,8.5 0 0 1 1,{corner} V9.5 A8.5,8.5 0 0 1 9.5,1 Z" '
               f'fill="{fill}" fill-opacity="0.75" fill-rule="evenodd"{body_clip}/>',
               f'    <path d="M17,1 H9.5 A8.5,8.5 0 0 0 1,9.5 V{corner} A8.5,8.5 0 0 0 9.5,{bottom} H17" fill="none" '
               'stroke="#2896ff" stroke-width="2"/>']
        lines += (['  <g transform="matrix(-1 0 0 1 16 0)">'] + cap + ['  </g>']) if piece == "right" else \
            [line[2:] for line in cap]
    # The stock objectives have a 28-unit header; the button key has a
    # 28-unit footer. Clip the black fill to the rounded panel interior.
    top = 2 if header else height - 28
    divider = 28 if header else height - 28
    if piece == "left":
        lines += [f'  <defs><clipPath id="strip"><rect x="2" y="2" width="{width + 16}" height="{height - 4}" rx="7.5"/></clipPath></defs>']
    if piece == "right":
        lines += [f'  <defs><clipPath id="strip"><rect x="-16" y="2" width="{width + 14}" height="{height - 4}" rx="7.5"/></clipPath></defs>']
    clip = ' clip-path="url(#strip)"' if piece != "center" else ""
    opacity = ' fill-opacity="0.75"' if not header else ""
    lines += [f'  <g{clip}>',
              f'    <rect x="0" y="{top}" width="{width}" height="26" fill="black"{opacity}/>',
              f'    <rect x="0" y="{divider}" width="{width}" height="0.75" fill="#2896ff"/>',
              '  </g>']
    lines += ["</svg>", ""]
    return "\n".join(lines)


# where the PC headers' text is (units of their 512x64), and its colour
TITLE_LEFT, TITLE_TOP, TITLE_CAP = 30.68, 33.5, 21.5
TITLE_COLOR = (0x29, 0x95, 0xFD, 255)


def _header(name: str, bitmap: str, platform=None) -> list:
    return _widget(name, [("controller", 1), ("left", 35), ("top", 11), ("width", 363), ("height", 59),
                          ("bitmap", bitmap), ("platform", platform)], [])


# a text field's B and Back do nothing of their own (the engine would go
# back from the field): its screen's (SCREEN_BACK) cancel the edit, else
# leave
FIELD_BACK = ['<on event="b"/>', '<on event="back"/>']
SCREEN_BACK = ['<on event="b" run="gamespy back handler"/>', '<on event="back" run="gamespy back handler"/>']


def _value_row(base: str, key: str, label_index: int, run: str) -> list:
    """a row of a label and a value the player sets (A edits it)"""
    lines = _widget(f"{base}/op_{key}", [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                         ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                    [f'<on event="a" run="{run}"/>', f'<on event="start" run="{run}"/>', *FIELD_BACK,
                     '<on event="left_mouse" run="mouse emit accept event"/>',
                     f'<child widget="{base}/{key}_label"/>', f'<child widget="{base}/{key}_value" x="230" y="1"/>'])
    lines += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 230), ("height", 22),
                                             ("string_list", f"{base}/labels"), ("string_index", label_index or None),
                                             ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                             ("text_y", 4)], [])
    lines += _widget(f"{base}/{key}_value", [("type", "text"), ("controller", 1), ("width", 280), ("height", 22),
                                             ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("align", "center"),
                                             ("text_y", 5), ("text_flags", "no_focus_test")], [])
    return lines


# the server browser's words (menu_functions.c's lobby_browser_update, in
# this order: its _browser_word_*): the line below the rows, the rows' status,
# the counts, and the error panel's
BROWSER_WORDS = [
    # (the chosen game's line: "12 of 16 players", and its state)
    "of", "players", "player", "under way", "full or starting",
    # (where it was listed, and what it needs)
    "Dedicated server by", "listed on", "master server", "master servers", "Dedicated server",
    "Hosted by a player (anonymous listing)", "Password required",
    "Needs the newer build: Settings > Network Setup > Check for updates",
    "Runs an older build: it can be joined once it updates",
    # (a join, as it goes: p2p_join_progress)
    "Asking the brokers for the host...", "The host answered: connecting (UDP hole punching)...",
    "Connected: joining the game...",
    "The host did not answer (offline, or its listing is stale)",
    "No route to the host: a forwarded port on either side helps",
    "The host refused: full", "The host refused: closed", "The host refused: banned",
    "The host refused: wrong password", "The host refused: another version", "The host refused: its invite changed",
    "Strict NAT: turn on UPnP, or forward a port",
    # (the list's state)
    "Internet play is off (Settings)", "The server browser is off (network.public_lobby)",
    "Looking for public games...", "Games on a newer version (update the game to join them):",
    "No public games found", "No games match the filters",
    "No master server answered: showing player-hosted games only",
    # (the counts: "SERVERS: 8 (3 dedicated)"; the rows' status)
    "dedicated", "JOINING", "NEWER", "OLDER", "FAILED", "LIVE", "CLOSED",
    # (the error panel: nothing can be listed)
    "Internet play is off:\\nSettings > Network Setup",
    "Neither the master servers\\nnor the brokers answer:\\ncheck the connection",
    # (a refusal added later, last so that the others keep their places)
    "The host refused: your player name",
    # (and a dedicated server's VPN policy's: port/linux/game/network_link.c)
    "The host blocks VPNs: link Discord in Settings > Network Setup",
    "The host refused: it allows no VPNs",
    "The host refused your Discord link: link it again",
    "The host refused: your Discord account is too new",
    "The host refused: your Discord account is in the game",
    "The host blocks VPNs: link Discord with its master (Settings > Network Setup)",
    "The host refused: too many players from your address",
]


def _join_game_extras() -> list:
    """the browser's added titles, Direct Link's clipboard button, the
    password screen, and the browser's words"""
    base = f"{MT}/join_game"
    lines = _password_screen()
    lines += _strings(f"{base}/browser_words", BROWSER_WORDS)
    # (the error panel's words, in a box as the error box: its own text is
    # the game's error messages')
    lines += _widget(f"{base}/error_words", [("type", "text"), ("controller", 1), ("width", 280), ("height", 159),
                                             ("bitmap", "bitmaps/alert_bkd"), ("font", "ui\\large_ui"),
                                             ("color", "#FF2896FF"), ("text_x", 9),
                                             ("text_y", 35), ("text_flags", "no_focus_test")], [])
    lines += _header(f"{base}/header_server_browser", f"{base}/header_server_browser")
    lines += _header(f"{base}/header_direct_link", f"{base}/header_direct_link")
    lines += _widget(f"{base}/button_clipboard", [("type", "text"), ("width", 128), ("height", 24),
                                                  ("bitmap", "bitmaps/text_button_background"),
                                                  ("text", "PASTE LINK"), ("font", "ui\\small_ui"),
                                                  ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="direct ip connect go"/>', '<on event="start" run="direct ip connect go"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    return lines


def _password_screen() -> list:
    """the server browser's password screen: a game with a password chosen
    (menu_functions.c's lobby_browser_select), its password typed (shown as
    stars), JOIN GAME joins it if it is the game's; else its help says so"""
    base = f"{MT}/join_game/password"
    spec = {"screen": "password_screen", "spacing": 28, "help_top": 120,
            "header": ("header_password", f"{MT}/join_game/header_password")}
    extra = _value_row(base, "password", 0, "port password edit")
    extra += _button(f"{base}/button_defaults", 3, [])
    extra += _widget(f"{base}/button_ok", [("type", "text"), ("width", 128), ("height", 24),
                                           ("bitmap", "bitmaps/text_button_background"), ("text", "JOIN GAME"),
                                           ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                                           ("text_y", 2)],
                     ['<on event="a" run="port password join"/>', '<on event="start" run="port password join"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    extra += _strings(f"{base}/labels", ["PASSWORD:"])
    # (by its state: menu_functions.c's password_screen_update)
    extra += _strings(f"{base}/help_strings", [
        "",
        "This game has a password. Type it, then press\\nEnter to join.",
        "That is not the game's password. Try again.",
        "Could not join the game.",
    ])
    lines = _screen(base, spec, [(f"{base}/op_password", None)], ["port password update"],
                    ['<on event="created" run="port password init"/>'], extra)
    # (no Defaults: the bar is JOIN GAME and CANCEL; B goes back at once,
    # even while typing, which the screen starts with)
    lines = [line.replace(f'<child widget="{base}/button_defaults" y="1"/>', "") for line in lines]
    lines = [line.replace('<on event="b" back="true"/>', '<on event="b" run="port password back"/>')
             .replace('<on event="back" back="true"/>', '<on event="back" run="port password back"/>')
             for line in lines]
    return lines


def _server_settings() -> list:
    """Create Game's server settings (in the PC version's place): the game's
    name, the most players (up to the port's 128), its invite link, and
    whether the server browser lists it (PUBLIC or PRIVATE: an internet
    game's)"""
    base = f"{MT}/server_settings"
    # (twelve rows: closer together, the help lower)
    spec = {"screen": "server_settings_screen", "spacing": 24, "help_top": 366,
            "header": ("header_server_settings", f"{base}/header_server_settings")}
    rows, extra = [], []
    extra += _value_row(base, "server_name", 0, "ss edit server name")
    rows.append((f"{base}/op_server_name", None))
    extra += _widget(f"{base}/op_max_players", [("width", 512), ("height", 28),
                                                ("flags", "pass_unhandled_to_focused_child"),
                                                ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<child widget="{base}/max_players_label"/>',
                      f'<child widget="{base}/max_players_spinner" x="320" y="1"/>'])
    extra += _widget(f"{base}/max_players_label", [("type", "text"), ("controller", 1), ("width", 300),
                                                   ("height", 22), ("string_list", f"{base}/labels"),
                                                   ("string_index", 1), ("font", "ui\\large_ui"),
                                                   ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/max_players_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", 147), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("strings", "|".join(str(count) for count in MAXIMUM_PLAYERS)), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 150 19 156")], [])
    rows.append((f"{base}/op_max_players", None))
    extra += _value_row(base, "invite", 2, "ss copy invite")
    rows.append((f"{base}/op_invite", None))
    # (PUBLIC or PRIVATE: menu_functions.c's server_settings_update)
    extra += _widget(f"{base}/op_listing", [("width", 512), ("height", 28),
                                            ("flags", "pass_unhandled_to_focused_child"),
                                            ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<child widget="{base}/listing_label"/>',
                      f'<child widget="{base}/listing_spinner" x="320" y="1"/>'])
    extra += _widget(f"{base}/listing_label", [("type", "text"), ("controller", 1), ("width", 300),
                                               ("height", 22), ("string_list", f"{base}/labels"),
                                               ("string_index", 9), ("font", "ui\\large_ui"),
                                               ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/listing_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", 147), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("strings", "PUBLIC|PRIVATE"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 150 19 156")],
                     ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    rows.append((f"{base}/op_listing", None))
    # PASSWORD (a public internet game's: menu_functions.c's server_settings_update),
    # which players from the server browser must type
    extra += _value_row(base, "password", 10 + len(COOP_SETUP_SCREENS), "ss edit server password")
    rows.append((f"{base}/op_password", None))
    # MAP SHARING (map_share.c), a screen of its own as co-op's options are,
    # for any game: SETUP_OPTIONS_PLACE below it
    key, _, _, _ = MAP_SHARE_SETUP
    target = f"{base}/{key}/{key}_screen"
    extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                         ("flags", "pass_unhandled_to_focused_child"),
                                         ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<on event="a" open="{target}"/>', f'<on event="start" open="{target}"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>',
                      f'<child widget="{base}/{key}_label"/>'])
    extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                                             ("string_list", f"{base}/labels"),
                                             ("string_index", 11 + len(COOP_SETUP_SCREENS)),
                                             ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                             ("text_y", 4)], [])
    rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE - 1))
    # BOTS (a LOCAL game's: bots.c), in MAP SHARING's place (BOTS_SETUP),
    # its value what the screen chose: "4, DISTRIBUTED, EVEN", or NONE
    # (menu_functions.c's server_settings_update), as the gametype's rows'
    key = BOTS_SETUP[0]
    target = f"{base}/{key}/{key}_screen"
    extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                         ("flags", "pass_unhandled_to_focused_child"),
                                         ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<on event="a" open="{target}"/>', f'<on event="start" open="{target}"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>',
                      f'<child widget="{base}/{key}_label"/>', f'<child widget="{base}/{key}_value" x="230" y="1"/>'])
    extra += _widget(f"{base}/{key}_value", [("type", "text"), ("controller", 1), ("width", 280), ("height", 22),
                                             ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("align", "center"),
                                             ("text_y", 5), ("text_flags", "no_focus_test")], [])
    extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 230), ("height", 22),
                                             ("string_list", f"{base}/labels"),
                                             ("string_index", 12 + len(COOP_SETUP_SCREENS) +
                                              len(CAMPAIGN_SETUP_SCREENS)),
                                             ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                             ("text_y", 4)], [])
    rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE - 1))
    # (BOTS' row above; TAG PROFILE's, after the gametype's rows, is below)
    # co-op's options, in the place of the gametype's rows, which co-op hides
    # as multiplayer hides co-op's (menu_functions.c's server_settings_update):
    # rows opening their screens, as the gametype's options' do
    # (_setup_option_screen; COOP_SETUP_SCREENS)
    for index, (key, title, _, _) in enumerate(COOP_SETUP_SCREENS):
        target = f"{base}/{key}/{key}_screen"
        extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                             ("flags", "pass_unhandled_to_focused_child"),
                                             ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<on event="a" open="{target}"/>', f'<on event="start" open="{target}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>'])
        extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                                                 ("string_list", f"{base}/labels"), ("string_index", 10 + index),
                                                 ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                                 ("text_y", 4)], [])
        rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE + index))
    # the campaign's own screens, after co-op's options (shown with the
    # randomizer or the skulls on, for a campaign map: menu_functions.c's
    # server_settings_update): rows that open the screens a level's
    # difficulty opens, whose OK comes back here (CAMPAIGN_SETUP_SCREENS)
    for index, (key, _, _) in enumerate(CAMPAIGN_SETUP_SCREENS):
        extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                             ("flags", "pass_unhandled_to_focused_child"),
                                             ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<on event="a" run="ss {key}"/>', f'<on event="start" run="ss {key}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>'])
        extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                                                 ("string_list", f"{base}/labels"),
                                                 ("string_index", 12 + len(COOP_SETUP_SCREENS) + index),
                                                 ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                                 ("text_y", 4)], [])
        rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE + len(COOP_SETUP_SCREENS) + index))
    # the gametype's options for this game (the gametype editor's screens,
    # editing a copy of the gametype chosen: "port setup edit")
    for index, (key, screen) in enumerate(SETUP_OPTION_SCREENS):
        target = f"main_menu/settings_select/multiplayer_setup/{screen}"
        extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                             ("flags", "pass_unhandled_to_focused_child"),
                                             ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<on event="a" run="port setup edit" open="{target}"/>',
                          f'<on event="start" run="port setup edit" open="{target}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>', f'<child widget="{base}/{key}_value" x="230" y="1"/>'])
        extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 230), ("height", 22),
                                                 ("string_list", f"{base}/labels"), ("string_index", 3 + index),
                                                 ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                                 ("text_y", 4)], [])
        extra += _widget(f"{base}/{key}_value", [("type", "text"), ("controller", 1), ("width", 280), ("height", 22),
                                                 ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("align", "center"),
                                                 ("text_y", 5), ("text_flags", "no_focus_test")], [])
        rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE + index))
    # TAG PROFILE (a multiplayer game's on the game's own maps, after its
    # TAG PROFILE screen: _tag_profile_screen), last: the Tag Injector's
    # multiplayer profile the game plays, or NONE, which the game keeps (who
    # sees it is chosen there, not changed under its players); A goes back
    # to the screen. menu_functions.c's server_settings_update shows it and
    # packs the rows shown (TAG_PROFILE_SETUP)
    key = TAG_PROFILE_SETUP[0]
    extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                         ("flags", "pass_unhandled_to_focused_child"),
                                         ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     ['<on event="a" run="ss tag profile change"/>', '<on event="start" run="ss tag profile change"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>',
                      f'<child widget="{base}/{key}_label"/>', f'<child widget="{base}/{key}_value" x="230" y="1"/>'])
    extra += _widget(f"{base}/{key}_value", [("type", "text"), ("controller", 1), ("width", 280), ("height", 22),
                                             ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("align", "center"),
                                             ("text_y", 5), ("text_flags", "no_focus_test")], [])
    extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 230), ("height", 22),
                                             ("string_list", f"{base}/labels"),
                                             ("string_index", 13 + len(COOP_SETUP_SCREENS) +
                                              len(CAMPAIGN_SETUP_SCREENS)),
                                             ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                             ("text_y", 4)], [])
    rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE + len(SETUP_OPTION_SCREENS)))
    extra += _button(f"{base}/button_defaults", 3, [])
    extra += _widget(f"{base}/button_ok", [("type", "text"), ("width", 128), ("height", 24),
                                           ("bitmap", "bitmaps/text_button_background"), ("text", "START GAME"),
                                           ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                                           ("text_y", 2)],
                     [f'<on event="a" run="ss start game" open="{MT}/lobby/lobby_screen"/>',
                      f'<on event="start" run="ss start game" open="{MT}/lobby/lobby_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    extra += _strings(f"{base}/labels", ["GAME NAME:", "MAXIMUM PLAYERS:", "INVITE LINK:", "GAME TYPE:",
                                         "PLAYER OPTIONS:", "ITEM OPTIONS:", "VEHICLE OPTIONS:", "INDICATOR OPTIONS:",
                                         "TEAMPLAY OPTIONS:", "LISTING:",
                                         *(f"{title}:" for _, title, _, _ in COOP_SETUP_SCREENS), "PASSWORD:",
                                         f"{MAP_SHARE_SETUP[1]}:",
                                         *(f"{title}:" for _, title, _ in CAMPAIGN_SETUP_SCREENS),
                                         f"{BOTS_SETUP[1]}:", f"{TAG_PROFILE_SETUP[1]}:"])
    extra += _strings(f"{base}/help_strings", [
        "",
        "The name the game shows in the lists of games.\\nEnter changes it.",
        "The most players the game takes (up to 128).",
        "Players join this game by its invite link, or your\\nDiscord invite. Enter copies the link again.",
        "The gametype and its rules, for this game.",
        "Lives, health, shields and respawning, for this game.",
        "Weapons, grenades and starting equipment, for\\nthis game.",
        "Each team's vehicles and their respawn time, for\\nthis game.",
        "The motion tracker and nav points, for this game.",
        "Friendly fire and team balance, for this game.",
        # (LISTING's, by its choice; PUBLIC's with a publisher key is last)
        "Anyone can see and join your game: it is listed\\nfor everyone through the brokers.",
        "Only players with your invite link can join.",
        # (co-op's options' rows: COOP_SETUP_SCREENS)
        *(help_text for _, _, help_text, _ in COOP_SETUP_SCREENS),
        # (PASSWORD's)
        "Players from the Server Browser must type it to\\njoin (an invite link needs none). Enter sets it.",
        # (MAP SHARING's)
        MAP_SHARE_SETUP[2],
        # (the campaign's screens' rows: CAMPAIGN_SETUP_SCREENS)
        *(help_text for _, _, help_text in CAMPAIGN_SETUP_SCREENS),
        # (BOTS': BOTS_SETUP)
        BOTS_SETUP[2],
        # (LISTING's PUBLIC, on a machine with a publisher key: menu_functions.c's
        # server_settings_update)
        "Listed for everyone through the brokers, and on\\nthe master servers with your publisher key.",
        # (TAG PROFILE's: TAG_PROFILE_SETUP)
        TAG_PROFILE_SETUP[2],
    ])
    lines = _screen(base, spec, rows, ["server settings update"],
                    ['<on event="created" run="server settings init"/>'], extra)
    # (co-op's options' screens, map sharing's and the bots')
    for key, title, _, screen_rows in [*COOP_SETUP_SCREENS, MAP_SHARE_SETUP, BOTS_SETUP]:
        lines += _setup_option_screen(f"{base}/{key}", key, screen_rows, fit=True)
    # (no Defaults: the bar is START and CANCEL; B cancels the name's edit
    # first)
    lines = [line.replace(f'<child widget="{base}/button_defaults" y="1"/>', "") for line in lines]
    lines = [line.replace('<on event="b" back="true"/>', SCREEN_BACK[0])
             .replace('<on event="back" back="true"/>', SCREEN_BACK[1]) for line in lines]
    return lines


MAXIMUM_PLAYERS = [2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128]
# CO-OP OPTIONS' amounts of PER PLAYER (network.coop_enemies, percentages)
# and STATIC MULTIPLIER (network.coop_enemies_multiplier), as menu_functions.c's
# gametype_options have their values
COOP_ENEMIES_PERCENTAGES = [25, 50, 100, 150, 200]
COOP_ENEMIES_MULTIPLIERS = [2, 4, 8, 16, 32]
# co-op's Server Setup options, in screens of their own as the gametype's
# are (its rows: _server_settings), each its key, its title, its row's help
# and its rows (as TEAMPLAY_ROWS: each spinner's key, label, words and the
# help of each of them; menu_functions.c's gametype_options give their
# settings' values)
COOP_SETUP_SCREENS = [
    ("coop_options", "CO-OP OPTIONS", "Team wipes, friendly fire, enemies, collisions and\\nthe campaign's scoring, for this game.", [
        ("coop_friendly_fire", "FRIENDLY FIRE:", ["OFF", "ON", "SHIELD ONLY", "EXPLOSIVES ONLY"], [
            "Players can not be hurt by weapons and explosives\\nfired by the other players.",
            "Players can be hurt by weapons or explosives\\nfired by the other players.",
            "Damage from the other players will only reduce\\nshields. Health will be unaffected.",
            "Players can be hurt by damage from explosives\\nfired by the other players.",
        ]),
        ("coop_extra_enemies", "EXTRA ENEMIES:", ["PER PLAYER", "STATIC MULTIPLIER", "NONE"], [
            "Enemy squads grow with the players: by the value of\\n'Per Player' for each player past the first.",
            "Enemy squads use 'Multiplier', however many players\\nthere are.",
            "Enemy squads are as the campaign has them.",
        ]),
        ("coop_extra_pickups", "EXTRA PICKUPS:", ["PER PLAYER", "STATIC MULTIPLIER", "NONE"], [
            "Pickups grow with the players: by the value of\\n'Per Player' for each player past the first.",
            "Pickups use 'Multiplier', up to eight times as many,\\nhowever many players there are.",
            "Pickups are as the campaign has them.",
        ]),
        ("coop_extra_vehicles", "EXTRA VEHICLES:", ["PER PLAYER", "STATIC MULTIPLIER", "NONE"], [
            "Vehicles respawn after being driven a short distance\\nbased on the number of players: by two thirds the value of\\n'Per Player' for each player past the first.",
            "Vehicles respawn after being driven a short distance,\\nwith extra copies equal to ('Multiplier' - 1) * 2/3,\\nrounded down, however many players there are.",
            "Vehicles are as the campaign has them.",
        ]),
        ("coop_enemies_multiplier", "MULTIPLIER:", [f"{value}X" for value in COOP_ENEMIES_MULTIPLIERS],
         ["Static amounts for the options set to this mode.\\nPickups are capped at eight times as many."] * len(COOP_ENEMIES_MULTIPLIERS)),
        ("coop_enemies_per_player", "PER PLAYER:", [f"{value}%" for value in COOP_ENEMIES_PERCENTAGES],
         ["For each player past the first, options set to Per\\nPlayer get this much more (100%: as many again)."] *
         len(COOP_ENEMIES_PERCENTAGES)),
        ("coop_player_collisions", "PLAYER COLLISIONS:", ["ON", "OFF"], [
            "Players bump into each other, as in the campaign.",
            "Players walk through each other, so that no one\\nblocks a doorway. The AI's characters still block.",
        ]),
        # (the campaign's scoring: game.campaign_score, as Settings > Gameplay has it, and how it is kept)
        ("coop_campaign_score", "CAMPAIGN SCORING:", ["ON", "OFF"], [
            "Scores each kill: the score and time at the bottom\\nright, and a summary at the end of each level.",
            "No score, time or summary.",
        ]),
        ("coop_score_type", "SCORING TYPE:", ["INDIVIDUAL", "TEAM"], [
            "Each player has a score of their own, from the\\nenemies they kill.",
            "The players' scores are added together: every player\\nsees the team's score.",
        ]),
        # (network.coop_ally_friendly_fire: game_engine.c's
        # game_engine_friendly_damage)
        ("coop_ally_friendly_fire", "ALLY FRIENDLY FIRE:", ["ON", "OFF"], [
            "Players can hurt and kill their AI allies, as in\\nthe campaign.",
            "Players can not hurt their AI allies, so that no\\none can kill Keyes or the Marines a level needs.",
        ]),
        ("coop_team_wipe", "TEAM WIPE:", ["AUTO", "REVERT CHECKPOINT", "RESPAWN ONLY"], [
            "Private games revert the checkpoint; public games\\nonly respawn. A changed team or missing save\\nalways uses respawn only.",
            "Revert the checkpoint after everyone dies. A changed\\nteam or missing save uses respawn only.",
            "Respawn at checkpoint positions after everyone dies.\\nThe world keeps its progress.",
        ]),
    ]),
    ("voice_options", "VOICE AND VOTING", "Voice chat, text chat and votes to kick a player,\\nfor this game.",
     TEAMPLAY_ROWS),
]
# Server Setup's rows for a co-op game's campaign screens: the randomizer's
# and the skulls', as a level's difficulty opens them (its key, which its
# row runs as "ss <key>", its title and its row's help)
CAMPAIGN_SETUP_SCREENS = [
    ("randomizer", "RANDOMIZER", "How this level's enemies and weapons are swapped,\nfor every player."),
    ("skulls", "SKULLS", "The skulls every player plays this level with."),
]
# Server Setup's map sharing (port/linux/game/map_share.c), for any game,
# laid out as co-op's options are (its key, its title, its row's help and
# its rows: menu_functions.c's gametype_options give their settings' values)
MAP_SHARE_UPLOADS = (256, 512, 1024, 2048, 4096, 8192, 16384, 20480)
MAP_SHARE_SETUP = ("map_share_options", "MAP SHARING",
                   "Sending players who join without the game's\\nCustom Edition map the map, and how fast.", [
    ("map_share", "SHARE MAPS:", ["ON", "OFF"], [
        "Players who join without the map download it\\nfrom you and the other players.",
        "Players who join without the map cannot play.",
    ]),
    ("map_share_upload", "UPLOAD LIMIT:", [f"{kb} KB/S" if kb < 1024 else f"{kb // 1024} MB/S"
                                           for kb in MAP_SHARE_UPLOADS],
     ["The most you send of maps, so that the game\\nkeeps the rest of your upload."] * len(MAP_SHARE_UPLOADS)),
])
# Server Setup's bots (port/linux/game/bots.c, the settings' bots.count,
# bots.skill and bots.teams), a LOCAL game's only (no other machine joins
# it), in MAP SHARING's place, which a LOCAL game hides (menu_functions.c's
# server_settings_update); laid out as co-op's options are. The bots join
# the lobby as the screen's OK sets them, in a Slayer game (and SWAT,
# Infection and Gun Game, Slayer's)
# (none up to the player cap but the host: bots.h's BOTS_MAXIMUM, 127)
BOTS_COUNTS = list(range(128))
BOTS_SKILLS = ["recruit", "easy", "normal", "heroic", "veteran", "legendary"]
# (SKILL's choices: DISTRIBUTED, bots.skill's default, a curve of the
# levels, then each level: bots_combat.c's bots_combat_distributed_skill)
BOTS_SKILL_CHOICES = ["distributed", *BOTS_SKILLS]
BOTS_SETUP = ("bots_options", "BOTS",
              "Computer players in this game: how many, how\\nwell they play, and their teams. Slayer only.", [
    ("bots_count", "BOTS:", [str(count) if count else "NONE" for count in BOTS_COUNTS],
     ["No bots: only the players on this computer."] +
     ["Bots join the lobby with you, as far as the game\\nhas room: Slayer, CTF and King of the Hill."] *
     (len(BOTS_COUNTS) - 1)),
    ("bots_skill", "SKILL:", [skill.upper() for skill in BOTS_SKILL_CHOICES], [
        "Each bot its own level, on a curve: most NORMAL\\nand HEROIC, a few RECRUIT and LEGENDARY.",
        "For new players: they react late, aim loosely\\nand never throw grenades.",
        "They react slowly, and their aim trails a target\\nthat turns back.",
        "They aim for the head once your shields are down,\\nand back off when they are losing.",
        "They lead their grenades and run in to finish a\\nfight they have won.",
        "They throw where they last saw you, back off as\\ntheir shields pop, and take high ground.",
        "The best: the fastest reactions, and no aim error\\neven at long range.",
    ]),
    ("bots_teams", "TEAMS:", ["EVEN", "AGAINST YOU"], [
        "In a team game, the bots join both teams, evening\\nthem with the players.",
        "In a team game, every bot is on the team the\\nplayers are not on.",
    ]),
])
# Server Setup's TAG PROFILE (port/linux/game/tag_multiplayer.c, the setting
# game.multiplayer_tag_profile), a multiplayer game's on one of the game's own
# maps: the Tag Injector's multiplayer profile it plays (its key, its title
# and its row's help)
TAG_PROFILE_SETUP = ("tag_profile", "TAG PROFILE",
                     "Chosen for the whole game before this screen:\\nwho can see it. A goes back to it.")
# the game's TAG PROFILE screen (menu_functions.c's tag_profile_asked: the
# Tag Injector on, a multiplayer profile made, one of the game's own
# multiplayer maps), between the gametype and Server Setup: its rows
TAG_PROFILE_ROWS = 9
# (Server Setup's place of its options' rows, co-op's or the gametype's: after
# MAP SHARING's)
SETUP_OPTIONS_PLACE = 6
# (their screens' titles)
TITLES.update({f"{MT}/server_settings/{key}/header_{key}": title
               for key, title, _, _ in [*COOP_SETUP_SCREENS, MAP_SHARE_SETUP, BOTS_SETUP]})
# (the rows of a co-op options screen in one place, the one shown: PER
# PLAYER's and MULTIPLIER's, by EXTRA ENEMIES')
SETUP_OPTION_SAME_PLACE = set()


def _tag_profile_screen() -> list:
    """the game's TAG PROFILE screen, which the gametype opens before Server
    Setup when the game can play a Tag Injector multiplayer profile: NONE
    (listed for every build) first, then the profiles (listed for builds
    with the mod alone); A one for the game, then Server Setup ("mp tag
    profile choose"), whose B comes back here; B back to the gametype"""
    base = f"{MT}/server_settings/tag_profile"
    lines = _widget(f"{base}/tag_profile_screen",
                    [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                     ("bitmap", "bitmaps/gradient")],
                    ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                     f'<child widget="{MT}/server_settings/header_server_settings"/>',
                     f'<child widget="{base}/tag_profile_title"/>',
                     f'<child widget="{base}/tag_profile_menu"/>'])
    lines += _widget(f"{base}/tag_profile_title",
                     [("type", "text"), ("controller", 1), ("left", 68), ("top", 74), ("width", 504), ("height", 22),
                      ("text", "TAG INJECTOR PROFILE"), ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/tag_profile_help",
                     [("type", "text"), ("controller", 1), ("left", 68), ("top", 388), ("width", 482), ("height", 24),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF")], [])
    children = ['<data input="mp tag profiles update"/>', '<on event="created" run="mp tag profiles init"/>']
    extra = []
    for index in range(TAG_PROFILE_ROWS):
        children.append(f'<child{attributes([("widget", f"{base}/tag_profile_{index}"), ("x", 54), ("y", 100 + index * 31)])}/>')
        extra += _widget(f"{base}/tag_profile_{index}",
                         [("type", "text"), ("width", 512), ("height", 28), ("bitmap", "bitmaps/option_bkds"),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)],
                         ['<on event="a" run="mp tag profile choose"/>',
                          '<on event="start" run="mp tag profile choose"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>'])
    children.append(f'<child{attributes([("widget", f"{base}/tag_profile_button_bar"), ("y", 414)])}/>')
    lines += _widget(f"{base}/tag_profile_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/tag_profile_help")], children)
    lines += _widget(f"{base}/tag_profile_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += extra
    return lines


def _setup_option_screen(base: str, key: str, rows: list, fit: bool = False) -> list:
    """a screen of options laid out as the gametype's are (Teamplay
    Options'), its spinners setting config.toml's settings (menu_functions.c's
    "port setup options init" and "port setup options save", OK), each
    choice's help shown as the gametype's are ("game settings lists text
    update": the help strings, each spinner's choices' in turn)"""
    lines = _widget(f"{base}/{key}_screen", [("width", 640), ("height", 480),
                                            ("flags", "pass_unhandled_to_focused_child"),
                                            ("bitmap", "bitmaps/gradient")],
                    [f'<child widget="{base}/header_{key}"/>', f'<child widget="{base}/{key}_menu"/>'])
    lines += _header(f"{base}/header_{key}", f"{base}/header_{key}")
    lines += _widget(f"{base}/{key}_help", [("type", "text"), ("controller", 1), ("left", 68), ("top", 341),
                                           ("width", 482), ("height", 79), ("string_list", f"{base}/cap_{key}"),
                                           ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    children = ['<data input="game settings lists text update"/>',
                '<on event="created" run="port setup options init"/>']
    # (30 apart; with fit, closer for more rows than eight, the last 28 tall
    # clear of the help at 341: Server Setup's, whose rows are all shown, as
    # OpenCE Options' are not)
    places = len([row_key for row_key, *_ in rows if row_key not in SETUP_OPTION_SAME_PLACE])
    # (without fit, more than nine rows are closer, to stay above the help
    # text)
    spacing = (min(30, (341 - 1 - 73 - 28) // max(1, places - 1)) if fit else
               30 if len(rows) <= 9 else 25)
    place = -1
    for row_key, *_ in rows:
        if row_key not in SETUP_OPTION_SAME_PLACE:
            place += 1
        children.append(f'<child widget="{base}/op_{row_key}" x="54" y="{73 + spacing * place}"/>')
    children.append(f'<child widget="{base}/{key}_button_bar" y="414"/>')
    lines += _widget(f"{base}/{key}_menu", [("type", "column_list"), ("width", 640), ("height", 480),
                                           ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                           ("description", f"{base}/{key}_help")], children)
    lines += _widget(f"{base}/{key}_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                 ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>', f'<child widget="{base}/{key}_button_ok" x="380" y="1"/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += _button(f"{base}/{key}_button_ok", 1, ['<on event="a" run="port setup options save" back="true"/>',
                                                     '<on event="start" run="port setup options save" back="true"/>'])
    for index, (row_key, label, strings, _) in enumerate(rows):
        lines += _widget(f"{base}/op_{row_key}", [("width", 512), ("height", 28),
                                                 ("flags", "pass_unhandled_to_focused_child"),
                                                 ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{base}/{row_key}_label"/>',
                          f'<child widget="{base}/{row_key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{base}/{row_key}_label", [("type", "text"), ("controller", 1), ("width", 300),
                                                     ("height", 22), ("string_list", f"{base}/{key}_labels"),
                                                     ("string_index", index), ("font", "ui\\large_ui"),
                                                     ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{row_key}_spinner",
                         [("type", "spinner"), ("top", 2), ("width", 206), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("string_list", f"{base}/var_{row_key}"), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
                          ("list_flags", "items_from_strings"),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7"), ("footer_bounds", "7 208 19 214")],
                         ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
        lines += _strings(f"{base}/var_{row_key}", strings)
    lines += _strings(f"{base}/{key}_labels", [label for _, label, *_ in rows])
    lines += _strings(f"{base}/cap_{key}", [text for *_, helps in rows for text in helps])
    return lines
# Server Setup's rows of the gametype's options: the gametype editor's
# screens
SETUP_OPTION_SCREENS = [
    ("game_type", "playlist_edit/game_type_select/gametype_select_screen"),
    ("player_options", "player_options_edit/player_options_screen"),
    ("item_options", "item_options_edit/item_options_screen"),
    ("vehicle_options", "vehicle_options_edit/vehicle_options_screen"),
    ("indicator_options", "indicator_options_edit/indicator_options_screen"),
    ("team_options", "teamplay_options_edit/teamplay_options_screen"),
]


def _lobby() -> list:
    """the lobby the host's and the joining players wait in (the Xbox's
    pregame's functions, which need no widget of theirs): up to the port's
    128 players, scrolling; the game's map and gametype; the countdown"""
    base = f"{MT}/lobby"
    # (split screen: another controller's START joins it, its B leaves alone:
    # menu_functions.c's lobby_join. After a game it is the only screen,
    # network_game_reset_to_pregame_ui opening it in place of the Xbox's
    # pregame: leaving it goes to the main menu, as the PC version's postgame
    # screens do)
    lines = _widget(f"{base}/lobby_screen", [("width", 640), ("height", 480),
                                             ("flags", "pass_unhandled_to_focused_child main_menu_if_no_history"),
                                             ("bitmap", "bitmaps/gradient")],
                    ['<on event="created" run="port lobby open"/>',
                     '<on event="created" run="net server accept conx"/>',
                     '<on event="created" run="net server allow start"/>',
                     '<on event="b" run="port lobby leave" back="true"/>',
                     '<on event="back" run="port lobby leave" back="true"/>',
                     # (another controller's START joins, its ADD PLAYER too: both
                     # ask player 1's Y, which opens the 4-way screen as player 1's)
                     '<on event="start" run="port lobby join"/>',
                     f'<on event="y" run="port lobby add player" open="{base}/join4_screen"/>',
                     # (a dedicated server's map vote: its screen, opened as
                     # the vote begins by a posted X, and again with X while
                     # it runs: menu_functions.c's map_vote_lobby_check. Y is
                     # the 4-way join screen's)
                     f'<on event="x" run="port map vote open" open="{base}/map_vote/map_vote_screen" branch="true"/>',
                     '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                     f'<child widget="{base}/lobby_list"/>',
                     f'<child widget="{base}/header_lobby"/>',
                     f'<child widget="{base}/lobby_join_help"/>'])
    lines += _header(f"{base}/header_lobby", f"{base}/header_lobby")
    lines += _widget(f"{base}/lobby_join_help", [("type", "text"), ("controller", 1), ("left", 355), ("top", 446),
                                                 ("width", 275), ("height", 24), ("font", "ui\\small_ui"),
                                                 ("color", "#FF2896FF"), ("align", "right"), ("text_y", 5),
                                                 ("text_flags", "no_focus_test")], [])
    # (its rows: any controller's left and right switch its own player's team,
    # on the rows only, so that they move along the buttons)
    rows = [f'<child widget="{base}/list_item_{index}" x="20" y="{73 + 30 * index}"/>' for index in range(11)]
    lines += _widget(f"{base}/lobby_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                            ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                            ("description", f"{base}/lobby_desc")],
                     ['<data input="net splitscreen prejoin players"/>', '<data input="port lobby update"/>',
                      *rows,
                      f'<child widget="{base}/lobby_button_bar" y="414"/>'])
    for index in range(11):
        lines += _widget(f"{base}/list_item_{index}",
                         [("width", 390), ("height", 28),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("align", "center"), ("text_y", 3)],
                         ['<on event="left right" run="swap player team"/>',
                          '<child widget="main_menu/new_select/list_item_text" x="25"/>',
                          '<child widget="main_menu/new_select/list_item_arrows"/>'])
    lines += _widget(f"{base}/lobby_desc", [("width", 640), ("height", 480)],
                     ['<child widget="main_menu/current_profile_name"/>',
                      f'<child widget="{base}/lobby_right_item" x="22" y="2"/>'])
    lines += _widget(f"{base}/lobby_right_item", [("controller", 1), ("left", 406), ("top", 75), ("width", 162),
                                                  ("height", 326),
                                                  ("bitmap", "bitmaps/spinner_list_3_wide_item_background")],
                     # (a campaign level's picture and name in place of the
                     # map's, for network co-op: menu_functions.c's
                     # lobby_map_show)
                     [f'<child widget="{base}/lobby_map_pic"/>', f'<child widget="{base}/lobby_map_name"/>',
                      '<child widget="main_menu/solo_level_select/replay_level_right_pic"/>',
                      '<child widget="main_menu/solo_level_select/replay_level_right_name"/>',
                      f'<child widget="{base}/lobby_game_data"/>'])
    lines += _widget(f"{base}/lobby_map_pic", [("controller", 1), ("left", 419), ("top", 87), ("width", 140),
                                               ("height", 114), ("bitmap", "ui\\shell\\bitmaps\\mp_map_grafix")], [])
    lines += _widget(f"{base}/lobby_map_name", [("type", "text"), ("controller", 1), ("left", 417), ("top", 204),
                                                ("width", 146), ("height", 43), ("string_list", "main_menu/mp_map_list"),
                                                ("font", "ui\\large_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/lobby_game_data", [("type", "text"), ("controller", 1), ("left", 417), ("top", 250),
                                                 ("width", 146), ("height", 144), ("font", "ui\\small_ui"),
                                                 ("color", "#FF2896FF")], [])
    # (its words: menu_functions.c's lobby_update. "<name> - dedicated": the
    # game's host is a dedicated server)
    lines += _strings(f"{base}/lobby_words", ["dedicated"])
    lines += _widget(f"{base}/lobby_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                  ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     # (SWITCH TEAM first: hidden in a game without teams, which
                     # moves the focus to the first shown, START NOW; at the end
                     # it traps none)
                     [f'<child widget="{base}/lobby_button_team" x="120" y="1"/>',
                      f'<child widget="{base}/lobby_button_start" x="250" y="1"/>',
                      f'<child widget="{base}/lobby_button_add" x="380" y="1"/>',
                      f'<child widget="{base}/lobby_button_leave" x="510" y="1"/>'])
    for key, caption, handlers in (
        ("add", "ADD PLAYER", [f'<on event="a" run="port lobby add player" open="{base}/join4_screen"/>',
                               f'<on event="start" run="port lobby add player" open="{base}/join4_screen"/>']),
        ("team", "SWITCH TEAM", ['<on event="a" run="swap player team"/>', '<on event="start" run="swap player team"/>']),
        ("start", "START NOW", ['<on event="a" run="net game speed start"/>',
                                '<on event="start" run="net game speed start"/>']),
        ("leave", "LEAVE", ['<on event="a" run="mouse emit back event"/>',
                            '<on event="start" run="mouse emit back event"/>'])):
        lines += _widget(f"{base}/lobby_button_{key}", [("type", "text"), ("width", 128), ("height", 24),
                                                       ("bitmap", "bitmaps/text_button_background"),
                                                       ("text", caption), ("font", "ui\\small_ui"),
                                                       ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                         handlers + ['<on event="left_mouse" run="mouse emit accept event"/>'])
    # a dedicated server's map vote (network_map_vote.c), as the lobby: its
    # choices a row each (A votes), the focused one's map at the right with the
    # time left; B (or the vote's end) goes back to the lobby
    vote = f"{base}/map_vote"
    lines += _widget(f"{vote}/map_vote_screen", [("width", 640), ("height", 480),
                                                 ("flags", "pass_unhandled_to_focused_child"),
                                                 ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{vote}/map_vote_list"/>',
                      f'<child widget="{vote}/map_vote_title"/>'])
    lines += _widget(f"{vote}/map_vote_title", [("type", "text"), ("controller", 1), ("left", 30), ("top", 30),
                                                ("width", 580), ("height", 36), ("text", "VOTE FOR THE NEXT GAME"),
                                                ("font", "ui\\large_ui"), ("color", "#FFFFFFFF"),
                                                ("text_flags", "no_focus_test")], [])
    vote_rows = [f'<child widget="{vote}/list_item_{index}" x="20" y="{73 + 30 * index}"/>' for index in range(8)]
    lines += _widget(f"{vote}/map_vote_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                               ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                               ("description", f"{vote}/map_vote_desc")],
                     ['<data input="port map vote update"/>', *vote_rows,
                      f'<child widget="{vote}/map_vote_button_bar" y="414"/>'])
    for index in range(8):
        lines += _widget(f"{vote}/list_item_{index}",
                         [("width", 390), ("height", 28),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("align", "center"), ("text_y", 3)],
                         ['<on event="a" run="port map vote choose"/>',
                          '<on event="start" run="port map vote choose"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          '<child widget="main_menu/new_select/list_item_text" x="25"/>',
                          '<child widget="main_menu/new_select/list_item_arrows"/>'])
    lines += _widget(f"{vote}/map_vote_desc", [("width", 640), ("height", 480)],
                     ['<child widget="main_menu/current_profile_name"/>',
                      f'<child widget="{base}/lobby_right_item" x="22" y="2"/>'])
    lines += _widget(f"{vote}/map_vote_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                     ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{vote}/map_vote_button_lobby" x="510" y="1"/>'])
    lines += _widget(f"{vote}/map_vote_button_lobby", [("type", "text"), ("width", 128), ("height", 24),
                                                       ("bitmap", "bitmaps/text_button_background"),
                                                       ("text", "LOBBY"), ("font", "ui\\small_ui"),
                                                       ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="mouse emit back event"/>', '<on event="start" run="mouse emit back event"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    # a split screen player's profile, chosen as they join the lobby (any
    # controller's presses: Co-op's rows; the host's countdown waits)
    lines += _widget(f"{base}/player_profile_screen", [("width", 640), ("height", 480),
                                                       ("flags", "pass_unhandled_to_focused_child"),
                                                       ("bitmap", "bitmaps/gradient")],
                     ['<on event="created" run="net server defer start"/>',
                      '<on event="deleted" run="net server allow start"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/player_profile_list"/>',
                      f'<child widget="{base}/header_add_player"/>',
                      f'<child widget="{base}/player_profile_help" x="20" y="416"/>'])
    lines += _header(f"{base}/header_add_player", f"{base}/header_add_player")
    lines += _widget(f"{base}/player_profile_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", "main_menu/profile_manager/player_profile_extended_desc")],
                     ['<data input="3wide player profile list update"/>',
                      '<on event="created" run="port lobby player list initialize"/>',
                      '<on event="deleted" run="player profile list dispose"/>',
                      '<on event="custom_activation" run="port lobby player choose" back="true" branch="true"/>',
                      *[f'<child widget="{MT}/coop/list_item_{index}" x="20" y="{73 + 30 * index}"/>'
                        for index in range(11)],
                      f'<child widget="{MT}/coop/player_2_button_bar" y="414"/>'])
    lines += _widget(f"{base}/player_profile_help", [("type", "text"), ("width", 350), ("height", 24),
                                                     ("text", "The new player's profile."),
                                                     ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("text_y", 5),
                                                     ("text_flags", "no_focus_test")], [])
    # a game under way's lobby, before joining it (the browser's rows of
    # games in progress): what its advertisement tells, JOIN GAME. Split
    # screen players join here, as in the lobby, before JOIN GAME: the game
    # starts at once for the machine, with the players it brings
    lines += _widget(f"{base}/preview_screen", [("width", 640), ("height", 480),
                                                ("flags", "pass_unhandled_to_focused_child"),
                                                ("bitmap", "bitmaps/gradient")],
                     ['<on event="created" run="port lobby open"/>',
                      '<on event="b" run="port lobby preview leave" back="true"/>',
                      '<on event="back" run="port lobby preview leave" back="true"/>',
                      '<on event="start" run="port lobby preview add"/>',
                      f'<on event="y" run="port lobby add player" open="{base}/join4_screen"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/preview_list"/>',
                      f'<child widget="{base}/header_lobby"/>',
                      f'<child widget="{base}/lobby_join_help"/>'])
    lines += _widget(f"{base}/preview_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                              ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                              ("description", f"{base}/lobby_desc")],
                     ['<data input="port lobby preview update"/>',
                      f'<child widget="{base}/preview_status" x="30" y="75"/>',
                      f'<child widget="{base}/preview_button_bar" y="414"/>'])
    lines += _widget(f"{base}/preview_status", [("type", "text"), ("controller", 1), ("width", 360), ("height", 300),
                                                ("font", "ui\\large_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/preview_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                    ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{base}/preview_button_join" x="250" y="1"/>',
                      f'<child widget="{base}/preview_button_add" x="380" y="1"/>',
                      f'<child widget="{base}/preview_button_back" x="510" y="1"/>'])
    for key, caption, run in (("join", "JOIN GAME", "port lobby preview join"),
                              ("add", "ADD PLAYER", "port lobby add player"),
                              ("back", "BACK", "mouse emit back event")):
        opened = f' open="{base}/join4_screen"' if key == "add" else ""
        lines += _widget(f"{base}/preview_button_{key}", [("type", "text"), ("width", 128), ("height", 24),
                                                         ("bitmap", "bitmaps/text_button_background"),
                                                         ("text", caption), ("font", "ui\\small_ui"),
                                                         ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                         [f'<on event="a" run="{run}"{opened}/>', f'<on event="start" run="{run}"{opened}/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _join4(base)
    return lines


def _join4(base: str) -> list:
    """ADD PLAYER's screen: the Xbox's split screen profile select (its own
    widgets and bitmaps, ui\\shell\\...\\4way_profile_select), a quarter for
    each controller. A controller not playing presses A to join, chooses its
    profile on the Xbox's spinner ("player profile list initialize": one no
    other player has) and presses A to select it; B goes back a step. A
    selected or playing quarter's A (or Enter) brings the players who
    selected theirs into the lobby, player 1's B goes back without them. The
    keyboard's SPACE makes the keyboard player 1 alone, freeing the gamepad
    it shared to join (menu_functions.c's join4). Each quarter takes its own
    controller's presses (pc_menu_screen_routes_by_controller)"""
    xbox = "ui\\shell\\main_menu\\multiplayer_type_select\\split_screen\\4way_profile_select\\"
    connected = "ui\\shell\\main_menu\\multiplayer_type_select\\connected\\4way_profile_select\\"
    quarters = [(70, 76), (328, 76), (70, 242), (328, 242)]
    lines = _widget(f"{base}/join4_screen",
                    [("width", 640), ("height", 480), ("bitmap", "ui\\shell\\bitmaps\\gradient"),
                     ("flags", "pass_unhandled_to_focused_child no_focused_child pass_unhandled_to_all_children "
                               "pass_handled_to_all_children")],
                    ['<on event="created" run="port join4 open"/>',
                     '<on event="created" run="net server defer start"/>',
                     # (player 1's B, Escape: back without them; a screen
                     # with no B of its own goes back at any controller's,
                     # before its quarters see it)
                     '<on event="b" run="port join4 back" back="true"/>',
                     '<on event="back" run="port join4 back" back="true"/>',
                     '<data input="port join4 update"/>',
                     f'<child widget="{xbox}header_select_profile"/>',
                     *[f'<child widget="{base}/join4_qtr_join" controller="{index + 1}" x="{x}" y="{y}"/>'
                       for index, (x, y) in enumerate(quarters)],
                     '<child widget="ui\\shell\\main_menu\\button_key"/>',
                     f'<child widget="{base}/join4_hint" x="30" y="419"/>'])
    # (a quarter for a controller not playing: "Press A to Join"; the screen
    # sends a player's quarter "black" (their profile) and the quarter of the
    # controller whose START opened it "white" (its spinner): join4_open)
    lines += _widget(f"{base}/join4_qtr_join",
                     [("width", 255), ("height", 165), ("bitmap", "ui\\shell\\bitmaps\\split_screen_bkd"),
                      ("flags", "pass_unhandled_to_focused_child")],
                     [f'<on event="black" run="port join4 playing" replace="{base}/join4_qtr_playing"/>',
                      f'<on event="white" run="port join4 opener" replace="{base}/join4_qtr_select"/>',
                      f'<on event="a" run="port join4 join" replace="{base}/join4_qtr_select"/>',
                      f'<on event="start" run="port join4 join" replace="{base}/join4_qtr_select"/>',
                      # (a widget with no B of its own goes back at it)
                      '<on event="b"/>', '<on event="back"/>',
                      f'<child widget="{connected}press_start_to_join" x="54" y="20"/>',
                      f'<child widget="{connected}press_start_to_join_controller"/>'])
    lines += _widget(f"{base}/join4_qtr_select",
                     [("width", 254), ("height", 164), ("bitmap", f"{xbox}4way_profile_border"),
                      ("flags", "pass_unhandled_to_focused_child")],
                     ['<on event="created" run="port join4 choosing"/>',
                      f'<on event="a" run="port join4 select" replace="{base}/join4_qtr_ready"/>',
                      f'<on event="start" run="port join4 select" replace="{base}/join4_qtr_ready"/>',
                      f'<on event="b" run="port join4 leave" replace="{base}/join4_qtr_join"/>',
                      '<on event="back"/>',
                      f'<child widget="{xbox}qtr_screen_profile_color_pic"/>',
                      f'<child widget="{xbox}qtr_screen_profile_desc"/>',
                      f'<child widget="{xbox}qtr_screen_profile_spinner"/>'])
    selected = [f'<child widget="{xbox}qtr_screen_profile_color_pic_selected"/>',
                f'<child widget="{xbox}qtr_screen_profile_selected_text"/>',
                f'<child widget="{xbox}qtr_screen_profile_selected_name"/>']
    lines += _widget(f"{base}/join4_qtr_ready",
                     [("width", 254), ("height", 164), ("bitmap", f"{xbox}4way_profile_border")],
                     ['<on event="created" run="port join4 ready"/>',
                      '<on event="a" run="port join4 done" back="true"/>',
                      '<on event="start" run="port join4 done" back="true"/>',
                      f'<on event="b" run="port join4 unready" replace="{base}/join4_qtr_select"/>',
                      '<on event="back"/>',
                      *selected])
    # (a player in the game already)
    lines += _widget(f"{base}/join4_qtr_playing",
                     [("width", 254), ("height", 164), ("bitmap", f"{xbox}4way_profile_border")],
                     ['<on event="a" run="port join4 done" back="true"/>',
                      '<on event="start" run="port join4 done" back="true"/>',
                      '<on event="b"/>', '<on event="back"/>',
                      *selected])
    lines += _widget(f"{base}/join4_hint", [("type", "text"), ("width", 345), ("height", 24),
                                            ("font", "ui\\small_ui"), ("color", "#FF2896FF"),
                                            ("text_flags", "no_focus_test")], [])
    return lines


def _create_local() -> list:
    """Create Game's LOCAL, after LAN: a multiplayer game of this machine
    alone, as the Xbox's Split Screen is (no other machine joins it, so the
    bots can play it: port/linux/game/bots.c), made as LAN's is, then the
    Map screen's multiplayer maps (menu_functions.c's multiplayer_host)"""
    return _widget(f"{MT}/multiplayer_type_create_local_item",
                   [("type", "text"), ("left", 51), ("width", 232), ("height", 32), ("bitmap", "bitmaps/list_item_bkd"),
                    ("string_list", f"{MT}/multiplayer_options"), ("string_index", 8), ("font", "ui\\large_ui"),
                    ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                   ['<on event="a" run="join controller to mp game"/>',
                    f'<on event="a" run="mp type set mode" open="{MT}/connected/connected_map_select_wrapper"/>',
                    '<on event="left_mouse" run="mouse emit accept event"/>'])


def _coop() -> list:
    """Co-op, the campaign for two players on this machine in split screen
    (the Xbox's Cooperative Play): Multiplayer's CO-OP CAMPAIGN, then player
    2's profile, chosen with player 2's controller (its rows take any
    controller's presses, as the shared rows do only controller 1's), then
    New Game's levels and difficulty, which either player's controller uses
    (ui_widget.c's widget_takes_events_of_controller; menu_functions.c's
    coop_begin)"""
    base = f"{MT}/coop"
    lines = _widget(f"{MT}/multiplayer_type_coop_item",
                    [("type", "text"), ("left", 51), ("width", 232), ("height", 32), ("bitmap", "bitmaps/list_item_bkd"),
                     ("string_list", f"{MT}/multiplayer_options"), ("string_index", 7), ("font", "ui\\large_ui"),
                     ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                    [f'<on event="a" run="port coop begin" open="{base}/player_2_profile_screen" branch="true"/>',
                     f'<on event="start" run="port coop begin" open="{base}/player_2_profile_screen" branch="true"/>',
                     '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/player_2_profile_screen", [("width", 640), ("height", 480),
                                                         ("flags", "pass_unhandled_to_focused_child"),
                                                         ("bitmap", "bitmaps/gradient")],
                     ['<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/player_2_profile_list"/>',
                      f'<child widget="{base}/header_player_2"/>',
                      f'<child widget="{base}/player_2_help" x="20" y="416"/>'])
    lines += _header(f"{base}/header_player_2", f"{base}/header_player_2")
    rows = [f'<child widget="{base}/list_item_{index}" x="20" y="{73 + 30 * index}"/>' for index in range(11)]
    lines += _widget(f"{base}/player_2_profile_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", "main_menu/profile_manager/player_profile_extended_desc")],
                     ['<data input="3wide player profile list update"/>',
                      '<on event="created" run="port coop player 2 list initialize"/>',
                      '<on event="deleted" run="player profile list dispose"/>',
                      '<on event="custom_activation" run="port coop player 2" '
                      'open="main_menu/solo_level_select/solo_level_select_screen" branch="true"/>',
                      *rows, f'<child widget="{base}/player_2_button_bar" y="414"/>'])
    for index in range(11):
        lines += _widget(f"{base}/list_item_{index}",
                         [("width", 390), ("height", 28),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("align", "center"), ("text_y", 3)],
                         ['<on event="a" run="single prev cl item activated"/>',
                          '<on event="start" run="single prev cl item activated"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          '<child widget="main_menu/new_select/list_item_text" x="25"/>',
                          '<child widget="main_menu/new_select/list_item_arrows"/>'])
    lines += _widget(f"{base}/player_2_help", [("type", "text"), ("width", 350), ("height", 24),
                                               ("text", "Player 2: choose with your controller."),
                                               ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("text_y", 5),
                                               ("text_flags", "no_focus_test")], [])
    lines += _widget(f"{base}/player_2_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                     ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>',
                      '<child widget="main_menu/profile_manager/profile_manager_button_ok" x="380" y="1"/>',
                      '<child widget="common_button_back" x="510" y="1"/>'])
    return lines


def _item_options_extras() -> list:
    """Item Options' rows of the port's: the map's weapons (YES or NO), and
    the loadout, CATEGORY (the weapon set) or CUSTOM (each player's primary
    and secondary weapons)"""
    base = "main_menu/settings_select/multiplayer_setup/item_options_edit"
    lines = []
    for key, strings, label in (("map_weapons", "var_map_weapons", 8), ("loadout", "var_loadout", 5),
                                ("primary_weapon", "var_loadout_weapon", 6),
                                ("secondary_weapon", "var_loadout_weapon", 7)):
        lines += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                               ("flags", "pass_unhandled_to_focused_child"),
                                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{base}/{key}_label"/>',
                          f'<child widget="{base}/{key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                                                   ("string_list", f"{base}/item_options_labels"),
                                                   ("string_index", label), ("font", "ui\\large_ui"),
                                                   ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{key}_spinner",
                         [("type", "spinner"), ("top", 2), ("width", 206), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("string_list", f"{base}/{strings}"), ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
                          ("align", "center"), ("text_y", 4), ("list_flags", "items_from_strings"),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7"), ("footer_bounds", "7 208 19 214")],
                         ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    lines += _strings(f"{base}/var_map_weapons", ["YES", "NO"])
    lines += _strings(f"{base}/var_loadout", ["CATEGORY", "CUSTOM"])
    lines += _strings(f"{base}/var_loadout_weapon", LOADOUT_WEAPONS)
    return lines


def _player_options_extras() -> list:
    """The host's aim-assist policy and shield effects, hidden while editing
    saved gametypes."""
    base = PLAYER_EDIT
    lines = []
    for key, label_index, words in (("aim_assist", 8, ["ALLOWED", "BLOCK M+KB"]),
                                    ("shield_effects", 9, SHIELD_EFFECTS)):
        lines += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
            ("flags", "pass_unhandled_to_focused_child"), ("bitmap", "bitmaps/option_bkds"),
            ("color", "#FF2896FF")], [f'<child widget="{base}/{key}_label"/>',
            f'<child widget="{base}/{key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1),
            ("width", 280), ("height", 22), ("string_list", f"{base}/player_options_labels"),
            ("string_index", label_index), ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
            ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{key}_spinner", [("type", "spinner"), ("top", 2),
            ("width", 206), ("height", 20), ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
            ("string_list", f"{base}/var_{key}"), ("font", "ui\\large_ui"),
            ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
            ("list_flags", "items_from_strings"), ("header_bitmap", "bitmaps/arrow_sm_left"),
            ("footer_bitmap", "bitmaps/arrow_sm_right"), ("header_bounds", "7 -13 19 -7"),
            ("footer_bounds", "7 208 19 214")], ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    return lines + _strings(f"{base}/var_aim_assist", ["ALLOWED", "BLOCK M+KB"]) + \
        _strings(f"{base}/var_shield_effects", SHIELD_EFFECTS)


def _opence_options() -> list:
    """the gametype editor's OPENCE OPTIONS: its row, after the editor's own
    (as they are laid out), and its screen, as the gametype's options' are
    (_setup_option_screen), its spinners the gametype's (menu_functions.c's
    gametype_options), its header its words (no picture of its own)"""
    lines = _setup_option_screen(OPENCE_EDIT, "opence", OPENCE_ROWS)
    lines = [line.replace("port setup options init", "mp profile init opence options")
             .replace("port setup options save", "mp profile set opence options")
             .replace(f'bitmap="{OPENCE_EDIT}/header_opence"',
                      'type="text" text="OPENCE OPTIONS" font="ui\\large_ui" color="#FFFFFFFF" text_x="13" text_y="16"')
             for line in lines]
    lines += _widget(OPENCE_ITEM, [("type", "text"), ("left", 51), ("top", 309), ("width", 232), ("height", 32),
                                   ("bitmap", "bitmaps/list_item_bkd"), ("text", "OPENCE OPTIONS"),
                                   ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                     [f'<on event="a" open="{OPENCE_EDIT}/opence_screen"/>',
                      f'<on event="start" open="{OPENCE_EDIT}/opence_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    return lines


def _teamplay_options_extras() -> list:
    """Teamplay Options' rows of the port's: the gametype's FORCE TEAM
    SPAWNS (game_engine.c), then the host's voice chat
    (port/linux/game/network_voice.c) and vote kicks (network_votekick.c),
    in Server Setup's copy only (menu_functions.c's gametype_options_init),
    kept in config.toml"""
    lines = []
    for index, (key, label, strings, _) in enumerate(TEAMPLAY_EXTRA_ROWS):
        lines += _widget(f"{TEAMPLAY_EDIT}/op_{key}", [("width", 512), ("height", 28),
                                                       ("flags", "pass_unhandled_to_focused_child"),
                                                       ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{TEAMPLAY_EDIT}/{key}_label"/>',
                          f'<child widget="{TEAMPLAY_EDIT}/{key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{TEAMPLAY_EDIT}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300),
                                                          ("height", 22),
                                                          ("string_list", f"{TEAMPLAY_EDIT}/teamplay_options_labels"),
                                                          ("string_index", 3 + index), ("font", "ui\\large_ui"),
                                                          ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{TEAMPLAY_EDIT}/{key}_spinner",
                         [("type", "spinner"), ("top", 2), ("width", 206), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("string_list", f"{TEAMPLAY_EDIT}/var_{key}"), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
                          ("list_flags", "items_from_strings"),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7"), ("footer_bounds", "7 208 19 214")],
                         ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
        lines += _strings(f"{TEAMPLAY_EDIT}/var_{key}", strings)
    return lines


def _map_kind() -> list:
    """the map lists' first row (New Game's and the Map screen's), as the
    gametype list's chooser: a spinner of SINGLEPLAYER or MULTIPLAYER maps,
    the Custom Edition maps of the custom_maps folder of either kind, or the
    DAILY CHALLENGES (menu_functions.c's map lists), for either controller (split screen
    co-op's New Game takes both)"""
    lines = _widget(MAP_KIND_CHOOSER, [("width", 256), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                       ("bitmap", "bitmaps/option_bkds_sm"), ("font", "ui\\large_ui"),
                                       ("color", "#FF2896FF"), ("align", "center"), ("text_y", 3)],
                    [f'<child widget="{MAP_KIND_CHOOSER}_spinner" x="9" y="2"/>'])
    # (wider than the gametype chooser's, for CUSTOM SINGLEPLAYER, its
    # arrows at the row's ends)
    lines += _widget(f"{MAP_KIND_CHOOSER}_spinner",
                     [("type", "spinner"), ("width", 238), ("height", 22), ("flags", "left_right_tabs_items"),
                      ("string_list", "main_menu/new_select/var_map_kinds"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1), ("list_flags", "items_from_strings"),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 238 19 244")],
                     ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    # (and the daily challenges, today's and the days before: community.c's,
    # each played at its own difficulty with its own randomizer and skulls)
    lines += _strings("main_menu/new_select/var_map_kinds",
                      ["SINGLEPLAYER", "MULTIPLAYER", "CUSTOM SINGLEPLAYER", "CUSTOM MULTIPLAYER", "DAILY CHALLENGES"])
    return lines


def _level_records() -> list:
    """New Game's level details' records (LEVEL_RECORD): three lines, the
    profile's best score, time and medals of the level (PENDING for none),
    set by menu_functions.c, in the small font under the level's words (four
    lines at most, from 258, at LEVEL_DATA_TEXT_SCALE), where the
    difficulties' icons were; and those icons' badges (LEVEL_BADGE), over
    the picture's bottom"""
    lines = _widget(LEVEL_RECORD, [("type", "text"), ("controller", 1), ("left", 417), ("top", 334),
                                   ("width", 146), ("height", 64), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF")],
                    [])
    # (the difficulty_indicator's picture, half its size)
    lines += _widget(LEVEL_BADGE, [("controller", 1), ("width", LEVEL_BADGE_SIZE[0]), ("height", LEVEL_BADGE_SIZE[1]),
                                   ("bitmap", LEVEL_BADGE_BITMAP)], [])
    lines += _widget(LEVEL_BADGE_PLATE, [("controller", 1), ("width", LEVEL_BADGE_PLATE_PLACE[2]),
                                         ("height", LEVEL_BADGE_PLATE_PLACE[3]), ("bitmap", LEVEL_BADGE_PLATE_BITMAP)],
                     [])
    return lines


def _leaderboard_screen(base: str, init: str, buttons: list) -> list:
    """a leaderboard's screen (base/leaderboard_screen): its title, a line of
    what it is of, the columns' headings (LEADERBOARD_COLUMNS) and their
    LEADERBOARD_ROWS rows, a note (none yet, loading, the server down), and
    the button bar's buttons (BACK at the right). init is run as it opens;
    menu_functions.c's "port leaderboard update" shows the rows, whichever
    leaderboard it is"""
    rows = []
    for index in range(LEADERBOARD_ROWS):
        top = 150 + 24 * index
        rows += [f'<child widget="{base}/row_{name}" y="{top}"/>' for name, *_ in LEADERBOARD_COLUMNS]
    lines = _widget(f"{base}/leaderboard_screen",
                    [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                     ("bitmap", "bitmaps/gradient")],
                    [f'<data input="port leaderboard update"/>',
                     f'<on event="created" run="{init}"/>',
                     '<on event="b" back="true"/>', '<on event="back" back="true"/>',
                     f'<child widget="{base}/title"/>', f'<child widget="{base}/level"/>',
                     *[f'<child widget="{base}/heading_{name}"/>' for name, *_ in LEADERBOARD_COLUMNS],
                     f'<child widget="{base}/note"/>',
                     *rows, f'<child widget="{base}/button_bar" y="414"/>'])
    text = [("type", "text"), ("controller", 1)]
    lines += _widget(f"{base}/title", text + [("left", 60), ("top", 40), ("width", 520), ("height", 40),
                                              ("text", "LEADERBOARD"), ("font", "ui\\large_ui"),
                                              ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/level", text + [("left", 60), ("top", 80), ("width", 520), ("height", 30),
                                              ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    for name, label, left, width, align, large in LEADERBOARD_COLUMNS:
        lines += _widget(f"{base}/heading_{name}",
                         text + [("left", left), ("top", 122), ("width", width), ("height", 22), ("text", label),
                                 ("font", "ui\\small_ui"), ("color", "#FF808080"), ("align", align)], [])
        # (the rows', placed by the screen's children: menu_functions.c's
        # leaderboard_update; the name, score and time in the large font,
        # the rest in the small one, a line lower to share its foot)
        lines += _widget(f"{base}/row_{name}",
                         text + [("left", left), ("top", 0 if large else 4), ("width", width), ("height", 22),
                                 ("font", "ui\\large_ui" if large else "ui\\small_ui"),
                                 ("color", "#FF2896FF" if large else "#FFB8C8E0"), ("align", align)], [])
    lines += _widget(f"{base}/note", text + [("left", 90), ("top", 150), ("width", 470), ("height", 60),
                                             ("font", "ui\\small_ui"), ("color", "#FFB0B0B0")], [])
    lines += _widget(f"{base}/button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                            ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [*buttons, '<child widget="common_button_back" x="510" y="1"/>'])
    return lines


def _leaderboard() -> list:
    """New Game's LEADERBOARD button (LEADERBOARD_BUTTON) and its screen:
    the level's name, then a row for each player with a record of it, its
    name, best score and best time, or a note of none; the online
    leaderboard's (community.c), or this machine's profiles' (VIEW: its
    button, "port leaderboard view"). menu_functions.c's "port leaderboard
    init" reads them as the screen opens; "port leaderboard update" shows
    them"""
    lines = _widget(LEADERBOARD_BUTTON, [("type", "text"), ("width", 128), ("height", 24),
                                         ("bitmap", "bitmaps/text_button_background"), ("text", "LEADERBOARD"),
                                         ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                                         ("text_y", 2)],
                    [f'<on event="a" open="{LEADERBOARD}/leaderboard_screen"/>',
                     f'<on event="start" open="{LEADERBOARD}/leaderboard_screen"/>',
                     '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _leaderboard_screen(LEADERBOARD, "port leaderboard init",
                                 [f'<child widget="{LEADERBOARD}/button_view" x="24" y="1"/>'])
    lines += _text_button(f"{LEADERBOARD}/button_view", None,
                          ['<on event="a" run="port leaderboard view"/>',
                           '<on event="start" run="port leaderboard view"/>'])
    return lines


# ---------- the daily challenge (port/linux/game/community.c): Campaign's
# DAILY CHALLENGE (with Settings > Gameplay > Community's on) opens
# today's, from the community server or, offline, made on this machine (its
# level, difficulty, enemies and skulls, and this install's best of it); PLAY
# plays it with its settings, LEADERBOARD shows the day's, PAST DAYS the
# challenges before it, each of which opens the same screen. When the server
# cannot be reached, a window asks what to do: the default server (a custom
# one's), another server's address (the default's), offline, or again
# (menu_functions.c's "port daily" functions)

DAILY_ITEM_WIDTH = 320
# the challenge's screen's words: the widget, left, top, width, height,
# font, colour, its own words, and its text_scale
DAILY_TEXTS = [
    ("title", 60, 36, 520, 36, "large", "#FF2896FF", "DAILY CHALLENGE"),
    ("date", 60, 72, 520, 20, "small", "#FF909090", None),
    ("mission", 214, 100, 366, 18, "small", "#FF909090", None),
    ("level", 214, 118, 366, 28, "large", "#FFFFFFFF", None, 90),
    ("difficulty", 266, 166, 314, 28, "large", "#FFFFD659", None),
    ("details", 60, 224, 520, 66, "small", "#FFB8C8E0", None),
    ("skulls_heading", 60, 294, 520, 18, "small", "#FF909090", None),
    ("skulls", 60, 340, 520, 32, "small", "#FFB8C8E0", None),
    ("mine", 60, 372, 520, 22, "large", "#FF2896FF", None),
    ("note", 60, 396, 520, 18, "small", "#FFB0B0B0", None),
]
# the skulls' icons (skulls/icons, a frame a skull): as many as a challenge
# turns on, in a row
DAILY_SKULL_ICONS = 16
DAILY_SKULL_ICON_SPACE = 28
DAILY_HISTORY_ROWS = 10
# the past days' columns: the widgets' name, heading, left (in the row),
# width, alignment and whether in the large font
# (the small font's capitals are some 9 units wide: the mission's name, the
# longest 27 characters, in it; the day TODAY for today's; PLAYERS online only,
# and PLAYERS and RANK with scoring on)
DAILY_HISTORY_COLUMNS = [
    ("date", "DAY", 8, 86, None, True),
    ("level", "MISSION", 98, 244, None, False),
    ("difficulty", "DIFFICULTY", 346, 92, None, False),
    ("players", "PLAYERS", 440, 64, "right", False),
    ("mine", "RANK", 508, 48, "right", False),
]
DAILY_HISTORY_LEFT = 40
DAILY_HISTORY_WIDTH = 560


def _daily() -> list:
    """Campaign's DAILY CHALLENGE (DAILY_ITEM: its picture, tools/ce_menus.py's)
    and its screens: the day's challenge, its leaderboard, the past days, and
    the window of a server not reached"""
    text = [("type", "text"), ("controller", 1)]
    lines = _widget(DAILY_ITEM, [("type", "text"), ("width", DAILY_ITEM_WIDTH), ("height", 33),
                                 ("bitmap", "main_menu/menu_daily")],
                    [f'<on event="a" run="port daily today" open="{DAILY}/daily_screen"/>',
                     f'<on event="start" run="port daily today" open="{DAILY}/daily_screen"/>',
                     '<on event="left_mouse" run="mouse emit accept event"/>'])
    # the day's challenge: its level's picture, number and name, its
    # difficulty's icon and name, what is swapped, its skulls' icons and
    # names, and this install's best of it; the note (a line) says what is
    # wrong or what the challenge is (menu_functions.c's daily_update)
    icons = [f'<child widget="{DAILY}/skull_icon" x="{60 + index * DAILY_SKULL_ICON_SPACE}" y="312"/>'
             for index in range(DAILY_SKULL_ICONS)]
    lines += _widget(f"{DAILY}/daily_screen",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<data input="port daily update"/>', '<on event="created" run="port daily init"/>',
                      # (back to Campaign's menu, however many days were looked at)
                      '<on event="b" run="port daily back"/>', '<on event="back" run="port daily back"/>',
                      *[f'<child widget="{DAILY}/{name}"/>' for name, *_ in DAILY_TEXTS],
                      f'<child widget="{DAILY}/picture"/>', f'<child widget="{DAILY}/difficulty_icon"/>', *icons,
                      f'<child widget="{DAILY}/button_bar" y="414"/>'])
    for name, left, top, width, height, font, color, label, *scale in DAILY_TEXTS:
        lines += _widget(f"{DAILY}/{name}", text + [("left", left), ("top", top), ("width", width), ("height", height),
                                                    *([("text", label)] if label else []),
                                                    ("font", f"ui\\{font}_ui"), ("color", color),
                                                    *([("text_scale", scale[0])] if scale else [])], [])
    # (the level's picture, as New Game's has it: ui.map's sp_levels, a
    # frame a level; the difficulties' icons, Normal to Legendary)
    lines += _widget(f"{DAILY}/picture", [("controller", 1), ("left", 60), ("top", 100), ("width", 140),
                                          ("height", 114), ("bitmap", "ui\\shell\\bitmaps\\sp_levels")], [])
    lines += _widget(f"{DAILY}/difficulty_icon",
                     [("controller", 1), ("left", 214), ("top", 152), ("width", 44), ("height", 48),
                      ("bitmap", "ui\\shell\\main_menu\\difficulty_select\\difficulty_options_small")], [])
    lines += _widget(f"{DAILY}/skull_icon", [("controller", 1), ("width", 24), ("height", 24),
                                             ("bitmap", "skulls/icons")], [])
    lines += _widget(f"{DAILY}/button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                             ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{DAILY}/button_play" x="24" y="1"/>',
                      f'<child widget="{DAILY}/button_leaderboard" x="154" y="1"/>',
                      f'<child widget="{DAILY}/button_history" x="284" y="1"/>',
                      '<child widget="common_button_back" x="510" y="1"/>',
                      # (RANDOM RUN at the top right, beside the title, though
                      # the bar's: the focus goes to it with the others)
                      f'<child widget="{DAILY}/button_random" x="452" y="-374"/>'])
    lines += _text_button(f"{DAILY}/button_random", "RANDOM RUN", ['<on event="a" run="port daily random"/>',
                                                                  '<on event="start" run="port daily random"/>'])
    # (the map lists' DAILY CHALLENGES' skulls over the level's picture)
    lines += _widget(DAILY_LEVEL_SKULL, [("controller", 1), ("width", 24), ("height", 24),
                                         ("bitmap", "skulls/icons")], [])
    lines += _text_button(f"{DAILY}/button_play", "PLAY", ['<on event="a" run="port daily play"/>',
                                                          '<on event="start" run="port daily play"/>'])
    lines += _text_button(f"{DAILY}/button_leaderboard", "LEADERBOARD",
                          [f'<on event="a" open="{DAILY}/leaderboard/leaderboard_screen"/>',
                           f'<on event="start" open="{DAILY}/leaderboard/leaderboard_screen"/>'])
    lines += _text_button(f"{DAILY}/button_history", "PAST DAYS",
                          [f'<on event="a" open="{DAILY}/history_screen"/>',
                           f'<on event="start" open="{DAILY}/history_screen"/>'])
    # the day's leaderboard, as New Game's
    lines += _leaderboard_screen(f"{DAILY}/leaderboard", "port daily leaderboard init", [])
    # the past days, newest first: a row each (DAILY_HISTORY_ROWS of them,
    # scrolling over more, as the mods' list), A opening its challenge
    rows = [f'<child widget="{DAILY}/history_row_{index + 1}" x="{DAILY_HISTORY_LEFT}" y="{104 + 30 * index}"/>'
            for index in range(DAILY_HISTORY_ROWS)]
    lines += _widget(f"{DAILY}/history_screen",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                      f'<child widget="{DAILY}/history_title"/>',
                      *[f'<child widget="{DAILY}/history_heading_{name}"/>' for name, *_ in DAILY_HISTORY_COLUMNS],
                      f'<child widget="{DAILY}/history_note"/>', f'<child widget="{DAILY}/history_list"/>'])
    lines += _widget(f"{DAILY}/history_title", text + [("left", 60), ("top", 36), ("width", 520), ("height", 36),
                                                       ("text", "PAST CHALLENGES"), ("font", "ui\\large_ui"),
                                                       ("color", "#FF2896FF")], [])
    for name, label, left, width, align, large in DAILY_HISTORY_COLUMNS:
        lines += _widget(f"{DAILY}/history_heading_{name}",
                         text + [("left", DAILY_HISTORY_LEFT + left), ("top", 82), ("width", width), ("height", 20),
                                 ("text", label), ("font", "ui\\small_ui"), ("color", "#FF808080"),
                                 ("align", align)], [])
    lines += _widget(f"{DAILY}/history_note", text + [("left", 90), ("top", 110), ("width", 470), ("height", 60),
                                                      ("font", "ui\\small_ui"), ("color", "#FFB0B0B0")], [])
    lines += _widget(f"{DAILY}/history_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children")],
                     ['<data input="port history update"/>', '<on event="created" run="port history init"/>', *rows,
                      f'<child widget="{DAILY}/history_button_bar" y="414"/>'])
    for index in range(DAILY_HISTORY_ROWS):
        lines += _widget(f"{DAILY}/history_row_{index + 1}",
                         [("width", DAILY_HISTORY_WIDTH), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("color", "#FF2896FF")],
                         [f'<on event="a" run="port daily choose" open="{DAILY}/daily_screen"/>',
                          f'<on event="start" run="port daily choose" open="{DAILY}/daily_screen"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          *[f'<child widget="{DAILY}/history_{name}" x="{left}"/>'
                            for name, label, left, *_ in DAILY_HISTORY_COLUMNS]])
    for name, label, left, width, align, large in DAILY_HISTORY_COLUMNS:
        lines += _widget(f"{DAILY}/history_{name}",
                         text + [("width", width), ("height", 22), ("font", f"ui\\{'large' if large else 'small'}_ui"),
                                 ("color", "#FF2896FF" if large else "#FFB8C8E0"), ("align", align),
                                 ("text_y", 4 if large else 7), ("text_flags", "no_focus_test")], [])
    lines += _widget(f"{DAILY}/history_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<child widget="common_button_back" x="510" y="1"/>'])
    lines += _daily_connection()
    return lines


# the server not reached's window's rows (its question, as the skulls'):
# the widget, its words, the function A runs; TRY THE DEFAULT SERVER with a
# custom server, the address (a field) with the default one, in one place
DAILY_CONNECTION_ROWS = [
    ("connection_default", "TRY THE DEFAULT SERVER", "port daily try default", 0),
    ("connection_address", None, "port daily connection edit", 0),
    ("connection_offline", "GO OFFLINE", "port daily go offline", 1),
    ("connection_retry", "TRY AGAIN", "port daily retry", 2),
]


def _daily_connection() -> list:
    """the window the daily challenge's screen opens when the server cannot be
    reached (menu_functions.c's daily_menu_poll): what happened, and what to
    do, its rows each running its function, which goes back to the challenge;
    B and Cancel back to it as it was"""
    base = DAILY
    lines = _widget(f"{base}/connection_screen",
                    [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                     ("bitmap", "bitmaps/gradient")],
                    ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                     f'<child widget="{base}/connection_title"/>', f'<child widget="{base}/connection_text"/>',
                     f'<child widget="{base}/connection_menu"/>'])
    lines += _widget(f"{base}/connection_title",
                     [("type", "text"), ("controller", 1), ("left", 40), ("top", 40), ("width", 560), ("height", 36),
                      ("text", "NO CONNECTION"), ("font", "ui\\large_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/connection_text",
                     [("type", "text"), ("controller", 1), ("left", 40), ("top", 84), ("width", 560), ("height", 196),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF")], [])
    children = ['<data input="port daily connection update"/>']
    for row, _, _, place in DAILY_CONNECTION_ROWS:
        children.append(f'<child{attributes([("widget", f"{base}/{row}"), ("x", 64), ("y", 290 + place * 32)])}/>')
    children.append('<child widget="common_button_cancel" x="510" y="415"/>')
    lines += _widget(f"{base}/connection_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children")], children)
    for row, label, function, _ in DAILY_CONNECTION_ROWS:
        handlers = [f'<on event="a" run="{function}"/>', f'<on event="start" run="{function}"/>',
                    '<on event="left_mouse" run="mouse emit accept event"/>']
        if label:
            lines += _widget(f"{base}/{row}",
                             [("type", "text"), ("width", 512), ("height", 28), ("bitmap", "bitmaps/option_bkds"),
                              ("text", label), ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                              ("text_y", 4)], handlers)
            continue
        # (the address: its label, and its value the field shows)
        lines += _widget(f"{base}/{row}",
                         [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [*handlers, '<on event="b" run="port field back"/>', '<on event="back" run="port field back"/>',
                          f'<child widget="{base}/connection_address_label"/>',
                          f'<child widget="{base}/connection_address_value" x="200" y="1"/>'])
        lines += _widget(f"{base}/connection_address_label",
                         [("type", "text"), ("controller", 1), ("width", 200), ("height", 22), ("text", "SERVER ADDRESS:"),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/connection_address_value",
                         [("type", "text"), ("controller", 1), ("width", 306), ("height", 22), ("font", "ui\\small_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 5), ("text_flags", "no_focus_test")],
                         [])
    return lines


def _news_screen() -> list:
    """the news window: a release's name in two rows, its title and, after
    its " - ", its description (each in the small font when the large one's
    is too wide; a name of one part in the second row), and under it its day
    and build, centred over a panel
    (bitmaps/controller_border) of NEWS_ROWS lines of its notes; under the
    panel this build's number and whether it is the latest, and under that
    the buttons that
    turn to the newer and the older releases (the menus' arrows either side
    of which release it is), scroll (the menus' scroll arrows, shown while
    there is more than the panel holds) and close it (B as the console's
    picture of it: the menus' %-codes); the title's, the lines' and the
    release's texts menu_functions.c's alone, which makes them as long as it
    needs.
    Up and down (the D-pad, the sticks, the mouse wheel) scroll a line, left
    and right go to the newer and the older release, B, Back or X again close
    it. And the main menu's hint that X opens it (WIDGET_PATCHES)"""
    base = NEWS
    left, top = NEWS_PANEL[0] + 49, NEWS_PANEL[1] + 104
    close = ['<on event="b" run="port news close"/>', '<on event="back" run="port news close"/>',
             '<on event="x" run="port news close"/>']
    scroll = [f'<on event="{event}" run="port news up"/>' for event in ("up", "stick_up", "right_stick_up")]
    scroll += [f'<on event="{event}" run="port news down"/>' for event in ("down", "stick_down", "right_stick_down")]
    scroll += ['<on event="left" run="port news newer"/>', '<on event="right" run="port news older"/>']
    lines = _widget(f"{base}/news_screen", [("width", 640), ("height", 480), ("bitmap", "bitmaps/gradient_big")],
                    ['<data input="port news update"/>', '<on event="created" run="port news init"/>', *close,
                     *scroll, f'<child widget="{base}/news_panel" x="{NEWS_PANEL[0]}" y="{NEWS_PANEL[1]}"/>',
                     f'<child widget="{base}/news_title" x="{left}" y="{top - 68}"/>',
                     f'<child widget="{base}/news_title_small" x="{left}" y="{top - 62}"/>',
                     f'<child widget="{base}/news_title_2" x="{left}" y="{top - 42}"/>',
                     f'<child widget="{base}/news_title_2_small" x="{left}" y="{top - 36}"/>',
                     f'<child widget="{base}/news_subtitle" x="{left}" y="{top - 16}"/>',
                     *[f'<child widget="{base}/news_line" x="{left + 16}" y="{top + 12 + 18 * row}"/>'
                       for row in range(NEWS_ROWS)],
                     # (just under the panel, over the buttons)
                     f'<child widget="{base}/news_build" x="{left}" y="{top + 253}"/>',
                     # (under the panel at its left, the release's arrows
                     # either side of which release it is, d-pad left and
                     # right to a tap or a click, as the scroll arrows below)
                     f'<child widget="{base}/dpad_left_butn_news" x="{left + 16}" y="{top + 275 + 7}"/>',
                     f'<child widget="{base}/news_page_text" x="{left + 40}" y="{top + 275}"/>',
                     f'<child widget="{base}/dpad_right_butn_news" x="{left + 112}" y="{top + 275 + 7}"/>',
                     # (under the panel, the buttons left to right from a little
                     # left of its middle: the up arrow, "Scroll", the down
                     # arrow, B's picture and "Back"; widgets of their own,
                     # which the menus' centring of a text with pictures in it
                     # put apart; no picture of the D-pad is drawn in text, so
                     # the menus' own scroll arrows. The arrows are the d-pad's
                     # up and down to a tap or a click (a key's icon by their
                     # names: ui_widget.c's ui_mouse_key_button), each further
                     # than a key's label from "Scroll", so that it is neither's.
                     # Their pictures sit high in their 16 texels: 7 down puts
                     # their middles at the text's)
                     f'<child widget="{base}/dpad_up_butn_news" x="{left + 164}" y="{top + 275 + 7}"/>',
                     f'<child widget="{base}/news_scroll_text" x="{left + 198}" y="{top + 275}"/>',
                     f'<child widget="{base}/dpad_down_butn_news" x="{left + 254}" y="{top + 275 + 7}"/>',
                     # (Back's tap and click: B's, as the hint's is X's)
                     f'<child widget="{base}/b_butn_news_back" x="{left + 300}" y="{top + 275}"/>',
                     f'<child widget="{base}/news_back_text" x="{left + 300}" y="{top + 275}"/>'])
    lines += _widget(f"{base}/news_panel", [("width", 640), ("height", 480), ("bitmap", "bitmaps/controller_border")],
                     [])
    for name, font, height, color in (("title", "large", 30, "#FF2896FF"), ("title_small", "small", 20, "#FF2896FF"),
                                      ("title_2", "large", 30, "#FF2896FF"),
                                      ("title_2_small", "small", 20, "#FF2896FF"),
                                      ("subtitle", "small", 18, "#FFCCCCCC"), ("build", "small", 18, "#FFCCCCCC")):
        lines += _widget(f"{base}/news_{name}", [("type", "text"), ("controller", 1), ("width", 507),
                                                 ("height", height), ("font", f"ui\\{font}_ui"), ("color", color),
                                                 ("align", "center"), ("text_flags", "no_focus_test")], [])
    lines += _widget(f"{base}/news_line", [("type", "text"), ("controller", 1), ("width", 475), ("height", 18),
                                          ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"),
                                          ("text_flags", "no_focus_test")], [])
    for name in ("up", "down", "left", "right"):
        lines += _widget(f"{base}/dpad_{name}_butn_news", [("width", 16), ("height", 16),
                                                           ("bitmap", f"bitmaps/arrow_sm_{name}")], [])
    lines += _widget(f"{base}/news_page_text", [("type", "text"), ("controller", 1), ("width", 64), ("height", 20),
                                               ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                                               ("text_flags", "no_focus_test")], [])
    for name, text, width in (("scroll", "Scroll", 48), ("back", "%b-button Back", 120)):
        lines += _widget(f"{base}/news_{name}_text", [("type", "text"), ("controller", 1), ("width", width),
                                                      ("height", 20), ("text", text), ("font", "ui\\small_ui"),
                                                      ("color", "#FFFFFFFF"), ("text_flags", "no_focus_test")], [])
    # (the hint's tap and click: a widget named as a screen key's icon is X's
    # to the pointer, and the text beside it its label, ui_widget.c's
    # ui_mouse_key_button; nothing of it drawn, the hint's own picture is)
    lines += _widget(f"{base}/x_butn_news", [("width", 16), ("height", 20)], [])
    lines += _widget(f"{base}/b_butn_news_back", [("width", 16), ("height", 20)], [])
    lines += _widget(f"{base}/news_hint", [("type", "text"), ("controller", 1), ("width", 150), ("height", 20),
                                          ("text", "%x-button NEWS"), ("font", "ui\\small_ui"),
                                          ("color", "#FFCCCCCC"), ("text_flags", "no_focus_test")], [])
    return lines


def _invite_qr_screen() -> list:
    """(the Switch's) Server Setup's invite link as a QR code, for a phone
    to read: its own screen, opened by the invite row's A (menu_functions.c's
    invite_copy), which only B closes; the code is drawn above its line of
    text (menu_functions.c's link_qr_draw)"""
    base = f"{MT}/server_settings/invite_qr"
    lines = _widget(f"{base}/invite_qr_screen", [("width", 640), ("height", 480), ("flags", "pause_game"),
                                                 ("bitmap", "bitmaps/gradient")],
                    ['<on event="b" run="ss invite qr back"/>',
                     f'<child widget="{base}/invite_qr_title"/>',
                     f'<child widget="{base}/invite_qr_text"/>'])
    lines += _widget(f"{base}/invite_qr_title", [("type", "text"), ("controller", 1), ("left", 35), ("top", 24),
                                                 ("width", 570), ("height", 30), ("text", "INVITE LINK"),
                                                 ("font", "ui\\large_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/invite_qr_text", [("type", "text"), ("controller", 1), ("left", 40), ("top", 410),
                                                ("width", 560), ("height", 18),
                                                ("text", "Scan it with your phone for the invite link. B: back"),
                                                ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                                                ("text_flags", "no_focus_test")], [])
    return lines


def multiplayer_files() -> dict:
    """the port's multiplayer widgets: the browser's additions, the server
    settings, the lobby"""
    head = ['<?xml version="1.0" encoding="UTF-8"?>',
            "<!-- The port's multiplayer screens, in the PC version's style (tools/port_settings.py) -->", "<menus>"]
    return {
        f"{MT}/join_game".replace("/", ".") + ".port.xml": head + _join_game_extras() + ["</menus>", ""],
        f"{MT}/server_settings".replace("/", ".") + ".xml": head + _server_settings() + _tag_profile_screen() +
        _invite_qr_screen() +
        ["</menus>", ""],
        f"{MT}/lobby".replace("/", ".") + ".xml": head + _lobby() + ["</menus>", ""],
        f"{MT}/coop".replace("/", ".") + ".xml": head + _coop() + ["</menus>", ""],
        f"{MT}/local".replace("/", ".") + ".xml": head + _create_local() + ["</menus>", ""],
        "main_menu/new_select".replace("/", ".") + ".port.xml": head + _map_kind() + ["</menus>", ""],
        "main_menu/solo_level_select".replace("/", ".") + ".port.xml": head + _level_records() + _leaderboard() +
        ["</menus>", ""],
        # (and the main menu's news)
        NEWS.replace("/", ".") + ".xml": head + _news_screen() + ["</menus>", ""],
        "main_menu/settings_select/multiplayer_setup/item_options_edit".replace("/", ".") + ".port.xml": head + _item_options_extras() + ["</menus>", ""],
        PLAYER_EDIT.replace("/", ".") + ".port.xml": head + _player_options_extras() + ["</menus>", ""],
        TEAMPLAY_EDIT.replace("/", ".") + ".port.xml": head + _teamplay_options_extras() + ["</menus>", ""],
        OPENCE_EDIT.replace("/", ".") + ".port.xml": head + _opence_options() + ["</menus>", ""],
        MODS.replace("/", ".") + ".xml": [head[0], "<!-- The port's Mods screen, in the PC version's style "
                                          "(tools/port_settings.py) -->", head[2]] + _mods() + ["</menus>", ""],
        DAILY.replace("/", ".") + ".xml": [head[0], "<!-- The port's Daily Challenge screens, in the PC version's "
                                           "style (tools/port_settings.py) -->", head[2]] + _daily() +
        ["</menus>", ""],
        THEATER.replace("/", ".") + ".xml": [head[0], "<!-- The port's Theater screen, in the PC version's style "
                                             "(tools/port_settings.py) -->", head[2]] + _theater() + ["</menus>", ""],
    }


REPLACED_FOLDERS = [f"{MT}/server_settings"]


# ---------- the mods (port/linux/game/mods.c, docs/mods.md): the main menu's
# MODS, which opens the list of the mods folder's mods, in their load order,
# each turned on or off and moved up or down (menu_functions.c's "port mods"
# functions)

# the redraws' main menu items (svg/shell/main_menu/menu_*.svg): their
# letters' top and scale, and where their middle is
MENU_ITEM_TOP, MENU_ITEM_SCALE, MENU_ITEM_MIDDLE = 6.57, 0.9037, 130.0
# the space between two of the redraws' letters, in their units (their
# capitals are 21.5 high), for the pairs the port's items have: no redraw has
# these pairs, so each is the sum of a space after the first letter and one
# before the second, fitted (least squares) to the 53 pairs the redraws' items
# and headers have (D-I, of the main menu's Credits, gives D's right side).
# DAILY CHALLENGE's the same way, fitted to all 110 pairs of the redraws'
# words and the 24 that a space parts ("Y C": the space between words too)
MENU_ITEM_SPACES = {"MO": 3.17, "OD": 2.8, "DS": 2.3,
                    "DA": 0.93, "AI": 1.03, "IL": 4.12, "LY": -3.02, "Y C": 10.89, "CH": 3.13, "HA": 1.74,
                    "AL": 0.71, "LL": 0.8, "LE": 0.19, "EN": 4.22, "NG": 2.72, "GE": 2.5,
                    # (THEATER's: ER as the redraws' PLAYER has it; the
                    # others set by eye beside the PC version's items)
                    "TH": 2.5, "HE": 2.6, "EA": 1.5, "AT": 0.5, "TE": 1.5, "ER": 3.24}
# the mods' status icons (port/assets/icons/lucide): a mod out of the order it
# asks for, and one that cannot be played (another mod says it cannot be
# played with it, or this build cannot play it): menu_functions.c's
# mods_update shows frame 1 or 2
MOD_STATUS_ICONS = [("triangle-alert", "#FFB020"), ("circle-x", "#FF4040")]


def redraw_letters(redraws: Path) -> dict:
    """the redraws' letters (their <path id="gA">s, one drawing of each
    letter in every item and header), by letter"""
    letters = {}
    for svg in sorted(redraws.rglob("*.svg")):
        if any(part.startswith("_") for part in svg.relative_to(redraws).parts):
            continue
        for letter, path in re.findall(r'<path id="g([A-Z])" d="([^"]+)"', svg.read_text()):
            if letters.setdefault(letter, path) != path:
                sys.exit(f"{svg}: the redraws draw {letter} two ways")
    return letters


def menu_item_frames(text: str, redraws: Path, width: int = 256) -> list:
    """the frames (svg, width, height) of a main menu item of the port's: its
    text in the redraws' own letters (redraw_letters), spaced, placed and
    coloured as their items are, dim, then white with their blue glow; a
    wider one (DAILY CHALLENGE's) as wide as `width`, about the same middle"""
    from fontTools.pens.boundsPen import BoundsPen
    from fontTools.svgLib.path import parse_path

    letters = redraw_letters(redraws)
    glyphs = text.replace(" ", "")
    widths = []
    for letter in glyphs:
        pen = BoundsPen(None)
        parse_path(letters[letter], pen)
        widths.append(pen.bounds[2])
    # (each pair's space; a pair a space parts is keyed with it, "Y C")
    pairs = re.findall(r"(?=([A-Z] ?[A-Z]))", text)
    spaces = [MENU_ITEM_SPACES[pair] for pair in pairs]
    middle = MENU_ITEM_MIDDLE + (width - 256) / 2
    x = middle - (sum(widths) + sum(spaces)) * MENU_ITEM_SCALE / 2
    uses = []
    for index, letter in enumerate(glyphs):
        uses.append(f'<use href="#g{letter}" transform="translate({x:.2f},{MENU_ITEM_TOP}) scale({MENU_ITEM_SCALE})"/>')
        x += (widths[index] + (spaces[index] if index < len(spaces) else 0)) * MENU_ITEM_SCALE
    paths = [f'    <path id="g{letter}" d="{letters[letter]}"/>' for letter in sorted(set(glyphs))]
    head = ['<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
            f'width="{width}" height="64" viewBox="0 0 {width} 64">', "  <defs>", *paths]
    dim = [*head, "  </defs>", f'  <!-- "{text}" (normal state: dim blue) -->',
           '  <g id="text" fill="#2895ff" fill-opacity="0.467">', *[f"    {use}" for use in uses], "  </g>", "</svg>", ""]
    glow = [*head, f'    <filter id="glow" filterUnits="userSpaceOnUse" x="0" y="0" width="{width}" height="64">',
            '      <feGaussianBlur stdDeviation="2.19"/>', "    </filter>", "  </defs>",
            f'  <!-- "{text}" (highlighted: white with blue glow) -->',
            '  <g fill="#2895ff" stroke="#2895ff" stroke-width="6.3" stroke-linejoin="round" opacity="0.650" '
            'filter="url(#glow)">', *[f"    {use}" for use in uses], "  </g>",
            '  <g id="text" fill="#fff">', *[f"    {use}" for use in uses], "  </g>", "</svg>", ""]
    return [("\n".join(dim), width, 64), ("\n".join(glow), width, 64)]


def mod_status_frames(icons: Path, size: int = 24) -> list:
    """the frames of the mods' status icon, `size` pixels square (the menus
    draw a bitmap at its size): none, out of order, incompatible"""
    blank = f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 24 24"/>\n'
    return [(blank, size, size)] + [
        ((icons / f"{icon}.svg").read_text().replace('stroke="currentColor"', f'stroke="{color}"')
         .replace('width="24"', f'width="{size}"').replace('height="24"', f'height="{size}"'), size, size)
        for icon, color in MOD_STATUS_ICONS]


def _text_button(name: str, text, handlers: list, width: int = 128) -> list:
    """a button of the button bar with its own words; with none (None), the
    code's (a text widget's words of the file are drawn again each frame,
    over the code's: ui_widget.c)"""
    pairs = [("type", "text"), ("width", width), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
             *([("text", text)] if text is not None else []), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"),
             ("align", "center"), ("text_y", 2)]
    return _widget(name, pairs, handlers + ['<on event="left_mouse" run="mouse emit accept event"/>'])


def _mods() -> list:
    """the main menu's MODS and its screen, laid out as the lobby is: on the
    left two thirds the mods as a resource pack list has them (the one at the
    top playing over those below it: the load order upside down), a row each
    with its status icon, name and ON or OFF; on the right third, in the
    lobby's frame, the mod chosen: its name, what is wrong with it, whether it
    is on and its priority, and what its mod.toml says. A turns the mod chosen
    on or off, left and right (and MOVE UP and MOVE DOWN, for the mouse) move
    it; OK keeps it all (load_order.toml) and plays them, Cancel forgets it
    (menu_functions.c)"""
    base = MODS
    lines = _widget("main_menu/main_menu_item_mods",
                    [("type", "text"), ("width", 256), ("height", 33), ("bitmap", "main_menu/menu_mods")],
                    [f'<on event="a" open="{base}/mods_screen"/>', f'<on event="start" open="{base}/mods_screen"/>',
                     '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/mods_screen", [("width", 640), ("height", 480),
                                             ("flags", "pass_unhandled_to_focused_child pause_game"),
                                             ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/mods_list"/>',
                      f'<child widget="{base}/header_mods"/>'])
    lines += _header(f"{base}/header_mods", f"{base}/header_mods")
    # (the lobby's rows, MOD_ROWS of them, scrolling over more mods)
    rows = [f'<child widget="{base}/op_mod_{index + 1}" x="20" y="{73 + 30 * index}"/>' for index in range(MOD_ROWS)]
    lines += _widget(f"{base}/mods_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                           ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                           ("description", f"{base}/mod_details")],
                     ['<data input="port mods update"/>', '<on event="created" run="port mods init"/>',
                      '<on event="deleted" run="port mods close"/>', *rows,
                      f'<child widget="{base}/button_bar" y="414"/>'])
    for index in range(MOD_ROWS):
        lines += _widget(f"{base}/op_mod_{index + 1}",
                         [("width", 390), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("color", "#FF2896FF")],
                         ['<on event="a" run="port mods toggle"/>', '<on event="start" run="port mods toggle"/>',
                          '<on event="left" run="port mods move up"/>', '<on event="right" run="port mods move down"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/mod_status" x="8" y="2"/>',
                          f'<child widget="{base}/mod_name" x="34"/>',
                          f'<child widget="{base}/mod_state" x="318"/>'])
    lines += _widget(f"{base}/mod_status", [("controller", 1), ("width", 24), ("height", 24),
                                            ("bitmap", "mods/status")], [])
    lines += _widget(f"{base}/mod_name",
                     [("type", "text"), ("controller", 1), ("width", 284), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_y", 4)], [])
    lines += _widget(f"{base}/mod_state",
                     [("type", "text"), ("controller", 1), ("width", 66), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4), ("text_flags", "no_focus_test")], [])
    # (in the lobby's frame, sel_list_desc_bkd's: the mod chosen's name, the
    # icon of what is wrong with it, and the rest)
    lines += _widget(f"{base}/mod_details", [("width", 640), ("height", 480)],
                     [f'<child widget="{base}/mod_details_name"/>', f'<child widget="{base}/mod_details_status"/>',
                      f'<child widget="{base}/mod_details_text"/>'])
    lines += _widget(f"{base}/mod_details_name",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 84), ("width", 200), ("height", 44),
                      ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/mod_details_status", [("controller", 1), ("left", 411), ("top", 132), ("width", 16),
                                                    ("height", 16), ("bitmap", "mods/status_small")], [])
    lines += _widget(f"{base}/mod_details_text",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 130), ("width", 200),
                      ("height", 272), ("font", "ui\\small_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{base}/button_up" y="1"/>', f'<child widget="{base}/button_down" x="130" y="1"/>',
                      f'<child widget="{base}/button_ok" x="380" y="1"/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += _text_button(f"{base}/button_up", "MOVE UP", ['<on event="a" run="port mods move up"/>',
                                                          '<on event="start" run="port mods move up"/>'])
    lines += _text_button(f"{base}/button_down", "MOVE DOWN", ['<on event="a" run="port mods move down"/>',
                                                              '<on event="start" run="port mods move down"/>'])
    lines += _button(f"{base}/button_ok", 1, ['<on event="a" run="port mods save" back="true"/>',
                                              '<on event="start" run="port mods save" back="true"/>'])
    return lines


# ---------- the Theater (port/linux/REPLAY.md): the main menu's THEATER,
# the replays a host or a client records (port/linux/game/network_replay.c),
# laid out as the Mods screen is: on the left the replays, a row each, the
# newest first (its map and when, and its length), scrolling over more, A
# playing the one chosen (network_replay_playback.c); on the right, in the
# lobby's frame, what the one chosen is; and the buttons: RECORD (ON or OFF,
# network.record_replays), DELETE (the one chosen, at a second press), EXPORT
# (Android's: the app's folder picker, HaloActivity.exportReplays) and BACK
# (menu_functions.c's "port theater" functions). The Switch and the PS5 have
# fewer rows: under them a QR code of their replay page
# (network_replay_page.c)

THEATER_ROWS = 10
THEATER_QR_ROWS = 6
THEATER_ROW_TOP = 73


def _theater() -> list:
    base = THEATER
    lines = _widget("main_menu/main_menu_item_theater",
                    [("type", "text"), ("width", 256), ("height", 33), ("bitmap", "main_menu/menu_theater")],
                    [f'<on event="a" open="{base}/theater_screen"/>', f'<on event="start" open="{base}/theater_screen"/>',
                     '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/theater_screen", [("width", 640), ("height", 480),
                                                ("flags", "pass_unhandled_to_focused_child pause_game"),
                                                ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/theater_list"/>',
                      f'<child widget="{base}/header_theater"/>',
                      f'<child widget="{base}/theater_qr_text" platform="switch ps5"/>'])
    lines += _header(f"{base}/header_theater", f"{base}/header_theater")
    rows = []
    for index in range(THEATER_ROWS):
        rows.append(f'<child{attributes([("widget", f"{base}/op_replay_{index + 1}"), ("x", 20), ("y", THEATER_ROW_TOP + 30 * index), ("platform", "desktop android web" if index >= THEATER_QR_ROWS else None)])}/>')
    lines += _widget(f"{base}/theater_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                              ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                              ("description", f"{base}/theater_details")],
                     ['<data input="port theater update"/>', *rows, f'<child widget="{base}/button_bar" y="414"/>'])
    for index in range(THEATER_ROWS):
        lines += _widget(f"{base}/op_replay_{index + 1}",
                         [("width", 390), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("color", "#FF2896FF")],
                         ['<on event="a" run="port theater watch"/>', '<on event="start" run="port theater watch"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/theater_replay_name" x="12"/>',
                          f'<child widget="{base}/theater_replay_length" x="290"/>'])
    lines += _widget(f"{base}/theater_replay_name",
                     [("type", "text"), ("controller", 1), ("width", 276), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_y", 4)], [])
    lines += _widget(f"{base}/theater_replay_length",
                     [("type", "text"), ("controller", 1), ("width", 90), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "right"), ("text_y", 4), ("text_flags", "no_focus_test")], [])
    # (the Switch's and the PS5's: the replay page's QR code, drawn over this
    # line by menu_functions.c, network_replay_page.c)
    lines += _widget(f"{base}/theater_qr_text",
                     [("type", "text"), ("controller", 1), ("left", 20), ("top", 392), ("width", 390), ("height", 18),
                      ("text", "Scan it to download or upload replays."), ("font", "ui\\small_ui"),
                      ("color", "#FFFFFFFF"), ("align", "center"), ("text_flags", "no_focus_test")], [])
    # (in the lobby's frame, sel_list_desc_bkd's, as the mods' details)
    lines += _widget(f"{base}/theater_details", [("width", 640), ("height", 480)],
                     [f'<child widget="{base}/theater_details_text"/>'])
    lines += _widget(f"{base}/theater_details_text",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 84), ("width", 200),
                      ("height", 318), ("font", "ui\\small_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{base}/button_record" y="1"/>',
                      f'<child widget="{base}/button_delete" x="130" y="1"/>',
                      f'<child widget="{base}/button_export" x="260" y="1" platform="android"/>',
                      f'<child widget="{base}/button_back" x="510" y="1"/>'])
    lines += _text_button(f"{base}/button_record", None, ['<on event="a" run="port theater record"/>',
                                                                  '<on event="start" run="port theater record"/>'])
    lines += _text_button(f"{base}/button_delete", "DELETE", ['<on event="a" run="port theater delete"/>',
                                                             '<on event="start" run="port theater delete"/>'])
    lines += _text_button(f"{base}/button_export", "EXPORT", ['<on event="a" run="port theater export"/>',
                                                             '<on event="start" run="port theater export"/>'])
    lines += _text_button(f"{base}/button_back", "BACK", ['<on event="a" back="true"/>',
                                                         '<on event="start" back="true"/>'])
    return lines
