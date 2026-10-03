"""Build a random-access, single-file pack from the selected 3DO resources."""

from __future__ import annotations

import hashlib
import argparse
from pathlib import Path
import struct

from package_game import SOURCE, source_files


PACK = Path(__file__).resolve().parent.parent / "Rash.pak"
TEMP = PACK.with_suffix(".pak.tmp")
HEADER = struct.Struct("<4sIII")
ENTRY = struct.Struct("<48sII")


def digest(path: Path) -> bytes:
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            sha.update(block)
    return sha.digest()


def build_pack(files: list[Path] | None = None,
               source: Path = SOURCE) -> Path:
    if files is None:
        files = source_files(source)
    if not files or files[0].relative_to(source).as_posix() != "rashOpt.rsrc":
        raise SystemExit("rashOpt.rsrc must be the first pack entry")
    if TEMP.exists():
        raise SystemExit(f"Refusing to overwrite unfinished pack: {TEMP}")
    data_start = HEADER.size + len(files) * ENTRY.size
    records = []
    cursor = data_start
    for path in files:
        name = path.relative_to(source).as_posix().encode("ascii")
        size = path.stat().st_size
        if len(name) >= 48 or cursor + size > 0x7FFFFFFF:
            raise SystemExit(f"Pack entry too large: {path}")
        records.append((name, cursor, size))
        cursor += size
    try:
        with TEMP.open("wb") as out:
            out.write(HEADER.pack(b"RRPK", 1, len(files), data_start))
            for name, offset, size in records:
                out.write(ENTRY.pack(name, offset, size))
            for path in files:
                with path.open("rb") as source:
                    while block := source.read(1024 * 1024):
                        out.write(block)
        if TEMP.stat().st_size != cursor:
            raise RuntimeError("Pack size mismatch")
        with TEMP.open("rb") as packed:
            magic, version, count, start = HEADER.unpack(
                packed.read(HEADER.size))
            if (magic, version, count, start) != (
                b"RRPK", 1, len(files), data_start
            ):
                raise RuntimeError("Pack header mismatch")
            for path, expected in zip(files, records):
                name, offset, size = ENTRY.unpack(packed.read(ENTRY.size))
                if (name.rstrip(b"\0"), offset, size) != expected:
                    raise RuntimeError(f"Pack index mismatch: {path}")
            for path, (_, offset, size) in zip(files, records):
                packed.seek(offset)
                sha = hashlib.sha256()
                remaining = size
                while remaining:
                    block = packed.read(min(1024 * 1024, remaining))
                    if not block:
                        raise RuntimeError(f"Truncated pack entry: {path}")
                    sha.update(block)
                    remaining -= len(block)
                if sha.digest() != digest(path):
                    raise RuntimeError(f"Pack content mismatch: {path}")
        TEMP.replace(PACK)
    except BaseException:
        TEMP.unlink(missing_ok=True)
        raise
    print(f"Pack: {PACK}")
    print(f"Files: {len(files)}")
    print(f"Bytes: {PACK.stat().st_size}")
    print(f"SHA-256: {digest(PACK).hex()}")
    return PACK


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SOURCE,
                        help="path to a legally obtained 3DO Rash resource tree")
    args = parser.parse_args()
    build_pack(source=args.source.resolve())
