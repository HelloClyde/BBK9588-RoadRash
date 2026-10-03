"""Extract the Highway mountain CEL as compact 8-bit pixels and RGB565 LUT.

The second CEL resource in Highwayopt.rsrc is an unpacked, linear 8-bpp CEL.
Its pixel bytes hold a five-bit PLUT index and a three-bit intensity value.
"""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "local-data/3do-eu-extracted/Rash/Highway/Highwayopt.rsrc"
OUTPUT = ROOT / "local-data/road_backdrop_data.h"


def chunks(data: bytes):
    offset = 0
    while offset + 8 <= len(data):
        tag = data[offset : offset + 4]
        size = struct.unpack_from(">I", data, offset + 4)[0]
        if size < 8 or offset + size > len(data):
            raise ValueError("Invalid CEL chunk length")
        yield tag, data[offset : offset + size]
        offset += size
    if offset != len(data):
        raise ValueError("Trailing CEL bytes")


def extract(data: bytes) -> tuple[int, int, bytes, list[int]]:
    if data[:4] != b"RSRC":
        raise ValueError("Not a 3DO RSRC container")
    table = data.find(b"RTBL", 50)
    if table < 0:
        raise ValueError("Course resource table missing")
    count = struct.unpack_from(">I", data, table + 8)[0]
    cel_offset = cel_size = None
    for index in range(count):
        record = table + 16 + index * 32
        tag, number, offset, size = struct.unpack_from(">4I", data, record)
        if tag == int.from_bytes(b"CEL ", "big") and number == 2:
            cel_offset, cel_size = offset, size
            break
    if cel_offset is None or cel_offset + cel_size > len(data):
        raise ValueError("Highway mountain CEL not found")
    found = dict(chunks(data[cel_offset : cel_offset + cel_size]))
    if not all(tag in found for tag in (b"CCB ", b"PLUT", b"PDAT")):
        raise ValueError("Highway mountain CEL is incomplete")
    ccb = found[b"CCB "]
    width, height = struct.unpack_from(">II", ccb, 72)
    pre0 = struct.unpack_from(">I", ccb, 64)[0]
    if (width, height) != (360, 94) or (pre0 & 7) != 5:
        raise ValueError("Unexpected Highway mountain CEL format")
    plut = found[b"PLUT"]
    palette_count = struct.unpack_from(">I", plut, 8)[0]
    if palette_count != 32 or len(plut) != 12 + 2 * palette_count:
        raise ValueError("Unexpected Highway mountain PLUT")
    colors = struct.unpack_from(f">{palette_count}H", plut, 12)
    pixels = found[b"PDAT"][8:]
    if len(pixels) != width * height:
        raise ValueError("Unexpected Highway mountain pixel count")
    palette: list[int] = []
    for value in range(256):
        color = colors[value & 31]
        intensity = (value >> 5) + 1
        red = min(31, ((color >> 10) & 31) * intensity // 8)
        green = min(31, ((color >> 5) & 31) * intensity // 8)
        blue = min(31, (color & 31) * intensity // 8)
        palette.append((red << 11) | ((green << 1 | green >> 4) << 5) | blue)
    return width, height, pixels, palette


def rows(values, width: int = 24) -> str:
    return ",\n    ".join(
        ", ".join(str(value) for value in values[i : i + width])
        for i in range(0, len(values), width)
    )


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if SOURCE.exists():
        width, height, pixels, palette = extract(SOURCE.read_bytes())
        header = (
            "/* Generated locally from the user's 3DO Highway mountain CEL. */\n"
            f"#define ROAD_BACKDROP_WIDTH {width}\n"
            f"#define ROAD_BACKDROP_HEIGHT {height}\n"
            "static const unsigned char g_original_backdrop_indices[] = {\n"
            f"    {rows(pixels)}\n"
            "};\n"
            "static const road_pixel_t g_original_backdrop_palette[] = {\n"
            f"    {rows(palette, 16)}\n"
            "};\n"
        )
        print(f"Extracted Highway mountain: {width}x{height}, "
              f"{len(pixels)} 8-bit pixels")
    else:
        header = (
            "/* No local 3DO disc extracted; use generated test sky. */\n"
            "#define ROAD_BACKDROP_WIDTH 0\n"
            "#define ROAD_BACKDROP_HEIGHT 0\n"
        )
        print("No local 3DO resource found; building procedural test sky")
    OUTPUT.write_text(header, encoding="ascii")


if __name__ == "__main__":
    main()
