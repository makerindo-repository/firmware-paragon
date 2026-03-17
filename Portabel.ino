#include <Wire.h>
#include <Adafruit_INA219.h>
#include <BH1750.h>
#include <HardwareSerial.h>

#define NEXTION_RX 16
#define NEXTION_TX 17
#define SDA_PIN 21
#define SCL_PIN 22
#define Buzzer 25

HardwareSerial nexSerial(2);

Adafruit_INA219 ina219;
BH1750 lightMeter(0x23);

float voltageRaw = 0;
float voltageFiltered = 0;

float holdVoltage = 0;
float maxVoltage = 0;

unsigned long dropTimer = 0;

int batteryPercent = 0;
int lastPercent = 100;

int luxValue = 0;

bool chargingMode = false;

int chargeAnimation = 0;

unsigned long startTime;
unsigned long chargeAnimTimer = 0;

bool startupBeepDone = false;


// ==========================
// kirim ke nextion
// ==========================
void sendNextion(String cmd)
{
  nexSerial.print(cmd);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
}


// ==========================
// hitung persen baterai
// ==========================
int hitungPersen(float v)
{
  if(v >= 12.7) return 100;
  if(v <= 11.4) return 0;

  return (v - 11.4) * 100 / (12.7 - 11.4);
}


// ==========================
// indikator baterai
// ==========================
void updateBatteryIndicator(int persen)
{
  sendNextion("j0.val=" + String(persen));
  sendNextion("t1.txt=\"" + String(persen) + "%\"");

  if(persen > 30)
    sendNextion("j0.pco=2016");
  else if(persen > 20)
    sendNextion("j0.pco=65504");
  else
    sendNextion("j0.pco=63488");
}


// ==========================
// buzzer baterai rendah
// ==========================
void buzzerLowBattery(float v)
{

  static unsigned long lastBeepTime = 0;
  static bool buzzerRunning = false;
  static unsigned long buzzerStart = 0;

  if(v <= 11.5)
  {
    
    if(!buzzerRunning && millis() - lastBeepTime >= 120000)
    {
      buzzerRunning = true;
      buzzerStart = millis();
      lastBeepTime = millis();
    }

    if(buzzerRunning)
    {
      digitalWrite(Buzzer, HIGH);
      delay(150);
      digitalWrite(Buzzer, LOW);
      delay(150);
      digitalWrite(Buzzer, HIGH);
      delay(150);
      digitalWrite(Buzzer, LOW);

      if(millis() - buzzerStart >= 2000)
      {
        buzzerRunning = false;
      }
    }

  }
  else
  {
    digitalWrite(Buzzer, LOW);
    buzzerRunning = false;
  }

}


// ==========================
// setup
// ==========================
void setup()
{
  Serial.begin(115200);

  nexSerial.begin(9600, SERIAL_8N1, NEXTION_RX, NEXTION_TX);

  Wire.begin(SDA_PIN, SCL_PIN);

  ina219.begin();
  lightMeter.begin();

  pinMode(Buzzer, OUTPUT);
  digitalWrite(Buzzer, LOW);

  startTime = millis();

  sendNextion("t3.txt=\"NILAI LUX\"");
}


// ==========================
// loop
// ==========================
void loop()
{

  voltageRaw = ina219.getBusVoltage_V();

  // BATASI RANGE TEGANGAN
if(voltageRaw > maxVoltage && voltageRaw <= 12.7)
{
  maxVoltage = voltageRaw;
  holdVoltage = voltageRaw;
}

  // ==========================
  // FILTER
  // ==========================
  voltageFiltered = (voltageFiltered * 0.92) + (voltageRaw * 0.08);


  // ==========================
  // MAX VOLTAGE (RAW tertinggi)
  // ==========================
  if(voltageRaw > maxVoltage && voltageRaw <= 12.7)
{
  maxVoltage = voltageRaw;
  holdVoltage = voltageRaw;
}

  // ==========================
  // DETEKSI CHARGING
  // ==========================
  if(voltageFiltered > 13.0)
    chargingMode = true;
  else
    chargingMode = false;


  // ==========================
  // DETEKSI PENURUNAN BATERAI
  // ==========================
 float delta = holdVoltage - voltageRaw;

// jika raw hampir sama dengan hold → sinkronisasi
if(abs(delta) <= 0.024)
{
    holdVoltage = voltageRaw;
    dropTimer = 0;
}

// jika raw lebih rendah jauh dari hold
else if(delta > 0.060)
{
    if(dropTimer == 0)
        dropTimer = millis();

    if(millis() - dropTimer > 30000)
    {
        holdVoltage -= 0.01;
        dropTimer = millis();
    }
}

// jika raw lebih tinggi → update hold
else if(voltageRaw > holdVoltage)
{
    holdVoltage = voltageRaw;
    dropTimer = 0;
}


  luxValue = (int)lightMeter.readLightLevel();


  // ==========================
  // BUZZER STARTUP
  // ==========================
  if(!startupBeepDone)
  {
    digitalWrite(Buzzer, HIGH);
    delay(120);
    digitalWrite(Buzzer, LOW);
    delay(120);
    digitalWrite(Buzzer, HIGH);
    delay(120);
    digitalWrite(Buzzer, LOW);

    startupBeepDone = true;
  }

  buzzerLowBattery(voltageFiltered);

  sendNextion("t2.txt=\"" + String(luxValue) + "\"");


  Serial.print("Raw: ");
  Serial.print(voltageRaw);

  Serial.print(" Filter: ");
  Serial.print(voltageFiltered);

  Serial.print(" Hold: ");
  Serial.print(holdVoltage);

  Serial.print(" Max: ");
  Serial.print(maxVoltage);

  Serial.print(" Lux: ");
  Serial.print(luxValue);

  Serial.print(" Mode: ");
  Serial.println(chargingMode ? "CHARGING" : "BATTERY");


  if(millis() - startTime < 5000)
  {
    sendNextion("t0.txt=\"Stabilisasi Sensor\"");
    delay(200);
    return;
  }


  // ==========================
  // MODE CHARGING
  // ==========================
  if(chargingMode)
  {

    sendNextion("t0.txt=\"Mode Charging\"");
    sendNextion("t1.txt=\"\"");
    sendNextion("j0.pco=2016");

    if(millis() - chargeAnimTimer >= 800)
    {
      chargeAnimTimer = millis();

      if(chargeAnimation == 0) chargeAnimation = 20;
      else if(chargeAnimation == 20) chargeAnimation = 40;
      else if(chargeAnimation == 40) chargeAnimation = 60;
      else if(chargeAnimation == 60) chargeAnimation = 80;
      else if(chargeAnimation == 80) chargeAnimation = 100;
      else chargeAnimation = 20;

      sendNextion("j0.val=" + String(chargeAnimation));
    }

  }

  // ==========================
  // MODE BATERAI
  // ==========================
  else
  {

    sendNextion("t0.txt=\"Mode Baterai Aktif\"");

    batteryPercent = hitungPersen(holdVoltage);

    if(batteryPercent > lastPercent)
      batteryPercent = lastPercent;

    if(batteryPercent < lastPercent - 1)
      batteryPercent = lastPercent - 1;

    lastPercent = batteryPercent;

    updateBatteryIndicator(batteryPercent);

  }

  delay(200);

}
// #include <Wire.h>
// #include <Adafruit_INA219.h>
// #include <BH1750.h>
// #include <HardwareSerial.h>

