#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

VERSION_FILE="$ROOT_DIR/version.txt"
[ -f "$VERSION_FILE" ] || { echo "error: $VERSION_FILE not found" >&2; exit 1; }
APP_NAME="$(grep -E '^APP_NAME=' "$VERSION_FILE" | head -n1 | cut -d'=' -f2-)"
VERSION="$(grep -E '^APP_VERSION=' "$VERSION_FILE" | head -n1 | cut -d'=' -f2-)"
[ -n "$APP_NAME" ] || { echo "error: APP_NAME missing in $VERSION_FILE" >&2; exit 1; }
[ -n "$VERSION" ] || { echo "error: APP_VERSION missing in $VERSION_FILE" >&2; exit 1; }
APP_ID="$(printf '%s' "$APP_NAME" | tr '[:upper:]' '[:lower:]' | tr -cd 'a-z0-9')"

DEB_DISTRO_TAG="${MOXA_DEB_DISTRO_TAG:-}"
if [ -z "$DEB_DISTRO_TAG" ] && [ -f /etc/os-release ]; then
    OS_ID="$(. /etc/os-release && echo "$ID")"
    OS_VERSION_ID="$(. /etc/os-release && echo "$VERSION_ID")"
    if [ "$OS_ID" = "ubuntu" ] && [ -n "$OS_VERSION_ID" ]; then
        DEB_DISTRO_TAG="ubuntu${OS_VERSION_ID}"
    fi
fi
OUT_DIR_NAME="${DEB_DISTRO_TAG:-linux}"

BUILD_DIR="${MOXA_BUILD_DIR:-$ROOT_DIR/build/linux-release}"
OUT_DIR="${MOXA_OUT_DIR:-$SCRIPT_DIR/linux-$OUT_DIR_NAME}"

CLEAN=0
for arg in "$@"; do
    case "$arg" in
        --clean) CLEAN=1 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

if [ "$CLEAN" = "1" ]; then
    echo "==> Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "error: dpkg-deb not found. Install it with: sudo apt install dpkg-dev" >&2
    exit 1
fi

NPROC="$(nproc 2>/dev/null || echo 4)"

echo "==> Configuring (${DEB_DISTRO_TAG:-host toolchain})"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "==> Building"
cmake --build "$BUILD_DIR" --target deploy_linux -j"$NPROC"

echo "==> Packaging .deb"
"$SCRIPT_DIR/../packaging/linux/build-deb-installer.sh" \
    "$BUILD_DIR/package/$APP_ID" "$OUT_DIR" "$VERSION" "$APP_NAME" "$DEB_DISTRO_TAG"

echo "==> Done: $OUT_DIR"
