# Moxa UPort Manager

Moxa UPort 11x0 serisi (UPort 1110/1130/1130I/1150/1150I, USB Console) USB-seri
adaptörleri için Qt tabanlı bir Linux GUI aracı. İki şeyi otomatikleştirir:

1. **Sürücü yönetimi** — Moxa'nın kendi `mxu11x0` çekirdek modülünü, uygulama
   içine gömülü kaynak kodundan, çalışan çekirdeğe göre derler/kurar/günceller/
   kaldırır. Ubuntu 22.04–24.04 arası (farklı çekirdek sürümleri: 6.8, 7.0, …)
   otomatik olarak desteklenir; sürücü, dağıtım sürümüne değil, o an çalışan
   çekirdeğin başlık dosyalarına göre derlenir.
2. **Port ayarları** — bağlı Moxa portlarının RS-232/RS-422/RS-485 (2 telli /
   4 telli) arayüz modunu, terminalde elle çalıştırılan
   `sudo setserial /dev/ttyUSBx port N` komutunun GUI karşılığıyla ayarlar;
   isteğe bağlı olarak kalıcı bir udev kuralıyla bu modu her takılışta
   otomatik uygular.

Bu proje, [`resources/driver/mxu11x0`](resources/driver/mxu11x0) altındaki
Moxa'nın resmi `moxa-uport-1100-series-linux-kernel-6.x-driver-v7.0` sürücü
kaynağının derlenmiş dosyalardan arındırılmış bir kopyasını içerir (yeni
çekirdeklerde derlenebilmesi için gereken `mxu1_break` imza düzeltmesi zaten
uygulanmış durumda).

## Klasör yapısı

```
app/        MainWindow + mainwindow.ui (GUI, Qt Designer ile düzenlenebilir) + main()
core/       İş mantığı: sistem/çekirdek tespiti, sürücü yönetimi, port yönetimi,
            (pkexec ile) ayrıcalıklı komut çalıştırma
resources/
  driver/   Gömülü Moxa mxu11x0 sürücü kaynağı (uygulamayla birlikte paketlenir)
  scripts/  moxa-helper.sh — pkexec ile root olarak çalıştırılan tek yetkili betik
packaging/
  linux/    app.desktop, icon.png, DeployLinux.cmake, build-deb-installer.sh, docker/
package/    build_linux.sh, build_linux_ubuntu22.04.sh ve hazır .deb çıktıları
version.txt Uygulama adı/sürümü (CMake ve paketleme betiği buradan okur)
```

## Nasıl çalışır (mimari)

* GUI hiçbir zaman doğrudan root olarak çalışmaz. Root gerektiren tüm
  işlemler (`build`, `install`, `remove`, `set-mode`, `install-udev`,
  `remove-udev`) `pkexec resources/scripts/moxa-helper.sh <komut> …` ile tek
  bir betiğe yönlendirilir; kullanıcı standart PolicyKit kimlik doğrulama
  penceresini görür. Betik, aldığı her argümanı (aygıt yolu, VID/PID, mod
  değeri) sıkı bir regex ile doğrular.
* Sürücü sorgulama, port listeleme ve mevcut modu okuma (`setserial -G`)
  root gerektirmez ve doğrudan uygulama içinden çalışır.
* `core/systeminfo.cpp`, gömülü kaynakları çalışma zamanında şu sırayla arar:
  `MOXA_GUI_RESOURCE_DIR` ortam değişkeni → `/opt/<app-id>` (paketlenmiş
  kurulum) → derleme dizinindeki `resources/` (geliştirme sırasında).

## Geliştirme sırasında derleme ve çalıştırma

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
./build/MoxaUportManager
```

Sürücü sorgulama ve port listeleme (root gerektirmeyen kısımlar) doğrudan
çalışır. `pkexec` ile başlayan işlemler (Kur/Güncelle, Kaldır, mod uygulama,
kalıcı kural) için, `build/` dizini paketlenmemiş olduğundan PolicyKit bazı
sistemlerde ek uyarı gösterebilir; gerçek davranışı görmek için aşağıdaki
`.deb` paketiyle kurup denemeniz önerilir.

## `.deb` paketi oluşturma (Ubuntu 22.04 ve 24.04 için ayrı ayrı)

Her Ubuntu sürümü için paketi **o sürümün kendisinde** derlemek gerekir, çünkü
`build-deb-installer.sh` paketin `Depends:` alanına, derleme makinesinde
fiilen kurulu olan Qt paketlerinin (ve glibc'nin) sürümünü yazar — 22.04'te
`libqt6widgets6`, 24.04'te `libqt6widgets6t64` gibi. `package/` altındaki
iki betik bu farkı kendisi hallediyor:

```bash
./package/build_linux.sh                 # bu makinenin kendi Ubuntu sürümü için
./package/build_linux_ubuntu22.04.sh      # 24.04'te çalışsanız bile, Docker içinde
                                           # gerçek bir Ubuntu 22.04 ile derler
```

Çıktı `package/linux-ubuntu<sürüm>/moxauportmanager-<versiyon>-ubuntu<sürüm>-amd64.deb`
olarak oluşur (bu depoda hazır iki örnek de mevcut). Kurulum:

```bash
sudo dpkg -i package/linux-ubuntu24.04/moxauportmanager-1.0.0-ubuntu24.04-amd64.deb
sudo apt -f install   # eksik bağımlılık varsa
```

`/opt/moxauportmanager` altına kurar, `/usr/bin/moxauportmanager`
başlatıcısını ve uygulama menüsü girdisini ekler, kullanıcıyı `dialout`
grubuna dahil eder (seri port erişimi için).

`docker` yoksa 22.04 betiği çalışmaz; o durumda gerçek bir Ubuntu 22.04
makinesinde/VM'inde `./package/build_linux.sh` çalıştırmak yeterlidir —
sonucu otomatik olarak `linux-ubuntu22.04/` altına yazar.

## Manuel doğrulama / sorun giderme

`RS485_KURULUM_README.md` (sürücü kaynağının yanındaki orijinal not) bu
uygulamanın otomatikleştirdiği adımların elle karşılığını içerir:

```bash
lsmod | grep mxu11x0
modinfo mxu11x0
readlink -f /sys/class/tty/ttyUSB0/device/driver   # .../drivers/mxu1150 görmelisiniz
setserial -G /dev/ttyUSB0                          # port 0x0001 = RS-485 2W
```

Mod değer tablosu: `0=RS-232, 1=RS-485 2W, 2=RS-422, 3=RS-485 4W`.
