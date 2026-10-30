#!/usr/bin/env python3
"""Embeds the high-res HUD textures (port/assets/hud, made by
tools/hud_assets.py), the menus' titles (port/assets/titles, made by
tools/title_assets.py), their controller button icons (port/assets/buttons,
made by tools/button_assets.py), the fonts the text is drawn with
(port/assets/fonts), the menus' files (port/assets/menus, made by
tools/ce_menus.py), the game's own sounds (port/assets/sounds, WAV files of
16-bit PCM), the tag packs' manifests (port/assets/tag_packs, TOML) and
SMAA's shader and lookup textures (port/third_party/smaa) in the game as C
data:

    python tools/embed_assets.py [--platform PORT] OUTPUT.c

writes OUTPUT.c with each PNG and the bitmap it stands for (its tag, index
and checksum of its pixels, from port/assets/hud/layout.json,
port/assets/titles/titles.json and port/assets/buttons/buttons.json), the
fonts and their tags from port/assets/fonts/fonts.json, the menu files
listed in port/assets/menus/menus.json, every .toml file of
port/assets/tag_packs, and SMAA's shader and lookup textures, as
port/linux/src/hud_hires.h, text_hires.h, menu_files.h, port_tag_packs.h and
xgpu_post.c declare them. With --platform, a port's own button art
(port/assets/buttons/platforms/PORT, made by tools/button_assets.py platform)
is embedded beside the common PNGs it has: one row each, with the contexts
its letters are right in and whose art it is (hud_hires.h), for
display.xbox_buttons to choose the port's or the Xbox's. The builds generate it
(hud_assets_build, called by tools/linux_build.py, windows_build.py and
android_build.py), so the assets are the committed source and Android needs
no files beside its guest image.

The data are 32-bit words, not bytes: the Android build passes the guest's
assembly through tools/android_asm_convert.py, which rewrites identifiers in
operands (it keeps .ascii strings as they are, but once garbled them).
"""

import json
import os
import struct
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

ROOT = Path(__file__).resolve().parent.parent
HUD_ASSETS = Path("port/assets/hud")
LAYOUT = HUD_ASSETS / "layout.json"
CE_LAYOUT = HUD_ASSETS / "ce_layout.json"
CE_STOCK = HUD_ASSETS / "ce_stock.json"
# the HUD textures whose bitmaps Halo Custom Edition lays out alike
CUSTOM_EDITION_HUD = HUD_ASSETS / "custom_edition.json"
TITLE_ASSETS = Path("port/assets/titles")
TITLE_LIST = TITLE_ASSETS / "titles.json"
BUTTON_ASSETS = Path("port/assets/buttons")
BUTTON_LIST = BUTTON_ASSETS / "buttons.json"
# a port's own button art (button_assets.py platform), by the port's name
BUTTON_PLATFORMS = BUTTON_ASSETS / "platforms"
CONTROLLER_ASSETS = Path("port/assets/controller_buttons")
CONTROLLER_LIST = CONTROLLER_ASSETS / "buttons.json"
FONT_ASSETS = Path("port/assets/fonts")
FONT_LIST = FONT_ASSETS / "fonts.json"
MENU_ASSETS = Path("port/assets/menus")
MENU_LIST = MENU_ASSETS / "menus.json"
# the tag packs' manifests, each maps/<name>.pack (port/linux/src/port_tag_packs.c)
TAG_PACK_ASSETS = Path("port/assets/tag_packs")
# SMAA's files and the names port/linux/src/xgpu_post.c declares them by
SMAA_ASSETS = Path("port/third_party/smaa")
SMAA_FILES = (("SMAA.hlsl", "xgpu_smaa_shader"), ("area_tex.zlib", "xgpu_smaa_area_texture"),
              ("search_tex.zlib", "xgpu_smaa_search_texture"))
