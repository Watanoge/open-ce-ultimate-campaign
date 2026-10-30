"""Create Game's LOCAL and Server Setup's BOTS, as tools/port_settings.py
writes them, the PC menus' files beside them (tools/ce_menus.py's, kept by
hand here), and the settings port/linux/game/menu_functions.c gives their
spinners."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import port_settings  # noqa: E402

MT = port_settings.MT
MENUS = ROOT / "port/assets/menus"
FUNCTIONS = ROOT / "port/linux/game/menu_functions.c"


def _strings(text: str, name: str) -> list:
    block = text.split(f'<strings name="{name}"', 1)[1].split("</strings>", 1)[0]
    return re.findall(r'<string text="([^"]*)"', block)


def test_create_local_item():
    """LOCAL is a row of Create Game's, after LAN, made as LAN's is, with its
    own words and description; the list's files have it where the generator
    puts it."""
    local = "\n".join(port_settings.multiplayer_files()["main_menu.multiplayer_type_select.local.xml"])
    item = f"{MT}/multiplayer_type_create_local_item"
    assert f'name="{item}"' in local
    assert 'run="join controller to mp game"' in local
    assert f'open="{MT}/connected/connected_map_select_wrapper"' in local

    menu = (MENUS / "ce/main_menu.multiplayer_type_select.xml").read_text()
    children = re.findall(r'<child widget="' + re.escape(MT) + r'/(multiplayer_type_[a-z_]+_item)"([^/]*)/>', menu)
    rows = [name for name, _ in children if name != "multiplayer_type_gametypes_item"]
    assert rows == port_settings.MULTIPLAYER_ROWS
    for name, attributes in children:
        if name == "multiplayer_type_gametypes_item":
            continue
        x, y = port_settings.multiplayer_row_place(name)
        assert f'y="{y}"' in attributes, name
        assert (f'x="{x}"' in attributes) == bool(x), name
    # (CO-OP's highlight ends above the divider and Edit Gametypes)
    assert port_settings.multiplayer_row_place("multiplayer_type_coop_item")[1] + 32 <= 349

    strings = (MENUS / "ce/strings.xml").read_text()
    options = _strings(strings, f"{MT}/multiplayer_options")
    assert options == [text.replace("\\n", "\n") for text in port_settings.STRING_OVERRIDES[f"{MT}/multiplayer_options"]]
    assert options[8] == "LOCAL" and 'string_index="8"' in local
    # (a description each for the rows that take events, in their order:
    # ui_widget_port_list_index)
    descriptions = _strings(strings, f"{MT}/multiplayer_option_descriptions")
    assert descriptions == port_settings.STRING_OVERRIDES[f"{MT}/multiplayer_option_descriptions"]
    taking_events = [row for row in rows if row not in port_settings.MULTIPLAYER_LABEL_ROWS]
    assert len(descriptions) == len(taking_events) + 1
    assert "bots" in descriptions[taking_events.index("multiplayer_type_create_local_item")]

    files = json.loads((MENUS / "menus.json").read_text())["files"]
    assert "ce/main_menu.multiplayer_type_select.local.xml" in files


def test_local_mode_hosts_alone():
    """The row's mode hosts a game no other machine joins."""
    functions = FUNCTIONS.read_text()
    assert 'strstr(name, "create_local") ? _multiplayer_mode_host_local' in functions
    assert "ui_widget_port_host_local(widget, event, widget_deleted)" in functions
    handlers = (ROOT / "source/interface/ui_widget_event_handler_functions.c").read_text()
    body = handlers.split("boolean ui_widget_port_host_local(", 1)[1].split("\n}\n", 1)[0]
    assert "network_game_accept_remote_connections(FALSE)" in body


