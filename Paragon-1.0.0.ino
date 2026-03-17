//Nama Program    :     PARAGON SMART LIGHTING BOX 
//Author          :     Dimas Ramadhani Ario Prayitno
//Versi           :     1.0.0
//Ownership       :     PT MAKERINDO PRIMA SOLUSI 
//Tanggal         :     15 Maret 2026
//Deskripsi       :     Membaca sensor BH1750 dan menampilkan nilai lux ke Nextion Display


#include <Wire.h>
#include <BH1750.h>
#include <HardwareSerial.h>

// --- Definisi Pin ---
#define PIN_BUZZER 25
#define PIN_RX_NEXTION 16
#define PIN_TX_NEXTION 17
#define SDA 21
#define SCL 22


// --- Inisialisasi Objek ---
BH1750 lightMeter;
HardwareSerial nexSerial(2);

// Variabel Global
int lastLux = -1;           // Untuk cek perubahan angka (t1)
String lastStatusText = ""; // Untuk cek perubahan teks (t2)
int lastLevel = 0;          // Untuk melacak level kelipatan 1000 terakhir

void setup() {
  Serial.begin(115200);
  
  // Inisialisasi Nextion
  nexSerial.begin(9600, SERIAL_8N1, PIN_RX_NEXTION, PIN_TX_NEXTION);
  
  // Inisialisasi Sensor
  Wire.begin(21, 22);
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Siap"));
  } else {
    Serial.println(F("Error BH1750"));
  }
  
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, 0);
}

void loop() {
  // --- 1. STABILISASI (Rata-rata 10 sampel) ---
  long totalRaw = 0;
  for(int i = 0; i < 15; i++) {
    totalRaw += lightMeter.readLightLevel();
    delay(10); 
  }
  int avgRaw = totalRaw /15;

  // --- 2. KALIBRASI (Dibagi 25--**20) ---
  int calibratedLux = avgRaw/5;

  Serial.print("Lux: ");
  Serial.println(calibratedLux);

  // --- 3. UPDATE DISPLAY ANGKA (Object t1) ---
  if (calibratedLux != lastLux) {
    sendNextionCommand("t1.txt=\"" + String(calibratedLux) + "\"");
    lastLux = calibratedLux;
  }

  // --- 4. UPDATE DISPLAY TEKS STATUS (Object t2) ---
  // String currentText = "";
  // if (calibratedLux < 1000) {
  //   currentText = "Nilai Lux Belum Terpenuhi";
  // } 
  // else if (calibratedLux >= 1000 && calibratedLux < 2000) {
  //   currentText = "Nilai Lux terpenuhi";
  // } 
  // else if (calibratedLux >= 2050) {
  //   currentText = "Nilai Lux Maksimal";
  // }

  // // Kirim ke Nextion hanya jika teks berubah
  // if (currentText != lastStatusText) {
  //   sendNextionCommand("t2.txt=\"" + currentText + "\"");
  //   lastStatusText = currentText;
  // }

  // --- 5. LOGIKA BUZZER KELIPATAN 1000 ---
  // Kita hitung kita sedang berada di "Lantai" berapa.
  // Rumus: Lux / 1000. 
  // Contoh: 900 -> 0 | 1000 -> 1 | 2500 -> 2 | 3000 -> 3
  int currentLevel = calibratedLux / 1000;

  // SYARAT BUNYI:
  // 1. Level sekarang LEBIH TINGGI dari level terakhir (Naik tangga)
  // 2. Level sekarang bukan 0 (agar tidak bunyi saat alat baru nyala di gelap)
  if (currentLevel > lastLevel && currentLevel > 0) {
    
    Serial.print("Naik ke Level: "); Serial.println(currentLevel);
    
    // Bunyikan Buzzer 3 Detik
    digitalWrite(PIN_BUZZER, 1);
    delay(2500); 
    digitalWrite(PIN_BUZZER, 0);
    
    // Simpan level sekarang agar tidak bunyi terus menerus di angka yang sama
    lastLevel = currentLevel; 
  }

  // RESET LEVEL (Jika cahaya turun)
  // Misal dari 2500 (Level 2) turun ke 1900 (Level 1).
  // Kita harus turunkan lastLevel ke 1. 
  // Supaya kalau nanti naik lagi ke 2000, dia bisa bunyi lagi.
  if (currentLevel < lastLevel) {
    lastLevel = currentLevel;
  }

  delay(500); 
}

// --- Fungsi Kirim ke Nextion ---
void sendNextionCommand(String cmd) {
  nexSerial.print(cmd);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
}