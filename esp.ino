#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Wi-Fi credentials
const char* ssid = "Dvn.";             //nama Wi-Fi anda (SSID)
const char* password = "Devin2005";     //password Wi-Fi anda

// Firebase credentials
#define API_KEY "AIzaSyD6AceFtQNwRMzBPcFh7zufk4HpYA0KOrk"              //Found in Project Settings > General
#define DATABASE_URL "https://iots-34ff0-default-rtdb.asia-southeast1.firebasedatabase.app/"   //From Realtime Database URL
#define USER_EMAIL "tandrewdevin4@gmail.com"    //email user saat membuat database
#define USER_PASSWORD "Devin2005!"            //password user yang telah didaftarkan

#define dht 23
#define ldr 19
#define soil 18


void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== SMART PLANT GREENHOUSE ===");
  Serial.println("Inisialisasi sistem...\n");

  // Pin modes
  pinMode(LDR_PIN, INPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);
  pinMode(OBJECT_PIN, INPUT);

  // Connect WiFi
  connectWiFi();

  // Setup NTP Time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sinkronisasi waktu dengan NTP...");
  delay(2000);

  // Firebase config
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  Serial.println("Menghubungkan ke Firebase...");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  unsigned long fbStart = millis();
  while (!Firebase.ready() && millis() - fbStart < 10000) {
    Serial.print(".");
    delay(500);
  }
  if (Firebase.ready()) {
    Serial.println("\n✓ Firebase terhubung!");
    Serial.println("✓ Sistem siap monitoring!\n");
  } else {
    Serial.println("\n⚠ Firebase gagal terhubung, sistem tetap berjalan...\n");
  }
}

// Fungsi untuk mendapatkan timestamp epoch dalam milliseconds
unsigned long getTimestamp() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("⚠ Gagal mendapat waktu NTP, gunakan millis()");
    return millis();
  }
  time(&now);
  return (unsigned long)now * 1000; // Convert ke milliseconds untuk JavaScript
}

// Fungsi untuk membaca sensor dan kirim ke Firebase
void bacaDanKirimData() {
  Serial.println("\n========================================");
  Serial.println("|        PEMBACAAN SENSOR GREENHOUSE    |");
  Serial.println("========================================");

  // === BACA LDR (Cahaya) ===
  int rawLdr = analogRead(LDR_PIN);
  // Mapping: LDR semakin gelap = nilai ADC semakin tinggi
  // Invert untuk mendapat persentase cahaya (0% = gelap, 100% = terang)
  int lightLevel = map(rawLdr, 4095, 0, 0, 100);
  lightLevel = constrain(lightLevel, 0, 100);

  Serial.printf("🌞 Cahaya: %d %% (ADC=%d)\n", lightLevel, rawLdr);

  // === BACA SOIL MOISTURE ===
  int rawSoil = analogRead(SOIL_PIN);
  // Mapping: Sensor kering = nilai tinggi, basah = nilai rendah
  // Invert untuk mendapat persentase kelembaban (0% = kering, 100% = basah)
  int soilPercent = map(rawSoil, 4095, 0, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  Serial.printf("💧 Kelembaban Tanah: %d %% (ADC=%d)\n", soilPercent, rawSoil);
  if (soilPercent < 40) {
    Serial.println("⚠ STATUS: KERING - Perlu penyiraman!");
  } else {
    Serial.println("✓ STATUS: Kelembaban cukup");
  }

  // === BACA SENSOR DIGITAL ===
  motionDetected = digitalRead(PIR_PIN) == HIGH;
  flameDetected = digitalRead(FLAME_PIN) == HIGH;
  objectDetected = digitalRead(OBJECT_PIN) == HIGH;

  Serial.printf("👣 Gerakan (PIR): %s\n", motionDetected ? "TERDETEKSI ⚠" : "Tidak ada");
  Serial.printf("🔥 Api: %s\n", flameDetected ? "TERDETEKSI 🚨" : "Aman");
  Serial.printf("📦 Objek: %s\n", objectDetected ? "TERDETEKSI" : "Tidak ada");

  // === KIRIM KE FIREBASE ===
  if (Firebase.ready()) {
    Serial.println("\n➡ Mengirim data ke Firebase...");

    String basePath = "/greenhouse/sensors";
    bool allSuccess = true;

    // Kirim Light Level
    if (Firebase.RTDB.setInt(&fbdo, basePath + "/lightLevel", lightLevel)) {
      Serial.println("  ✓ lightLevel terkirim");
    } else {
      Serial.printf("  ✗ lightLevel gagal: %s\n", fbdo.errorReason().c_str());
      allSuccess = false;
    }

    // Kirim Soil Moisture
    if (Firebase.RTDB.setInt(&fbdo, basePath + "/soilMoisture", soilPercent)) {
      Serial.println("  ✓ soilMoisture terkirim");
    } else {
      Serial.printf("  ✗ soilMoisture gagal: %s\n", fbdo.errorReason().c_str());
      allSuccess = false;
    }

    // Kirim Motion (PIR)
    if (Firebase.RTDB.setBool(&fbdo, basePath + "/motion", motionDetected)) {
      Serial.println("  ✓ motion terkirim");
    } else {
      Serial.printf("  ✗ motion gagal: %s\n", fbdo.errorReason().c_str());
      allSuccess = false;
    }

    // Kirim Flame
    if (Firebase.RTDB.setBool(&fbdo, basePath + "/flame", flameDetected)) {
      Serial.println("  ✓ flame terkirim");
    } else {
      Serial.printf("  ✗ flame gagal: %s\n", fbdo.errorReason().c_str());
      allSuccess = false;
    }

    // Kirim Object
    if (Firebase.RTDB.setBool(&fbdo, basePath + "/object", objectDetected)) {
      Serial.println("  ✓ object terkirim");
    } else {
      Serial.printf("  ✗ object gagal: %s\n", fbdo.errorReason().c_str());
      allSuccess = false;
    }

    // Kirim Timestamp (epoch milliseconds untuk JavaScript Date)
    unsigned long timestamp = getTimestamp();
    if (Firebase.RTDB.setDouble(&fbdo, basePath + "/timestamp", timestamp)) {
      Serial.printf("  ✓ timestamp terkirim (%lu)\n", timestamp);
    } else {
      Serial.printf("  ✗ timestamp gagal: %s\n", fbdo.errorReason().c_str());
      allSuccess = false;
    }

    if (allSuccess) {
      Serial.println("\n✓ Semua data berhasil dikirim!");
    } else {
      Serial.println("\n⚠ Beberapa data gagal dikirim");
    }

  } else {
    Serial.println("⚠ Firebase belum siap, skip pengiriman");
  }

  Serial.println("__________________________________________\n");

  // Delay kecil untuk stabilitas
  delay(100);
}