// #define NEXTION_RX 16
// #define NEXTION_TX 17

// #define SDA_PIN 21
// #define SCL_PIN 22
// #define Buzzer 25

// HardwareSerial nexSerial(2);

// Adafruit_INA219 ina219;
// BH1750 lightMeter(0x23);

// float voltageRaw = 0;
// float voltageFiltered = 0;

// float voltageSamples[10];
// int sampleIndex = 0;

// int batteryPercent = 0;
// int lastBatteryPercent = -1;

// int luxValue = 0;

// bool chargingMode = false;

// int chargeAnimation = 0;

// unsigned long startTime;
// unsigned long chargeAnimTimer = 0;

// bool startupBeepDone = false;


// // ==========================
// // kirim ke nextion
// // ==========================
// void sendNextion(String cmd)
// {
//   nexSerial.print(cmd);
//   nexSerial.write(0xFF);
//   nexSerial.write(0xFF);
//   nexSerial.write(0xFF);
// }


// // ==========================
// // stabilisasi tegangan INA219
// // ==========================
// float getStableVoltage(float newValue)
// {

//   voltageSamples[sampleIndex] = newValue;

//   sampleIndex++;

//   if(sampleIndex >= 10)
//     sampleIndex = 0;

//   float sum = 0;

//   for(int i=0;i<10;i++)
//   {
//     sum += voltageSamples[i];
//   }

//   float average = sum / 10.0;

//   // pembulatan 0.02V
//   float stable = round(average * 50.0) / 50.0;

//   return stable;

// }


// // ==========================
// // hitung persen baterai
// // ==========================
// int hitungPersen(float v)
// {
//   if(v >= 12.7) return 100;
//   if(v <= 11.7) return 0;

//   return (v - 11.7) * 100 / (12.7 - 11.7);
// }


// // ==========================
// // indikator baterai
// // ==========================
// void updateBatteryIndicator(int persen)
// {

//   sendNextion("j0.val=" + String(persen));

//   sendNextion("t1.txt=\"" + String(persen) + "%\"");

//   if(persen > 30)
//     sendNextion("j0.pco=2016");
//   else if(persen > 20)
//     sendNextion("j0.pco=65504");
//   else
//     sendNextion("j0.pco=63488");

// }


// // ==========================
// // buzzer baterai rendah
// // ==========================
// void buzzerLowBattery(float v)
// {

//   static unsigned long lastBeepTime = 0;
//   static bool buzzerRunning = false;
//   static unsigned long buzzerStart = 0;

//   if(v <= 11.7)
//   {
    
//     if(!buzzerRunning && millis() - lastBeepTime >= 120000)
//     {
//       buzzerRunning = true;
//       buzzerStart = millis();
//       lastBeepTime = millis();
//     }

//     if(buzzerRunning)
//     {
//       digitalWrite(Buzzer, HIGH);
//       delay(150);
//       digitalWrite(Buzzer, LOW);
//       delay(150);

//       if(millis() - buzzerStart >= 2000)
//       {
//         buzzerRunning = false;
//       }
//     }

//   }
//   else
//   {
//     digitalWrite(Buzzer, LOW);
//     buzzerRunning = false;
//   }

// }


// // ==========================
// // setup
// // ==========================
// void setup()
// {

//   Serial.begin(115200);

//   nexSerial.begin(9600, SERIAL_8N1, NEXTION_RX, NEXTION_TX);

//   Wire.begin(SDA_PIN, SCL_PIN);

//   ina219.begin();
//   lightMeter.begin();

//   pinMode(Buzzer, OUTPUT);

//   digitalWrite(Buzzer, LOW);

//   startTime = millis();

//   sendNextion("t3.txt=\"NILAI LUX\"");

// }


// // ==========================
// // loop
// // ==========================
// void loop()
// {

//   voltageRaw = ina219.getBusVoltage_V();

//   voltageFiltered = getStableVoltage(voltageRaw);

//   luxValue = (int)lightMeter.readLightLevel();


//   // buzzer startup
//   if(!startupBeepDone)
//   {
//     digitalWrite(Buzzer, HIGH);
//     delay(120);
//     digitalWrite(Buzzer, LOW);
//     delay(120);

//     startupBeepDone = true;
//   }


//   buzzerLowBattery(voltageFiltered);

//   sendNextion("t2.txt=\"" + String(luxValue) + "\"");


//   // deteksi charging
//   if(voltageFiltered > 13.2)
//     chargingMode = true;
//   else
//     chargingMode = false;


//   Serial.print("Voltage Raw: ");
//   Serial.print(voltageRaw);

//   Serial.print(" | Voltage Stable: ");
//   Serial.print(voltageFiltered);

