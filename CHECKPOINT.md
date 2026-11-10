# Checkpoint Zenith Dot — 2026-11-10

Serah terima untuk agent berikutnya (Codex). Isinya posisi kerja terakhir, cara kodenya disusun, sisa pekerjaan, dan aturan main dari user.

## Posisi sekarang

- Branch `main` di `4f9d681`, sudah di-push ke `origin` (github.com/alifihsan7/Zenith-Dot). Working tree bersih kecuali file ini.
- Build terbaru sudah ter-install di `%LOCALAPPDATA%\Zenith Dot\ZenithDot.exe` dan jalan saat login.
- Proyek: overlay "dynamic island" untuk Windows 11 dalam C++20, Win32, Direct2D, DirectWrite, dan C++/WinRT. Satu proses, satu jendela layered di tengah atas monitor utama.
- Desain acuannya preview HTML `experiments/zenith-dot-v7.html`. Folder `experiments/` di-gitignore, jadi cuma ada di mesin lokal. Catatan keputusan dan backlog ada di `experiments/NOTES.md`; baca itu juga.

## Progres port V7 ke native

| Tahap | Isi | Status |
|---|---|---|
| 1 | Dot ↔ pill, panel hover (Now Playing, Focus, System), morph cover dan judul, warna aksen dari cover | Selesai, `55c4fed` |
| 2 | Bubble split liquid: Copy, charger, baterai lemah, Focus done; alert inline saat panel terbuka | Selesai, `4f9d681` |
| 3 | Sembunyi saat fullscreen, indikator mic dan kamera, HUD volume dan brightness | Sudah diimplementasikan dan build Release lulus. Verifikasi di desktop interaktif masih terbuka. |
| 4 | Scratchpad dan lirik | Panel lirik penuh. Scratchpad masih terbuka. |

Sebelumnya juga sudah selesai: island tidak mencuri fokus (`WS_EX_NOACTIVATE` + `MA_NOACTIVATE`) dan warna aksen ikut cover (`6efa8bf`).

## Build, install, jalankan

Visual Studio 2022 (Community dan Build Tools) serta CMake sudah terpasang. CMake ada di `C:\Program Files\CMake\bin`. Shell yang sudah lama terbuka mungkin belum kenal PATH-nya; refresh dulu:

```powershell
$env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')
cmake -S . -B out -G "Visual Studio 17 2022" -A x64
cmake --build out --config Release --parallel
```

- Target build bersih: nol error dan nol warning di `/W4`. Jaga tetap begitu.
- Sebelum menimpa exe atau menjalankan build baru, matikan instance lama: `Get-Process ZenithDot | Stop-Process -Force`. Aplikasinya single-instance lewat mutex, jadi instance kedua langsung keluar.
- Install: `.\scripts\install.ps1 -Executable .\out\Release\ZenithDot.exe`
- GitHub Actions tidak bisa dipakai: akun user terkunci karena masalah billing. Build dan tes harus lokal.

## Peta kode

| File | Isi |
|---|---|
| `src/main.cpp` | `App`: jendela, tray, timer, routing pointer, clipboard listener, event daya (charger dan baterai lemah) |
| `src/island.hpp/.cpp` | `Island`: seluruh UI. State, animasi, hit testing, dan rendering Direct2D |
| `src/media.hpp/.cpp` | `MediaMonitor`: sesi GSMTC, metadata, cover (96×96, crop tengah), warna dari cover, daftar sumber, seek, pilih sumber |
| `src/power.*` | Status baterai dan AC |
| `src/system.*` | CPU (`GetSystemTimes`) dan RAM untuk layar System |
| `src/desktop.*` | Sinyal fullscreen, aktivitas mic/kamera, volume Core Audio, dan brightness WMI |

### Hal penting di `Island`

- **Koordinat "stage" dalam DIP:** x = 0 adalah garis tengah monitor, y = 0 adalah tepi atas work area. Tiap frame, surface layered dipotong pas ke area yang digambar (`draw()` → `bounds`). `origin_` adalah offset pixel surface, `anchor_` adalah titik tengah atas monitor di koordinat layar.
- **Hit testing layered window:** pointer cuma diterima di pixel yang alpha-nya bukan nol. Area hover pill dicat hitam dengan alpha 1/255 supaya bisa di-hover tapi tidak kelihatan. Target klik dicatat ulang tiap frame di `targets_`.
- **Siklus frame:** `App::frame()` memanggil `island_.tick()` → `draw()` → `frameInterval()`. Intervalnya 10 ms saat ada animasi, 50 ms saat panel terbuka atau alert tampil, 33 ms untuk waveform, 250 ms saat menunggu, dan 0 (timer mati) saat diam.
- **Hover:** masuk butuh pointer di area pill asli (delay 50 ms). Saat panel terbuka, `tick()` mem-poll `GetCursorPos`: panel tetap terbuka selama pointer dalam 28 px dari pinggirnya, tutup 185 ms setelah keluar, dan klik di luar panel langsung menutup (lewat `GetAsyncKeyState`). `keepHeight_` menahan tinggi panel lama sebagai area dekat setelah ganti layar, sampai pointer masuk ke ukuran panel yang baru. Ini perbaikan bug: klik tab System membuat panel menyusut dan langsung tertutup.
- **Alert** (`notify` → `startAlert` → `tick` → `finishAlert`): bubble buka 540 ms, tahan `hold` (default 1800 ms), tutup 440 ms. Alert yang datang saat ada alert lain diantrekan berdasarkan prioritas. Copy beruntun memperbarui bubble yang sedang tampil. Saat panel terbuka, alert tampil inline 2 detik. Pill tidak boleh berubah bentuk selama bubble tampil (`bubbleShowing()`).
- **Geometri split liquid** (`neck`, `connectedPath`, `releasedPath`, `PathBuilder`) adalah port 1:1 dari JS V7, termasuk konstanta `pinchAt`, `releaseGap`, `neckSpread`, dan `handleSize`. Kalau mengubah perilakunya, ubah di preview dulu.
- Semua ukuran dan timing diambil dari V7 dan disimpan sebagai konstanta di atas `island.cpp`.

