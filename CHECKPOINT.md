# Checkpoint Zenula — 2026-11-14

Serah terima untuk sesi berikutnya. Kode adalah acuan perilaku; README cuma ringkasan fitur.

## Posisi sekarang

- `main` sinkron dengan `origin/main` di `2ace78e`. Build Release bersih (nol warning, `/W4`).
- Sesi 2026-11-13/14 menambah: ikon burung hantu dan logo di Home, jam Home dengan zona (WIB/WITA/WIT), Scratchpad dua mode (Notes dan Tasks), slider brightness di System, picker lokasi sholat dengan pencarian di dalam panel Prayer, island yang opaque, warna Scratchpad/Drop Zone yang ikut aksen sesi, dan redesign Settings (palet netral, halaman Prayer sendiri).
- Settings di-redesign total (2026-11-14): lima halaman (General, Island, Features, Alerts & privacy, Prayer), kartu bergaris tipis, segmented satu track, tombol Try di dalam baris, urutan fitur langsung di daftar, preview pill statis. Dialog lokasi ikut gaya yang sama. Mockup: `experiments/zenula-settings-redesign.html` (folder `experiments` di-ignore git).
- Dot only (2026-11-15) menggantikan Auto-shrink: island tetap dot, hover membuka panel, bubble tetap muncul. Saklarnya di panel Home dan Settings > General > Behavior.
- Sesi 2026-11-15: Dot only menggantikan Auto-shrink; chime Prayer (satu nada sinus, bukan lonceng) dan Timer; sesi Focus (template, Custom, rounds, DND otomatis); Timers dan Home di-redesign (timeline sesi, jam besar); pemilih sumber media jadi daftar seperti Audio Output; pesan error Settings jadi dialog gelap. Lanjutan ada di `experiments/NOTES.md`: redesign fitur berikutnya mulai dari Now Playing (mockup dulu), lalu aksen per fitur dan tema.
- Fitur yang sengaja dibuang: bubble privasi (Camera/Mic/Screen cuma jadi ikon), agenda kalender di Home, tile Wi-Fi/Bluetooth/mode daya.
- RAM dan CPU (2026-11-14, musik diputar, pill dengan visualizer): private memory ~15 MB dan CPU ~0,5% satu core, sama dengan awal sesi. Tanpa musik, timer frame mati total.
- Settings (proses terpisah) memakai Direct2D software: sekitar 8 MB private memory saat dibuka, stabil di sekitar 16 MB setelah berpindah halaman. Target hardware dulu memuat driver NVIDIA dan memakan sekitar 57 MB.

## Belum diuji di layar

- Slider brightness di layar laptop (jalur WMI). PC user laptop, tapi layar utamanya monitor DDC/CI; uji ini dilewati atas permintaan user (2026-11-14).

## Build dan jalankan

```powershell
$env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')
Get-Process Zenula -ErrorAction SilentlyContinue | Stop-Process -Force
cmake --build out\Zenula --config Release --parallel
Start-Process out\Zenula\Release\Zenula.exe
```

- Setelah perubahan kode, matikan Zenula, build `out\Zenula`, lalu jalankan lagi (user minta ini otomatis). Kalau mematikan proses diblokir, build ke folder terpisah (`cmake -S . -B out\Check`) untuk cek warning, lalu minta user restart.
- Build directory yang dipakai `out\Zenula` (`out\CMakeCache.txt` masih menunjuk ke folder lama `zenith-dot`). Kalau menambah file `.cpp`, jalankan `cmake -S . -B out\Zenula` dulu.
- User belum memasang installer; Zenula jalan langsung dari `out\Zenula\Release\Zenula.exe`.
- Settings dibuka dengan `[Diagnostics.Process]::Start($exe, '--settings')`. `Start-Process -ArgumentList '--settings'` tidak dikenali.
- GitHub Actions tidak bisa dipakai (akun terkunci billing). Build dan tes lokal.

## Peta kode