//   Serial.print(" | Lux: ");
//   Serial.print(luxValue);

//   Serial.print(" | Mode: ");
//   Serial.println(chargingMode ? "CHARGING" : "BATTERY");


//   if(millis() - startTime < 5000)
//   {
//     sendNextion("t0.txt=\"Stabilisasi Sensor\"");
//     delay(200);
//     return;
//   }


//   // ==========================
//   // MODE CHARGING
//   // ==========================
//   if(chargingMode)
//   {

//     sendNextion("t0.txt=\"Mode Charging\"");

//     sendNextion("t1.txt=\"\"");

//     if(millis() - chargeAnimTimer >= 800)
//     {

//       chargeAnimTimer = millis();

//       if(chargeAnimation == 0) chargeAnimation = 20;
//       else if(chargeAnimation == 20) chargeAnimation = 40;
//       else if(chargeAnimation == 40) chargeAnimation = 60;
//       else if(chargeAnimation == 60) chargeAnimation = 80;
//       else if(chargeAnimation == 80) chargeAnimation = 100;
//       else chargeAnimation = 20;

//       sendNextion("j0.val=" + String(chargeAnimation));

//     }

//   }

//   // ==========================
//   // MODE BATERAI
//   // ==========================
//   else
//   {

//     sendNextion("t0.txt=\"Mode Baterai Aktif\"");

//     batteryPercent = hitungPersen(voltageFiltered);

//     if(batteryPercent != lastBatteryPercent)
//     {
//       updateBatteryIndicator(batteryPercent);
//       lastBatteryPercent = batteryPercent;
//     }

//   }

//   delay(200);

// }


// // #include <Wire.h>
// // #include <Adafruit_INA219.h>
// // #include <BH1750.h>
// // #include <HardwareSerial.h>

// // #define NEXTION_RX 16
// // #define NEXTION_TX 17

// // #define SDA_PIN 21
// // #define SCL_PIN 22
// // #define Buzzer 25

// // HardwareSerial nexSerial(2);

// // Adafruit_INA219 ina219;
// // BH1750 lightMeter(0x23);

// // // =============================
// // // FILTER TEGANGAN
// // // =============================
// // float voltageRaw = 0;
// // float voltageFiltered = 0;

// // float voltageSamples[10];
// // int sampleIndex = 0;

// // // =============================
// // // COULOMB COUNTING
// // // =============================
// // float batteryCapacity = 7.0;   // kapasitas baterai (Ah)
// // float remainingAh = 7.0;

// // unsigned long lastCurrentTime = 0;

// // // =============================
// // int batteryPercent = 100;
// // int lastBatteryPercent = -1;

// // int luxValue = 0;

// // bool chargingMode = false;

// // int chargeAnimation = 0;

// // unsigned long startTime;
// // unsigned long chargeAnimTimer = 0;

// // bool startupBeepDone = false;


// // // ==========================
// // // kirim ke nextion
// // // ==========================
// // void sendNextion(String cmd)
// // {
// //   nexSerial.print(cmd);
// //   nexSerial.write(0xFF);
// //   nexSerial.write(0xFF);
// //   nexSerial.write(0xFF);
// // }


// // // ==========================
// // // stabilisasi tegangan
// // // ==========================
// // float getStableVoltage(float newValue)
// // {

// //   voltageSamples[sampleIndex] = newValue;

// //   sampleIndex++;

// //   if(sampleIndex >= 10)
// //     sampleIndex = 0;

// //   float sum = 0;

// //   for(int i=0;i<10;i++)
// //   {
// //     sum += voltageSamples[i];
// //   }

// //   float average = sum / 10.0;

// //   float stable = round(average * 50.0) / 50.0;

// //   return stable;

// // }


// // // ==========================
// // // indikator baterai
// // // ==========================
// // void updateBatteryIndicator(int persen)
// // {

// //   sendNextion("j0.val=" + String(persen));

// //   sendNextion("t1.txt=\"" + String(persen) + "%\"");

// //   if(persen > 30)
// //     sendNextion("j0.pco=2016");
// //   else if(persen > 20)
// //     sendNextion("j0.pco=65504");
// //   else
// //     sendNextion("j0.pco=63488");

// // }


// // // ==========================
// // // buzzer baterai rendah
// // // ==========================
// // void buzzerLowBattery(int persen)
// // {

// //   static unsigned long lastBeepTime = 0;

// //   if(persen <= 10)
// //   {

// //     if(millis() - lastBeepTime > 120000)
// //     {

// //       digitalWrite(Buzzer, HIGH);
// //       delay(150);
// //       digitalWrite(Buzzer, LOW);
// //       delay(150);
// //       digitalWrite(Buzzer, HIGH);
// //       delay(150);
// //       digitalWrite(Buzzer, LOW);

// //       lastBeepTime = millis();

// //     }

// //   }

// // }


// // // ==========================
// // // setup
// // // ==========================
// // void setup()
// // {

// //   Serial.begin(115200);

// //   nexSerial.begin(9600, SERIAL_8N1, NEXTION_RX, NEXTION_TX);

// //   Wire.begin(SDA_PIN, SCL_PIN);

// //   ina219.begin();
// //   lightMeter.begin();

// //   pinMode(Buzzer, OUTPUT);

// //   digitalWrite(Buzzer, LOW);

// //   startTime = millis();

// //   sendNextion("t3.txt=\"NILAI LUX\"");

// //   lastCurrentTime = millis();

// // }


// // // ==========================
// // // loop
// // // ==========================
// // void loop()
// // {

// //   // ==========================
// //   // BACA SENSOR
// //   // ==========================

// //   voltageRaw = ina219.getBusVoltage_V();

// //   voltageFiltered = getStableVoltage(voltageRaw);

// //   luxValue = (int)lightMeter.readLightLevel();

// //   float current_mA = ina219.getCurrent_mA();
// //   float current_A = current_mA / 1000.0;


// //   // ==========================
// //   // COULOMB COUNTING
// //   // ==========================

// //   unsigned long now = millis();

