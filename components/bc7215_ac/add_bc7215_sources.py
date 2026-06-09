from pathlib import Path

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR")).resolve()

# Find the repository root robustly instead of depending on a fixed number of
# parent directories. This script is normally called from:
#   firmware/.esphome/build/<node-name>/
repo_root = None
for p in [project_dir] + list(project_dir.parents):
    if (p / "deps" / "bc7215_ac_lib").exists():
        repo_root = p
        break

if repo_root is None:
    raise FileNotFoundError(
        f"Cannot find repo root from PROJECT_DIR={project_dir}; "
        "expected deps/bc7215_ac_lib to exist."
    )

lib_root = repo_root / "deps" / "bc7215_ac_lib"
idf_example_root = lib_root / "examples" / "ESP-IDF"

include_dirs = [
    idf_example_root / "components" / "bc7215" / "include",
    idf_example_root / "components" / "bc7215ac" / "include",
    idf_example_root / "main" / "include",

    lib_root,
    lib_root / "include",
    lib_root / "src",
    lib_root / "bc7215_ac_lib",
]

env.Append(CPPPATH=[str(p) for p in include_dirs if p.exists()])

sources = [
    idf_example_root / "components" / "bc7215" / "bc7215.cpp",
    idf_example_root / "components" / "bc7215ac" / "bc7215ac.cpp",

    # Extra C sources used by the AC library.
    lib_root / "bc7215_ac_lib" / "bc7215_ac_lib.c",
    lib_root / "bc7215_ac_lib" / "bc7215_lib.c",
]

for src in sources:
    if not src.exists():
        raise FileNotFoundError(f"BC7215 source file not found: {src}")
    print(f"BC7215 extra source: {src}")

# Compile these external source files as normal independent compilation units
# and explicitly add the resulting objects to the final PlatformIO link step.
objects = []
for src in sources:
    target = f"$BUILD_DIR/bc7215_extra/{src.stem}_{abs(hash(str(src))) & 0xffff:x}.o"
    objects.extend(env.Object(target=target, source=str(src)))

env.Append(PIOBUILDFILES=objects)
