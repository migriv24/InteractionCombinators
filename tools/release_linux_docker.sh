#!/usr/bin/env bash
# release_linux_docker.sh — build the Linux release the way it is published.
#
#   ./tools/release_linux_docker.sh            # x86_64
#   ./tools/release_linux_docker.sh arm64      # arm64, through emulation (slow)
#
# This is the recipe behind the .tar.gz on the releases page, kept as a script
# so the next one is the same build and not a new adventure. It needs Docker
# (Docker Desktop on Windows), and it works from a machine that has all five
# repositories, including the two that are not published.
#
# WHY A CONTAINER, AND WHY AN OLD ONE. A Linux binary runs on a machine whose
# glibc is at least as new as the one it was built against. Ubuntu 20.04 gives
# glibc 2.31, which reaches back to 2020 and covers Debian 11, Ubuntu 20.04+,
# Mint, Pop!_OS and the Raspberry Pi OS of that era. The compiler and CMake come
# from PPAs on top, so the TOOLS are new while the RUNTIME FLOOR stays old.
#
# Three things this recipe got wrong before it got them right, each of which
# produced a download that would not start (2026-09-23):
#   1. libstdc++ must be STATIC. Built with GCC 11 against a machine that has
#      GCC 9's runtime, a dynamic link means `GLIBCXX_3.4.29 not found`.
#      (build_linux.sh passes -static-libstdc++ -static-libgcc.)
#   2. OpenGL must be the CLASSIC libGL.so.1, not glvnd's libOpenGL.so.0 — the
#      glvnd packages are not on every desktop. (Void Maiz's CMakeLists sets
#      OpenGL_GL_PREFERENCE LEGACY.)
#   3. The binary must carry an $ORIGIN rpath, or it cannot find the
#      libvoidcore.so sitting right beside it. (Interaction Combinators'
#      CMakeLists sets it.)
# All three are fixed in the sources; this comment is here so that nobody
# "simplifies" one of them back out.
set -euo pipefail

arch="${1:-x86_64}"
case "$arch" in
    x86_64|amd64) platform="linux/amd64"; suffix="x86_64" ;;
    arm64|aarch64) platform="linux/arm64"; suffix="arm64" ;;
    *) echo "unknown architecture: $arch (use x86_64 or arm64)" >&2; exit 1 ;;
esac

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$(tr -d '[:space:]' < "$root/VERSION")"
container="ic-linux-$suffix"
bundle="$(mktemp -d)/voidfamily-sources.tar.gz"

echo "== bundling sources =="
"$root/tools/bundle_sources.sh" "$bundle" >/dev/null

echo "== container ($platform) =="
docker rm -f "$container" >/dev/null 2>&1 || true
docker run -d --platform "$platform" --name "$container" ubuntu:20.04 sleep infinity >/dev/null

docker exec "$container" bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq software-properties-common wget gnupg ca-certificates >/dev/null
add-apt-repository -y ppa:ubuntu-toolchain-r/test >/dev/null 2>&1
wget -qO- https://apt.kitware.com/keys/kitware-archive-latest.asc |
    gpg --dearmor -o /usr/share/keyrings/kitware.gpg
echo "deb [signed-by=/usr/share/keyrings/kitware.gpg] https://apt.kitware.com/ubuntu/ focal main" \
    > /etc/apt/sources.list.d/kitware.list
apt-get update -qq
apt-get install -y -qq g++-11 cmake ninja-build \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev >/dev/null
'

docker cp "$bundle" "$container:/vf.tar.gz"
docker exec "$container" bash -c '
set -e
mkdir -p /work && cd /work && tar -xzf /vf.tar.gz
cd /work/projects/InteractionCombinators
sed -i "s/\r$//" tools/build_linux.sh   # in case the sources came from Windows
export CC=gcc-11 CXX=g++-11
bash tools/build_linux.sh
'

# Prove it runs before anyone downloads it: a virtual screen, software GL, and
# the two-window bench, which exercises the network as well as the window.
echo "== does it run? =="
docker exec "$container" bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get install -y -qq xvfb libgl1-mesa-dri libglx-mesa0 >/dev/null 2>&1
cd /work/projects/InteractionCombinators/build-linux/bin
export LIBGL_ALWAYS_SOFTWARE=1
xvfb-run -a -s "-screen 0 1280x800x24" ./interaction_combinators --quit-after-ms 3000 >/dev/null
echo "  the app starts and exits cleanly"
xvfb-run -a -s "-screen 0 1600x900x24" ./interaction_combinators_duo --selftest | tail -1
'

mkdir -p "$root/release"
out="InteractionCombinators-$version-linux-$suffix.tar.gz"
docker cp "$container:/work/projects/InteractionCombinators/release/$out" "$root/release/$out"
docker rm -f "$container" >/dev/null
echo "== $root/release/$out"
echo "Now: tools/package_release.ps1 -FeedOnly (puts it in void-updates.json),"
echo "then upload it and the feed to the release."
