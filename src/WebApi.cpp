// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "WebApi.h"
#include <AsyncJson.h>
#include "LoraTransceiver.h"
#include "version.h"
#include "NtpSettings.h"

#undef TAG
static const char* TAG = "webapi";

#define HTTP_PORT 80


// Replaces placeholder with button section in your web page
String processor(const String& var){
  //Serial.println(var);
  if(var == "COUNTERPLACEHOLDER"){
    String countervalues = "";
    if (LoraTransceiver.get_mode() == 1) {
      countervalues +=  "<h4>packets received: " + String(LoraTransceiver.get_no_received_packets()) + "</h4>";
      countervalues +=  "<h4>packets missed: " + String(LoraTransceiver.get_no_misssed_packets()) + "</h4>";
      countervalues +=  "<h4>packets corrupted: " + String(LoraTransceiver.get_no_corrupted_packets()) + "</h4>";
    }
    else if (LoraTransceiver.get_mode() == 2) {
      countervalues +=  "<h4>packets sent: " + String(LoraTransceiver.get_no_sent_packets()) + "</h4>";
    }
    return countervalues;
  }
  else if (var == "BOOTTIMEANDDATE") {
    return "<h4>Up since: " + NtpSettings.get_boottime_and_date() + "</h4>";
  }
  else if (var == "VERSION") {
    return "<h4>" + String(Lora_Mqtt_Gateway::version) + "</h4>";
  }
  else if (var == "NOW") {
    return "<h4> counter values at " + NtpSettings.getLocalTimeAndDate() + "</h4>";
  }
  else if (var == "DEVICEID") {
    return NetworkSettings.deviceID();
  }
  return String();
}


WebApiClass::WebApiClass()
    : _server(HTTP_PORT)
{
}

void WebApiClass::init(Scheduler& scheduler)
{
    _server.on("/api/i18n/languages", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "application/json", "[]");
    });

    _webApiWsLive.init(_server, scheduler);
    _webApiWebApp.init(_server, scheduler);

    _server.begin();
}

void WebApiClass::reload()
{
    _webApiWsLive.reload();
}

void WebApiClass::sendTooManyRequests(AsyncWebServerRequest* request)
{
    auto response = request->beginResponse(429, asyncsrv::T_text_plain, "Too Many Requests");
    response->addHeader(asyncsrv::T_retry_after, "60");
    request->send(response);
}

bool WebApiClass::parseRequestData(AsyncWebServerRequest* request, AsyncJsonResponse* response, JsonDocument& json_document)
{
    auto& retMsg = response->getRoot();
    retMsg["type"] = "warning";

    if (!request->hasParam("data", true)) {
        retMsg["message"] = "No values found!";
        retMsg["code"] = WebApiError::GenericNoValueFound;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return false;
    }

    const String json = request->getParam("data", true)->value();
    const DeserializationError error = deserializeJson(json_document, json);
    if (error) {
        retMsg["message"] = "Failed to parse data!";
        retMsg["code"] = WebApiError::GenericParseError;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}

uint64_t WebApiClass::parseSerialFromRequest(AsyncWebServerRequest* request, String param_name)
{
    if (request->hasParam(param_name)) {
        String s = request->getParam(param_name)->value();
        return strtoll(s.c_str(), NULL, 16);
    }

    return 0;
}

bool WebApiClass::sendJsonResponse(AsyncWebServerRequest* request, AsyncJsonResponse* response, const char* function, const uint16_t line)
{
    bool ret_val = true;
    if (response->overflowed()) {
        auto& root = response->getRoot();

        root.clear();
        root["message"] = String("500 Internal Server Error: ") + function + ", " + line;
        root["code"] = WebApiError::GenericInternalServerError;
        root["type"] = "danger";
        response->setCode(500);
        ESP_LOGE(TAG, "WebResponse failed: %s, %" PRIu16 "", function, line);
        ret_val = false;
    }

    response->setLength();
    request->send(response);
    return ret_val;
}

WebApiClass WebApi;
