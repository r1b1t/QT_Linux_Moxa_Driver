#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DISTRO_TAG="ubuntu22.04"
DOCKER_DIR="$ROOT_DIR/packaging/linux/docker"
IMAGE="moxauportmanager-linux-build:$DISTRO_TAG"

if ! command -v docker >/dev/null 2>&1; then
    echo "error: docker not found. Install it with: sudo apt install docker.io" >&2
    exit 1
fi

echo "==> Preparing build image $IMAGE"
docker build -t "$IMAGE" -f "$DOCKER_DIR/$DISTRO_TAG.Dockerfile" "$DOCKER_DIR"

echo "==> Building inside $DISTRO_TAG container"
docker run --rm \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e MOXA_BUILD_DIR="$ROOT_DIR/build/linux-$DISTRO_TAG-release" \
    -e MOXA_DEB_DISTRO_TAG="$DISTRO_TAG" \
    -v "$ROOT_DIR:$ROOT_DIR" \
    -w "$ROOT_DIR" \
    "$IMAGE" \
    "$SCRIPT_DIR/build_linux.sh" "$@"
