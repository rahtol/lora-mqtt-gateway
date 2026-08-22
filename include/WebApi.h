#pragma once

#include "WebApi_ws_live.h"
#include "WebApi_webapp.h"
#include "WebApi_sysstatus.h"
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>
#include "WebApi_errors.h"
#include "WebApi_mqtt.h"
#include "WebApi_network.h"

class WebApiClass {
public:
    WebApiClass();
    void init(Scheduler& scheduler);
    void reload();

    static void sendTooManyRequests(AsyncWebServerRequest* request);
    void writeConfig(JsonVariant& retMsg, const WebApiError code = WebApiError::GenericSuccess, const String& message = "Settings saved!");

    static bool parseRequestData(AsyncWebServerRequest* request, AsyncJsonResponse* response, JsonDocument& json_document);
    static uint64_t parseSerialFromRequest(AsyncWebServerRequest* request, String param_name = "inv");
    static bool sendJsonResponse(AsyncWebServerRequest* request, AsyncJsonResponse* response, const char* function, const uint16_t line);

    AsyncWebServer _server;

private:

    WebApiWsLiveClass _webApiWsLive;
    WebApiWebappClass _webApiWebApp;
    WebApiSysstatusClass _webApiSysstatus;
    WebApiMqttClass _webApiMqtt;
    WebApiNetworkClass _webApiNetwork;
};

extern WebApiClass WebApi;
