#!/usr/bin/env python3
"""Writes the campaign's scoring values (port/linux/game/campaign_score.c's)
from port/linux/game/campaign_score_values.json, as
port/linux/game/campaign_score_values.c:

    python3 tools/campaign_score_values.py [--check]
    python3 tools/campaign_score_values.py --pack maps/watanoge_randomizer.pack

configure.py runs it (write()) and lists the JSON among its inputs, so that
ninja reruns configure.py, and so this, when a value changes. --check fails
when the C file is not what the JSON makes (tools/test_campaign_score_values.py).

The JSON's actor types are actor_types.h's (elite, grunt, marine, ...) and
its skulls skulls.c's keys (iron, black_eye, ...): a name of neither is an
error, not a value that never scores.

--pack lists every actor variant (with its actor's type) and vehicle of the
campaign's maps, from the randomizer's tag pack (port/linux/game/tag_pack.c's,
format 2, which the game writes in maps/, or tools/tag_pack.py), with the base points the JSON gives each and
the rule that gives them: the words to match are the tags' names it shows."""
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
JSON_PATH = ROOT / "port/linux/game/campaign_score_values.json"
C_PATH = ROOT / "port/linux/game/campaign_score_values.c"
ACTOR_TYPES = ROOT / "source/ai/actor_types.h"
SKULLS_C = ROOT / "port/linux/game/skulls.c"
MEDALS_C = ROOT / "port/linux/game/campaign_medals.c"
MEDAL_WEAPONS = ("sniper", "shotgun", "flame", "grenade_stick", "splatter")
DIFFICULTIES = ("easy", "normal", "heroic", "legendary")
STYLES = ("regular", "one_shot_one_kill", "headshot", "melee", "backsmack")
COMBINES = ("multiply", "highest")


class ValuesError(Exception):
    pass


def actor_types() -> list:
    """actor_types.h's actor types, in its enum's order (elite first)"""
    text = ACTOR_TYPES.read_text()
    enum = re.search(r"_actor_elite\s*=\s*0,(.*?)NUMBER_OF_ACTOR_TYPES", text, re.S)
    if not enum:
        raise ValuesError(f"{ACTOR_TYPES}: no actor type enum")
    return ["elite"] + re.findall(r"_actor_(\w+)\s*,", enum.group(1))