// //   float dt = (now - lastCurrentTime) / 1000.0;

// //   lastCurrentTime = now;

// //   float usedAh = current_A * (dt / 3600.0);

// //   if(current_A > 0) // arus keluar baterai
// //   {
// //     remainingAh -= usedAh;
// //   }
// //   else // charging
// //   {
// //     remainingAh -= usedAh;
// //   }

// //   if(remainingAh > batteryCapacity)
// //     remainingAh = batteryCapacity;

// //   if(remainingAh < 0)
// //     remainingAh = 0;

// //   batteryPercent = (remainingAh / batteryCapacity) * 100;


// //   // ==========================
// //   // BUZZER STARTUP
// //   // ==========================

// //   if(!startupBeepDone)
// //   {

// //     digitalWrite(Buzzer, HIGH);
// //     delay(120);
// //     digitalWrite(Buzzer, LOW);

// //     startupBeepDone = true;

// //   }


// //   buzzerLowBattery(batteryPercent);


// //   // ==========================
// //   // TAMPILKAN LUX
// //   // ==========================

// //   sendNextion("t2.txt=\"" + String(luxValue) + "\"");


// //   // ==========================
// //   // DETEKSI CHARGING
// //   // ==========================

// //   if(voltageFiltered > 13.2)
// //     chargingMode = true;
// //   else
// //     chargingMode = false;


// //   // ==========================
// //   // SERIAL DEBUG
// //   // ==========================

// //   Serial.print("Voltage: ");
// //   Serial.print(voltageFiltered);

// //   Serial.print(" | Current: ");
// //   Serial.print(current_A);

// //   Serial.print(" | Remaining Ah: ");
// //   Serial.print(remainingAh);

// //   Serial.print(" | Percent: ");
// //   Serial.print(batteryPercent);

// //   Serial.print(" | Lux: ");
// //   Serial.print(luxValue);

// //   Serial.print(" | Mode: ");
// //   Serial.println(chargingMode ? "CHARGING" : "BATTERY");


// //   // ==========================
// //   // STABILISASI SENSOR
// //   // ==========================

// //   if(millis() - startTime < 5000)
// //   {
// //     sendNextion("t0.txt=\"Stabilisasi Sensor\"");
// //     delay(200);
// //     return;
// //   }


// //   // ==========================
// //   // MODE CHARGING
// //   // ==========================

// //   if(chargingMode)
// //   {

// //     sendNextion("t0.txt=\"Mode Charging\"");

// //     sendNextion("t1.txt=\"\"");

// //     if(millis() - chargeAnimTimer >= 800)
// //     {

// //       chargeAnimTimer = millis();

// //       if(chargeAnimation == 0) chargeAnimation = 20;
// //       else if(chargeAnimation == 20) chargeAnimation = 40;
// //       else if(chargeAnimation == 40) chargeAnimation = 60;
// //       else if(chargeAnimation == 60) chargeAnimation = 80;
// //       else if(chargeAnimation == 80) chargeAnimation = 100;
// //       else chargeAnimation = 20;

// //       sendNextion("j0.val=" + String(chargeAnimation));

// //     }

// //   }

// //   // ==========================
// //   // MODE BATERAI
// //   // ==========================

// //   else
// //   {

// //     sendNextion("t0.txt=\"Mode Baterai Aktif\"");

// //     if(batteryPercent != lastBatteryPercent)
// //     {
// //       updateBatteryIndicator(batteryPercent);
// //       lastBatteryPercent = batteryPercent;
// //     }

// //   }

// //   delay(200);
// // }


// // #include <Wire.h>
// // #include <Adafruit_INA219.h>
// // #include <BH1750.h>
// // #include <HardwareSerial.h>

// // #define NEXTION_RX 16
// // #define NEXTION_TX 17
// // #define SDA_PIN 21
// // #define SCL_PIN 22
// // #define Buzzer 25

// // HardwareSerial nexSerial(2);

// // Adafruit_INA219 ina219;
// // BH1750 lightMeter(0x23);

// // // ======================
// // // FILTER TEGANGAN
// // // ======================
// // float voltageRaw = 0;
// // float voltageFiltered = 0;

// // float voltageSamples[10];
// // int sampleIndex = 0;

// // // ======================
// // // COULOMB COUNTING
// // // ======================
// // float batteryCapacity = 7.0;  
// // float remainingAh = 0;

// // unsigned long lastCurrentTime = 0;

// // // ======================
// // int batteryPercent = 0;
// // int lastBatteryPercent = -1;

// // int luxValue = 0;

// // bool chargingMode = false;

// // int chargeAnimation = 0;

// // unsigned long startTime;
// // unsigned long chargeAnimTimer = 0;

// // bool startupBeepDone = false;
// // bool socInitialized = false;


// // // ==========================
// // // kirim ke nextion
// // // ==========================
// // void sendNextion(String cmd)
// // {
// //   nexSerial.print(cmd);
// //   nexSerial.write(0xFF);
// //   nexSerial.write(0xFF);
// //   nexSerial.write(0xFF);
// // }


// // // ==========================
// // // FILTER TEGANGAN
// // // ==========================
// // float getStableVoltage(float newValue)
// // {

// //   voltageSamples[sampleIndex] = newValue;

// //   sampleIndex++;

// //   if(sampleIndex >= 10)
// //     sampleIndex = 0;

// //   float sum = 0;

// //   for(int i=0;i<10;i++)
// //   sum += voltageSamples[i];

// //   float average = sum / 10.0;

// //   float stable = round(average * 50.0) / 50.0;

// //   return stable;

// // }


// // // ==========================
// // // ESTIMASI SOC DARI TEGANGAN
// // // ==========================
// // int estimateSOCfromVoltage(float v)
// // {

// //   if(v >= 12.7) return 100;
// //   if(v >= 12.5) return 80;
// //   if(v >= 12.3) return 60;
// //   if(v >= 12.1) return 40;
// //   if(v >= 11.9) return 20;
// //   return 5;

// // }


// // // ==========================
// // // UPDATE BATTERY INDICATOR
// // // ==========================
// // void updateBatteryIndicator(int persen)
// // {

