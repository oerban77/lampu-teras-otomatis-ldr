# Lampu Teras Otomatis LDR — ESP32 + Sensor LDR + Relay

Lampu teras yang menyala sendiri begitu hari mulai gelap, dan mati
sendiri saat pagi tiba. Sensor LDR membaca cahaya; begitu gelap, ESP32
menyalakan lampu lewat relay. Tidak ada lagi teras gelap saat pulang
malam, dan tidak ada lagi lampu yang menyala terus sampai pagi.

Video penjelasan (60 detik):
https://youtube.com/shorts/JUL71p4JdgM

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak alat |
| Sensor LDR + resistor 10k | Pembagi tegangan, baca cahaya (ADC GPIO34) |
| Relay 1 channel (aktif LOW) | Saklar lampu 220V (kontak NO) |
| Lampu teras 220V | Beban — fasa lewat kontak relay |
| LED hijau | Menyala mengikuti status lampu |
| Tombol | Override manual (INPUT_PULLUP) |

## Wiring

| Dari | Ke |
|---|---|
| LDR (satu kaki) | ESP32 3V3 |
| LDR (kaki lain / node) | ESP32 GPIO34 (ADC) + resistor 10k ke GND |
| Relay IN | ESP32 GPIO26 |
| Relay VCC | ESP32 VIN 5V |
| Relay COM / NO | Fasa lampu 220V (netral langsung ke lampu) |
| LED hijau | ESP32 GPIO32 (seri 220 Ω ke GND) |
| Tombol | ESP32 GPIO14 → GND |
| Semua GND | Digabung |

> ⚠️ HATI-HATI: relay memutus kabel fasa 220V. Rakit dan uji hanya
> bila kamu sudah paham listrik 220V — atau minta bantuan yang ahli.

Skema lengkap: [schematic.png](schematic.png)

## Cara kerja

1. ESP32 membaca sensor LDR tiap 1 detik, dirata-rata dari 5 bacaan
   supaya nilainya tidak goyang.
2. Gelap (nilai ADC di bawah `AMBANG_GELAP` 1500) → relay ON, lampu
   teras menyala, LED hijau menyala.
3. Terang (nilai ADC di atas `AMBANG_TERANG` 2000) → relay OFF, lampu
   dan LED mati.
4. HYSTERESIS: ambang nyala (1500) berbeda dengan ambang mati (2000),
   jadi lampu tidak berkedip saat cahaya di sekitar ambang (misalnya
   senja atau lampu tetangga).
5. Tombol override: tekan sekali = pindah ke mode manual dan lampu
   berubah ke kondisi sebaliknya; tekan lagi = kembali ke mode
   otomatis (lampu mengikuti sensor).
6. Notifikasi Telegram terkirim setiap lampu berubah dan setiap ganti
   mode (bila `USE_TELEGRAM` 1).

## Konfigurasi (di awal sketch)

- `AMBANG_GELAP` (1500) — ADC di bawah ini = gelap, lampu NYALA.
- `AMBANG_TERANG` (2000) — ADC di atas ini = terang, lampu MATI.
  Harus LEBIH BESAR dari `AMBANG_GELAP`; selisihnya adalah hysteresis.
  Sesuaikan dengan kondisi terasmu (cek nilai ADC lewat Serial Monitor).
- `JUMLAH_BACAAN` (5) — bacaan ADC yang dirata-rata tiap pengukuran.
- `INTERVAL_BACA_MS` (1000) — baca sensor tiap 1 detik.
- `RELAY_AKTIF_LOW` (true) — modul relay umum aktif LOW.
- `USE_TELEGRAM` (0) — kirim notifikasi Telegram. Set 1 dan isi
  `WIFI_SSID`, `WIFI_PASS`, `BOT_TOKEN`, `CHAT_ID` untuk mengaktifkannya.

## Library yang dibutuhkan

| Library | Versi | Keterangan |
|---|---|---|
| ArduinoJson | 6.21.5 | Dipakai UniversalTelegramBot (WAJIB v6 — v7 tidak dipakai di sini) |
| UniversalTelegramBot | 1.3.0 | Notifikasi Telegram (hanya bila `USE_TELEGRAM` 1) |

Semua library di atas juga tersedia sebagai ZIP di folder [`libraries/`](libraries/)
— install lewat Arduino IDE: Sketch → Include Library → Add .ZIP Library.

## Compile

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 LampuTeras
```

Terverifikasi 20 Nov 2026: sketch 298.293 byte (22% flash),
RAM 23.312 byte (7%), ESP32 core 4.0.0.

## Lisensi

MIT — lihat [LICENSE](LICENSE).
