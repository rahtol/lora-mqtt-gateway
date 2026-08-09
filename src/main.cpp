#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include "MessageOutput.h"
#include "NetworkSettings.h"
#include "NtpSettings.h"
#include "MqttSettings.h"
#include "LoraTransceiver.h"
#include "version.h"
#include "WebApi.h"
#include <Scheduler.h>

namespace Lora_Mqtt_Gateway {

const char* version = "Project Lora_Mqtt_Gateway, Version 1.01, 09.08.2026 16:12";

}

AsyncWebSocket ws("/console");

int deviceID ()
{
    // currently the gateway acts as receiver only
    return 2;
}

void onWebsocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  if(type == WS_EVT_CONNECT)
  {
    //client connected
    Serial.printf("ws[%s][%u] connect\r\n", server->url(), client->id());
    client->printf("%s, macAddress=%s\r\n", Lora_Mqtt_Gateway::version, NetworkSettings.macAddress().c_str());
    client->ping();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    //client disconnected
    Serial.printf("ws[%s][%u] disconnect: %u\r\n", server->url(), client->id(), 0);
  }  
  else if(type == WS_EVT_ERROR)
  {
    //error was received from the other end
    Serial.printf("ws[%s][%u] error(%u): %s\r\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  }
  else if(type == WS_EVT_PONG)
  {
    //pong message was received (in response to a ping request maybe)
    Serial.printf("ws[%s][%u] pong[%u]: %s\r\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } 
  else if(type == WS_EVT_DATA)
  {
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->opcode == WS_TEXT)
    {
      data[info->index + info->len] = 0;
      MessageOutput.logf("ws[%s][%u] %s-message[%llu]: %s", server->url(), client->id(), "text", info->len, (char*)data);
      LoraTransceiver.exec_cmd(String((char*)data));
    }
    else
    {
      String s = "";
      for(size_t i=0; i < info->len; i++) {
        s += String(data[info->index + i], HEX) + " ";
      }
      MessageOutput.logf("ws[%s][%u] %s-message[%llu]: %s", server->url(), client->id(), "binary", info->len, s.c_str());
    }
  }
}


void setup()
{
    using namespace std::placeholders;

    Serial.begin(115200);
    MessageOutput.logf(Lora_Mqtt_Gateway::version);

    // Initialize WiFi
    MessageOutput.logf("Initialize Network... ");
    NetworkSettings.init();
    MessageOutput.logf("... done Initialize Network, macAddress=%s", NetworkSettings.macAddress().c_str());

    ArduinoOTA
      .onStart([]() {
        LoraTransceiver.set_mode_idle();

        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
          type = "sketch";
        } else {  // U_SPIFFS
          type = "filesystem";
        }

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
          Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
          Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
          Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
          Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
          Serial.println("End Failed");
        }
      });

    ArduinoOTA.begin();

    // Enable Websocket handling
    WebApi._server.addHandler(&ws);
    ws.onEvent(std::bind(&onWebsocketEvent, _1, _2, _3, _4, _5, _6));
    MessageOutput.setWebsocket(&ws);

    // Initialize WebApi
    MessageOutput.logf("Initialize WebApi... ");
    WebApi.init(scheduler);

    // Initialize NTP
    MessageOutput.logf("Initialize NTP... ");
    NtpSettings.init();
    MessageOutput.logf("done");

    // Initialize MqTT
    MessageOutput.logf("Initialize MqTT... ");
    MqttSettings.init(NetworkSettings.deviceID());
    MessageOutput.logf("... done initialize MqTT, deviceID=%s", NetworkSettings.deviceID().c_str());

    // Initialize Lora Transceiver
    MessageOutput.logf("Initialize Lora Transceiver... ");
    LoraTransceiver.init();
    MessageOutput.logf("... done initialize Lora Transceiver");

    // initialize Lora Transceiver mode based on deviceID
    if (deviceID() == 1) {
      MessageOutput.logf("device_1 recognised: Set Lora Transceiver to transmitting mode");
      LoraTransceiver.set_mode_transmitting(10000, 64, 224);
    }
    else if (deviceID() == 2) {
      MessageOutput.logf("device_2 recognised: Set Lora Transceiver to receiving mode");
      LoraTransceiver.set_mode_receiving();
    }
    else {
      MessageOutput.logf("device not recognised: Set Lora Transceiver to idle mode");
      LoraTransceiver.set_mode_idle();
    }

}

void loop()
{
  ArduinoOTA.handle();
  ws.cleanupClients();
  
  MessageOutput.loop();
  yield();
  NetworkSettings.loop();
  yield();
  NtpSettings.loop();
  yield();
  MqttSettings.loop();
  yield();
  LoraTransceiver.loop();
  yield();

  scheduler.execute();
}