# the desktop windows' icon (tools/app_icon.py; port/linux/src/sdl_platform.c)
WINDOW_ICON = Path("port/assets/icon/opence-icon-256.png")
# the game's own sounds, WAV files of 16-bit PCM (port/assets/sounds), and the
# names their makers declare them by (port/linux/game/skulls.c): Grunt
# Birthday Party's cheer; a skull to be found picked up, dropped by the enemy
# killed, and humming on the floor
SOUND_ASSETS = Path("port/assets/sounds")
SOUND_FILES = (("grunt_birthday_party.wav", "skulls_party_sound"), ("skull_sfx1.wav", "skulls_pickup_sound"),
               ("skull_sfx2.wav", "skulls_carrier_sound"), ("skull_sfx3.wav", "skulls_floor_sound"))
# the pointer the menus show in place of the system's (port/linux/src/sdl_platform.c)
MENU_CURSOR = Path("port/assets/cursor/menu_cursor.png")
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def font_files() -> List[str]:
    """The font files fonts.json uses, each once."""
    if not (ROOT / FONT_LIST).is_file():
        return []
    fonts = json.loads((ROOT / FONT_LIST).read_text())["fonts"]
    return sorted({font["file"] for font in fonts})


# hud_hires.h's HUD_HIRES_CONTEXT_* bits: where a picture's buttons are right
# (the Switch's menus with A and B swapped, its menus without, a game: all
# three), and whose art it is where a port has its own
CONTEXT_PLACES = 1 | 2 | 4
CONTEXT_PLATFORM_ART = 8
CONTEXT_XBOX_ART = 16


def platform_overrides(platform: Optional[str]) -> Dict[str, List[dict]]:
    """The port's own button art, by the common PNG's name: each PNG and its contexts."""
    listing = BUTTON_PLATFORMS / (platform or "") / "buttons.json"
    if not platform or not (ROOT / listing).is_file():
        return {}
    result: Dict[str, List[dict]] = {}
    for override in json.loads((ROOT / listing).read_text())["overrides"]:
        result.setdefault(override["name"], []).append(override)
    return result


def textures(platform: Optional[str] = None) -> List[tuple]:
    """The textures: each one's folder, its entry in its list, whether it
    is the menus' (a title or a button icon, drawn with the high-res text),
    its PNG's name and the contexts it is drawn in (0: all); a port's own
    button art beside the common PNG it stands for, each marked as whose."""
    result = []
    overrides = platform_overrides(platform)
    for folder, listing, title in ((HUD_ASSETS, LAYOUT, False), (TITLE_ASSETS, TITLE_LIST, True),
                                   (BUTTON_ASSETS, BUTTON_LIST, True), (HUD_ASSETS, CE_LAYOUT, False),
                                   (CONTROLLER_ASSETS, CONTROLLER_LIST, True)):
        if not (ROOT / listing).is_file():
            continue
        for asset in json.loads((ROOT / listing).read_text())["assets"]:
            if listing == BUTTON_LIST and asset["name"] in overrides:
                # (the port's art, and the Xbox's beside it, for display.xbox_buttons to choose:
                # hud_hires.h's HUD_HIRES_CONTEXT_PLATFORM_ART and _XBOX_ART)
                result += [(BUTTON_PLATFORMS / platform, asset, title, override["png"],
                            (override["contexts"] or CONTEXT_PLACES) | CONTEXT_PLATFORM_ART)
                           for override in overrides[asset["name"]]]
                result.append((folder, asset, title, f"{asset['name']}.png", CONTEXT_PLACES | CONTEXT_XBOX_ART))
            else:
                result.append((folder, asset, title, f"{asset['name']}.png", 0))
    return result


def menu_files() -> List[str]:
    """The menus' files menus.json lists, relative to their folder."""
    if not (ROOT / MENU_LIST).is_file():
        return []
    return json.loads((ROOT / MENU_LIST).read_text())["files"]


