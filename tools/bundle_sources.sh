#!/usr/bin/env bash
# bundle_sources.sh — one archive holding everything the Linux build needs.
#
#   ./tools/bundle_sources.sh [destination.tar.gz]
#
# Two of the five repositories (Void Palabra, Void Allomone) are not published,
# so a Linux machine cannot clone what it needs. This makes one file to copy
# over — by USB stick, scp, whatever — after which:
#
#   tar -xzf voidfamily-sources.tar.gz && cd projects/InteractionCombinators
#   ./tools/build_linux.sh
#
# Sources only: no .git, no build directories, no release artifacts. It is a
# copy for building, not a clone for developing — commits still happen on the
# machine that holds the repositories.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
parent="$(dirname "$root")"
dest="${1:-$parent/voidfamily-sources.tar.gz}"

repos=(InteractionCombinators VoidMaiz VoidCore VoidPalabra VoidAllomone)
for r in "${repos[@]}"; do
    [ -d "$parent/$r" ] || { echo "missing: $parent/$r" >&2; exit 1; }
done

# --transform puts them under projects/ so the layout survives the trip: the
# builds find each other by being siblings, and nothing else.
tar -czf "$dest" -C "$parent" \
    --exclude='.git' \
    --exclude='build' --exclude='build-*' --exclude='*/android/build' \
    --exclude='release' --exclude='node_modules' \
    --transform='s,^,projects/,' \
    "${repos[@]}"

echo "wrote $dest ($(du -h "$dest" | cut -f1))"
echo "on the Linux machine:"
echo "  tar -xzf $(basename "$dest") && cd projects/InteractionCombinators"
echo "  ./tools/build_linux.sh"
