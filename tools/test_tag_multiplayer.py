"""The Tag Injector in multiplayer (port/linux/game/tag_multiplayer.c,
docs/tag_injector.md "In multiplayer"): Server Setup's TAG PROFILE, as
tools/port_settings.py writes it and menu_functions.c runs it, the
multiplayer profiles' settings as the menu and the game read them, and the
share's place in the netcode (its message, its advertisement's flag and
map sharing's file kind)."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import port_settings  # noqa: E402

MT = port_settings.MT
MENUS = ROOT / "port/assets/menus"
GAME = ROOT / "port/linux/game"
FUNCTIONS = GAME / "menu_functions.c"
TAG_MULTIPLAYER = GAME / "tag_multiplayer.c"


def _strings(text: str, name: str) -> list:
    block = text.split(f'<strings name="{name}"', 1)[1].split("</strings>", 1)[0]
    return re.findall(r'<string text="([^"]*)"', block)


def _c_strings(text: str, opener: str) -> list:
    """the first strings of each entry of a C array's initializer"""
    block = text.split(opener, 1)[1].split("};", 1)[0]
    return re.findall(r'\{ "([a-z_]*)"', block)


def test_server_setup_tag_profile():
    """Server Setup's TAG PROFILE: the row the generator writes (the PC
    menus' file is the generator's), its label and help where
    menu_functions.c looks for them, and its runs handled there."""
    generated = "\n".join(port_settings.multiplayer_files()["main_menu.multiplayer_type_select.server_settings.xml"])
    kept = (MENUS / "ce/main_menu.multiplayer_type_select.server_settings.xml").read_text()
    assert generated.strip() == kept.strip()
    functions = FUNCTIONS.read_text()
    key, title, help_text = port_settings.TAG_PROFILE_SETUP
    base = f"{MT}/server_settings"

    assert f'<child widget="{base}/op_{key}" x="54"' in kept
    # (each run a name menu_tags.c knows: one it does not makes the game
    # refuse every menu file, and keep the Xbox's menus)
    known = (ROOT / "port/linux/game/menu_tags.c").read_text().split("port_function_names[] =", 1)[1].split("};", 1)[0]
    for run in ("ss tag profile change",):
        assert f'run="{run}"' in kept
        assert f'"{run}"' in functions
        assert f'"{run}"' in known
    labels = _strings(kept, f"{base}/labels")
    label_index = int(re.search(r'name="' + re.escape(f"{base}/{key}_label") + r'"[^>]* string_index="(\d+)"',
                                kept).group(1))
    assert labels[label_index] == f"{title}:"
    helps = _strings(kept, f"{base}/help_strings")
    help_index = int(re.search(r"#define TAG_PROFILE_SETUP_HELP (\d+)", functions).group(1))
    assert helps[help_index] == help_text
    # (the publisher's help, last before it, where menu_functions.c has it)
    publisher_index = int(re.search(r"#define SERVER_LISTING_PUBLISHER_HELP (\d+)", functions).group(1))
    assert "publisher key" in helps[publisher_index]
    # (the help line's two lines at most: test_skulls_menu.py's)
    assert len(help_text.split("\\n")) <= 2 and len(help_text.replace("\\n", "")) <= 88
    # (its value, and the setting it writes)
    assert f'named(list, "{key}_value", 0)' in functions
    assert '{ "game.multiplayer_tag_profile", _config_string, "\\"\\""' in \
        (ROOT / "port/linux/src/port_config.c").read_text()
    assert 'config_write("game.multiplayer_tag_profile"' in TAG_MULTIPLAYER.read_text()