| File | Isi |
|---|---|
| `src/island.*` | Seluruh UI island: state, animasi, hit testing, Direct2D |
| `src/main.cpp` | `App`: jendela, tray, timer, input, popup edit (timer, task, cari lokasi), integrasi Windows |
| `src/settings_window.cpp` | Jendela Settings: satu pass Direct2D menggambar semua; tombol tak terlihat cuma memegang fokus, Tab, Alt+huruf, dan nama untuk screen reader |
| `src/prayer_picker.cpp` | Dialog lokasi sholat untuk Settings |
| `src/tasks.*` | Daftar Tasks, disimpan di `%LOCALAPPDATA%\Zenith Dot\tasks.txt` |
| `src/scratchpad.*` | Editor dan file catatan `scratchpad.txt` |
| `src/brightness.*` | Brightness layar island: DDC/CI, cadangan WMI, di worker thread |
| `src/media.*`, `src/lyrics.*`, `src/audio_spectrum.*` | Sesi media, lirik LRCLIB, visualizer |
| `src/prayer*` | Jadwal SIHAT Kemenag dan 517 lokasi |

### Aturan animasi di `island.cpp`

- Bentuk island (dot, pill, panel, bubble) memakai spring 500 ms yang bisa berbalik arah.
- Isi panel memakai tiga tingkat: `fastMotion` 120 ms (hover), `baseMotion` 180 ms (toggle, menu, digit), `moveMotion` 260 ms (baris, sorotan, crossfade, slider). Pengecualian disengaja: bubble, lirik, roll jam 320 ms.
- Baris daftar (Drop Zone, Tasks, App Volume, Bluetooth, Home) masuk dengan fade dan geser 6 px, yang hilang memudar, dan perubahan beruntun melanjutkan dari posisi yang terlihat.
- Semua animasi cuma jalan setelah interaksi atau perubahan nyata, lalu timer frame berhenti. Animasi baru harus menambah syaratnya ke `moving()`.

## Cara tes di layar

Tidak ada test otomatis. Verifikasi lewat skrip PowerShell di desktop user:

- Screenshot: `SetProcessDPIAware()` lalu `Graphics.CopyFromScreen`. `PrintWindow` kadang melewatkan teks di tombol owner-draw yang sedang fokus; kalau ragu, cek ulang dengan `CopyFromScreen`. Panel Settings atau dialog bisa di-capture lewat `GetWindowRect`.
- Kursor dan klik: `SetCursorPos` dan `mouse_event`. Simpan posisi kursor dan kembalikan.
- **Keyboard: cek dulu jendela foreground milik proses Zenula sebelum `keybd_event`.** Tanpa cek ini, ketikan pernah masuk ke chat user.
- Island di layar user tampil dengan skala sekitar 125%. Ambil koordinat klik dari screenshot, jangan dari angka DIP di kode.
- Panel island terbuka lewat hover pada pill, bukan klik. Untuk mengklik isi panel, hover dulu lalu klik dalam satu urutan; panel menutup begitu cursor dikembalikan.
- Jangan ubah data user tanpa backup: `scratchpad.txt`, `tasks.txt`, clipboard, brightness monitor, dan registry `HKCU\Software\Zenith Dot`. Kembalikan setelah tes.
- Jendela overlay tidak terlihat dari shell yang di-sandbox; jalankan skrip interaksi tanpa sandbox.
- ID untuk `BM_CLICK` di Settings: halaman 1010 General, 1011 Island, 1012 Features, 1015 Alerts & privacy, 1017 Prayer; detail fitur 1040–1044, kembali 1025; ganti lokasi 1071; reset 1016 (memunculkan konfirmasi Yes/No); pindah urutan 1080+fitur (naik) dan 1085+fitur (turun); pilihan segmented 1300 + baris × 4 + opsi (Rhythm: 1316–1319); stepper Custom 1100–1105 (Focus, Break, Rounds −/+).
- `reg import` tidak menghapus value yang baru dibuat selama tes (misalnya `FeatureOrderV3` setelah tes urutan). Cek dan hapus value itu, lalu kirim `WM_APP + 13` ke `ZenithDot.Overlay` supaya island memuat ulang. Dari PowerShell, `FindWindowW` butuh `[NullString]::Value`, bukan `$null`.

## Aturan main dari user

- Bahasa Indonesia santai (lu/gue). UI tetap bahasa Inggris.
- Efisiensi RAM dan CPU di atas segalanya. Tidak ada polling atau animasi yang jalan terus.
- Commit pakai `feat:`, `fix:`, `style:` (atau `docs:`), tanpa `Co-authored-by`. Commit dan push setelah user setuju, atau langsung kalau user minta "commit push per kerjaan".
- "catat" artinya tulis ke `experiments/NOTES.md`; "catat dulu" artinya jangan diimplementasi.
