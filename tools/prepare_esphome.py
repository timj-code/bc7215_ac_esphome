#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SUBMODULE = ROOT / "deps" / "bc7215_ac_lib"

REQUIRED_DIRS = [
    SUBMODULE / "examples" / "ESP-IDF" / "components" / "bc7215_ac_lib",
    SUBMODULE / "examples" / "ESP-IDF" / "components" / "bc7215",
    SUBMODULE / "examples" / "ESP-IDF" / "components" / "bc7215ac",
]


def run(cmd):
    print("+", " ".join(cmd))
    subprocess.run(cmd, cwd=ROOT, check=True)


def main():
    if not SUBMODULE.exists() or not any(SUBMODULE.iterdir()):
        print("Initializing git submodules...")
        run(["git", "submodule", "update", "--init", "--recursive"])

    missing = [p for p in REQUIRED_DIRS if not p.is_dir()]
    if missing:
        print("Required dependency directories are missing:")
        for p in missing:
            print("  -", p)
        print()
        print("Try:")
        print("  git submodule update --init --recursive")
        return 1

    local_paths = ROOT / "local_paths.yaml"
    project_dir = ROOT.as_posix()

    local_paths.write_text(
        f'bc7215_project_dir: "{project_dir}"\n',
        encoding="utf-8",
    )

    print()
    print("Generated:")
    print(f"  {local_paths}")
    print()
    print("Now you can run:")
    print("  esphome compile bedroom_bc7215_ac.yaml")
    return 0


if __name__ == "__main__":
    sys.exit(main())
