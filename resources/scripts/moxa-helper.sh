#!/bin/bash
set -euo pipefail

TARGET_MODULE="mxu11x0"
COMPETING_MODULES=("ti_usb_3410_5052" "mxuport")
MXUPORT_BLACKLIST_FILE="/etc/modprobe.d/blacklist-mxuport.conf"
UDEV_RULE_FILE="/etc/udev/rules.d/99-moxa-uport-rs485.rules"

die() {
    echo "HATA: $*" >&2
    exit 1
}

require_driver_src() {
    local src="$1"
    [ -n "$src" ] || die "sürücü kaynak dizini verilmedi"
    [ -f "$src/mxu11x0/driver/Makefile" ] || die "geçersiz sürücü kaynak dizini: $src"
}

ensure_build_deps() {
    local kver
    kver="$(uname -r)"
    local need_install=0

    command -v make >/dev/null 2>&1 || need_install=1
    command -v gcc >/dev/null 2>&1 || need_install=1
    command -v setserial >/dev/null 2>&1 || need_install=1
    [ -d "/lib/modules/$kver/build" ] || need_install=1

    if [ "$need_install" -eq 1 ]; then
        echo "==> Derleme bağımlılıkları kuruluyor (build-essential, linux-headers-$kver, setserial)..."
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq || echo "uyarı: apt-get update başarısız oldu, mevcut paket önbelleğiyle devam ediliyor" >&2
        apt-get install -y build-essential "linux-headers-$kver" setserial
    fi

    [ -d "/lib/modules/$kver/build" ] || die "çekirdek $kver için build dizini bulunamadı; linux-headers-$kver paketi kurulamadı"
}

do_build() {
    local src="$1"
    require_driver_src "$src"
    ensure_build_deps
    echo "==> Sürücü derleniyor (çekirdek $(uname -r))..."
    ( cd "$src/mxu11x0" && make clean >/dev/null 2>&1 || true )
    ( cd "$src/mxu11x0" && make )
    echo "==> Derleme tamamlandı."
}

do_install() {
    local src="$1"
    do_build "$src"

    echo "==> Rakip sürücüler devre dışı bırakılıyor..."
    if ! grep -q "^blacklist mxuport$" "$MXUPORT_BLACKLIST_FILE" 2>/dev/null; then
        echo "blacklist mxuport" >> "$MXUPORT_BLACKLIST_FILE"
    fi
    for mod in "${COMPETING_MODULES[@]}"; do
        rmmod "$mod" 2>/dev/null || true
    done

    echo "==> Moxa mxu11x0 sürücüsü kuruluyor..."
    ( cd "$src/mxu11x0" && make install )

    rmmod "$TARGET_MODULE" 2>/dev/null || true
    modprobe "$TARGET_MODULE"

    echo "==> Kurulum tamamlandı: $(modinfo -F version "$TARGET_MODULE" 2>/dev/null || echo bilinmiyor)"
}

do_remove() {
    local src="$1"
    require_driver_src "$src"

    echo "==> Moxa mxu11x0 sürücüsü kaldırılıyor..."
    ( cd "$src/mxu11x0" && make remove )
    rm -f "$MXUPORT_BLACKLIST_FILE"

    echo "==> Kaldırma tamamlandı."
}

do_set_mode() {
    local device="$1"
    local mode="$2"

    [[ "$device" =~ ^/dev/ttyUSB[0-9]+$ ]] || die "geçersiz aygıt yolu: $device"
    [[ "$mode" =~ ^[0-3]$ ]] || die "geçersiz mod değeri: $mode (0-3 olmalı)"
    [ -e "$device" ] || die "aygıt bulunamadı: $device"

    setserial "$device" port "$mode"
    echo "==> $device -> port $mode uygulandı."
}

do_install_udev() {
    local vid="$1"
    local pid="$2"
    local mode="$3"

    [[ "$vid" =~ ^[0-9a-fA-F]{4}$ ]] || die "geçersiz idVendor: $vid"
    [[ "$pid" =~ ^[0-9a-fA-F]{4}$ ]] || die "geçersiz idProduct: $pid"
    [[ "$mode" =~ ^[0-3]$ ]] || die "geçersiz mod değeri: $mode (0-3 olmalı)"

    printf 'SUBSYSTEM=="tty", ATTRS{idVendor}=="%s", ATTRS{idProduct}=="%s", RUN+="/usr/bin/setserial /dev/%%k port %s"\n' \
        "$vid" "$pid" "$mode" > "$UDEV_RULE_FILE"

    udevadm control --reload-rules
    udevadm trigger
    echo "==> Kalıcı udev kuralı yazıldı: $UDEV_RULE_FILE"
}

do_remove_udev() {
    rm -f "$UDEV_RULE_FILE"
    udevadm control --reload-rules
    udevadm trigger
    echo "==> Kalıcı udev kuralı kaldırıldı."
}

cmd="${1:-}"
shift || true

case "$cmd" in
    build)          do_build "$@" ;;
    install)        do_install "$@" ;;
    remove)         do_remove "$@" ;;
    set-mode)       do_set_mode "$@" ;;
    install-udev)   do_install_udev "$@" ;;
    remove-udev)    do_remove_udev "$@" ;;
    *) die "bilinmeyen komut: $cmd" ;;
esac
