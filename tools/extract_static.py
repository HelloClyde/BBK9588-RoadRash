"""Decode Highway static debris and derive positions from RHZD weights.

FAM 148/149 share six static-object animations in their fourth family table.
Their second RPDT frames show the debris clearly. The original random spread
and difficulty filter are replaced with each template's midpoint so the
preview remains reproducible. RHZD effect entries are dynamic spawns and are
intentionally excluded.
"""

from pathlib import Path
import struct

from extract_course import RECORD_BYTES, SOURCE as COURSE_SOURCE, resource, trace_primary_route, u32
from extract_scenery import SOURCE as FAMILY_SOURCE, family, frame_from_table, rows


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "local-data/road_static_data.h"
FAMILY_ID = 148
STATIC_FAMILIES = (148, 149)
SPRITE_COUNT = 6


def child(data: bytes, parent: int, index: int) -> int:
    count = u32(data, parent)
    if count > 64 or index >= count:
        raise ValueError("Unexpected family resource tree")
    offset = parent + u32(data, parent + 4 + 4 * index)
    if offset + 8 > len(data):
        raise ValueError("Family child outside record")
    return offset


def static_sprites(data: bytes) -> list[tuple[int, int, list[int]]]:
    static_group = child(data, 0, 4)
    if u32(data, static_group) < SPRITE_COUNT:
        raise ValueError("FAM 148 static-object group incomplete")
    sprites = []
    for index in range(SPRITE_COUNT):
        entry = child(data, static_group, index)
        animation = child(data, entry, 0)
        table = child(data, entry, 1)
        if data[animation : animation + 4] != b"ANIM" or data[table : table + 4] != b"OFST":
            raise ValueError(f"FAM 148 static object {index} has unexpected layout")
        if u32(data, animation + 16) != 3:
            raise ValueError("Unexpected static-object animation frame count")
        sprites.append(frame_from_table(data, table, 1))
    return sprites


def segment_hazards(data: bytes, rsgs: int, segment: int,
                    sample_count: int) -> list[tuple[int, int, int, int, int]]:
    record = rsgs + 16 + RECORD_BYTES * segment
    schedule = rsgs + u32(data, record + 12)
    hazards = rsgs + u32(data, record + 28)
    if data[schedule : schedule + 4] != b"RRSM" or data[hazards : hazards + 4] != b"RHZD":
        raise ValueError(f"Segment {segment} lacks schedule or hazards")
    event_count = u32(data, schedule + 8)
    hazard_count = u32(data, hazards + 8)
    if u32(data, schedule + 4) != 24 + 12 * event_count or u32(data, hazards + 4) != 16 + 8 * hazard_count:
        raise ValueError(f"Invalid hazard table size in segment {segment}")
    events = [struct.unpack_from(">iBBHI", data, schedule + 24 + 12 * i)
              for i in range(event_count)]
    active: dict[int, int] = {}
    event_index = 0
    accumulated_sample = 0
    output = []
    for index in range(hazard_count):
        control, detail = struct.unpack_from(">II", data, hazards + 16 + 8 * index)
        accumulated_sample += (control >> 24) & 0x7F
        spread = (control >> 15) & 0x1E
        sample = accumulated_sample + spread // 2
        while event_index < len(events) and events[event_index][0] <= sample:
            _, state, group, _, family_id = events[event_index]
            if state == 1:
                active[group] = family_id
            event_index += 1
        selector = (control >> 8) & 255
        variant = selector & 63
        if (sample >= sample_count or control & 0x80000000 or
                variant >= SPRITE_COUNT or
                active.get(selector >> 6) not in STATIC_FAMILIES):
            continue
        lateral = detail & 0xFFF
        if lateral & 0x800:
            lateral -= 0x1000
        output.append((sample, lateral, variant, control & 15,
                       int(bool(control & 0x80))))
    return output


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if not COURSE_SOURCE.exists() or not FAMILY_SOURCE.exists():
        OUTPUT.write_text("#define ROAD_STATIC_COUNT 0\n", encoding="ascii")
        print("No local Highway resources; static objects disabled")
        return
    course = COURSE_SOURCE.read_bytes()
    families = FAMILY_SOURCE.read_bytes()
    family_data = family(families, FAMILY_ID)
    sprites = static_sprites(family_data)
    if static_sprites(family(families, 149)) != sprites:
        raise ValueError("FAM 148/149 static debris sprites differ")
    rsgs, _ = resource(course, b"RSGS")
    node, size = resource(course, b"RNOD")
    route = trace_primary_route(course, node, size)
    placements: list[tuple[int, int, int, int, int]] = []
    start = 0
    for segment, samples in route:
        placements.extend((start + sample, lateral, variant, visibility, mirror)
                          for sample, lateral, variant, visibility, mirror
                          in segment_hazards(course, rsgs, segment, samples))
        start += samples
    placements.sort(key=lambda item: item[0])
    lines = [
        "/* Generated locally from Highway RHZD and FAM 148 static frames. */",
        f"#define ROAD_STATIC_COUNT {len(placements)}",
        f"#define ROAD_STATIC_SPRITE_COUNT {len(sprites)}",
    ]
    for index, (width, height, pixels) in enumerate(sprites):
        lines.extend((f"static const road_pixel_t g_original_static_{index}[] = {{",
                      f"    {rows(pixels)}", "};"))
    lines.append("static const road_sprite_t g_original_static_sprites[] = {")
    lines.extend(f"    {{{width}u, {height}u, g_original_static_{index}}},"
                 for index, (width, height, _) in enumerate(sprites))
    lines.append("};")
    lines.append("static const road_static_placement_t g_original_static_placements[] = {")
    lines.extend(f"    {{{sample}u, {lateral}, {variant}u, {visibility}u, {mirror}u}},"
                 for sample, lateral, variant, visibility, mirror in placements)
    lines.append("};")
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"Extracted Highway FAM 148 static debris: {len(sprites)} sprites, "
          f"{len(placements)} placements")


if __name__ == "__main__":
    main()
