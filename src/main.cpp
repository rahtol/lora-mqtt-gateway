#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>

#define ss 12
#define rst 14
#define dio0 2

void setup() 
{
  Serial.begin(115200);
  while (!Serial);
  Serial.println("LoRa Receiver");

  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  pinMode(17, OUTPUT);
  digitalWrite(17, LOW);

  LoRa.setPins(ss, rst, dio0);    //setup LoRa transceiver module

  while (!LoRa.begin(433E6))     //433E6 - Asia, 866E6 - Europe, 915E6 - North America
  {
    Serial.println(".");
    delay(500);
  }
//  LoRa.setSyncWord(0xA5);
  Serial.println("LoRa Initializing OK!");
}

void loop() 
{
  int packetSize = LoRa.parsePacket();    // try to parse packet
  if (packetSize) 
  {
    
    Serial.print("Received packet len=");

    while (LoRa.available())              // read packet
    {
      uint8_t buffer[256];
      size_t len = LoRa.readBytes(buffer, 255);
      Serial.print(len);
      uint32_t seqnr = *((uint32_t *) buffer);
      Serial.print(" seqnr=");
      Serial.print(seqnr);
    }
    Serial.print(" with RSSI ");         // print RSSI of packet
    Serial.println(LoRa.packetRssi());
  }
}