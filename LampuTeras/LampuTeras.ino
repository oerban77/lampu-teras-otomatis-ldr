// Lampu Teras Otomatis LDR — ESP32 + Sensor LDR + Relay
//
// Cara kerja:
//  - Sensor LDR (pembagi tegangan dengan resistor 10k) dibaca ESP32 lewat
//    ADC tiap 1 detik, dirata-rata dari 5 bacaan supaya tidak goyang.
//  - Gelap (nilai ADC di bawah ambang nyala) -> relay ON, lampu teras
//    menyala, LED hijau menyala. Terang (nilai ADC di atas ambang mati)
//    -> relay OFF, lampu dan LED mati.
//  - HYSTERESIS: ambang nyala (AMBANG_GELAP) berbeda dengan ambang mati
//    (AMBANG_TERANG), jadi lampu tidak berkedip saat cahaya di sekitar
//    ambang (misalnya senja atau lampu tetangga).
//  - Tombol override manual: tekan sekali = pindah ke mode manual dan
//    lampu berubah ke kondisi sebaliknya; tekan lagi = kembali ke mode
//    otomatis (lampu mengikuti sensor).
//  - Opsional: notifikasi Telegram setiap lampu berubah dan setiap ganti
//    mode. Set USE_TELEGRAM 1 dan isi WIFI_SSID, WIFI_PASS, BOT_TOKEN,
//    CHAT_ID di bawah. Default 0 = tanpa WiFi, tanpa Telegram.
//
// Wiring:
//  - LDR : satu kaki -> 3V3, kaki lain -> GPIO34 (ADC) + resistor 10k
//    ke GND (pembagi tegangan; gelap = resistansi LDR besar = ADC kecil)
//  - Relay (modul 1 channel, aktif LOW) : IN -> GPIO26,
//    VCC -> VIN 5V, GND -> GND; kontak COM/NO memutus kabel fasa lampu
//    220V (HATI-HATI: kabel 220V hanya untuk yang sudah paham listrik!)
//  - Lampu teras 220V : fasa lewat kontak relay, netral langsung
//  - LED hijau : GPIO32 (seri resistor 220 ohm ke GND), ikut status lampu
//  - Tombol override : GPIO14 -> GND (INPUT_PULLUP, aktif LOW)
//
// Library yang dibutuhkan (semua tersedia di Library Manager):
//  - UniversalTelegramBot 1.3.0 + ArduinoJson 6.21.5
//    (hanya bila USE_TELEGRAM 1; JANGAN pakai ArduinoJson v7)
// Compile: arduino-cli compile --fqbn esp32:esp32:esp32 LampuTeras
// (terverifikasi 20 Nov 2026 — angka compile ada di README.md)

#include <Arduino.h>

// ---------- KONFIGURASI ----------
#define USE_TELEGRAM 0 // 1 = kirim notifikasi Telegram, 0 = tanpa Telegram

#define PIN_LDR 34        // ADC1, input-only — sensor LDR
#define PIN_RELAY 26      // IN modul relay
#define PIN_LED_HIJAU 32  // ikut status lampu
#define PIN_TOMBOL 14     // override manual, aktif LOW

#define RELAY_AKTIF_LOW true // modul relay umum aktif LOW (IN LOW = kontak menutup)

#define AMBANG_GELAP 1500  // ADC di bawah ini = gelap -> lampu NYALA
#define AMBANG_TERANG 2000 // ADC di atas ini = terang -> lampu MATI
// (AMBANG_TERANG harus LEBIH BESAR dari AMBANG_GELAP — selisihnya adalah
//  hysteresis; rentang 1500..2000 = lampu mempertahankan kondisi terakhir)

#define JUMLAH_BACAAN 5        // bacaan ADC yang dirata-rata tiap pengukuran
#define INTERVAL_BACA_MS 1000  // baca sensor tiap 1 detik
#define DEBOUNCE_TOMBOL_MS 200 // abaikan pantulan tombol di bawah 200 ms
#define ADC_MAKS 4095          // resolusi ADC ESP32 12-bit (0..4095)

const char *WIFI_SSID = "NAMA_WIFI";
const char *WIFI_PASS = "PASSWORD_WIFI";
const char *BOT_TOKEN = "TOKEN_BOT_TELEGRAM";
const char *CHAT_ID = "CHAT_ID_TELEGRAM";

#if USE_TELEGRAM
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
WiFiClientSecure telegramClient;
UniversalTelegramBot bot(BOT_TOKEN, telegramClient);

void kirimTelegram(const String &pesan) {
  if (WiFi.status() != WL_CONNECTED) return;
  bot.sendMessage(CHAT_ID, pesan, "");
}
#else
void kirimTelegram(const String &) {}
#endif

