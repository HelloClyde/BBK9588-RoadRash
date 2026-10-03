"""Fetch four pinned upstream traversal files into ignored local-data.

The upstream project publishes no redistribution license, so these files are
not committed to the port repository. Every download is SHA-256 verified.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import urllib.request


ROOT = Path(__file__).resolve().parent.parent
PIN = "97af68f3aabd71b173d8e5b8fed1e03ef106100d"
BASE = f"https://raw.githubusercontent.com/trapexit/3do-decomp-road-rash/{PIN}/src"
FILES = {
    "track_traversal_runtime.h": "d49a6207000af3ff1ca4433c14d7d76065496c9b60aee8a7b2fc06ad371b4ba0",
    "advance_road_path_traversal.c": "a0ec176c6bf15f703143b470c24b9829fd53e72271de0dcb1062ccb761c8f1ac",
    "initialize_road_lane_width_traversal.c": "5facf0f799911758ff554d93d987826d71e40f4460a9494f0bdc925a5a105323",
    "advance_road_lane_width_traversal.c": "a8ebe4a13a380fb0de506c284f841e667a15c1c076b047ec91ec8d270839dd11",
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path,
                        help="optional local checkout of the pinned upstream revision")
    args = parser.parse_args()
    dest = ROOT / "local-data" / "upstream-src"
    dest.mkdir(parents=True, exist_ok=True)
    for name, expected in FILES.items():
        path = dest / name
        if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == expected:
            continue
        if args.upstream:
            data = (args.upstream / "src" / name).read_bytes()
        else:
            with urllib.request.urlopen(f"{BASE}/{name}", timeout=30) as response:
                data = response.read()
        digest = hashlib.sha256(data).hexdigest()
        if digest != expected:
            raise SystemExit(f"Hash mismatch for {name}: {digest}")
        path.write_bytes(data)
    (dest / "REVISION.txt").write_text(PIN + "\n", encoding="ascii")
    print(f"Prepared {len(FILES)} verified upstream engine files at {PIN[:12]}")


if __name__ == "__main__":
    main()