def tag_pack_files() -> List[str]:
    """The tag packs' manifests, by name."""
    if not (ROOT / TAG_PACK_ASSETS).is_dir():
        return []
    return sorted(path.name for path in (ROOT / TAG_PACK_ASSETS).glob("*.toml"))


def smaa_files() -> List[tuple]:
    """SMAA's files that the checkout has, with their symbols."""
    return [(name, symbol) for name, symbol in SMAA_FILES if (ROOT / SMAA_ASSETS / name).is_file()]


def platform_of(prefix: str) -> Optional[str]:
    """The port whose own button art a build embeds: its prefix, if it has some."""
    return prefix if (ROOT / BUTTON_PLATFORMS / prefix / "buttons.json").is_file() else None


def hud_asset_inputs(platform: Optional[str] = None) -> List[Path]:
    """The files the generated source is made from."""
    inputs = [listing for listing in (LAYOUT, CE_LAYOUT, CE_STOCK, CUSTOM_EDITION_HUD, TITLE_LIST, BUTTON_LIST, CONTROLLER_LIST, FONT_LIST, MENU_LIST)
              if (ROOT / listing).is_file()]
    if platform:
        inputs.append(BUTTON_PLATFORMS / platform / "buttons.json")
    return [*inputs, *(folder / png for folder, _, _, png, _ in textures(platform)),
            *(FONT_ASSETS / name for name in font_files()), *(MENU_ASSETS / name for name in menu_files()),
            *(TAG_PACK_ASSETS / name for name in tag_pack_files()),
            *(SMAA_ASSETS / name for name, _ in smaa_files()),
            *([WINDOW_ICON] if (ROOT / WINDOW_ICON).is_file() else []),
            *([MENU_CURSOR] if (ROOT / MENU_CURSOR).is_file() else []),
            *(SOUND_ASSETS / name for name, _ in SOUND_FILES if (ROOT / SOUND_ASSETS / name).is_file())]


def hud_configure_inputs() -> List[Path]:
    """What configure.py is run again for: the lists of assets (and the
    folders, for files added or removed), not each file, which a change of a
    list may rename or remove."""
    inputs = []
    for folder, listing in ((HUD_ASSETS, LAYOUT), (HUD_ASSETS, CE_LAYOUT), (TITLE_ASSETS, TITLE_LIST), (BUTTON_ASSETS, BUTTON_LIST),
                            (CONTROLLER_ASSETS, CONTROLLER_LIST),
                            (FONT_ASSETS, FONT_LIST), (MENU_ASSETS, MENU_LIST)):
        if (ROOT / listing).is_file():
            inputs += [folder, listing]
    if (ROOT / TAG_PACK_ASSETS).is_dir():
        inputs.append(TAG_PACK_ASSETS)
    if (ROOT / SMAA_ASSETS).is_dir():
        inputs.append(SMAA_ASSETS)
    if (ROOT / SOUND_ASSETS).is_dir():
        inputs.append(SOUND_ASSETS)
    if (ROOT / BUTTON_PLATFORMS).is_dir():
        inputs += [BUTTON_PLATFORMS, *(listing.relative_to(ROOT) for listing in sorted((ROOT / BUTTON_PLATFORMS).glob("*/buttons.json")))]
    return inputs


def build_number_argument() -> str:
    """the build number for the command line of the rule that embeds it
    (HALO_BUILD_NUMBER, which configure.py is run with for a release's build:
    tools/release.py; its regeneration keeps it), "" for none"""
    number = os.environ.get("HALO_BUILD_NUMBER", "")
    return f"--build-number {number} " if number.isdigit() else ""


def words(data: bytes) -> List[str]:
    """data as lines of 32-bit words (padded with zeros)."""
    padded = data + b"\0" * (-len(data) % 4)
    values = struct.unpack(f"<{len(padded) // 4}I", padded)
    return ["\t" + ", ".join(f"0x{word:08x}" for word in values[start:start + 8]) + ","
            for start in range(0, len(values), 8)]