// ---------- STATUS ----------
bool lampuNyala = false;  // kondisi lampu saat ini
bool modeManual = false;  // true = tombol override aktif, sensor diabaikan
unsigned long bacaTerakhir = 0;
unsigned long tekanTerakhir = 0;
bool tombolDulu = HIGH;

// Baca sensor LDR: rata-rata JUMLAH_BACAAN bacaan ADC (0..ADC_MAKS).
// Gelap = nilai kecil (LDR ke 3V3, resistor 10k ke GND).
int bacaLDR() {
  long total = 0;
  for (int i = 0; i < JUMLAH_BACAAN; i++) {
    total += analogRead(PIN_LDR);
    delay(10); // jeda kecil antar bacaan supaya ADC stabil
  }
  return (int)(total / JUMLAH_BACAAN);
}

// Terapkan kondisi lampu ke relay + LED hijau, laporkan perubahannya.
void aturLampu(bool nyala) {
  if (nyala == lampuNyala) return; // tidak ada perubahan, tidak usah kerja
  lampuNyala = nyala;
  digitalWrite(PIN_RELAY, nyala == RELAY_AKTIF_LOW ? LOW : HIGH);
  digitalWrite(PIN_LED_HIJAU, nyala ? HIGH : LOW);
  Serial.print("Lampu teras: ");
  Serial.println(nyala ? "NYALA" : "MATI");
  kirimTelegram(nyala ? "Lampu teras NYALA — hari sudah gelap."
                      : "Lampu teras MATI — hari sudah terang.");
}

// Kendali otomatis dengan hysteresis: nyalakan hanya bila ADC jatuh di
// bawah AMBANG_GELAP, matikan hanya bila ADC naik di atas AMBANG_TERANG.
// Di antara kedua ambang, lampu mempertahankan kondisi terakhir.
void kendaliLampu(int nilaiLDR) {
  if (nilaiLDR < AMBANG_GELAP) {
    aturLampu(true);
  } else if (nilaiLDR > AMBANG_TERANG) {
    aturLampu(false);
  }
  Serial.print("LDR: ");
  Serial.print(nilaiLDR);
  Serial.print(" / ");
  Serial.print(ADC_MAKS);
  Serial.print(" — lampu ");
  Serial.println(lampuNyala ? "NYALA" : "MATI");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED_HIJAU, OUTPUT);
  pinMode(PIN_TOMBOL, INPUT_PULLUP);
  // Mulai dengan lampu MATI (relay tidak aktif, LED mati).
  digitalWrite(PIN_RELAY, RELAY_AKTIF_LOW ? HIGH : LOW);
  digitalWrite(PIN_LED_HIJAU, LOW);

#if USE_TELEGRAM
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long mulai = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - mulai < 20000) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    telegramClient.setInsecure();
    Serial.println("WiFi tersambung, notifikasi Telegram aktif.");
  } else {
    Serial.println("WiFi gagal — lanjut tanpa Telegram.");
  }
#endif

  Serial.println("Lampu Teras Otomatis LDR siap.");
  Serial.print("Ambang gelap: ");
  Serial.print(AMBANG_GELAP);
  Serial.print(" | ambang terang: ");
  Serial.println(AMBANG_TERANG);
}

void loop() {
  // Tombol override (aktif LOW, debounce): tiap tekan valid menukar mode.
  // Masuk mode manual = lampu langsung berubah ke kondisi sebaliknya;
  // keluar mode manual = kembali ke kendali sensor otomatis.
  bool tombol = digitalRead(PIN_TOMBOL);
  if (tombol == LOW && tombolDulu == HIGH &&
      millis() - tekanTerakhir >= DEBOUNCE_TOMBOL_MS) {
    tekanTerakhir = millis();
    modeManual = !modeManual;
    if (modeManual) {
      aturLampu(!lampuNyala);
      Serial.println("Mode MANUAL (sensor diabaikan).");
      kirimTelegram("Mode manual: lampu teras diubah manual.");
    } else {
      Serial.println("Mode OTOMATIS (lampu ikut sensor LDR).");
      kirimTelegram("Mode otomatis: lampu teras ikut sensor LDR.");
    }
  }
  tombolDulu = tombol;

  // Baca sensor tiap INTERVAL_BACA_MS; hanya kendalikan lampu bila
  // masih mode otomatis.
  if (millis() - bacaTerakhir >= INTERVAL_BACA_MS) {
    bacaTerakhir = millis();
    int nilaiLDR = bacaLDR();
    if (!modeManual) {
      kendaliLampu(nilaiLDR);
    } else {
      Serial.print("LDR: ");
      Serial.print(nilaiLDR);
      Serial.println(" (mode manual — sensor diabaikan)");
    }
  }
}
