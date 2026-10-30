"""The replay recorder's machine (network.record_replays): the host sends a tick's state to a machine index of its own
past the session's (network_distributed.h's DISTRIBUTED_RECORDER_MACHINE), so that a game of every machine, the host
and 127 bots or 128 players, is recorded whole; before, the recorder took a free session slot, and a full game had
none (its replays held no datagrams and played black)."""

import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, function, mutated, read, run  # noqa: E402

CASES = ["constants", "full-game", "every-machine-a-client", "not-recording", "reliable", "no-players"]
NETCODE = ("distributed_machine_is_local", "distributed_recorder_find", "distributed_client_machines",
           "distributed_recorder_machine", "distributed_viewing_machines", "distributed_send_to_machine",
           "distributed_send_to_machine_reliably", "distributed_batch_flush", "distributed_machine_sees_player")

# a fault, the function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    "recorder-in-a-session-slot": (("? DISTRIBUTED_RECORDER_MACHINE : NONE", "? HALO_PORT_MAXIMUM_NETWORK_MACHINES - 1 : NONE"),
                                   "distributed_recorder_find", "full-game"),
    "recorder-not-sent-to": (("machine_index >= DISTRIBUTED_VIEWING_MACHINES)\n\t\treturn;",
                              "machine_index >= HALO_PORT_MAXIMUM_NETWORK_MACHINES)\n\t\treturn;"),
                             "distributed_send_to_machine", "full-game"),
    "recorder-batch-to-a-client": (("else if (sender == DISTRIBUTED_RECORDER_MACHINE)", "else if (FALSE)"),
                                   "distributed_batch_flush", "full-game"),
    "recorder-reliable-to-a-client": (("if (machine_index == DISTRIBUTED_RECORDER_MACHINE)", "if (FALSE)"),
                                      "distributed_send_to_machine_reliably", "reliable"),
    "player-list-past-the-table": (("DATUM_INDEX_TO_ABSOLUTE_INDEX(machine_index) >= MAXIMUM_NETWORK_MACHINE_COUNT)",
                                    "DATUM_INDEX_TO_ABSOLUTE_INDEX(machine_index) >= MAXIMUM_NETWORK_MACHINE_COUNT + 4)"),
                                   "machine_get_player_list", "no-players"),
}


def enum_member(source, name):
    """An enum member's line as the source has it ("NAME = expression,")."""
    match = re.search(r"^\s*(" + re.escape(name) + r"\s*=\s*[^,\n]+),", source, re.MULTILINE)
    if not match:
        raise LookupError(f"enum member not found: {name}")
    return match.group(1)


def generated(fault=None, faulty_function=None):
    netcode = read("port/linux/game/network_distributed.c")
    header = read("port/linux/game/network_distributed.h")
    players = read("source/game/players.c")
    limits = read("port/linux/include/halo_port_limits.h")
    config = ""
    for name in ("HALO_PORT_MAXIMUM_NETWORK_MACHINES", "HALO_PORT_MAXIMUM_NETWORK_PLAYERS"):
        config += f"#define {name} {constant(limits, name)}\n"
    config += f"#define MAXIMUM_LOCAL_PLAYERS {constant(read('source/game/players.h'), 'MAXIMUM_LOCAL_PLAYERS')}\n"
    config += "enum\n{\n"
    for source, name in ((header, "DISTRIBUTED_RECORDER_MACHINE"), (header, "DISTRIBUTED_VIEWING_MACHINES"),
                         (netcode, "MAXIMUM_SENDERS"), (netcode, "HOST_SENDER")):
        config += f"\t{enum_member(source, name)},\n"
    config += "};\n"
    text = ""
    for source, names in ((players, ("machine_get_player_list",)), (netcode, NETCODE)):
        for name in names:
            code = function(source, name)
            if fault and name == faulty_function:
                code = mutated(code, *fault)
            text += code + "\n"
    return (("config.inc", config), ("under_test.inc", text))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("replay_recorder", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, faulty_function, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("replay_recorder", generated(fault, faulty_function)), case)
    assert status == CHECK_FAILED, f"the code with a fault ({control}) passed '{case}': the test cannot see it"


def test_viewing_lists_hold_the_recorder():
    # every list of the machines a tick's state goes to has room for the recorder past the session's machines
    for path in ("port/linux/game/network_distributed.c", "port/linux/game/network_objects.c",
                 "port/linux/game/network_actors.c", "port/linux/game/network_damage.c"):
        source = read(path)
        # (its calls, not its definition)
        calls = re.findall(r"distributed_viewing_machines\((?!\s*long \*)[^;]*\)", source)
        assert calls, path
        for call in calls:
            assert "DISTRIBUTED_VIEWING_MACHINES" in call, f"{path}: {call}"