// //   sendNextion("j0.val=" + String(persen));
// //   sendNextion("t1.txt=\"" + String(persen) + "%\"");

// //   if(persen > 30)
// //     sendNextion("j0.pco=2016");
// //   else if(persen > 20)
// //     sendNextion("j0.pco=65504");
// //   else
// //     sendNextion("j0.pco=63488");

// // }


// // // ==========================
// // // BUZZER LOW BATTERY
// // // ==========================
// // void buzzerLowBattery(int persen)
// // {

// //   static unsigned long lastBeep = 0;

// //   if(persen <= 10)
// //   {

// //     if(millis() - lastBeep > 120000)
// //     {

// //       digitalWrite(Buzzer, HIGH);
// //       delay(150);
// //       digitalWrite(Buzzer, LOW);
// //       delay(150);
// //       digitalWrite(Buzzer, HIGH);
// //       delay(150);
// //       digitalWrite(Buzzer, LOW);

// //       lastBeep = millis();

// //     }

// //   }

// // }


// // // ==========================
// // // SETUP
// // // ==========================
// // void setup()
// // {

// //   Serial.begin(115200);

// //   nexSerial.begin(9600, SERIAL_8N1, NEXTION_RX, NEXTION_TX);

// //   Wire.begin(SDA_PIN, SCL_PIN);

// //   ina219.begin();
// //   lightMeter.begin();

// //   pinMode(Buzzer, OUTPUT);

// //   digitalWrite(Buzzer, LOW);

// //   startTime = millis();

// //   sendNextion("t3.txt=\"NILAI LUX\"");

// //   lastCurrentTime = millis();

// // }


// // // ==========================
// // // LOOP
// // // ==========================
// // void loop()
// // {

// //   voltageRaw = ina219.getBusVoltage_V();
// //   voltageFiltered = getStableVoltage(voltageRaw);

// //   luxValue = (int)lightMeter.readLightLevel();

// //   float currentA = ina219.getCurrent_mA() / 1000.0;


// //   // ======================
// //   // INISIALISASI SOC
// //   // ======================
// //   if(!socInitialized && millis() - startTime > 5000)
// //   {

// //     int soc = estimateSOCfromVoltage(voltageFiltered);

// //     remainingAh = batteryCapacity * soc / 100.0;

// //     socInitialized = true;

// //   }


// //   // ======================
// //   // COULOMB COUNTING
// //   // ======================
// //   unsigned long now = millis();

// //   float dt = (now - lastCurrentTime) / 1000.0;

// //   lastCurrentTime = now;

// //   float deltaAh = currentA * (dt / 3600.0);

// //   remainingAh -= deltaAh;

// //   if(remainingAh > batteryCapacity)
// //     remainingAh = batteryCapacity;

// //   if(remainingAh < 0)
// //     remainingAh = 0;

// //   batteryPercent = round((remainingAh / batteryCapacity) * 100);


// //   // ======================
// //   // BUZZER STARTUP
// //   // ======================
// //   if(!startupBeepDone)
// //   {

// //     digitalWrite(Buzzer, HIGH);
// //     delay(120);
// //     digitalWrite(Buzzer, LOW);

// //     startupBeepDone = true;

// //   }


// //   buzzerLowBattery(batteryPercent);


// //   // ======================
// //   // TAMPILKAN LUX
// //   // ======================
// //   sendNextion("t2.txt=\"" + String(luxValue) + "\"");


// //   // ======================
// //   // DETEKSI CHARGING
// //   // ======================
// //   if(voltageFiltered >= 13.1)
// //     chargingMode = true;
// //   else
// //     chargingMode = false;


// //   // ======================
// //   // SERIAL DEBUG
// //   // ======================
// //   Serial.print("Voltage: ");
// //   Serial.print(voltageFiltered);

// //   Serial.print(" | Current: ");
// //   Serial.print(currentA);

// //   Serial.print(" | RemainingAh: ");
// //   Serial.print(remainingAh);

// //   Serial.print(" | Percent: ");
// //   Serial.print(batteryPercent);

// //   Serial.print(" | Lux: ");
// //   Serial.print(luxValue);

// //   Serial.print(" | Mode: ");
// //   Serial.println(chargingMode ? "CHARGING" : "BATTERY");


// //   // ======================
// //   // STABILISASI SENSOR
// //   // ======================
// //   if(millis() - startTime < 5000)
// //   {

// //     sendNextion("t0.txt=\"Stabilisasi Sensor\"");
// //     delay(200);
// //     return;

// //   }


// //   // ======================
// //   // MODE CHARGING
// //   // ======================
// //   if(chargingMode)
// //   {

// //     sendNextion("t0.txt=\"Mode Charging\"");
// //     sendNextion("t1.txt=\"\"");

// //     if(millis() - chargeAnimTimer >= 800)
// //     {

// //       chargeAnimTimer = millis();

// //       if(chargeAnimation == 0) chargeAnimation = 20;
// //       else if(chargeAnimation == 20) chargeAnimation = 40;
// //       else if(chargeAnimation == 40) chargeAnimation = 60;
// //       else if(chargeAnimation == 60) chargeAnimation = 80;
// //       else if(chargeAnimation == 80) chargeAnimation = 100;
// //       else chargeAnimation = 20;

// //       sendNextion("j0.val=" + String(chargeAnimation));

// //     }

// //   }

// //   // ======================
// //   // MODE BATERAI
// //   // ======================
// //   else
// //   {

// //     sendNextion("t0.txt=\"Mode Baterai Aktif\"");

// //     if(batteryPercent != lastBatteryPercent)
// //     {

// //       updateBatteryIndicator(batteryPercent);

// //       lastBatteryPercent = batteryPercent;

// //     }

// //   }

// //   delay(200);

// // }

// // #include <Wire.h>
// // #include <Adafruit_INA219.h>
// // #include <BH1750.h>
// // #include <HardwareSerial.h>

// // #define NEXTION_RX 16
// // #define NEXTION_TX 17
// // #define SDA_PIN 21
// // #define SCL_PIN 22
// // #define Buzzer 25