def skull_keys() -> list:
    """skulls.c's skulls' keys, in their table's order"""
    text = SKULLS_C.read_text()
    table = re.search(r"skull_descriptions\[NUMBER_OF_SKULLS\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not table:
        raise ValuesError(f"{SKULLS_C}: no skull_descriptions table")
    return re.findall(r'\{\s*"([a-z_]+)",', table.group(1))


def medal_keys() -> list:
    """campaign_medals.c's medals' keys, in their table's order (the frames
    of the menus' medals/icons: tools/ce_menus.py)"""
    text = MEDALS_C.read_text()
    table = re.search(r"campaign_medal_descriptions\[NUMBER_OF_CAMPAIGN_MEDALS\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not table:
        raise ValuesError(f"{MEDALS_C}: no campaign_medal_descriptions table")
    return re.findall(r'\{\s*"([a-z_]+)",', table.group(1))


def _c_string(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _c_float(value) -> str:
    return f"{float(value)!r}f"


def _number(where: str, value, integer: bool = False):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValuesError(f"{where}: {value!r} is not a number")
    if integer and value != int(value):
        raise ValuesError(f"{where}: {value!r} is not a whole number of points")
    return int(value) if integer else float(value)


def _combine(where: str, value) -> int:
    if value not in COMBINES:
        raise ValuesError(f"{where}: combine is one of {', '.join(COMBINES)}, not {value!r}")
    return COMBINES.index(value)


def _par_seconds(level: str, minutes) -> int:
    """a par time, in minutes (15.5: 15:30), in whole seconds"""
    minutes = _number(f"time.par_times.{level}", minutes)
    if minutes <= 0:
        raise ValuesError(f"time.par_times.{level}: {minutes} minutes is no time")
    return int(round(minutes * 60))


class _Writer:
    """the C file's lines: its word lists and tables, then the values"""

    def __init__(self):
        self.lines = []
        self.names = 0

    def name(self, kind: str) -> str:
        self.names += 1
        return f"campaign_score_{kind}_{self.names}"

    def words(self, where: str, words) -> str:
        if isinstance(words, str):
            words = [words]
        if not isinstance(words, list) or not words or not all(isinstance(word, str) and word for word in words):
            raise ValuesError(f"{where}: match is a word or a list of words")
        name = self.name("words")
        self.lines.append(f"static char const *const {name}[] = {{ "
                          + ", ".join(_c_string(word.lower()) for word in words) + ", NULL };")
        return name

    def matches(self, where: str, entries, sign: int) -> tuple:
        if not isinstance(entries, list):
            raise ValuesError(f"{where}: a list of {{ match, points }}")
        rows = []
        for index, entry in enumerate(entries):
            here = f"{where}[{index}]"
            if not isinstance(entry, dict) or "match" not in entry or "points" not in entry:
                raise ValuesError(f"{here}: needs match and points")
            points = _number(f"{here}.points", entry["points"], integer=True)
            if sign * points < 0:
                raise ValuesError(f"{here}.points: {points} should be {'positive' if sign > 0 else 'negative'}")
            rows.append(f"\t{{ {self.words(here, entry['match'])}, {points} }},")
        if not rows:
            return "NULL", 0
        name = self.name("matches")
        self.lines.append(f"static struct campaign_score_match_value const {name}[] =\n{{")
        self.lines += rows
        self.lines.append("};")
        return name, len(rows)

    def actors(self, where: str, table, types: list, sign: int) -> tuple:
        if not isinstance(table, dict):
            raise ValuesError(f"{where}: an object of actor types")
        rows = []
        for actor, entry in table.items():
            if actor.startswith("_"):
                continue
            here = f"{where}.{actor}"
            if actor not in types:
                raise ValuesError(f"{here}: not an actor type (actor_types.h: {', '.join(types)})")
            if not isinstance(entry, dict) or "default" not in entry:
                raise ValuesError(f"{here}: needs a default")
            points = _number(f"{here}.default", entry["default"], integer=True)
            if sign * points < 0:
                raise ValuesError(f"{here}.default: {points} should be {'positive' if sign > 0 else 'negative'}")
            variants, count = self.matches(f"{here}.variants", entry.get("variants", []), sign)
            rows.append(f"\t{{ {types.index(actor)}, {points}, {count}, {variants} }}, /* {actor} */")
        if not rows:
            return "NULL", 0
        name = self.name("actors")
        self.lines.append(f"static struct campaign_score_actor_value const {name}[] =\n{{")
        self.lines += rows
        self.lines.append("};")
        return name, len(rows)


def generate() -> str:
    """the C file campaign_score_values.json makes"""
    try:
        values = json.loads(JSON_PATH.read_text())
    except json.JSONDecodeError as error:
        raise ValuesError(f"{JSON_PATH}: {error}")
    types = actor_types()
    skulls = skull_keys()
    out = _Writer()

    difficulty = values.get("difficulty", {})
    difficulties = [_number(f"difficulty.{name}", difficulty.get(name)) for name in DIFFICULTIES]

    time = values.get("time", {})
    brackets = time.get("brackets", [])
    if not brackets:
        raise ValuesError("time.brackets: at least one")
    bracket_rows = []
    last = -1.0
    for index, bracket in enumerate(brackets):
        up_to = bracket.get("up_to_par")
        multiplier = _number(f"time.brackets[{index}].multiplier", bracket.get("multiplier"))
        if up_to is None:
            if index != len(brackets) - 1:
                raise ValuesError(f"time.brackets[{index}]: only the last has no up_to_par")
            up_to = -1.0
        else:
            up_to = _number(f"time.brackets[{index}].up_to_par", up_to)
            if up_to <= last:
                raise ValuesError(f"time.brackets[{index}].up_to_par: {up_to} after {last}: in order, rising")
            last = up_to
        bracket_rows.append(f"\t{{ {_c_float(up_to)}, {_c_float(multiplier)} }},")
    out.lines.append("static struct campaign_score_time_bracket const campaign_score_time_brackets[] =\n{")
    out.lines += bracket_rows
    out.lines.append("};")
    par_difficulty = time.get("par_difficulty", {})
    par_difficulties = [_number(f"time.par_difficulty.{name}", par_difficulty.get(name, 1.0)) for name in DIFFICULTIES]
    daily_challenge_par = _number("time.daily_challenge_par", time.get("daily_challenge_par", 1.0))
    for where, value in [*zip((f"time.par_difficulty.{name}" for name in DIFFICULTIES), par_difficulties),
                         ("time.daily_challenge_par", daily_challenge_par)]:
        if value <= 0:
            raise ValuesError(f"{where}: {value} should be more than 0")
    par_rows = [f"\t{{ {_c_string(level.lower())}, {_par_seconds(level, text)} }},"
                for level, text in time.get("par_times", {}).items() if not level.startswith("_")]
    if par_rows:
        out.lines.append("static struct campaign_score_par_time const campaign_score_par_times[] =\n{")
        out.lines += par_rows
        out.lines.append("};")

    style = values.get("style", {})
    style_combine = _combine("style", style.get("combine", "multiply"))
    styles = [_number(f"style.{name}", style.get(name)) for name in STYLES]

    enemies, enemy_count = out.actors("enemies", values.get("enemies", {}), types, 1)
    vehicles, vehicle_count = out.matches("vehicles", values.get("vehicles", []), 1)
    penalties = values.get("penalties", {})
    penalty_points = [_number(f"penalties.{name}", penalties.get(name), integer=True)
                      for name in ("death", "betrayal", "checkpoint_revert")]
    for name, points in zip(("death", "betrayal", "checkpoint_revert"), penalty_points):
        if points > 0:
            raise ValuesError(f"penalties.{name}: {points} should be negative (or 0)")
    allies, ally_count = out.actors("penalties.allies", penalties.get("allies", {}), types, -1)
    ally_vehicles, ally_vehicle_count = out.matches("penalties.vehicles", penalties.get("vehicles", []), -1)

    skull_values = values.get("skulls", {})
    skull_combine = _combine("skulls", skull_values.get("combine", "multiply"))
    skull_default = _number("skulls.default", skull_values.get("default", 1.0))
    skull_rows = []
    for key, multiplier in skull_values.get("multipliers", {}).items():
        if key not in skulls:
            raise ValuesError(f"skulls.multipliers.{key}: not a skull (skulls.c: {', '.join(skulls)})")
        skull_rows.append(f"\t{{ {_c_string(key)}, {_c_float(_number(f'skulls.multipliers.{key}', multiplier))} }},")
    if skull_rows:
        out.lines.append("static struct campaign_score_skull_value const campaign_score_skull_values[] =\n{")
        out.lines += skull_rows
        out.lines.append("};")

    medals = values.get("medals", {})
    keys = medal_keys()
    medal_rows = []
    for key, points in medals.get("points", {}).items():
        if key.startswith("_"):
            continue
        if key not in keys:
            raise ValuesError(f"medals.points.{key}: not a medal (campaign_medals.c: {', '.join(keys)})")
        medal_rows.append(f"\t{{ {_c_string(key)}, {_number(f'medals.points.{key}', points, integer=True)} }},")
    if medal_rows:
        out.lines.append("static struct campaign_score_medal_value const campaign_score_medal_values[] =\n{")
        out.lines += medal_rows
        out.lines.append("};")
    weapons = medals.get("weapons", {})
    for name in weapons:
        if not name.startswith("_") and name not in MEDAL_WEAPONS:
            raise ValuesError(f"medals.weapons.{name}: not one of {', '.join(MEDAL_WEAPONS)}")
    weapon_lists = [out.words(f"medals.weapons.{name}", weapons[name]) if weapons.get(name) else "NULL"
                    for name in MEDAL_WEAPONS]
    medal_numbers = [
        _c_float(_number("medals.multi_kill_seconds", medals.get("multi_kill_seconds", 4))),
        str(_number("medals.perfection_kills", medals.get("perfection_kills", 15), integer=True)),
        _c_float(_number("medals.close_call_health", medals.get("close_call_health", 0.25))),
        str(_number("medals.history", medals.get("history", 8), integer=True)),
        _c_float(_number("medals.popup_seconds", medals.get("popup_seconds", 3))),
        "1" if medals.get("scaled", True) else "0",
    ]

    body = [
        "struct campaign_score_values const campaign_score_values =",
        "{",
        "\t{ " + ", ".join(_c_float(value) for value in difficulties) + " },",
        f"\t{len(bracket_rows)}, campaign_score_time_brackets,",
        f"\t{len(par_rows)}, {'campaign_score_par_times' if par_rows else 'NULL'},",
        "\t{ " + ", ".join(_c_float(value) for value in par_difficulties) + " },",
        f"\t{_c_float(daily_challenge_par)},",
        f"\t{style_combine},",
        *(f"\t{_c_float(value)}," for value in styles),
        f"\t{enemy_count}, {enemies},",
        f"\t{vehicle_count}, {vehicles},",
        *(f"\t{points}," for points in penalty_points),
        f"\t{ally_count}, {allies},",
        f"\t{ally_vehicle_count}, {ally_vehicles},",
        f"\t{skull_combine},",
        f"\t{_c_float(skull_default)},",
        f"\t{len(skull_rows)}, {'campaign_score_skull_values' if skull_rows else 'NULL'},",
        *(f"\t{value}," for value in medal_numbers),
        "\t{ " + ", ".join(weapon_lists) + " },",
        f"\t{len(medal_rows)}, {'campaign_score_medal_values' if medal_rows else 'NULL'},",
        "};",
    ]
    head = [
        "/*",
        "CAMPAIGN_SCORE_VALUES.C",
        "",
        "Written by tools/campaign_score_values.py from campaign_score_values.json:",
        "edit that, not this (configure.py, and so ninja, writes this again).",
        "*/",
        "",
        "#include <stddef.h>",
        "",
        '#include "campaign_score_values.h"',
        "",
    ]
    return "\n".join(head + out.lines + [""] + body) + "\n"


def write() -> bool:
    """the C file written, if it is not already what the JSON makes: TRUE
    when it was"""
    text = generate()
    if C_PATH.exists() and C_PATH.read_text() == text:
        return False
    C_PATH.write_text(text)
    return True


# ---------- --pack: the campaign's actors and vehicles, as the JSON scores them

PACK_WINDOW = 0x803A6000


def _fourcc(text: bytes) -> int:
    return struct.unpack(">I", text)[0]


def pack_tags(path: Path) -> tuple:
    """a tag pack's actor variants ({(actor type, tag name): levels})
    and vehicles ({tag name: levels})"""
    types = actor_types()
    variants, vehicles = {}, {}
    with open(path, "rb") as pack:
        magic, version, _content, count = struct.unpack("<IIII", pack.read(16))
        if magic != _fourcc(b"rpak") or version != 2:
            raise ValuesError(f"{path}: not a tag pack of format 2")
        maps = []
        for _ in range(count):
            name = pack.read(32).split(b"\0")[0].decode("ascii")
            _length, _offset, tag_size, tags_at, _spans, _resources = struct.unpack("<6I", pack.read(24))
            maps.append((name, tag_size, tags_at))
        for level, tag_size, tags_at in maps:
            pack.seek(tags_at)
            blob = pack.read(tag_size)

            def u32(offset):
                return struct.unpack_from("<I", blob, offset)[0]

            def at(address):
                offset = address - PACK_WINDOW
                return offset if 0 <= offset < len(blob) else None

            def name_at(address):
                offset = at(address)
                return blob[offset:blob.find(b"\0", offset)].decode("latin1") if offset is not None else "?"

            # (the tag index: a tag's group, its name and its data; an actor
            # variant's actor reference's index at 16, an actor's type at 20)
            instances = at(u32(0))
            table = [(u32(instances + index * 0x20), name_at(u32(instances + index * 0x20 + 0x10)),
                      at(u32(instances + index * 0x20 + 0x14)))
                     for index in range(struct.unpack_from("<i", blob, 0x0C)[0])]
            for group, tag, base in table:
                if group == _fourcc(b"actv") and base is not None:
                    actor = u32(base + 16) & 0xFFFF
                    actor_type = "none"
                    if actor < len(table) and table[actor][0] == _fourcc(b"actr") and table[actor][2] is not None:
                        value = struct.unpack_from("<h", blob, table[actor][2] + 20)[0]
                        actor_type = types[value] if 0 <= value < len(types) else str(value)
                    variants.setdefault((actor_type, tag), set()).add(level)
                elif group == _fourcc(b"vehi"):
                    vehicles.setdefault(tag, set()).add(level)
    return variants, vehicles


def _match_rule(entries, lower: str):
    for index, entry in enumerate(entries):
        words = [entry["match"]] if isinstance(entry["match"], str) else entry["match"]
        for word in words:
            if word.lower() in lower:
                return entry["points"], f"[{index}] '{word.lower()}'"
    return None


def _actor_rule(table: dict, actor_type: str, lower: str, where: str):
    entry = table.get(actor_type)
    if not entry:
        return None
    found = _match_rule(entry.get("variants", []), lower)
    if found:
        return found[0], f"{where}.{actor_type}.variants{found[1]}"
    return entry["default"], f"{where}.{actor_type}.default"


def list_pack(path: Path) -> None:
    """every actor variant and vehicle, with the JSON's base points"""
    values = json.loads(JSON_PATH.read_text())
    penalties = values.get("penalties", {})
    variants, vehicles = pack_tags(path)
    print(f"{'POINTS':>6}  {'TYPE':15} {'TAG':66} {'RULE':42} LEVELS")
    for (actor_type, tag), levels in sorted(variants.items()):
        lower = tag.lower()
        rule = (_actor_rule(values.get("enemies", {}), actor_type, lower, "enemies") or
                _actor_rule(penalties.get("allies", {}), actor_type, lower, "penalties.allies") or (0, "-"))
        print(f"{rule[0]:>6}  {actor_type:15} {tag:66} {rule[1]:42} {' '.join(sorted(levels))}")
    print()
    for tag, levels in sorted(vehicles.items()):
        lower = tag.lower()
        found = _match_rule(values.get("vehicles", []), lower)
        rule = (found[0], f"vehicles{found[1]}") if found else None
        if not rule:
            found = _match_rule(penalties.get("vehicles", []), lower)
            rule = (found[0], f"penalties.vehicles{found[1]}") if found else (0, "-")
        print(f"{rule[0]:>6}  {'vehicle':15} {tag:66} {rule[1]:42} {' '.join(sorted(levels))}")


def main() -> int:
    try:
        if "--pack" in sys.argv[1:]:
            arguments = sys.argv[1:]
            index = arguments.index("--pack")
            list_pack(Path(arguments[index + 1]) if index + 1 < len(arguments) else ROOT / "maps/watanoge_randomizer.pack")
            return 0
        if "--check" in sys.argv[1:]:
            if not C_PATH.exists() or C_PATH.read_text() != generate():
                print(f"{C_PATH} is not what {JSON_PATH.name} makes: run tools/campaign_score_values.py")
                return 1
            return 0
        print(f"{C_PATH}: {'written' if write() else 'unchanged'}")
    except ValuesError as error:
        print(f"campaign score values: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
