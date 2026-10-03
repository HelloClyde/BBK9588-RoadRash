"""Package the unified BDA with the needed 3DO assets and one music track."""

from __future__ import annotations

import hashlib
import argparse
from pathlib import Path
import tarfile
import zipfile


ROOT = Path(__file__).resolve().parent.parent
BDA = ROOT / "RoadRash.bda"
PACK = ROOT / "Rash.pak"
SOURCE = ROOT / "local-data" / "3do-eu-extracted" / "Rash"
OUTPUT = ROOT / "RoadRash-9588-no-video.zip"
TEMP = ROOT / "RoadRash-9588-no-video.zip.tmp"
COMPACT_OUTPUT = ROOT / "RoadRash-9588-no-video.tar.xz"
COMPACT_TEMP = ROOT / "RoadRash-9588-no-video.tar.xz.tmp"
EXPECTED_RESOURCE_COUNT = 32
EXPECTED_RESOURCE_BYTES = 59_621_760
STREAM_FILES = {
    "Streams/bgaudio/RashIF.RSRC",
    "Streams/bgaudio/SG.RustyCage_sw22.stream",
}

INSTALL = """暴力摩托 9588 无视频整包

先在电脑上解压，再按 9588 的常规 BDA 安装方式安装 RoadRash.bda。
将单个 Rash.pak 放到 B:\\应用\\数据\\游戏\\Rash\\Rash.pak；
没有 B: 盘时放到 A:\\应用\\数据\\游戏\\Rash\\Rash.pak。
不要直接把压缩包复制到设备，也不要把 Rash.pak 放到盘根目录。

资源包包含标题背景、五条赛道、车辆与对象素材、音效及一首音乐；
不含视频或其余音乐。若标题是黑底、赛道贴图缺失，请先确认
Rash.pak 的完整路径与校验和，再查看 RRDEBUG.LOG 中的
PACK_OPEN、MENU_ASSET_DONE、BIKE_FRAMES_DONE、ROAD_TEX_DONE。
"""


def source_files(source: Path = SOURCE) -> list[Path]:
    if not source.is_dir():
        raise SystemExit("Missing extracted Rash resource tree")
    files = []
    for path in source.rglob("*"):
        if not path.is_file():
            continue
        parts = path.relative_to(source).parts
        if "Streams" in parts and path.relative_to(source).as_posix() not in STREAM_FILES:
            continue
        files.append(path)
    # Firmware random reads of a small RSRC at the old pack tail failed in
    # emulator tests. Put the bike/road texture catalog first and retain a
    # deterministic order for every other file.
    files.sort(key=lambda path: (
        path.relative_to(source).as_posix() != "rashOpt.rsrc",
        path.relative_to(source).as_posix()))
    count = len(files)
    size = sum(path.stat().st_size for path in files)
    if count != EXPECTED_RESOURCE_COUNT or size != EXPECTED_RESOURCE_BYTES:
        raise SystemExit(
            f"Unexpected no-video resource set: {count} files, {size} bytes"
        )
    return files


def digest(stream) -> bytes:
    sha = hashlib.sha256()
    while chunk := stream.read(1024 * 1024):
        sha.update(chunk)
    return sha.digest()


def package_zip() -> None:
    if TEMP.exists():
        raise SystemExit(f"Refusing to overwrite unfinished package: {TEMP}")
    members = [("RoadRash.bda", BDA), ("Rash/Rash.pak", PACK)]
    with zipfile.ZipFile(TEMP, "w", zipfile.ZIP_DEFLATED,
                         compresslevel=6, allowZip64=True) as archive:
        archive.writestr("INSTALL.txt", INSTALL.encode("utf-8"))
        for name, path in members:
            archive.write(path, name)
    with zipfile.ZipFile(TEMP, "r") as archive:
        expected = {name for name, _ in members} | {"INSTALL.txt"}
        if set(archive.namelist()) != expected:
            raise RuntimeError("ZIP member list mismatch")
        for name, path in members:
            if archive.getinfo(name).file_size != path.stat().st_size:
                raise RuntimeError(f"ZIP size mismatch: {name}")
            with archive.open(name) as packed, path.open("rb") as original:
                if digest(packed) != digest(original):
                    raise RuntimeError(f"ZIP content mismatch: {name}")
    # Keep the previous verified package intact until the new one passes every
    # member check, then swap it into place in a single filesystem operation.
    TEMP.replace(OUTPUT)
    report(OUTPUT, len(members) + 1)


def package_compact() -> None:
    if COMPACT_TEMP.exists():
        raise SystemExit(f"Refusing to overwrite unfinished package: {COMPACT_TEMP}")
    members = [("RoadRash.bda", BDA), ("Rash/Rash.pak", PACK)]
    with tarfile.open(COMPACT_TEMP, "w:xz", preset=6) as archive:
        import io
        install_data = INSTALL.encode("utf-8")
        info = tarfile.TarInfo("INSTALL.txt")
        info.size = len(install_data)
        archive.addfile(info, io.BytesIO(install_data))
        for name, path in members:
            archive.add(path, arcname=name, recursive=False)
    with tarfile.open(COMPACT_TEMP, "r:xz") as archive:
        expected = {name for name, _ in members} | {"INSTALL.txt"}
        if set(archive.getnames()) != expected:
            raise RuntimeError("Compact archive member list mismatch")
        for name, path in members:
            member = archive.getmember(name)
            if member.size != path.stat().st_size:
                raise RuntimeError(f"Compact archive size mismatch: {name}")
            with archive.extractfile(member) as packed, path.open("rb") as original:
                if digest(packed) != digest(original):
                    raise RuntimeError(f"Compact archive content mismatch: {name}")
    COMPACT_TEMP.replace(COMPACT_OUTPUT)
    report(COMPACT_OUTPUT, len(members) + 1)


def report(path: Path, count: int) -> None:
    with path.open("rb") as packed:
        checksum = digest(packed).hex()
    print(f"Package: {path}")
    print(f"Members: {count} (BDA, one pack of {EXPECTED_RESOURCE_COUNT} resources, INSTALL.txt)")
    print(f"Resource bytes: {PACK.stat().st_size}")
    print(f"Archive bytes: {path.stat().st_size}")
    print(f"SHA-256: {checksum}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compact", action="store_true",
                        help="create a smaller tar.xz package; extract on a PC before installing")
    parser.add_argument("--source", type=Path, default=SOURCE,
                        help="path to a legally obtained 3DO Rash resource tree")
    args = parser.parse_args()
    source = args.source.resolve()
    files = source_files(source)
    if not BDA.is_file():
        raise SystemExit("Missing RoadRash.bda; build the game first")
    from build_resource_pack import build_pack
    build_pack(files, source=source)
    if args.compact:
        package_compact()
    else:
        package_zip()


if __name__ == "__main__":
    main()