// // HardwareSerial nexSerial(2);

// // Adafruit_INA219 ina219;
// // BH1750 lightMeter(0x23);

// // // ======================
// // // FILTER TEGANGAN
// // // ======================
// // float voltageRaw = 0;
// // float voltageFiltered = 0;

// // float voltageSamples[10];
// // int sampleIndex = 0;


// // // ======================
// // // COULOMB COUNTING
// // // ======================
// // float batteryCapacity = 7.0;
// // float remainingAh = 0;

// // unsigned long lastCurrentTime = 0;


// // // ======================
// // int batteryPercent = 0;
// // int lastBatteryPercent = -1;

// // int luxValue = 0;

// // bool chargingMode = false;

// // int chargeAnimation = 0;

// // unsigned long startTime;
// // unsigned long chargeAnimTimer = 0;

// // bool startupBeepDone = false;
// // bool socInitialized = false;

// // float voltageCalibration = 0.32;
// // float currentFiltered = 0;
// // // ==========================
// // // KIRIM KE NEXTION
// // // ==========================
// // void sendNextion(String cmd)
// // {
// //   nexSerial.print(cmd);
// //   nexSerial.write(0xFF);
// //   nexSerial.write(0xFF);
// //   nexSerial.write(0xFF);
// // }


// // // ==========================
// // // FILTER TEGANGAN INA219
// // // ==========================
// // float getStableVoltage(float newValue)
// // {

// //   voltageSamples[sampleIndex] = newValue;

// //   sampleIndex++;

// //   if(sampleIndex >= 10)
// //     sampleIndex = 0;

// //   float sum = 0;

// //   for(int i=0;i<10;i++)
// //     sum += voltageSamples[i];

// //   float average = sum / 10.0;

// //   float stable = round(average * 50.0) / 50.0;

// //   return stable;
// // }


// // // ==========================
// // // ESTIMASI SOC DARI TEGANGAN
// // // ==========================
// // int estimateSOCfromVoltage(float v)
// // {

// //   if(v >= 12.7) return 100;
// //   if(v >= 12.6) return 90;
// //   if(v >= 12.5) return 80;
// //   if(v >= 12.4) return 70;
// //   if(v >= 12.3) return 60;
// //   if(v >= 12.2) return 50;
// //   if(v >= 12.1) return 40;
// //   if(v >= 12.0) return 30;
// //   if(v >= 11.9) return 20;
// //   return 10;

// // }


// // // ==========================
// // // UPDATE BATTERY INDICATOR
// // // ==========================
// // void updateBatteryIndicator(int persen)
// // {

// //   sendNextion("j0.val=" + String(persen));
// //   sendNextion("t1.txt=\"" + String(persen) + "%\"");

// //   if(persen > 30)
// //     sendNextion("j0.pco=2016");
// //   else if(persen > 20)
// //     sendNextion("j0.pco=65504");
// //   else
// //     sendNextion("j0.pco=63488");

// // }


// // // ==========================
// // // BUZZER LOW BATTERY
// // // ==========================
// // void buzzerLowBattery(int persen)
// // {

// //   static unsigned long lastBeep = 0;

// //   if(persen <= 10)
// //   {

// //     if(millis() - lastBeep > 120000)
// //     {

// //       digitalWrite(Buzzer, HIGH);
// //       delay(150);
// //       digitalWrite(Buzzer, LOW);
// //       delay(150);
// //       digitalWrite(Buzzer, HIGH);
// //       delay(150);
// //       digitalWrite(Buzzer, LOW);

// //       lastBeep = millis();

// //     }

// //   }

// // }


// // // ==========================
// // // SETUP
// // // ==========================
// // void setup()
// // {

// //   Serial.begin(115200);

// //   nexSerial.begin(9600, SERIAL_8N1, NEXTION_RX, NEXTION_TX);

// //   Wire.begin(SDA_PIN, SCL_PIN);

// //   ina219.begin();
// //   lightMeter.begin();

// //   pinMode(Buzzer, OUTPUT);

// //   digitalWrite(Buzzer, LOW);

// //   startTime = millis();

// //   sendNextion("t3.txt=\"NILAI LUX\"");

// //   lastCurrentTime = millis();

// // }


// // // ==========================
// // // LOOP
// // // ==========================
// // void loop()
// // {
// // voltageRaw = ina219.getBusVoltage_V() + voltageCalibration;
// //   voltageFiltered = getStableVoltage(voltageRaw);

// //   luxValue = (int)lightMeter.readLightLevel();

// //   float currentA = ina219.getCurrent_mA() / 1000.0;

// // // Hilangkan noise arus kecil
// // if(abs(currentA) < 0.15)
// // {
// //   currentA = 0;
// // }

// // // Filter arus supaya tidak loncat
// // currentFiltered = currentFiltered * 0.7 + currentA * 0.3;

// //   // ======================
// //   // INISIALISASI SOC
// //   // ======================
// //   if(!socInitialized && millis() - startTime > 5000)
// //   {

// //     int soc = estimateSOCfromVoltage(voltageFiltered);

// //     remainingAh = batteryCapacity * soc / 100.0;

// //     socInitialized = true;

// //   }


// //   // ======================
// //   // COULOMB COUNTING
// //   // ======================
// //   unsigned long now = millis();

// //   float dt = (now - lastCurrentTime) / 1000.0;

// //   lastCurrentTime = now;

// //   float deltaAh = currentFiltered * (dt / 3600.0);
// //   if(abs(deltaAh) > 0.002)
// // {
// //   deltaAh = 0;
// // }

// // remainingAh -= deltaAh;

// //   if(remainingAh > batteryCapacity)
// //     remainingAh = batteryCapacity;

// //   if(remainingAh < 0)
// //     remainingAh = 0;


// //   batteryPercent = (remainingAh / batteryCapacity) * 100;

// //   // Clamp persen
// //   if(batteryPercent > 100) batteryPercent = 100;
// //   if(batteryPercent < 0) batteryPercent = 0;


