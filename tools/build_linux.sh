#!/usr/bin/env bash
# build_linux.sh — build Interaction Combinators on Linux, from the family's
# sources, and package what came out.
#
#   ./tools/build_linux.sh            # build + package into release/
#   ./tools/build_linux.sh --run      # build and start it
#
# WHY A SCRIPT AND NOT A GITHUB ACTION. The app links Void Maiz, which links
# Void Core, Void Palabra and Void Allomone. Core and Maiz are public; Palabra
# and Allomone are not (2026-09-22). A cloud runner cannot fetch them, and a
# build without Palabra is an app without LAN — which is the one thing a third
# device was wanted for. So the build runs where the sources already are.
#
# WHAT IT NEEDS. A C++20 compiler, CMake ≥ 3.20, Ninja, and the X11/GL headers
# GLFW compiles against (GLFW is vendored; its dependencies are not):
#
#   Debian/Ubuntu/Raspberry Pi OS:
#     sudo apt install build-essential cmake ninja-build git \
#          libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
#          libgl1-mesa-dev curl
#   Fedora:
#     sudo dnf install gcc-c++ cmake ninja-build git libX11-devel \
#          libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel \
#          mesa-libGL-devel curl
#   Arch:
#     sudo pacman -S base-devel cmake ninja git libx11 libxrandr libxinerama \
#          libxcursor libxi mesa curl
#
# On Wayland this runs through XWayland, which is fine — GLFW is built for X11.
#
# WHERE THE SIBLINGS ARE. All five repos side by side, as on every other
# machine in this family:
#
#   projects/
#     InteractionCombinators/   ← you are here
#     VoidMaiz/  VoidCore/  VoidPalabra/  VoidAllomone/
#
# `tools/bundle_sources.sh` on a machine that has them all makes one archive to
# copy over, if this Linux box is not that machine.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
parent="$(dirname "$root")"
version="$(tr -d '[:space:]' < "$root/VERSION")"

missing=()
for repo in VoidMaiz VoidCore VoidPalabra VoidAllomone; do
    [ -d "$parent/$repo" ] || missing+=("$repo")
done
if [ ${#missing[@]} -gt 0 ]; then
    echo "missing beside $root: ${missing[*]}" >&2
    echo "(Void Core and Void Maiz are on GitHub under migriv24; the other two" >&2
    echo " are not published — copy them from the machine that has them.)" >&2
    exit 1
fi

# Void Core first: on desktop, Void Maiz links the core's BUILT shared library
# rather than compiling it, and refuses to configure until it exists.
cmake -S "$parent/VoidCore/core" -B "$parent/VoidCore/core/build" -G Ninja       -DCMAKE_BUILD_TYPE=Release
cmake --build "$parent/VoidCore/core/build"

build="$root/build-linux"
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$build"

bin="$build/bin/interaction_combinators"
[ -x "$bin" ] || { echo "the build produced no $bin" >&2; exit 1; }

# Did it get the network? Without Palabra, CMake quietly builds the solo app,
# and the difference shows up only when someone tries to join. Say it now.
if [ ! -x "$build/bin/interaction_combinators_duo" ]; then
    echo "WARNING: built WITHOUT networking (voidmaiz_net missing)." >&2
    echo "         Check that ../VoidPalabra is present and reconfigure." >&2
fi

name="InteractionCombinators-$version-linux-x86_64"
case "$(uname -m)" in
    aarch64|arm64) name="InteractionCombinators-$version-linux-arm64" ;;
esac
out="$root/release/$name"
rm -rf "$out"; mkdir -p "$out"
cp "$bin" "$out/"
# (plain `test && cp` would abort the script under `set -e` when it is absent)
if [ -x "$build/bin/interaction_combinators_duo" ]; then
    cp "$build/bin/interaction_combinators_duo" "$out/"
fi
# Void Core is a shared library; ship it beside the binary and find it there
core_so="$(grep -m1 '^VOIDCORE_LIBRARY:FILEPATH=' "$build/CMakeCache.txt" | cut -d= -f2- || true)"
if [ -n "${core_so:-}" ] && [ -f "$core_so" ]; then cp "$core_so" "$out/"; fi
cp "$root/LICENSE" "$root/README.md" "$out/" 2>/dev/null || true
cat > "$out/run.sh" <<'EOF'
#!/usr/bin/env sh
# Void Core sits beside the binary rather than in /usr/lib, so say so.
cd "$(dirname "$0")"
LD_LIBRARY_PATH="$PWD:${LD_LIBRARY_PATH:-}" exec ./interaction_combinators "$@"
EOF
chmod +x "$out/run.sh"
tar -czf "$out.tar.gz" -C "$root/release" "$name"
echo "built:    $bin"
echo "packaged: $out.tar.gz"

if [ "${1:-}" = "--run" ]; then exec "$out/run.sh"; fi
