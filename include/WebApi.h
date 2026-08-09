#pragma once

#include "WebApi_ws_live.h"
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>
#include "WebApi_errors.h"

class WebApiClass {
public:
    WebApiClass();
    void init(Scheduler& scheduler);
    void reload();

    static void sendTooManyRequests(AsyncWebServerRequest* request);

    static bool parseRequestData(AsyncWebServerRequest* request, AsyncJsonResponse* response, JsonDocument& json_document);
    static uint64_t parseSerialFromRequest(AsyncWebServerRequest* request, String param_name = "inv");
    static bool sendJsonResponse(AsyncWebServerRequest* request, AsyncJsonResponse* response, const char* function, const uint16_t line);

    AsyncWebServer _server;

private:

    WebApiWsLiveClass _webApiWsLive;
};

extern WebApiClass WebApi;