// //   // ======================
// //   // BUZZER STARTUP
// //   // ======================
// //   if(!startupBeepDone)
// //   {

// //     digitalWrite(Buzzer, HIGH);
// //     delay(120);
// //     digitalWrite(Buzzer, LOW);

// //     startupBeepDone = true;

// //   }


// //   buzzerLowBattery(batteryPercent);


// //   // ======================
// //   // TAMPILKAN LUX
// //   // ======================
// //   sendNextion("t2.txt=\"" + String(luxValue) + "\"");


// //   // ======================
// //   // DETEKSI CHARGING
// //   // ======================
// //   if(voltageFiltered >= 13.1)
// //     chargingMode = true;
// //   else
// //     chargingMode = false;

// // if(voltageFiltered >= 13.47 && currentA < 0.2)
// // {
// //   remainingAh = batteryCapacity;
// // }

// //   // ======================
// //   // SERIAL DEBUG
// //   // ======================
// //   Serial.print("Voltage: ");
// //   Serial.print(voltageFiltered);

// //   Serial.print(" | Current: ");
// //   Serial.print(currentA);

// //   Serial.print(" | RemainingAh: ");
// //   Serial.print(remainingAh);

// //   Serial.print(" | Percent: ");
// //   Serial.print(batteryPercent);

// //   Serial.print(" | Lux: ");
// //   Serial.print(luxValue);

// //   Serial.print(" | Mode: ");
// //   Serial.println(chargingMode ? "CHARGING" : "BATTERY");


// //   // ======================
// //   // STABILISASI SENSOR
// //   // ======================
// //   if(millis() - startTime < 5000)
// //   {

// //     sendNextion("t0.txt=\"Stabilisasi Sensor\"");
// //     delay(200);
// //     return;

// //   }


// //   // ======================
// //   // MODE CHARGING
// //   // ======================
// //   if(chargingMode)
// //   {

// //     sendNextion("t0.txt=\"Mode Charging\"");
// //     sendNextion("t1.txt=\"\"");

// //     if(millis() - chargeAnimTimer >= 800)
// //     {

// //       chargeAnimTimer = millis();

// //       if(chargeAnimation == 0) chargeAnimation = 20;
// //       else if(chargeAnimation == 20) chargeAnimation = 40;
// //       else if(chargeAnimation == 40) chargeAnimation = 60;
// //       else if(chargeAnimation == 60) chargeAnimation = 80;
// //       else if(chargeAnimation == 80) chargeAnimation = 100;
// //       else chargeAnimation = 20;

// //       sendNextion("j0.val=" + String(chargeAnimation));

// //     }

// //   }

// //   // ======================
// //   // MODE BATERAI
// //   // ======================
// //   else
// //   {

// //     sendNextion("t0.txt=\"Mode Baterai Aktif\"");

// //    updateBatteryIndicator(batteryPercent);

// //   }

// //   delay(200);

// // }

// #include <Wire.h>
// #include <Adafruit_INA219.h>
// #include <BH1750.h>
// #include <HardwareSerial.h>

// #define NEXTION_RX 16
// #define NEXTION_TX 17
// #define SDA_PIN 21
// #define SCL_PIN 22
// #define Buzzer 25

// HardwareSerial nexSerial(2);

// Adafruit_INA219 ina219;
// BH1750 lightMeter(0x23);

// // ======================
// // FILTER TEGANGAN
// // ======================
// float voltageRaw = 0;
// float voltageFiltered = 0;

// float voltageSamples[10];
// int sampleIndex = 0;

// float voltageCalibration = 0.991;

// // ======================
// // FILTER ARUS (BMS STYLE)
// // ======================
// float currentA = 0;
// float currentFiltered = 0;

// // ======================
// // FILTER LUX
// // ======================
// float luxFiltered = 0;

// // ======================
// // COULOMB COUNTING
// // ======================
// float batteryCapacity = 7.5;
// float remainingAh = 0;

// unsigned long lastCurrentTime = 0;

// // ======================
// int batteryPercent = 0;
// int lastBatteryPercent = -1;

// int luxValue = 0;

// bool chargingMode = false;
// bool previousChargingMode = false;

// int chargeAnimation = 0;

// unsigned long startTime;
// unsigned long chargeAnimTimer = 0;

// bool startupBeepDone = false;
// bool socInitialized = false;

// // ==========================
// // KIRIM KE NEXTION
// // ==========================
// void sendNextion(String cmd)
// {
//   nexSerial.print(cmd);
//   nexSerial.write(0xFF);
//   nexSerial.write(0xFF);
//   nexSerial.write(0xFF);
// }

// // ==========================
// // FILTER TEGANGAN
// // ==========================
// float getStableVoltage(float newValue)
// {
//   voltageSamples[sampleIndex] = newValue;
//   sampleIndex++;

//   if(sampleIndex >= 10)
//     sampleIndex = 0;

//   float sum = 0;

//   for(int i=0;i<10;i++)
//     sum += voltageSamples[i];

//   float average = sum / 10.0;

//   float stable = round(average * 50.0) / 50.0;

//   return stable;
// }

// // ==========================
// // ESTIMASI SOC DARI TEGANGAN
// // ==========================
// int estimateSOCfromVoltage(float v)
// {
//   if(v >= 12.7) return 100;
//   if(v >= 12.6) return 90;
//   if(v >= 12.5) return 80;
//   if(v >= 12.4) return 70;
//   if(v >= 12.3) return 60;
//   if(v >= 12.2) return 50;
//   if(v >= 12.1) return 40;
//   if(v >= 12.0) return 30;
//   if(v >= 11.9) return 20;
//   return 10;
// }

// // ==========================
// // UPDATE BATTERY INDICATOR
// // ==========================
// void updateBatteryIndicator(int persen)
// {
//   sendNextion("j0.val=" + String(persen));
//   sendNextion("t1.txt=\"" + String(persen) + "%\"");

//   if(persen > 30)
//     sendNextion("j0.pco=2016");
//   else if(persen > 20)
//     sendNextion("j0.pco=65504");
//   else
//     sendNextion("j0.pco=63488");
// }