def test_tag_profile_screen():
    """The game's TAG PROFILE screen, between the gametype and Server Setup:
    in the generated file, at the name menu_functions.c opens it by, its
    runs known to menu_tags.c and handled; the gametype's OK opens it in
    Server Setup's place, and its choice is the game's (not config.toml's),
    which decides whether the game is a modded one."""
    kept = (MENUS / "ce/main_menu.multiplayer_type_select.server_settings.xml").read_text()
    functions = FUNCTIONS.read_text()
    game = TAG_MULTIPLAYER.read_text()
    screen = f"{MT}/server_settings/tag_profile/tag_profile_screen"
    assert f'<widget name="{screen}"' in kept
    opened = re.search(r'#define TAG_PROFILE_SCREEN_NAME \\\n\t"([^"]+)"', functions).group(1)
    assert opened == "pc\\\\" + screen.replace("/", "\\\\")
    rows = int(re.search(r"#define TAG_PROFILE_ROWS (\d+)", functions).group(1))
    assert rows == port_settings.TAG_PROFILE_ROWS
    for index in range(rows):
        assert f'name="{MT}/server_settings/tag_profile/tag_profile_{index}"' in kept
    menu_tags = (ROOT / "port/linux/game/menu_tags.c").read_text()
    known = menu_tags.split("port_function_names[] =", 1)[1].split("};", 1)[0]
    inputs = menu_tags.split("port_game_data_input_names[] =", 1)[1].split("};", 1)[0]
    for run in ("mp tag profiles init", "mp tag profile choose"):
        assert f'run="{run}"' in kept and f'"{run}"' in functions and f'"{run}"' in known
    assert '<data input="mp tag profiles update"/>' in kept
    assert '"mp tag profiles update"' in functions and '"mp tag profiles update"' in inputs
    assert "return gametype_choose(widget, widget_deleted);" in functions
    assert "ui_widget_port_open(list, TAG_PROFILE_SCREEN_NAME, widget_deleted)" in functions
    # (the game's own, kept from the screen: the host builds and advertises
    # by it, and a new game starts with none)
    hosted = game.split("boolean tag_multiplayer_hosted(", 1)[1].split("\n}", 1)[0]
    assert "tag_multiplayer.game_profile[0]" in hosted and "hosted.active" not in hosted
    for function in ("static void host_build(", "static void host_update(", "void tag_multiplayer_level_loaded("):
        body = game.split(function, 1)[1].split("\n}", 1)[0]
        assert "tag_multiplayer_profile_chosen()" not in body, function
    host = functions.split("static boolean multiplayer_host(", 1)[1].split("\n}", 1)[0]
    assert "tag_multiplayer_game_profile_set(NONE);" in host


def test_settings_values():
    """The multiplayer profile's settings' values, as the menu writes them
    and tag_multiplayer.c reads them, in the same order (the first each's
    default, as port_tag_mp_profiles_create writes it)."""
    functions = FUNCTIONS.read_text()
    game = TAG_MULTIPLAYER.read_text()
    store = (ROOT / "port/linux/src/port_tag_mp_profiles.c").read_text()
    for menu, read, setting in (("tags_mp_rotations[]", "rotations[]", "rotation"),
                                ("tags_mp_replaces[]", "replaces[]", "replace"),
                                ("tags_mp_loadouts[]", "loadouts[]", "loadout")):
        menu_values = _c_strings(functions, f"static struct tags_choice const {menu} =")
        read_values = re.findall(r'"([a-z]+)"', game.split(f"static char const *const {read} =", 1)[1]
                                 .split(";", 1)[0])
        assert menu_values == read_values, setting
        assert f'"{setting}", "{read_values[0]}"' in store.split("mp_defaults[] =", 1)[1].split(";", 1)[0]


def test_stock_weapons():
    """The map's weapons a profile swaps are the stock HUDs' (by their keys,
    in their order: the settings' [manual] keys), by the multiplayer maps'
    weapon paths."""
    stock_huds = _c_strings((GAME / "tag_injector.c").read_text(), "} const stock_huds[] =")
    game = TAG_MULTIPLAYER.read_text()
    weapons = re.findall(r'\{ "([a-z_]+)", "([A-Z ]+)", "(weapons\\\\[^"]+)" \}',
                         game.split("stock_weapons[TAG_MULTIPLAYER_STOCK_WEAPONS] =", 1)[1].split("};", 1)[0])
    assert [key for key, _, _ in weapons] == stock_huds
    assert len(weapons) == int(re.search(r"TAG_MULTIPLAYER_STOCK_WEAPONS = (\d+)",
                                         (GAME / "tag_multiplayer.h").read_text()).group(1))
    for key, name, path in weapons:
        assert name == key.replace("_", " ").upper()
        # (each its folder's and its own name: weapons\shotgun\shotgun)
        folder, leaf = path.split("\\\\")[1:]
        assert folder == leaf


