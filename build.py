"""Build the resource-independent BBK 9588 Road Rash BDA.

Requires the pinned SDK submodule and its MIPS toolchain. Game disc resources
are only needed to make Rash.pak and are never embedded in the BDA.
"""

from __future__ import annotations

import os
import argparse
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parent
SDK = ROOT / "sdk"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path,
                        help="toolchain installation root, if not installed in sdk/.toolchain")
    args = parser.parse_args()
    if not (SDK / "bda_packer" / "__main__.py").is_file():
        raise SystemExit("Initialize the SDK: git submodule update --init sdk")
    env = os.environ.copy()
    env["PYTHONPATH"] = str(SDK) + os.pathsep + env.get("PYTHONPATH", "")
    env["BDA_SDK_INCLUDE"] = str(SDK / "sdk" / "include")
    env["PYTHONIOENCODING"] = "utf-8"
    subprocess.run([sys.executable, "prepare_upstream.py"],
                   cwd=ROOT, check=True)
    output = ROOT / "RoadRash.bda"
    command = [
        sys.executable, "-m", "bda_packer", "runtime_only_bda.c",
        "--title", "暴力摩托", "--category", "4",
        "--icon-png", "assets/road_rash_3do_icon.png",
        "-I", str(ROOT), "-o", str(output),
    ]
    if args.prefix:
        prefix = args.prefix.resolve() / "bin" / "mipsel-none-elf-"
        command.extend(["--prefix", str(prefix)])
    subprocess.run(command, cwd=ROOT, env=env, check=True)
    subprocess.run([sys.executable, "-m", "bda_packer.validate",
                    str(output)], cwd=ROOT, env=env, check=True)


if __name__ == "__main__":
    main()
