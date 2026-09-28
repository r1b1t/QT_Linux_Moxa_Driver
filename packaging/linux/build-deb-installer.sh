#!/usr/bin/env bash
set -euo pipefail

STAGING_DIR="${1:?staging directory (deploy_linux output) is required}"
OUTPUT_DIR="${2:?output directory is required}"
VERSION="${3:-1.0.0}"
APP_NAME="${4:-Moxa UPort Manager}"
DISTRO_TAG="${5:-}"
APP_ID="$(printf '%s' "$APP_NAME" | tr '[:upper:]' '[:lower:]' | tr -cd 'a-z0-9')"
[ -n "$APP_ID" ] || { echo "error: app name '$APP_NAME' has no letters/digits to derive a package id from" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ ! -x "$STAGING_DIR/bin/$APP_ID" ]; then
    echo "error: $STAGING_DIR does not look like a deploy_linux output (missing bin/$APP_ID)" >&2
    exit 1
fi
if [ ! -x "$STAGING_DIR/scripts/moxa-helper.sh" ]; then
    echo "error: $STAGING_DIR does not look like a deploy_linux output (missing scripts/moxa-helper.sh)" >&2
    exit 1
fi

ARCH="$(dpkg --print-architecture 2>/dev/null || echo amd64)"
PKG_ROOT="$(mktemp -d)"
trap 'rm -rf "$PKG_ROOT"' EXIT

mkdir -p \
    "$PKG_ROOT/DEBIAN" \
    "$PKG_ROOT/opt/$APP_ID" \
    "$PKG_ROOT/usr/bin" \
    "$PKG_ROOT/usr/share/applications" \
    "$PKG_ROOT/usr/share/icons/hicolor/256x256/apps"

cp -a "$STAGING_DIR/." "$PKG_ROOT/opt/$APP_ID/"

cat > "$PKG_ROOT/usr/bin/$APP_ID" <<EOF
#!/bin/sh
exec /opt/$APP_ID/bin/$APP_ID "\$@"
EOF
chmod 755 "$PKG_ROOT/usr/bin/$APP_ID"

sed \
    -e "s/^Name=.*/Name=$APP_NAME/" \
    -e "s/__APP_ID__/$APP_ID/g" \
    "$SCRIPT_DIR/app.desktop" > "$PKG_ROOT/usr/share/applications/$APP_ID.desktop"
cp "$SCRIPT_DIR/icon.png" "$PKG_ROOT/usr/share/icons/hicolor/256x256/apps/$APP_ID.png"

INSTALLED_SIZE_KB="$(du -sk "$PKG_ROOT/opt/$APP_ID" | cut -f1)"
DEB_VERSION="$VERSION${DISTRO_TAG:+~$DISTRO_TAG}"

LIBC6_DEPENDS="libc6"
if command -v objdump >/dev/null 2>&1; then
    GLIBC_MIN="$(objdump -T "$PKG_ROOT/opt/$APP_ID/bin/$APP_ID" 2>/dev/null \
        | grep -oE 'GLIBC_[0-9]+(\.[0-9]+)+' | cut -d_ -f2 | sort -Vu | tail -n1)" || true
    if [ -n "${GLIBC_MIN:-}" ]; then
        LIBC6_DEPENDS="libc6 (>= $GLIBC_MIN)"
    fi
else
    echo "warning: objdump not found; libc6 dependency left unversioned" >&2
fi

QT_DEPENDS=""
if command -v ldd >/dev/null 2>&1 && command -v dpkg >/dev/null 2>&1; then
    QT_LIBS="$(ldd "$PKG_ROOT/opt/$APP_ID/bin/$APP_ID" 2>/dev/null \
        | awk '{print $3}' | grep -E '/libQt[0-9]?(Core|Widgets|Gui)\.so' || true)"
    QT_PACKAGES=""
    for lib in $QT_LIBS; do
        real_lib="$(realpath "$lib" 2>/dev/null || echo "$lib")"
        pkg="$(dpkg -S "$real_lib" 2>/dev/null | head -n1 | cut -d: -f1 || true)"
        [ -n "$pkg" ] || continue
        case " $QT_PACKAGES " in *" $pkg "*) continue ;; esac
        QT_PACKAGES="$QT_PACKAGES $pkg"
    done
    for pkg in $QT_PACKAGES; do
        ver="$(dpkg-query -W -f='${Version}' "$pkg" 2>/dev/null || true)"
        if [ -n "$ver" ]; then
            QT_DEPENDS="${QT_DEPENDS:+$QT_DEPENDS, }$pkg (>= $ver)"
        else
            QT_DEPENDS="${QT_DEPENDS:+$QT_DEPENDS, }$pkg"
        fi
    done
fi
if [ -z "$QT_DEPENDS" ]; then
    echo "warning: could not resolve Qt runtime packages via ldd/dpkg; falling back to libqt6widgets6" >&2
    QT_DEPENDS="libqt6widgets6"
fi

cat > "$PKG_ROOT/DEBIAN/control" <<EOF
Package: $APP_ID
Version: $DEB_VERSION
Section: admin
Priority: optional
Architecture: $ARCH
Installed-Size: $INSTALLED_SIZE_KB
Maintainer: Nuri Berk Ünal <nberkunal10@gmail.com>
Depends: $LIBC6_DEPENDS, $QT_DEPENDS, policykit-1, setserial, build-essential
Recommends: linux-headers-generic
Description: $APP_NAME
 Moxa UPort 11x0 series (UPort 1110/1130/1150) USB-to-serial sürücüsünü
 kurar/günceller/kaldırır ve bağlı portların RS-232/RS-422/RS-485 (2W/4W)
 arayüz modunu ayarlar. Ubuntu 22.04-24.04 arası, çalışan çekirdeğe göre
 sürücüyü kendi makinesinde derler.
EOF

cat > "$PKG_ROOT/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ -n "${SUDO_USER:-}" ] && id "$SUDO_USER" >/dev/null 2>&1; then
    usermod -aG dialout "$SUDO_USER" 2>/dev/null || true
fi
exit 0
EOF
chmod 755 "$PKG_ROOT/DEBIAN/postinst"

find "$PKG_ROOT/opt" "$PKG_ROOT/usr" -type d -exec chmod 755 {} +
find "$PKG_ROOT/opt" "$PKG_ROOT/usr" -type f -exec chmod 644 {} +
chmod 755 "$PKG_ROOT/usr/bin/$APP_ID"
chmod 755 "$PKG_ROOT/opt/$APP_ID/bin/$APP_ID"
chmod 755 "$PKG_ROOT/opt/$APP_ID/scripts/moxa-helper.sh"

mkdir -p "$OUTPUT_DIR"
DEB_PATH="$OUTPUT_DIR/$APP_ID-${VERSION}${DISTRO_TAG:+-$DISTRO_TAG}-${ARCH}.deb"
dpkg-deb --root-owner-group --build "$PKG_ROOT" "$DEB_PATH" >/dev/null

echo "==> Paket hazır: $DEB_PATH"