def test_netcode():
    """The share's message, at a number of the mod's own; its
    advertisement's flag, apart from the others and making the game a
    modded one wherever the randomizer's does; the mod version raised; and
    map sharing's file kind for it, a bit of a request's byte."""
    distributed = (GAME / "network_distributed.h").read_text()
    assert re.search(r"_distributed_message_tag_share = 163,", distributed)
    assert "tag_multiplayer_handle_message(" in (GAME / "network_distributed.c").read_text()
    limits = (ROOT / "port/linux/include/halo_port_limits.h").read_text()
    flags = {name: int(value, 16) for name, value in
             re.findall(r"#define (HALO_PORT_ADVERTISED_[A-Z_]+_FLAG) (0x[0-9A-Fa-f]+)", limits)}
    assert flags["HALO_PORT_ADVERTISED_TAG_SHARE_FLAG"] == 0x40
    assert len(set(flags.values())) == len(flags)
    assert int(re.search(r"#define HALO_PORT_MOD_NETWORK_VERSION (\d+)", limits).group(1)) >= 4
    handler = (ROOT / "source/networking/network_server_message_handler.c").read_text()
    assert "HALO_PORT_ADVERTISED_TAG_SHARE_FLAG))" in handler
    assert "tag_multiplayer_advertisement(advertisement.reserved)" in handler
    assert "HALO_PORT_ADVERTISED_TAG_SHARE_FLAG)) != 0;" in \
        (ROOT / "source/networking/network_client_manager.c").read_text()
    server = (ROOT / "source/networking/network_server_manager.c").read_text()
    assert server.count("randomizer_show_prefix() || skulls_hosted() || tag_multiplayer_hosted()") == 2
    assert "skulls_hosted() || tag_multiplayer_hosted()" in (GAME / "skulls.c").read_text()
    share = (GAME / "map_share.h").read_text()
    kinds = re.findall(r"^\t(_map_share_file_[a-z]+|NUMBER_OF_MAP_SHARE_FILE_KINDS)",
                       share.split("_map_share_file_map = 0,", 1)[1], re.MULTILINE)
    assert kinds[-2:] == ["_map_share_file_tags", "NUMBER_OF_MAP_SHARE_FILE_KINDS"]
    assert len(kinds) <= 8


def test_engine_hooks():
    """The host's swaps where the engine spawns the map's items, before the
    gametype's no weapons on the map; and its loadout before a custom
    loadout's."""
    engine = (ROOT / "source/game/game_engine.c").read_text()
    spawn = engine.split("static void game_engine_update_item_spawn(\n\tvoid)\n{", 1)[1].split("\n}\n", 1)[0]
    assert spawn.index("tag_multiplayer_netgame_item(equipment_index, definition_index)") < \
        spawn.index("no_map_weapons")
    spawned = engine.split("void game_engine_postspawn_player_update(", 1)[1].split("\n}\n", 1)[0]
    assert spawned.index("tag_multiplayer_give_loadout(unit_index)") < spawned.index("_loadout_custom")
    randomizer = (GAME / "randomizer.c").read_text()
    loaded = randomizer.split("void randomizer_tags_loaded(char const *scenario_name)", 1)[1]
    assert loaded.index("tag_multiplayer_level_profile(scenario_name)") < loaded.index("campaign_leaf(")
    # (their tags before the menus' and the mods', as the game's own maps
    # load: cache_files.c, after a Custom Edition map's way, which plays none)
    cache = (ROOT / "source/cache/cache_files.c").read_text()
    own = cache.split("long scenario_tags_load(", 1)[1].split("custom_edition_cache_tags_load(", 1)[1]
    own = own.split("randomizer_tags_loaded(scenario_name);", 1)
    assert len(own) == 2 and "menu_tags_loaded(" in own[1]


def test_documented():
    docs = (ROOT / "docs/tag_injector.md").read_text()
    assert "## In multiplayer" in docs and "**MULTIPLAYER PROFILES**" in docs
    assert "_map_share_file_tags" in (ROOT / "port/linux/NETCODE.md").read_text()
