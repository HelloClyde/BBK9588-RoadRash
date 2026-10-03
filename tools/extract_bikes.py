"""Decode selected rear-view motorcycle frames from the local 3DO image.

Rash/rashOpt.rsrc ANIM 2 contains the player's motorcycle, and ANIM 3 has a
dark opponent motorcycle. These new-style RPDT frames hold two dimension
indices followed by an embedded PRE0 word and packed, coded 8-bit rows.
"""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "local-data/3do-eu-extracted/Rash/rashOpt.rsrc"
OUTPUT = ROOT / "local-data/road_bike_data.h"
EXTRA_FRAMES = (1, 2, 5, 6, 13, 14, 18, 19, 319,
                241, 242, 243, 244, 245, 246, 247,
                282, 283, 284, 285, 286, 289,
                303, 304, 305, 306, 307, 308,
                309, 310, 311, 312, 313,
                213, 214, 215, 216, 217, 219,
                225, 226, 227, 228, 229, 230,
                1, 31, 32)
FRAMES = ((2, 3), (2, 20), (2, 0)) + tuple(
    (2, index) for index in range(100, 118)) + ((3, 0),) + tuple(
    (3 if offset >= len(EXTRA_FRAMES) - 3 else 2, index)
    for offset, index in enumerate(EXTRA_FRAMES))
TRANSPARENT = 0xF81F


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def animation(data: bytes, number: int) -> bytes:
    if data[:4] != b"RSRC":
        raise ValueError("Not a 3DO RSRC container")
    table = data.find(b"RTBL", 40)
    if table < 0:
        raise ValueError("Rash resource table missing")
    count = u32(data, table + 8)
    for index in range(count):
        record = table + 16 + 32 * index
        tag, ident, offset, size = struct.unpack_from(">4I", data, record)
        if tag == int.from_bytes(b"ANIM", "big") and ident == number:
            if offset + size > len(data):
                raise ValueError("Animation record exceeds file")
            return data[offset : offset + size]
    raise ValueError(f"ANIM {number} missing")


def frame(data: bytes, number: int, animation_number: int) -> tuple[int, int, list[int]]:
    ccb, plut = data.find(b"CCB "), data.find(b"PLUT")
    if min(ccb, plut) < 0:
        raise ValueError("Motorcycle animation lacks CCB or PLUT")
    if u32(data, ccb + 64) & 7 != 5 or u32(data, ccb + 12) & 0x200 == 0:
        raise ValueError("Expected packed, coded 8-bit motorcycle CEL")
    if u32(data, plut + 8) != 32:
        raise ValueError("Expected 32-color motorcycle PLUT")
    colors = struct.unpack_from(">32H", data, plut + 12)
    divisor_bits = u32(data, ccb + 60) & 0x300
    divisor = {0x000: 16, 0x100: 2, 0x200: 4, 0x300: 8}[divisor_bits]
    frame_offsets: list[int] = []
    pos = 0
    while (pos := data.find(b"RPDT", pos)) >= 0:
        frame_offsets.append(pos)
        pos += 4
    if number >= len(frame_offsets):
        raise ValueError("Motorcycle frame missing")
    start = frame_offsets[number]
    size = u32(data, start + 4)
    end = start + size
    if size < 24 or end > len(data):
        raise ValueError("Invalid motorcycle RPDT length")
    width = u32(data, start + 8)
    pre0 = u32(data, start + 16)
    height = ((pre0 >> 6) & 0x3FF) + 1
    if not (1 <= width <= 128 and 1 <= height <= 128):
        raise ValueError("Unexpected motorcycle frame dimensions")
    pixels = data[start + 20 : end]

    def read_bits(bit: int, count: int) -> tuple[int, int]:
        value = 0
        for _ in range(count):
            if bit // 8 >= len(pixels):
                raise ValueError("Packed motorcycle packet exceeds frame")
            value = (value << 1) | ((pixels[bit // 8] >> (7 - bit % 8)) & 1)
            bit += 1
        return value, bit

    decoded: list[int] = []
    offset = 0
    for _ in range(height):
        bit = offset * 8
        row_words, bit = read_bits(bit, 16)
        offset += (row_words + 2) * 4
        row: list[int] = []
        while len(row) < width:
            kind, bit = read_bits(bit, 2)
            if kind == 0:
                break
            run, bit = read_bits(bit, 6)
            run += 1
            if kind == 1:
                for _ in range(run):
                    value, bit = read_bits(bit, 8)
                    row.append(value)
            elif kind == 2:
                row.extend([-1] * run)
            else:
                value, bit = read_bits(bit, 8)
                row.extend([value] * run)
        decoded.extend((row[:width] + [-1] * width)[:width])

    output: list[int] = []
    for value in decoded:
        if value < 0:
            output.append(TRANSPARENT)
            continue
        color = colors[value & 31]
        intensity = (value >> 5) + 1
        red = min(31, ((color >> 10) & 31) * intensity // divisor)
        green = min(31, ((color >> 5) & 31) * intensity // divisor)
        blue = min(31, (color & 31) * intensity // divisor)
        rgb565 = red << 11 | ((green << 1 | green >> 4) << 5) | blue
        if rgb565 == TRANSPARENT:
            raise ValueError("Transparent key collides with motorcycle color")
        # This dark opponent frame uses PLUT black for its clear pixels.
        output.append(TRANSPARENT if animation_number == 3 and rgb565 == 0
                      else rgb565)
    return width, height, output


def rows(values: list[int], width: int = 20) -> str:
    return ",\n    ".join(
        ", ".join(str(value) for value in values[i : i + width])
        for i in range(0, len(values), width)
    )


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if not SOURCE.exists():
        OUTPUT.write_text(
            "/* No local 3DO disc extracted; use procedural motorcycles. */\n"
            "#define ROAD_BIKE_COUNT 0\n", encoding="ascii"
        )
        print("No local 3DO motorcycle resource found; using procedural bikes")
        return
    data = SOURCE.read_bytes()
    animations = {number: animation(data, number) for number, _ in FRAMES}
    sprites = [frame(animations[number], index, number)
               for number, index in FRAMES]
    lines = [
        "/* Generated locally from the user's 3DO Rash/rashOpt.rsrc. */",
        f"#define ROAD_BIKE_COUNT {len(sprites)}",
    ]
    for slot, ((number, index), (width, height, pixels)) in enumerate(
        zip(FRAMES, sprites)
    ):
        lines.extend((
            f"static const road_pixel_t g_original_bike_{slot}[] = {{",
            f"    {rows(pixels)}",
            "};",
        ))
        print(f"Extracted ANIM {number} frame {index}: {width}x{height}")
    lines.append("static const road_sprite_t g_original_bikes[] = {")
    for slot, (width, height, _) in enumerate(sprites):
        lines.append(f"    {{{width}u, {height}u, g_original_bike_{slot}}},")
    lines.append("};")
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="ascii")


if __name__ == "__main__":
    main()
