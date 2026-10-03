"""Decode Highway repeated roadside sprites from the user's local 3DO disc.

The chosen FAM sprites are packed, coded 4/6-bit CEL frames. Each packed row
begins with an 8-bit length in 32-bit words; packets then contain a 2-bit
kind and a 6-bit run length. The generated header stays in ignored local-data.
"""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "local-data/3do-eu-extracted/Rash/Families.RSRC"
OUTPUT = ROOT / "local-data/road_scenery_data.h"
FAMILY_IDS = (101, 102, 148, 149, 169, 171, 174, 175, 240)
TRANSPARENT = 0xF81F


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def family(data: bytes, number: int) -> bytes:
    if data[:4] != b"RSRC":
        raise ValueError("Not a 3DO RSRC container")
    table = data.find(b"RTBL", 40)
    if table < 0:
        raise ValueError("Family resource table missing")
    count = u32(data, table + 8)
    for index in range(count):
        record = table + 16 + 32 * index
        tag, ident, offset, size = struct.unpack_from(">4I", data, record)
        if tag == int.from_bytes(b"FAM ", "big") and ident == number:
            if offset + size > len(data):
                raise ValueError("Family record exceeds file")
            return data[offset : offset + size]
    raise ValueError(f"Family {number} missing")


def chunks_from_table(data: bytes, table: int) -> tuple[dict[bytes, int], list[int]]:
    if table < 0:
        raise ValueError("Family animation offset table missing")
    first_offset = u32(data, table + 4)
    if first_offset < 8 or first_offset > len(data) - table:
        raise ValueError("Invalid family animation table")
    chunks: dict[bytes, int] = {}
    offsets: list[int] = []
    for pos in range(table + 4, table + first_offset, 4):
        offset = table + u32(data, pos)
        if offset + 8 > len(data):
            raise ValueError("Family chunk outside record")
        chunks.setdefault(data[offset : offset + 4], offset)
        offsets.append(offset)
    return chunks, offsets


