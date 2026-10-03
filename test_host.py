"""Resource-free host regressions for the standalone source checkout."""

from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parent
OUT = ROOT / ".build" / "host-tests"
TESTS = {
    "resource_paths": ["test_resource_paths.c"],
    "frontend_flow": ["road_frontend.c", "test_frontend_flow.c"],
    "landscape_input": ["road_frontend.c", "test_landscape_input.c"],
    "virtual_touch": ["test_virtual_touch.c"],
    "career": ["road_career.c", "test_road_career.c"],
    "profile": ["road_career.c", "road_profile.c", "test_road_profile.c"],
    "audio_mixer": ["road_audio_runtime.c", "test_road_audio_mixer.c"],
    "dynamic_visual": ["road_core.c", "test_dynamic_visual.c"],
}


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, sources in TESTS.items():
        output = OUT / (name + (".exe" if sys.platform == "win32" else ""))
        subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        *sources, "-o", str(output)], cwd=ROOT, check=True)
        subprocess.run([str(output)], cwd=ROOT, check=True)
    print(f"Passed {len(TESTS)} resource-free host tests")


if __name__ == "__main__":
    main()