def hud_assets_build(n: Any, prefix: str, output: Path) -> List[Path]:
    """Emits the rule that generates output; returns [output]. It is made
    whatever assets the checkout has (with none, its tables are empty), so
    that the symbols the platform layer refers to are always defined."""
    platform = platform_of(prefix)
    inputs = hud_asset_inputs(platform)
    n.rule(
        name=f"{prefix}_embed_assets",
        command="$python tools/embed_assets.py " + (f"--platform {platform} " if platform else "") +
        build_number_argument() + "$out",
        description=f"{prefix.upper()} EMBED $out",
    )
    n.build(outputs=output, rule=f"{prefix}_embed_assets",
            implicit=[Path("tools/embed_assets.py"), *inputs])
    return [output]


def server_ui_build(n: Any, prefix: str, output: Path) -> List[Path]:
    """Emits the rule that generates a dedicated server's admin page as C
    (tools/server_ui_embed.py, port/assets/server_ui); returns [output]"""
    ui = Path("port/assets/server_ui")
    inputs = sorted(path for path in (ROOT / ui).glob("*") if path.is_file())
    n.rule(
        name=f"{prefix}_server_ui",
        command="$python tools/server_ui_embed.py $out",
        description=f"{prefix.upper()} EMBED $out",
    )
    n.build(outputs=output, rule=f"{prefix}_server_ui",
            implicit=[Path("tools/server_ui_embed.py"), Path("tools/server_sim.py"),
                      *(ui / path.name for path in inputs)])
    return [output]


def png_size(data: bytes, name: str) -> tuple:
    """The width and height of an 8-bit RGBA, non-interlaced PNG (the only
    kind port/linux/src/hud_hires.c reads)."""
    if data[:8] != PNG_SIGNATURE or data[12:16] != b"IHDR":
        sys.exit(f"{name}: not a PNG")
    width, height, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", data[16:29])
    if depth != 8 or colour != 6 or interlace != 0:
        sys.exit(f"{name}: must be 8-bit RGBA and not interlaced")
    return width, height


