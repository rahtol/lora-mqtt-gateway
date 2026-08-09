// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>
#include "InverterData.h"

class WebApiWsLiveClass {
public:
    WebApiWsLiveClass();
    void init(AsyncWebServer& server, Scheduler& scheduler);
    void reload();

private:
    static void generateInverterCommonJsonResponse(JsonObject& root, InverterData *inv);
    static void generateInverterChannelJsonResponse(JsonObject& root, InverterData *inv);
    static void generateCommonJsonResponse(JsonVariant& root);

    static void addField(JsonObject& parent, const String& name, const float value, const String& unit, const uint8_t digits);

    void onLivedataStatus(AsyncWebServerRequest* request);
    void onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

    AsyncWebSocket _ws;
    AsyncAuthenticationMiddleware _simpleDigestAuth;

    uint32_t _lastPublishStats[INV_MAX_COUNT] = { 0 };

    std::mutex _mutex;

    Task _wsCleanupTask;
    void wsCleanupTaskCb();

    Task _sendDataTask;
    void sendDataTaskCb();
};
