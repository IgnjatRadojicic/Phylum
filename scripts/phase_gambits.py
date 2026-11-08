"""Write the game's AI gambits into the server's phase XML.

The installer's recap_parser keeps none of a phase's gambits (it leaves an empty <gambit />, puts the
gambit count in <phaseType> and one stray name in <cGambitDefinition>). The real data sits in the
binary .Phase assets in AssetData_Binary.package. This reads them and rewrites every
data/serverdata/phase/<name>.Phase.xml it finds with the decoded gambits.

Layout (Darkspin content/sqlite/ai_decode.go, checked against all 171 phases of 5.3.0.103):
  header 16 bytes: gambit array pointer, count (+4), phase type (+8), start node (+12)
  gambit 52 bytes: condition hash (+0), condition (+12), condition props (+16, count at +20),
                   ability hash (+24), ability (+36), ability props (+40, count at +44), randomize (+48)
  a property 188 bytes: key (+0), name[80] (+4), type (+84), text[80] (+88)
  After the arrays, each set pointer is followed by its string or array, in field order.

Run again after the installer has rebuilt serverdata:
  python scripts/phase_gambits.py --game "<Darkspore folder>" [--package P] [--phases DIR]
"""
import argparse
import os
import struct
import sys
from xml.sax.saxutils import escape

DEFAULT_GAME = "C:/Program Files (x86)/Steam/steamapps/common/Darkspore"
PHASE_TYPE = 0x30728CE7


def fnv(text):
    value = 0x811C9DC5
    for byte in text.lower().encode():
        value = (value * 0x1000193) & 0xFFFFFFFF
        value ^= byte
    return value


def refpack(source, size):
    out = bytearray()
    position = 5
    while position < len(source):
        control = source[position]
        position += 1
        count = offset = 0
        if control >= 0xFC:
            literal = control & 3
            out += source[position:position + literal]
            break
        elif control >= 0xE0:
            literal = ((control & 0x1F) << 2) + 4
        elif control >= 0xC0:
            a, b, c = source[position:position + 3]
            position += 3
            literal = control & 3
            count = ((control & 0x0C) << 6) + c + 5
            offset = ((control & 0x10) << 12) + (a << 8) + b + 1
        elif control >= 0x80:
            a, b = source[position:position + 2]
            position += 2
            literal = a >> 6
            count = (control & 0x3F) + 4
            offset = ((a & 0x3F) << 8) + b + 1
        else:
            a = source[position]
            position += 1
            literal = control & 3
            count = ((control & 0x1C) >> 2) + 3
            offset = ((control & 0x60) << 3) + a + 1
        out += source[position:position + literal]
        position += literal
        for _ in range(count):
            out.append(out[-offset])
    if len(out) != size:
        raise ValueError(f"refpack: {len(out)} of {size} bytes")
    return bytes(out)


def phase_payloads(package):
    """{instance: payload} for every Phase in a DBPF 3 package."""
    with open(package, "rb") as handle:
        data = handle.read()
    if data[:4] != b"DBPF":
        raise ValueError("not a DBPF package")
    count = struct.unpack_from("<I", data, 0x24)[0]
    position = struct.unpack_from("<Q", data, 0x40)[0]
    flags = struct.unpack_from("<I", data, position)[0]
    position += 4
    shared = {}
    for bit, name in ((1, "type"), (2, "group"), (4, "high")):
        if flags & bit:
            shared[name] = struct.unpack_from("<I", data, position)[0]
            position += 4
    payloads = {}
    for _ in range(count):
        kind = shared.get("type")
        if kind is None:
            kind = struct.unpack_from("<I", data, position)[0]
            position += 4
        if "group" not in shared:
            position += 4
        if "high" not in shared:
            position += 4
        instance, offset, stored, size, compression, _flags = struct.unpack_from("<IIIIHH", data, position)
        position += 20
        if kind != PHASE_TYPE:
            continue
        raw = data[offset:offset + (stored & 0x7FFFFFFF)]
        payloads[instance] = refpack(raw, size) if compression == 0xFFFF else raw
    return payloads


class Cursor:
    def __init__(self, payload):
        self.payload = payload
        self.offset = 0

    def reserve(self, count, stride):
        start = self.offset
        self.offset += count * stride
        if self.offset > len(self.payload):
            raise ValueError("array past the end")
        return start

    def reference(self, field):
        if struct.unpack_from("<I", self.payload, field)[0] == 0:
            return None
        end = self.payload.index(b"\0", self.offset)
        text = self.payload[self.offset:end].decode("latin-1")
        self.offset = end + 1
        return text


def fixed(payload, offset, size):
    text = payload[offset:offset + size]
    end = text.find(b"\0")
    return (text[:end] if end >= 0 else text).decode("latin-1")