def main() -> None:
    arguments = sys.argv[1:]
    platform = None
    build_number = 0
    if len(arguments) >= 3 and arguments[0] == "--platform":
        platform, arguments = arguments[1], arguments[2:]
    if len(arguments) >= 3 and arguments[0] == "--build-number" and arguments[1].isdigit():
        build_number, arguments = int(arguments[1]), arguments[2:]
    if len(arguments) != 1:
        sys.exit("usage: embed_assets.py [--platform PORT] [--build-number N] OUTPUT.c")
    lines = [
        "/* generated by tools/embed_assets.py from port/assets: do not edit */",
        "",
        '#include "hud_hires.h"',
        "",
    ]
    table = []
    custom_edition = {}
    if (ROOT / CUSTOM_EDITION_HUD).is_file():
        custom_edition = {(entry["tag"], entry["bitmap"]): entry
                          for entry in json.loads((ROOT / CUSTOM_EDITION_HUD).read_text())["assets"]}
    for index, (folder, asset, title, name, contexts) in enumerate(textures(platform)):
        data = (ROOT / folder / name).read_bytes()
        width, height = png_size(data, name)
        scale = asset["scale"]
        if (width, height) != (asset["width"] * scale, asset["height"] * scale):
            sys.exit(f"{name}: {width}x{height}, not {scale}x its bitmap's {asset['width']}x{asset['height']}")
        lines.append(f"static const unsigned int asset{index}[] = {{")
        lines.extend(words(data))
        lines.append("};")
        lines.append("")
        tag = asset["tag"].replace("\\", "\\\\")
        coverage = int(asset.get("coverage", False) or any(cell.get("kind") == "meter" for cell in asset.get("cells", [])))
        point_threshold = int(asset.get("point_threshold", False) or any(cell.get("thresholds") for cell in asset.get("cells", [])))
        # (the sequences whose sprites it stands for, a bit each of 32; 0: the whole bitmap)
        sequences = [sprite["sequence"] for sprite in asset.get("sprites", [])]
        if any(not 0 <= sequence < 32 for sequence in sequences):
            sys.exit(f"{name}: sprite sequences must be 0 to 31 (hud_hires.h: sprites)")
        sprites = sum(1 << sequence for sequence in sequences)
        ce = custom_edition.get((asset["tag"], asset["bitmap"]), {})
        if ce.get("replacement", asset["name"]) != asset["name"]:
            ce = {}
        table.append(f'\t{{ "{tag}", {asset["bitmap"]}, {width}, {height}, 0x{asset["crc"]:08x}u, {coverage}, '
                     f'{point_threshold}, {int(title)}, 0x{sprites:x}u, '
                     f'{int(bool(ce))}, {int(asset.get("ce_only", False))}, {ce.get("width", 0)}, {ce.get("height", 0)}, '
                     f'{ce.get("size", 0)}, 0x{ce.get("crc", 0):08x}u, asset{index}, {len(data)}, {contexts} }},')
    lines.append("const struct hud_hires_embedded hud_hires_embedded[] =")
    lines.append("{")
    lines.extend(table)
    if not table:
        lines.append("\t{ 0 },")
    lines.append("};")
    lines.append(f"const unsigned int hud_hires_embedded_count = {len(table)};")
    stock = json.loads((ROOT / CE_STOCK).read_text())["bitmaps"]
    lines.append("const struct hud_hires_ce_stock hud_hires_ce_stock[] = {")
    for item in stock:
        tag = item['tag'].replace('\\', '\\\\')
        lines.append(f'\t{{ "{tag}", {item["bitmap"]}, {item["width"]}, {item["height"]}, '
                     f'{item["size"]}, 0x{item["crc"]:08x}u }},')
    lines.append("};")
    lines.append(f"const unsigned int hud_hires_ce_stock_count = {len(stock)};")
    lines.append("")
    # the fonts, and which draws each font tag (text_hires.h)
    lines.append('#include "text_hires.h"')
    lines.append("")
    files = font_files()
    for index, name in enumerate(files):
        lines.append(f"static const unsigned int font{index}[] = {{")
        lines.extend(words((ROOT / FONT_ASSETS / name).read_bytes()))
        lines.append("};")
        lines.append("")
    fonts = json.loads((ROOT / FONT_LIST).read_text())["fonts"] if files else []
    lines.append("const struct text_hires_embedded text_hires_embedded[] =")
    lines.append("{")
    for font in fonts:
        index = files.index(font["file"])
        tag = font["tag"].replace("\\", "\\\\")
        size = (ROOT / FONT_ASSETS / font["file"]).stat().st_size
        lines.append(f'\t{{ "{tag}", "{font["file"]}", font{index}, {size} }},')
    if not fonts:
        lines.append("\t{ 0 },")
    lines.append("};")
    lines.append(f"const unsigned int text_hires_embedded_count = {len(fonts)};")
    lines.append("")
    # the menus' files (menu_files.h)
    lines.append('#include "menu_files.h"')
    lines.append("")
    menus = menu_files()
    for index, name in enumerate(menus):
        data = (ROOT / MENU_ASSETS / name).read_bytes()
        if name.endswith(".png"):
            png_size(data, name)
        lines.append(f"static const unsigned int menu{index}[] = {{")
        lines.extend(words(data))
        lines.append("};")
        lines.append("")
    lines.append("const struct menu_file_embedded menu_files_embedded[] =")
    lines.append("{")
    for index, name in enumerate(menus):
        size = (ROOT / MENU_ASSETS / name).stat().st_size
        lines.append(f'\t{{ "{name}", menu{index}, {size} }},')
    if not menus:
        lines.append("\t{ 0 },")
    lines.append("};")
    lines.append(f"const unsigned int menu_files_embedded_count = {len(menus)};")
    lines.append("")
    # the tag packs' manifests (port_tag_packs.h), their text as it is
    lines.append('#include "port_tag_packs.h"')
    lines.append("")
    manifests = tag_pack_files()
    for index, name in enumerate(manifests):
        lines.append(f"static const unsigned int tag_pack{index}[] = {{")
        lines.extend(words((ROOT / TAG_PACK_ASSETS / name).read_bytes()))
        lines.append("};")
        lines.append("")
    lines.append("const struct tag_pack_manifest_embedded tag_pack_manifests_embedded[] =")
    lines.append("{")
    for index, name in enumerate(manifests):
        size = (ROOT / TAG_PACK_ASSETS / name).stat().st_size
        lines.append(f'\t{{ "{name}", tag_pack{index}, {size} }},')
    if not manifests:
        lines.append("\t{ 0 },")
    lines.append("};")
    lines.append(f"const unsigned int tag_pack_manifests_embedded_count = {len(manifests)};")
    lines.append("")
    # the sounds (each of size 0 that the checkout does not have), on every
    # renderer
    for name, symbol in SOUND_FILES:
        path = ROOT / SOUND_ASSETS / name
        data = path.read_bytes() if path.is_file() else b""
        lines.append("")
        lines.append(f"const unsigned int {symbol}[] = {{")
        lines.extend(words(data) if data else ["\t0,"])
        lines.append("};")
        lines.append(f"const unsigned long {symbol}_size = {len(data)};")
    lines.append("")
    # this build's number (community.c's: the main menu's news says whether
    # a newer release is out), 0 for a build made without one
    lines.append(f"const int halo_build_number = {build_number};")
    lines.append("")
    lines.append("#ifndef HALO_GLES")
    # SMAA's shader, as text a GLSL compiler takes (ASCII, ending in a NUL),
    # and its lookup textures (xgpu_post.c); each of size 0 that the
    # checkout does not have. The OpenGL ES renderer has no SMAA.
    present = dict(smaa_files())
    for name, symbol in SMAA_FILES:
        data = (ROOT / SMAA_ASSETS / name).read_bytes() if name in present else b""
        if data and name.endswith(".hlsl"):
            data = bytes(byte if byte < 0x80 else 0x20 for byte in data) + b"\0"
        lines.append("")
        lines.append(f"const unsigned int {symbol}[] = {{")
        lines.extend(words(data) if data else ["\t0,"])
        lines.append("};")
        lines.append(f"const unsigned long {symbol}_size = {len(data)};")
    # the windows' icon, a PNG (of size 0 where the checkout has none)
    data = (ROOT / WINDOW_ICON).read_bytes() if (ROOT / WINDOW_ICON).is_file() else b""
    lines.append("")
    lines.append("const unsigned int platform_window_icon[] = {")
    lines.extend(words(data) if data else ["\t0,"])
    lines.append("};")
    lines.append(f"const unsigned long platform_window_icon_size = {len(data)};")
    # the menus' pointer, a PNG (of size 0 where the checkout has none)
    data = (ROOT / MENU_CURSOR).read_bytes() if (ROOT / MENU_CURSOR).is_file() else b""
    lines.append("")
    lines.append("const unsigned int platform_menu_cursor[] = {")
    lines.extend(words(data) if data else ["\t0,"])
    lines.append("};")
    lines.append(f"const unsigned long platform_menu_cursor_size = {len(data)};")
    lines.append("")
    lines.append("#endif")
    output = Path(arguments[0])
    output.parent.mkdir(parents=True, exist_ok=True)
    text = "\n".join(lines) + "\n"
    if not output.is_file() or output.read_text() != text:
        output.write_text(text)


if __name__ == "__main__":
    main()