def test_server_setup_bots():
    """Server Setup has BOTS in MAP SHARING's place, opening a screen whose
    spinners set bots.count, bots.skill and bots.teams, every choice of
    each one menu_functions.c knows, in the same order."""
    server = "\n".join(port_settings.multiplayer_files()["main_menu.multiplayer_type_select.server_settings.xml"])
    functions = FUNCTIONS.read_text()
    config = (ROOT / "port/linux/src/port_config.c").read_text()
    key, title, help_text, rows = port_settings.BOTS_SETUP
    base = f"{MT}/server_settings"

    assert f'<child widget="{base}/op_{key}" x="54" y="193"/>' in server
    assert f'<child widget="{base}/op_map_share_options" x="54" y="193"/>' in server
    assert f'open="{base}/{key}/{key}_screen"' in server
    assert f'"op_{key}"' in functions
    # (its value, what the screen chose, which menu_functions.c writes)
    assert f'<child widget="{base}/{key}_value" x="230" y="1"/>' in server
    assert f'name="{base}/{key}_value"' in server
    assert f'named(list, "{key}_value", 0)' in functions
    labels = _strings(server, f"{base}/labels")
    label_index = int(re.search(r'name="' + re.escape(f"{base}/{key}_label") + r'"[^>]* string_index="(\d+)"',
                                server).group(1))
    assert labels[label_index] == f"{title}:"
    helps = _strings(server, f"{base}/help_strings")
    help_index = int(re.search(r"#define BOTS_SETUP_HELP (\d+)", functions).group(1))
    assert helps[help_index] == help_text
    assert f"{base}/{key}/header_{key}" in port_settings.TITLES
    bitmaps = (MENUS / "ce/bitmaps.xml").read_text()
    png = f"ce/port/{base}/{key}/header_{key}.png"
    assert f'png="{png}"' in bitmaps and (MENUS / png).is_file()
    assert png in json.loads((MENUS / "menus.json").read_text())["files"]

    settings = {"bots_count": ("bots.count", [str(count) for count in port_settings.BOTS_COUNTS]),
                "bots_skill": ("bots.skill", port_settings.BOTS_SKILL_CHOICES),
                "bots_teams": ("bots.teams", ["even", "against"])}
    assert [row for row, *_ in rows] == list(settings)
    for row, label, words, row_helps in rows:
        setting, values = settings[row]
        assert len(words) == len(row_helps) == len(values), row
        assert f'name="{base}/{key}/{row}_spinner"' in server
        match = re.search(r'\{ "' + row + r'_spinner", _option_setting, 0, 0, (\d+), \{ 0 \}, "([a-z_.]+)",\s*'
                          r'\{ ([^}]*) \} \}', functions)
        assert match, row
        assert int(match.group(1)) == len(values) and match.group(2) == setting
        assert re.findall(r'"([^"]*)"', match.group(3)) == values
        assert f'{{ "{setting}", ' in config
        # (the help line's two lines at most: test_skulls_menu.py's)
        for text in row_helps:
            assert len(text.split("\\n")) <= 2 and len(text.replace("\\n", "")) <= 88, text
    # (the game's own skill levels, in its order: bots.c's table)
    bots = (ROOT / "port/linux/game/bots.c").read_text()
    table = bots.split("bots_skills[NUMBER_OF_BOTS_SKILLS] =", 1)[1].split("};", 1)[0]
    assert re.findall(r'^\t\{ "([a-z]+)",', table, re.MULTILINE) == port_settings.BOTS_SKILLS
    # (DISTRIBUTED first, bots.skill's default, which bots.c reads)
    assert port_settings.BOTS_SKILL_CHOICES[0] == "distributed"
    assert '{ "bots.skill", _config_string, "\\"distributed\\""' in config
    assert 'if (!strcmp(name, "distributed"))' in bots
    # (up to the player cap but the host: bots.h's BOTS_MAXIMUM)
    assert "#define BOTS_MAXIMUM (HALO_PORT_MAXIMUM_NETWORK_PLAYERS - 1)" in (ROOT / "port/linux/game/bots.h").read_text()
    players = int(re.search(r"#define HALO_PORT_MAXIMUM_NETWORK_PLAYERS (\d+)",
                            (ROOT / "port/linux/include/halo_port_limits.h").read_text()).group(1))
    assert max(port_settings.BOTS_COUNTS) == players - 1
