//Nama Program    :     PARAGON SMART LIGHTING BOX 
//Author          :     Dimas Ramadhani Ario Prayitno
//Versi           :     1.0.1
//Ownership       :     PT MAKERINDO PRIMA SOLUSI 
//Tanggal         :     23 Februari 2026
//Deskripsi       :     Membaca dua sensor BH1750 dan menampilkan nilai lux ke Nextion Display

#include <Wire.h>
#include <HardwareSerial.h>

//-------------Alamat Sensor-------------//
#define ADDR1 0x23
#define ADDR2 0x5C

//-------------Pin-------------//
#define SDA_PIN 21
#define SCL_PIN 22
#define Buzzer 25

#define NEXTION_RX 16
#define NEXTION_TX 17

HardwareSerial nexSerial(2);

//-------------Fungsi Kirim Perintah Nextion-------------//
void sendCommand(String cmd){
  nexSerial.print(cmd);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
}

//-------------Fungsi Baca Sensor-------------//
float readRaw(uint8_t addr) {

  Wire.beginTransmission(addr);
  Wire.write(0x10);

  if (Wire.endTransmission() != 0) {
    return -999;
  }

  delay(180);

  Wire.requestFrom(addr, (uint8_t)2);

  if (Wire.available() == 2) {
    uint16_t level = Wire.read();
    level <<= 8;
    level |= Wire.read();
    return level / 1.2;
  }

  return -888;
}

//-------------Buzzer Startup-------------//
void buzzerStartup(){

    digitalWrite(Buzzer, HIGH);
    delay(200);
    digitalWrite(Buzzer, LOW);
    delay(50);


}

//-------------Setup-------------//
void setup() {

  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  nexSerial.begin(9600, SERIAL_8N1, NEXTION_RX, NEXTION_TX);

  pinMode(Buzzer, OUTPUT);

  buzzerStartup();   // bunyi saat ESP pertama nyala

  sendCommand("t0.txt=\"NILAI LUX\"");
  sendCommand("t1.txt=\"NILAI LUX\"");
}

//-------------Loop-------------//
void loop() {

  float lux1 = readRaw(ADDR1);
  float lux2 = readRaw(ADDR2);

  if(lux1 > 1000){
    digitalWrite(Buzzer, HIGH);
  }
  else{
    digitalWrite(Buzzer, LOW);
  }

  if(lux1 <= 0){
    sendCommand("t2.txt=\"-\"");
  }
  else{
    sendCommand("t2.txt=\"" + String(lux1,0) + "\"");
  }

  if(lux2 <= 0){
    sendCommand("t3.txt=\"-\"");
  }
  else{
    sendCommand("t3.txt=\"" + String(lux2,0) + "\"");
  }

  Serial.print("0x23: ");
  Serial.print(lux1);
  Serial.print(" | 0x5C: ");
  Serial.println(lux2);

  delay(1000);
}