def properties(cursor, base):
    count = struct.unpack_from("<I", cursor.payload, base + 4)[0]
    start = cursor.reserve(count, 188)
    out = []
    for index in range(count):
        offset = start + index * 188
        out.append({
            "key": struct.unpack_from("<I", cursor.payload, offset)[0],
            "name": fixed(cursor.payload, offset + 4, 80),
            "type": struct.unpack_from("<I", cursor.payload, offset + 84)[0],
            "value": fixed(cursor.payload, offset + 88, 80),
        })
    return out


def decode_phase(payload):
    cursor = Cursor(payload)
    cursor.reserve(1, 16)
    count = struct.unpack_from("<I", payload, 4)[0]
    start = cursor.reserve(count, 52)
    phase = {"type": struct.unpack_from("<I", payload, 8)[0], "start": payload[12] != 0, "gambits": []}
    for index in range(count):
        base = start + index * 52
        gambit = {"condition": cursor.reference(base + 12)}
        gambit["conditionProps"] = properties(cursor, base + 16)
        gambit["ability"] = cursor.reference(base + 36)
        gambit["abilityProps"] = properties(cursor, base + 40)
        gambit["randomize"] = payload[base + 48] != 0
        phase["gambits"].append(gambit)
    if cursor.offset != len(payload):
        raise ValueError(f"{len(payload) - cursor.offset} bytes left over")
    return phase


def property_xml(tag, entries, indent):
    if not entries:
        return f"{indent}<{tag} />\n"
    lines = [f"{indent}<{tag}>\n"]
    for entry in entries:
        lines.append(f"{indent}\t<entry>\n")
        lines.append(f"{indent}\t\t<key>{entry['key']}</key>\n")
        lines.append(f"{indent}\t\t<name>{escape(entry['name'])}</name>\n")
        lines.append(f"{indent}\t\t<type>0x{entry['type']:08x}</type>\n")
        lines.append(f"{indent}\t\t<value>{escape(entry['value'])}</value>\n")
        lines.append(f"{indent}\t</entry>\n")
    lines.append(f"{indent}</{tag}>\n")
    return "".join(lines)


def phase_xml(phase):
    lines = ['<?xml version="1.0"?>\n', "<phase>\n"]
    if phase["gambits"]:
        lines.append("\t<gambit>\n")
        for gambit in phase["gambits"]:
            lines.append("\t\t<entry>\n")
            if gambit["condition"]:
                lines.append(f"\t\t\t<condition>{escape(gambit['condition'])}</condition>\n")
            lines.append(property_xml("conditionProps", gambit["conditionProps"], "\t\t\t"))
            if gambit["ability"]:
                lines.append(f"\t\t\t<ability>{escape(gambit['ability'])}</ability>\n")
            lines.append(property_xml("abilityProps", gambit["abilityProps"], "\t\t\t"))
            lines.append(f"\t\t\t<randomizeCooldown>{'true' if gambit['randomize'] else 'false'}</randomizeCooldown>\n")
            lines.append("\t\t</entry>\n")
        lines.append("\t</gambit>\n")
    else:
        lines.append("\t<gambit />\n")
    lines.append(f"\t<phaseType>{phase['type']}</phaseType>\n")
    lines.append(f"\t<startNode>{'true' if phase['start'] else 'false'}</startNode>\n")
    # The server reads <phaseType> as the real type only when this is here.
    lines.append("\t<gambitSource>binary</gambitSource>\n")
    lines.append("</phase>\n")
    return "".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--game", default=DEFAULT_GAME, help="the Darkspore install folder")
    parser.add_argument("--package", help="default: <game>/Data/AssetData_Binary.package")
    parser.add_argument("--phases", help="default: <game>/DarksporeBin/Server/data/serverdata/phase")
    arguments = parser.parse_args()
    arguments.package = arguments.package or os.path.join(arguments.game, "Data", "AssetData_Binary.package")
    arguments.phases = arguments.phases or os.path.join(arguments.game, "DarksporeBin", "Server", "data", "serverdata", "phase")

    payloads = phase_payloads(arguments.package)
    written = gambits = missing = 0
    for file in sorted(os.listdir(arguments.phases)):
        if not file.endswith(".Phase.xml"):
            continue
        name = file[:-len(".Phase.xml")]
        instance = int(name, 16) if name.lower().startswith("0x") else fnv(name)
        payload = payloads.get(instance)
        if payload is None:
            missing += 1
            print(f"no binary phase for {file}", file=sys.stderr)
            continue
        phase = decode_phase(payload)
        with open(os.path.join(arguments.phases, file), "w", encoding="ascii", newline="\n") as handle:
            handle.write(phase_xml(phase))
        written += 1
        gambits += len(phase["gambits"])
    print(f"phases written: {written}, gambits: {gambits}, without binary: {missing}")
    return 0 if written else 1


if __name__ == "__main__":
    sys.exit(main())
