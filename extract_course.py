"""Build an ignored Highway course header from the user's local 3DO disc.

RSGS holds the 26 segment records. RNOD describes their graph, including two
forks; this version follows each primary branch to the finish. No disc data
is committed to the repository.
"""

from __future__ import annotations

import json
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "local-data/3do-eu-extracted/Rash/Highway/Highwayopt.rsrc"
OUTPUT = ROOT / "local-data/road_course_data.h"
MANIFEST = ROOT / "local-data/highway_route.json"
RECORD_BYTES = 52
NODE_BYTES = 32


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def resource(data: bytes, tag: bytes) -> tuple[int, int]:
    offset = data.find(tag)
    if offset < 0 or offset + 12 > len(data):
        raise ValueError(f"{tag!r} resource not found")
    size = u32(data, offset + 4)
    if size < 12 or offset + size > len(data):
        raise ValueError(f"{tag!r} resource has invalid length")
    return offset, size


def read_path(data: bytes, rsgs: int, segment_index: int,
              segment_count: int) -> tuple[list[int], list[int]]:
    if not 0 <= segment_index < segment_count:
        raise ValueError(f"Invalid segment index {segment_index}")
    record = rsgs + 16 + RECORD_BYTES * segment_index
    offset = rsgs + u32(data, record + 4)
    if data[offset : offset + 4] != b"RPTH":
        raise ValueError(f"Segment {segment_index} lacks RPTH")
    size, count = struct.unpack_from(">II", data, offset + 4)
    if count < 2 or count > 10000 or size != 16 + count * 2:
        raise ValueError(f"Invalid RPTH header for segment {segment_index}")
    if offset + size > len(data):
        raise ValueError("Truncated RPTH resource")
    samples = struct.unpack_from(f">{count * 2}b", data, offset + 16)
    curvature = list(samples[0::2])
    elevation = list(samples[1::2])
    # The disc's start/end elevations are not always their raw step sum;
    # junction repair adjusts them at runtime. Curvature needs no such fix.
    return curvature, elevation


def trace_primary_route(data: bytes, node_base: int,
                        node_size: int) -> list[tuple[int, int]]:
    offset = u32(data, node_base + 16)
    seen: set[int] = set()
    route: list[tuple[int, int]] = []
    while True:
        if offset in seen:
            raise ValueError("Cycle in Highway's primary route")
        seen.add(offset)
        if offset < 20 or offset + 8 > node_size:
            raise ValueError(f"RNOD offset out of bounds: {offset}")
        kind = u32(data, node_base + offset)
        if kind == 4:  # reverse reference marks the finish
            if not route:
                raise ValueError("Empty Highway route")
            return route
        if kind not in (0, 1, 2) or offset + NODE_BYTES > node_size:
            raise ValueError(f"Unexpected RNOD node {kind} at {offset}")
        primary = u32(data, node_base + offset + 8)
        payload = u32(data, node_base + offset + 16)
        if kind == 0:  # segment leaf
            endpoint = u32(data, node_base + offset + 20)
            route.append((payload, endpoint))
            offset = primary
        elif kind == 1:  # forward fork: choose primary branch
            offset = primary
        else:  # reverse fork: payload is the forward continuation
            offset = payload


def extract_highway(data: bytes) -> tuple[list[int], list[int], list[dict]]:
    if data[:4] != b"RSRC":
        raise ValueError("Not a 3DO RSRC container")
    rsgs, rsgs_size = resource(data, b"RSGS")
    node, node_size = resource(data, b"RNOD")
    segment_count = u32(data, rsgs + 8)
    if segment_count < 1 or rsgs + 16 + segment_count * RECORD_BYTES > rsgs + rsgs_size:
        raise ValueError("Invalid RSGS segment table")
    route = trace_primary_route(data, node, node_size)
    curvature: list[int] = []
    elevation: list[int] = []
    manifest: list[dict] = []
    for segment_index, endpoint in route:
        path_curve, path_elevation = read_path(data, rsgs, segment_index,
                                               segment_count)
        if endpoint != len(path_curve):
            raise ValueError(f"RNOD/RPTH length mismatch for segment {segment_index}")
        curvature.extend(path_curve)
        elevation.extend(path_elevation)
        manifest.append({
            "segment": segment_index,
            "samples": endpoint,
            "end_sample": len(curvature),
        })
    return curvature, elevation, manifest


def format_array(values: list[int]) -> str:
    return ",\n    ".join(
        ", ".join(str(value) for value in values[i : i + 24])
        for i in range(0, len(values), 24)
    )


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if SOURCE.exists():
        curvature, elevation, route = extract_highway(SOURCE.read_bytes())
        header = (
            "/* Generated locally from the user's 3DO Highwayopt.rsrc. */\n"
            f"#define ROAD_COURSE_SAMPLE_COUNT {len(curvature)}\n"
            f"#define ROAD_COURSE_SEGMENT_COUNT {len(route)}\n"
            "static const signed char g_original_course_curvature[] = {\n"
            f"    {format_array(curvature)}\n"
            "};\n"
            "static const signed char g_original_course_elevation[] = {\n"
            f"    {format_array(elevation)}\n"
            "};\n"
        )
        MANIFEST.write_text(json.dumps({
            "course": "Highway",
            "route": "primary",
            "segment_count": len(route),
            "sample_count": len(curvature),
            "elevation_step_sum": sum(elevation),
            "segments": route,
        }, indent=2) + "\n", encoding="ascii")
        print(f"Extracted Highway primary route: {len(route)} segments, "
              f"{len(curvature)} curvature samples")
    else:
        header = (
            "/* No local 3DO disc extracted; use the generated test road. */\n"
            "#define ROAD_COURSE_SAMPLE_COUNT 0\n"
            "#define ROAD_COURSE_SEGMENT_COUNT 0\n"
        )
        print("No local 3DO resource found; building procedural test course")
    OUTPUT.write_text(header, encoding="ascii")


if __name__ == "__main__":
    main()
