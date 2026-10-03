"""Extract Highway RROB roadside positions using its RRSM family schedule.

Nine sprite families used by repeated objects are emitted. The original
lateral jitter, animation selector, collision bounds, and exact 3DO
perspective are still outside this portable preview renderer.
"""

from pathlib import Path
import struct

from extract_course import RECORD_BYTES, SOURCE, resource, trace_primary_route, u32
from extract_scenery import FAMILY_IDS


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "local-data/road_object_data.h"
ROAD_OBJECT_FAMILIES = FAMILY_IDS


def segment_objects(data: bytes, rsgs: int, segment: int,
                    count: int) -> list[tuple[int, int, int, int, int]]:
    record = rsgs + 16 + segment * RECORD_BYTES
    object_offset = rsgs + u32(data, record + 8)
    schedule_offset = rsgs + u32(data, record + 12)
    if data[object_offset : object_offset + 4] != b"RROB":
        raise ValueError(f"Segment {segment} lacks RROB")
    if data[schedule_offset : schedule_offset + 4] != b"RRSM":
        raise ValueError(f"Segment {segment} lacks RRSM")
    object_size, entry_count, sample_count = struct.unpack_from(
        ">III", data, object_offset + 4)
    schedule_size, schedule_count = struct.unpack_from(
        ">II", data, schedule_offset + 4)
    if (sample_count != count or object_size != 20 + 4 * entry_count or
            schedule_size != 24 + 12 * schedule_count):
        raise ValueError(f"Invalid Highway object data in segment {segment}")
    events = [struct.unpack_from(">iBBHI", data, schedule_offset + 24 + 12 * i)
              for i in range(schedule_count)]
    active: dict[int, int] = {}
    event_index = 0
    sample = 0
    result: list[tuple[int, int, int, int, int]] = []
    for entry_index in range(entry_count):
        packed = u32(data, object_offset + 20 + entry_index * 4)
        rows = (packed >> 24) & 15
        row_spacing = ((packed >> 12) & 15) + 1 if rows else 1
        end = min(count, sample + rows * row_spacing + 1)
        selector = (packed >> 16) & 255
        group = selector >> 6
        spacing_code = (packed & 15) if rows else (((packed >> 8) & 0xF0) + (packed & 15))
        wide = (packed >> 29) & 1
        spacing = (1024 + (spacing_code << (8 if rows else 5))) if wide else (
            spacing_code << (6 if rows else 3))
        scale = (packed >> 4) & 15
        for point in range(sample, end):
            while event_index < len(events) and events[event_index][0] <= point:
                _, state, family_group, _, family_id = events[event_index]
                # State 1 is a forward load. State 2 is the matching reverse
                # traversal boundary, not a forward unload.
                if state == 1:
                    active[family_group] = family_id
                event_index += 1
            if (point - sample) % row_spacing or (selector & 63) == 63:
                continue
            family_id = active.get(group)
            if family_id not in ROAD_OBJECT_FAMILIES:
                continue
            side = (packed >> 28) & 1
            if (packed >> 31) & 1 and (point - sample) % 2 == 0:
                side ^= 1
            columns = ((packed >> 8) & 7) + 1
            for column in range(columns):
                result.append((point, side, ROAD_OBJECT_FAMILIES.index(family_id),
                               scale, min(65535, spacing * (column + 1))))
        sample += rows * row_spacing + 1
        if sample >= count:
            break
    return result


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if not SOURCE.exists():
        OUTPUT.write_text("#define ROAD_OBJECT_COUNT 0\n", encoding="ascii")
        print("No Highway resources; using test scenery placement")
        return
    data = SOURCE.read_bytes()
    rsgs, _ = resource(data, b"RSGS")
    node, node_size = resource(data, b"RNOD")
    route = trace_primary_route(data, node, node_size)
    placements: list[tuple[int, int, int, int, int]] = []
    start = 0
    for segment, samples in route:
        placements.extend((start + sample, side, variant, scale, lateral)
                          for sample, side, variant, scale, lateral
                          in segment_objects(data, rsgs, segment, samples))
        start += samples
    placements.sort(key=lambda item: item[0])
    lines = [
        "/* Generated locally from the user's 3DO Highway RROB/RRSM. */",
        f"#define ROAD_OBJECT_COUNT {len(placements)}",
        "static const road_scenery_placement_t g_original_object_placements[] = {",
    ]
    lines.extend(f"    {{{sample}u, {side}u, {variant}u, {scale}u, {lateral}u}},"
                 for sample, side, variant, scale, lateral in placements)
    lines.append("};")
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"Extracted Highway roadside positions: {len(placements)} placements")


if __name__ == "__main__":
    main()
