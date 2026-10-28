"""The Tag Injector's campaign profiles (port/linux/game/tag_campaign.c,
docs/tag_injector.md "Campaign profiles"): their settings as the menu writes
them, the store makes them and the game reads them, the campaign's weapons
and enemies by the keys each names them by, and the randomizer's hooks
asking tag_campaign.c."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GAME = ROOT / "port/linux/game"
FUNCTIONS = GAME / "menu_functions.c"
TAG_CAMPAIGN = GAME / "tag_campaign.c"
RANDOMIZER = GAME / "randomizer.c"
STORE = ROOT / "port/linux/src/port_tag_mp_profiles.c"


def _c_strings(text: str, opener: str) -> list:
    """the first strings of each entry of a C array's initializer"""
    block = text.split(opener, 1)[1].split("};", 1)[0]
    return re.findall(r'\{ "([a-z_]*)"', block)


def _array(text: str, opener: str) -> list:
    return re.findall(r'"([a-z_]*)"', text.split(opener, 1)[1].split(";", 1)[0])


def test_settings_values():
    """ROTATION and REPLACEMENT's values, as the menu writes them and
    tag_campaign.c reads them, in the same order (the first each's default,
    as the store makes a new profile with)."""
    functions = FUNCTIONS.read_text()
    game = TAG_CAMPAIGN.read_text()
    defaults = _array(STORE.read_text(), "campaign_defaults[] =")
    for menu, read in (("tags_campaign_rotations[]", "rotations[]"), ("tags_campaign_replaces[]", "replaces[]")):
        menu_values = _c_strings(functions, f"static struct tags_choice const {menu} =")
        read_values = _array(game, f"static char const *const {read} =")
        assert menu_values == read_values
    rotations = _array(game, "static char const *const rotations[] =")
    replaces = _array(game, "static char const *const replaces[] =")
    assert defaults == ["weapon_rotation", rotations[0], "weapon_replace", replaces[0],
                        "enemy_rotation", rotations[0], "enemy_replace", replaces[0]]


def test_store_keys():
    """The store's settings and tables are those the menu and the game ask
    for by name."""
    store = STORE.read_text()
    options = _array(store, "campaign_option_keys[] =")
    sections = _array(store, "campaign_sections[] =")
    used = FUNCTIONS.read_text() + TAG_CAMPAIGN.read_text()
    assert options == ["profile", "weapon_rotation", "weapon_replace", "enemy_rotation", "enemy_replace"]
    assert sections == ["weapons", "enemies", "loadout"]
    for key in options + sections:
        assert f'"{key}"' in used, key


def test_stock_tables():
    """The campaign's weapons are the randomizer's weapon kinds (by their
    settings' names and paths), the pickups first; its enemies are the
    randomizer's types, in TYPE_* order."""
    game = TAG_CAMPAIGN.read_text()
    randomizer = RANDOMIZER.read_text()
    weapons = re.findall(r'\{ "([a-z_]+)", "[A-Z ]+", "(weapons\\\\[^"]+)" \}',
                         game.split("stock_weapons[TAG_CAMPAIGN_STOCK_WEAPONS] =", 1)[1].split("};", 1)[0])
    kinds = re.findall(r'\{ "(weapons\\\\[^"]+)", "([a-z_]+)", \d, \d, \d, \d, (\d) \}',
                       randomizer.split("weapon_kinds[WEAPON_KIND_COUNT] =", 1)[1].split("};", 1)[0])
    assert sorted(weapons) == sorted((key, tag) for tag, key, _ in kinds)
    pickups = int(re.search(r"TAG_CAMPAIGN_PICKUP_WEAPONS = (\d+)", (GAME / "tag_campaign.h").read_text()).group(1))
    actors_only = {key for _, key, only in kinds if only == "1"}
    assert {key for key, _ in weapons[pickups:]} == actors_only
    assert not actors_only & {key for key, _ in weapons[:pickups]}
    enemies = _c_strings(game.split("} const stock_enemies[TAG_CAMPAIGN_STOCK_ENEMIES] =", 1)[1], "{")
    types = ["GRUNT"] + re.findall(r"TYPE_([A-Z_]+),",
                                   randomizer.split("TYPE_GRUNT = 0,", 1)[1].split("TYPE_COUNT", 1)[0])
    assert enemies == [name.lower() for name in types]


def test_randomizer_hooks():
    """The randomizer's placements ask tag_campaign.c on a campaign
    profile's level, the enemies' weapons by the actor variant, and its
    level is played alone only."""
    randomizer = RANDOMIZER.read_text()
    for call in ("tag_campaign_level_profile()", "tag_campaign_level_loaded(", "tag_campaign_level_unloaded()",
                 "tag_campaign_actor_variant(", "tag_campaign_pickup(", "tag_campaign_enemy_weapon("):
        assert call in randomizer, call
    loaded = randomizer.split("void randomizer_tags_loaded(", 1)[1]
    assert "if (!game && !network_game_distributed_client() && !community_challenge_running())\n\t\t" \
        "campaign_profile = tag_campaign_level_profile();" in loaded
    actors = (ROOT / "source/ai/actors.c").read_text()
    assert "extern long randomizer_enemy_weapon(long, long, long, long);" in actors
    assert "long randomizer_enemy_weapon(long weapon_definition, long unit_index, long attempt, long variant_index);" \
        in (GAME / "randomizer.h").read_text()


def test_config_setting():
    config = (ROOT / "port/linux/src/port_config.c").read_text()
    assert '{ "game.campaign_tag_profile", _config_string, "\\"\\""' in config
    assert 'config_string("game.campaign_tag_profile")' in TAG_CAMPAIGN.read_text()
