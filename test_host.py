"""Resource-free host regressions for the standalone source checkout."""

from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parent
OUT = ROOT / ".build" / "host-tests"
TESTS = {
    "resource_paths": ["tests/test_resource_paths.c"],
    "frontend_flow": ["src/road_frontend.c", "tests/test_frontend_flow.c"],
    "landscape_input": ["src/road_frontend.c", "tests/test_landscape_input.c"],
    "virtual_touch": ["tests/test_virtual_touch.c"],
    "career": ["src/road_career.c", "tests/test_road_career.c"],
    "profile": ["src/road_career.c", "src/road_profile.c", "tests/test_road_profile.c"],
    "audio_mixer": ["src/road_audio_runtime.c", "tests/test_road_audio_mixer.c"],
    "dynamic_visual": ["src/road_core.c", "tests/test_dynamic_visual.c"],
}


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, sources in TESTS.items():
        output = OUT / (name + (".exe" if sys.platform == "win32" else ""))
        subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-I", str(ROOT), "-I", str(ROOT / "src"),
                        *sources, "-o", str(output)], cwd=ROOT, check=True)
        subprocess.run([str(output)], cwd=ROOT, check=True)
    print(f"Passed {len(TESTS)} resource-free host tests")


if __name__ == "__main__":
    main()