## Tahap 3: acuan implementasi

Perilaku acuan ada di V7: cari `fullscreen`, `mic`, `camera`, `volume`, dan `brightness` di `experiments/zenith-dot-v7.html`.

1. **Sembunyi saat fullscreen.** V7: island hilang (opacity ke 0 dan geser −14 px dalam 200 ms), panel ditutup, dan alert diabaikan. Deteksinya pakai `SHQueryUserNotificationState` (`QUNS_BUSY`, `QUNS_RUNNING_D3D_FULL_SCREEN`, `QUNS_PRESENTATION_MODE`), ditambah cek apakah jendela foreground menutupi seluruh monitor (kecualikan desktop `Progman`/`WorkerW` dan shell). Pemicunya `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` dan poll ringan.
2. **Indikator mic dan kamera.** Data dari registry `HKCU\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\{microphone,webcam}`, termasuk subkey `NonPackaged`. Sebuah app sedang memakai perangkat kalau `LastUsedTimeStop == 0` dan `LastUsedTimeStart != 0`. Pantau perubahannya dengan `RegNotifyChangeKeyValue` (subtree) plus event. V7: dot idle berwarna oranye `#ff9f0a` (mic) atau hijau `#30d158` (kamera; kamera menang kalau keduanya aktif), pill menampilkan titik 6 px di kanan, dan saat mulai dipakai muncul bubble "Microphone"/"Camera" dengan nama app-nya (priority 2).
3. **HUD volume dan brightness.** V7: bubble dengan meter dan nilai persen (`kind` volume/brightness) yang diperbarui di tempat kalau tombol ditekan berulang. Volume bisa dideteksi lewat `IAudioEndpointVolume::RegisterControlChangeNotify`. Brightness lewat WMI `WmiMonitorBrightnessEvent`, atau poll `WmiMonitorBrightness`. **Risiko:** tidak ada API resmi untuk menekan flyout volume bawaan Windows 11. Saran: tampilkan HUD tanpa menekan flyout dulu, lalu tanyakan user apakah itu cukup. Untuk alert dengan `kind` HUD, perluas cabang update-di-tempat di `Island::notify` (sekarang cuma untuk Copy) dan tambahkan field meter ke `IslandAlert`.

## Tahap 4 dan hal yang perlu keputusan user

- **Scratchpad** butuh input teks, jadi island harus boleh mengambil fokus saat area teks diklik, lalu mengembalikan fokus ke app sebelumnya saat panel ditutup (pengecualian ini sudah dicatat di NOTES). Butuh kontrol edit sendiri, karena kontrol Win32 standar tidak tergambar di jendela layered dengan `UpdateLayeredWindow`.
- **Lirik:** Lookup ke LRCLIB saat dibuka. Kolom menampilkan lirik sepanjang lagu dengan wrapping dan auto-scroll. Up Next dihapus dari lingkup produk sesuai revisi user.
- Masih terbuka di NOTES: multi-monitor dan DPI. Sekarang island cuma di monitor utama.

## Cara tes yang sudah terbukti

Tidak ada test otomatis. Verifikasi dilakukan langsung di layar user:

- **Screenshot:** proses PowerShell `SetProcessDPIAware()`, lalu `Graphics.CopyFromScreen` di area tengah atas work area, lalu lihat PNG-nya.
- **Hover dan klik:** `SetCursorPos` dan `mouse_event`. **Simpan posisi kursor dan kembalikan setelahnya.** User sedang memakai mesinnya, jadi bilang ke user kalau kursornya akan bergerak.
- **Animasi split:** set clipboard (`Set-Clipboard`) untuk memicu Copy, lalu ambil beberapa frame pada ms tertentu. **Simpan isi clipboard user dulu dan kembalikan.**
- Posisi di layar (stage DIP pada skala 100%): pill y 8–48, tinggi panel Now Playing 236, nav di y 205–236. Tombol nav ada di x = −43, 0, dan +43 dari tengah.

## Aturan main dari user

- Ngobrol pakai bahasa Indonesia santai (lu/gue).
- **Commit tanpa trailer Co-Authored-By.** Commit dan push hanya kalau diminta.
- Ide UI baru dirombak dulu di preview `experiments/zenith-dot-vN.html`, baru di-port ke `src/` setelah user setuju. Port V7 sendiri sudah disetujui.
- "catat" artinya tulis ke `experiments/NOTES.md`. "catat dulu" artinya cuma ditulis, jangan diimplementasi.
- Update `README.md` (bagian fitur dan langkah verifikasi) serta `experiments/NOTES.md` setiap kali sebuah tahap selesai.