// // ==========================
// // BUZZER LOW BATTERY
// // ==========================
// void buzzerLowBattery(int persen)
// {
//   static unsigned long lastBeep = 0;

//   if(persen <= 10)
//   {
//     if(millis() - lastBeep > 120000)
//     {
//       digitalWrite(Buzzer, HIGH);
//       delay(150);
//       digitalWrite(Buzzer, LOW);
//       delay(150);
//       digitalWrite(Buzzer, HIGH);
//       delay(150);
//       digitalWrite(Buzzer, LOW);

//       lastBeep = millis();
//     }
//   }
// }

// // ==========================
// // SETUP
// // ==========================
// void setup()
// {
//   Serial.begin(115200);

//   nexSerial.begin(9600, SERIAL_8N1, NEXTION_RX, NEXTION_TX);

//   Wire.begin(SDA_PIN, SCL_PIN);

//   ina219.begin();
//   lightMeter.begin();

//   pinMode(Buzzer, OUTPUT);
//   digitalWrite(Buzzer, LOW);

//   startTime = millis();

//   sendNextion("t3.txt=\"NILAI LUX\"");

//   lastCurrentTime = millis();
// }

// // ==========================
// // LOOP
// // ==========================
// void loop()
// {

// // ======================
// // BACA SENSOR
// // ======================

// voltageRaw = ina219.getBusVoltage_V() * voltageCalibration;
// voltageFiltered = getStableVoltage(voltageRaw);

// currentA = ina219.getCurrent_mA() / 1000.0;

// // deadband arus (hilangkan noise)
// if(abs(currentA) < 0.15)
//   currentA = 0;

// // filter arus
// currentFiltered = currentFiltered * 0.7 + currentA * 0.3;

// // filter lux
// luxFiltered = luxFiltered * 0.7 + lightMeter.readLightLevel() * 0.3;
// luxValue = (int)luxFiltered;


// // ======================
// // INISIALISASI SOC
// // ======================
// if(!socInitialized && millis() - startTime > 5000)
// {
//   if(abs(currentFiltered) < 0.2)   // hanya saat idle
//   {
//     int soc = estimateSOCfromVoltage(voltageFiltered);
//     remainingAh = batteryCapacity * soc / 100.0;
//   }

//   socInitialized = true;
// }
// // ======================
// // COULOMB COUNTING
// // ======================
// unsigned long now = millis();

// float dt = (now - lastCurrentTime) / 1000.0;

// lastCurrentTime = now;

// float deltaAh = currentFiltered * (dt / 3600.0);

// // anti lonjakan
// if(abs(deltaAh) > 0.002)
//   deltaAh = 0;

// remainingAh -= deltaAh;

// if(remainingAh > batteryCapacity)
//   remainingAh = batteryCapacity;

// if(remainingAh < 0)
//   remainingAh = 0;


// // ======================
// // HITUNG PERSEN
// // ======================
// batteryPercent = (remainingAh / batteryCapacity) * 100;

// if(batteryPercent > 100) batteryPercent = 100;
// if(batteryPercent < 0) batteryPercent = 0;


// // ======================
// // DETEKSI CHARGING
// // ======================
// previousChargingMode = chargingMode;

// if(voltageFiltered >= 13)
//   chargingMode = true;
// else
//   chargingMode = false;


// // reset persen saat keluar charging
// if(previousChargingMode && !chargingMode)
// {
//   lastBatteryPercent = -1;
// }


// // ======================
// // AUTO KALIBRASI FULL
// // ======================
// if(voltageFiltered >= 13.3 && currentFiltered < 0.2)
// {
//   remainingAh = batteryCapacity;
// }


// // ======================
// // BUZZER STARTUP
// // ======================
// if(!startupBeepDone)
// {
//   digitalWrite(Buzzer, HIGH);
//   delay(120);
//   digitalWrite(Buzzer, LOW);

//   startupBeepDone = true;
// }

// buzzerLowBattery(batteryPercent);


// // ======================
// // TAMPILKAN LUX
// // ======================
// sendNextion("t2.txt=\"" + String(luxValue) + "\"");


// // ======================
// // SERIAL DEBUG
// // ======================
// Serial.print("Voltage: ");
// Serial.print(voltageFiltered);

// Serial.print(" | Current: ");
// Serial.print(currentFiltered);

// Serial.print(" | RemainingAh: ");
// Serial.print(remainingAh);

// Serial.print(" | Percent: ");
// Serial.print(batteryPercent);

// Serial.print(" | Lux: ");
// Serial.print(luxValue);

// Serial.print(" | Mode: ");
// Serial.println(chargingMode ? "CHARGING" : "BATTERY");


// // ======================
// // STABILISASI SENSOR
// // ======================
// if(millis() - startTime < 5000)
// {
//   sendNextion("t0.txt=\"Stabilisasi Sensor\"");
//   delay(200);
//   return;
// }


// // ======================
// // MODE CHARGING
// // ======================
// if(chargingMode)
// {

//   sendNextion("t0.txt=\"Mode Charging\"");
//   sendNextion("t1.txt=\"\"");

//   if(millis() - chargeAnimTimer >= 800)
//   {
//     chargeAnimTimer = millis();

//     if(chargeAnimation == 0) chargeAnimation = 20;
//     else if(chargeAnimation == 20) chargeAnimation = 40;
//     else if(chargeAnimation == 40) chargeAnimation = 60;
//     else if(chargeAnimation == 60) chargeAnimation = 80;
//     else if(chargeAnimation == 80) chargeAnimation = 100;
//     else chargeAnimation = 20;

//     sendNextion("j0.val=" + String(chargeAnimation));
//   }

// }


// // ======================
// // MODE BATERAI
// // ======================
// else
// {

//   sendNextion("t0.txt=\"Mode Baterai Aktif\"");

//   if(batteryPercent != lastBatteryPercent)
//   {
//     updateBatteryIndicator(batteryPercent);

//     lastBatteryPercent = batteryPercent;
//   }

// }

// delay(200);

// }