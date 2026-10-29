"""The tag packs' manifests (port/assets/tag_packs), as the game reads them
(port/linux/src/port_tag_packs.c) and tools/tag_pack.py writes their packs."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import tag_pack  # noqa: E402

GAME = ROOT / "port/linux/game"


def test_manifests_are_named_and_sized_as_the_game_reads_them():
    """Each is <owner>_<what>.toml, named so inside, of 1 to 32 maps of at
    most 31 letters, wanted by forms the game knows, and its hash its own."""
    found = tag_pack.manifests()
    assert found
    conditions = re.findall(r'\{ "([a-z_]+)", ', (GAME / "tag_pack_conditions.c").read_text())
    hashes = set()
    for name, path in found.items():
        assert re.fullmatch(r"[a-z0-9]+_[a-z0-9_]+", name), name
        manifest = tag_pack.Manifest(path)
        assert 1 <= len(manifest.maps) <= tag_pack.MAXIMUM_MAPS
        assert all(len(leaf) < tag_pack.MAP_NAME for leaf in manifest.maps)
        for wanted in tag_pack.tomllib.loads(path.read_text()).get("wanted", []):
            assert wanted == "always" or wanted.startswith("setting:") or \
                (wanted.startswith("condition:") and wanted[10:] in conditions), (name, wanted)
        hashes.add(manifest.content())
    assert len(hashes) == len(found)


def test_the_game_names_packs_that_exist():
    """Every pack the game's code asks for by name has a manifest."""
    found = tag_pack.manifests()
    sources = [GAME / "randomizer.h", GAME / "game_modes.h", GAME / "tag_injector.h",
               ROOT / "port/linux/include/halo_skulls.h"]
    names = set()
    for source in sources:
        names.update(re.findall(r'#define [A-Z_]+_PACK "([a-z0-9_]+)"', source.read_text()))
    assert names and names <= set(found), names - set(found)


def test_stock_huds_pack_has_every_stock_hud_map():
    """The Tag Injector's stock HUDs are copied from their maps of the stock
    HUDs' pack: it has each map tag_injector.c names."""
    injector = (GAME / "tag_injector.c").read_text()
    table = injector.split("} const stock_huds[] =", 1)[1].split("};", 1)[0]
    maps = set(re.findall(r'"[^"]+", "[^"]+", "([a-z0-9_]+)" \}', table))
    manifest = tag_pack.Manifest(tag_pack.manifests()["watanoge_stock_huds"])
    assert maps and maps <= set(manifest.maps), maps - set(manifest.maps)


def test_keep_rules():
    """Includes narrow, excludes remove, by the tag's name in any case."""
    randomizer = tag_pack.Manifest(tag_pack.manifests()["watanoge_randomizer"])
    modes = tag_pack.Manifest(tag_pack.manifests()["watanoge_gamemodes"])
    assert randomizer.keep(tag_pack.BITM, "characters\\elite\\bitmaps\\elite")
    assert not randomizer.keep(tag_pack.BITM, "levels\\b30\\bitmaps\\rock")
    assert not randomizer.keep(tag_pack.SND, "Sound\\Music\\b30\\x")
    assert randomizer.keep(tag_pack.SND, "levels\\b30\\x")
    assert modes.keep(tag_pack.BITM, "characters\\floodcarrier\\bitmaps\\x")
    assert modes.keep(tag_pack.SND, "effects\\x")
    assert not modes.keep(tag_pack.BITM, "characters\\elite\\bitmaps\\elite")
