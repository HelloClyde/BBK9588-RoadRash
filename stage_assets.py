"""Copy the original 3DO Rash tree to a mounted 9588 B: or A: volume."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "local-data" / "3do-eu-extracted" / "Rash"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("destination", type=Path,
                    help="mounted B: or A: volume root; 应用/数据/游戏/Rash is created within it")
    ap.add_argument("--manifest-only", action="store_true",
                    help="report the files and size without copying")
    selection = ap.add_mutually_exclusive_group()
    selection.add_argument("--core-only", action="store_true",
                           help="omit all 3DO Streams files")
    selection.add_argument("--no-video", action="store_true",
                           help="include music streams, omit video streams")
    selection.add_argument("--single-track", action="store_true",
                           help="include only Rusty Cage and RashIF.RSRC, omitting other music and video")
    ns = ap.parse_args()
    if not SOURCE.is_dir():
        ap.error(f"original game assets missing: {SOURCE}")
    def selected(path: Path) -> bool:
        parts = path.relative_to(SOURCE).parts
        if ns.core_only and "Streams" in parts:
            return False
        if ns.no_video and "Streams" in parts:
            return len(parts) > 2 and parts[1].lower() == "bgaudio"
        if ns.single_track and "Streams" in parts:
            return path.relative_to(SOURCE).as_posix() in {
                "Streams/bgaudio/RashIF.RSRC",
                "Streams/bgaudio/SG.RustyCage_sw22.stream",
            }
        return True

    files = [path for path in SOURCE.rglob("*") if path.is_file() and selected(path)]
    total = sum(path.stat().st_size for path in files)
    print(f"Original Rash tree: {len(files)} files, {total:,} bytes")
    if ns.manifest_only:
        return
    if not ns.destination.is_dir():
        ap.error(f"volume root is not a directory: {ns.destination}")
    target = ns.destination / "应用" / "数据" / "游戏" / "Rash"
    if target.exists():
        ap.error(f"target already exists; refusing to overwrite: {target}")
    free = shutil.disk_usage(ns.destination).free
    if free < total:
        ap.error(f"insufficient free space: need {total:,}, have {free:,}")
    if ns.core_only or ns.no_video or ns.single_track:
        target.mkdir(parents=True)
        for source_file in files:
            relative = source_file.relative_to(SOURCE)
            output_file = target / relative
            output_file.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source_file, output_file)
    else:
        shutil.copytree(SOURCE, target)
    copied = [path for path in target.rglob("*") if path.is_file()]
    if len(copied) != len(files) or sum(path.stat().st_size for path in copied) != total:
        raise RuntimeError("copied file count/size mismatch")
    print(f"Staged selected original resource tree at {target}")


if __name__ == "__main__":
    main()