def decode_pixels(pixels: bytes, width: int, height: int,
                  bits_per_pixel: int, colors: tuple[int, ...],
                  divisor: int = 0) -> list[int]:
    if not (1 <= width <= 128 and 1 <= height <= 128):
        raise ValueError("Unexpected sprite dimensions")
    if len(colors) not in (16, 32):
        raise ValueError("Expected a 16/32-color PLUT")

    def read_bits(bit: int, count: int) -> tuple[int, int]:
        value = 0
        for _ in range(count):
            if bit // 8 >= len(pixels):
                raise ValueError("Packed CEL packet exceeds pixel chunk")
            value = (value << 1) | ((pixels[bit // 8] >> (7 - bit % 8)) & 1)
            bit += 1
        return value, bit

    decoded: list[int] = []
    offset = 0
    for _ in range(height):
        bit = offset * 8
        row_words, bit = read_bits(bit, 16 if bits_per_pixel == 8 else 8)
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
                    value, bit = read_bits(bit, bits_per_pixel)
                    row.append(value)
            elif kind == 2:
                row.extend([-1] * run)
            else:
                value, bit = read_bits(bit, bits_per_pixel)
                row.extend([value] * run)
        decoded.extend((row[:width] + [-1] * width)[:width])
    if offset != len(pixels):
        raise ValueError("Packed CEL row lengths do not fill pixel chunk")

    output: list[int] = []
    for value in decoded:
        if value < 0:
            output.append(TRANSPARENT)
            continue
        color = colors[value & (len(colors) - 1)]
        red, green, blue = (color >> 10) & 31, (color >> 5) & 31, color & 31
        if bits_per_pixel == 8:
            intensity = (value >> 5) + 1
            red = min(31, red * intensity // divisor)
            green = min(31, green * intensity // divisor)
            blue = min(31, blue * intensity // divisor)
        rgb565 = red << 11 | ((green << 1 | green >> 4) << 5) | blue
        if rgb565 == TRANSPARENT:
            raise ValueError("Transparent key collides with sprite color")
        output.append(rgb565)
    return output


def frame_from_table(data: bytes, table: int,
                     frame_number: int = 0) -> tuple[int, int, list[int]]:
    chunks, offsets = chunks_from_table(data, table)
    if not all(tag in chunks for tag in (b"CCB ", b"PLUT", b"RPDT")):
        raise ValueError("Family first frame is incomplete")
    ccb, plut = chunks[b"CCB "], chunks[b"PLUT"]
    frames = [offset for offset in offsets if data[offset : offset + 4] == b"RPDT"]
    if frame_number >= len(frames):
        raise ValueError("Family frame index outside offset table")
    frame = frames[frame_number]
    bpp_code = u32(data, ccb + 64) & 7
    bits_per_pixel = {3: 4, 4: 6, 5: 8}.get(bpp_code)
    if bits_per_pixel is None or u32(data, ccb + 12) & 0x200 == 0:
        raise ValueError("Expected packed, coded 4/6/8-bit CEL")
    color_count = u32(data, plut + 8)
    if color_count not in (16, 32):
        raise ValueError("Expected a 16/32-color PLUT")
    colors = struct.unpack_from(f">{color_count}H", data, plut + 12)
    top, left, bottom, right = struct.unpack_from(">4h", data, frame + 8)
    width, height = right - left, bottom - top
    end = frame + u32(data, frame + 4)
    if end > len(data) or end < frame + 20:
        raise ValueError("Invalid pixel chunk length")
    # The RPDT reduced frame has a 16-byte header and a 4-byte PRE0 word.
    divisor_bits = u32(data, ccb + 60) & 0x300
    divisor = {0x000: 16, 0x100: 2, 0x200: 4, 0x300: 8}[divisor_bits]
    return width, height, decode_pixels(
        data[frame + 20 : end], width, height, bits_per_pixel, colors,
        divisor)


def first_frame(data: bytes) -> tuple[int, int, list[int]]:
    return frame_from_table(data, data.find(b"OFST"))


def full_marker_frame(data: bytes) -> tuple[int, int, list[int]]:
    """FAM 171 stores three full PDAT frames; the last is the complete post."""
    chunks, offsets = chunks_from_table(data, data.find(b"OFST"))
    if not all(tag in chunks for tag in (b"CCB ", b"PLUT", b"PDAT")):
        raise ValueError("Road marker animation is incomplete")
    ccb, plut = chunks[b"CCB "], chunks[b"PLUT"]
    frames = [offset for offset in offsets if data[offset : offset + 4] == b"PDAT"]
    if len(frames) != 3 or u32(data, ccb + 64) & 7 != 3:
        raise ValueError("Unexpected road marker frame layout")
    width, height = u32(data, ccb + 72), u32(data, ccb + 76)
    color_count = u32(data, plut + 8)
    colors = struct.unpack_from(f">{color_count}H", data, plut + 12)
    frame = frames[-1]
    end = frame + u32(data, frame + 4)
    if end > len(data) or end < frame + 12:
        raise ValueError("Invalid road marker PDAT length")
    return width, height, decode_pixels(
        data[frame + 12 : end], width, height, 4, colors)


def rows(values: list[int], width: int = 20) -> str:
    return ",\n    ".join(
        ", ".join(str(value) for value in values[i : i + width])
        for i in range(0, len(values), width)
    )


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if not SOURCE.exists():
        OUTPUT.write_text(
            "/* No local 3DO disc extracted; use procedural trees. */\n"
            "#define ROAD_SCENERY_COUNT 0\n", encoding="ascii"
        )
        print("No local 3DO family resource found; using procedural trees")
        return
    data = SOURCE.read_bytes()
    sprites = [(full_marker_frame if number == 171 else first_frame)(
        family(data, number)) for number in FAMILY_IDS]
    lines = [
        "/* Generated locally from the user's 3DO Families.RSRC. */",
        f"#define ROAD_SCENERY_COUNT {len(sprites)}",
        "#define ROAD_SCENERY_TRANSPARENT 0xF81F",
    ]
    for number, (width, height, pixels) in zip(FAMILY_IDS, sprites):
        lines.extend((
            f"static const road_pixel_t g_original_scenery_{number}[] = {{",
            f"    {rows(pixels)}",
            "};",
        ))
        print(f"Extracted FAM {number} roadside sprite: {width}x{height}")
    lines.append("static const road_sprite_t g_original_scenery[] = {")
    for number, (width, height, _) in zip(FAMILY_IDS, sprites):
        lines.append(
            f"    {{{width}u, {height}u, g_original_scenery_{number}}},"
        )
    lines.append("};")
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="ascii")


if __name__ == "__main__":
    main()
