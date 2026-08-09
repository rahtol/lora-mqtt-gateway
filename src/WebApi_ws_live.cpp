// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "WebApi_ws_live.h"
#include <AsyncJson.h>

#undef TAG
static const char* TAG = "webapi";

#ifndef PIN_MAPPING_REQUIRED
#define PIN_MAPPING_REQUIRED 0
#endif

WebApiWsLiveClass::WebApiWsLiveClass()
    : _ws("/livedata")
    , _wsCleanupTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::wsCleanupTaskCb, this))
    , _sendDataTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::sendDataTaskCb, this))
{
}

void WebApiWsLiveClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    server.on("/api/livedata/status", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiWsLiveClass::onLivedataStatus, this, _1)));

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    scheduler.addTask(_wsCleanupTask);
    _wsCleanupTask.enable();

    scheduler.addTask(_sendDataTask);
    _sendDataTask.enable();
    _simpleDigestAuth.setUsername("admin");
    _simpleDigestAuth.setRealm("live websocket");
    _simpleDigestAuth.setAuthType(AsyncAuthType::AUTH_DIGEST);

    reload();
}

void WebApiWsLiveClass::reload()
{
    _ws.removeMiddleware(&_simpleDigestAuth);
}

void WebApiWsLiveClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}

void WebApiWsLiveClass::sendDataTaskCb()
{
    // do nothing if no WS client is connected
    if (_ws.count() == 0) {
        return;
    }

    // Loop all inverters
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        auto inv = &inverter_data[i];
        if (inv == nullptr) {
            continue;
        }

        const uint32_t lastUpdateInternal = inv->t_lastUpdate;
        if (!((lastUpdateInternal > 0 && lastUpdateInternal > _lastPublishStats[i]) || (millis() - _lastPublishStats[i] > (10 * 1000)))) {
            continue;
        }

        _lastPublishStats[i] = millis();

        try {
            std::lock_guard<std::mutex> lock(_mutex);
            JsonDocument root;
            JsonVariant var = root;

            auto invArray = var["inverters"].to<JsonArray>();
            auto invObject = invArray.add<JsonObject>();

            generateCommonJsonResponse(var);
            generateInverterCommonJsonResponse(invObject, inv);
            generateInverterChannelJsonResponse(invObject, inv);

            String buffer;
            serializeJson(root, buffer);

            _ws.textAll(buffer);

        } catch (const std::bad_alloc& bad_alloc) {
            ESP_LOGE(TAG, "Call to /api/livedata/status temporarely out of resources. Reason: \"%s\".", bad_alloc.what());
        } catch (const std::exception& exc) {
            ESP_LOGE(TAG, "Unknown exception in /api/livedata/status. Reason: \"%s\".", exc.what());
        }
    }
}

void WebApiWsLiveClass::generateCommonJsonResponse(JsonVariant& root)
{
    auto totalObj = root["total"].to<JsonObject>();
    addField(totalObj, "Power", inverter_data[0].ac_data.power, "W", 1);
    addField(totalObj, "YieldDay", inverter_data[0].ac_data.yieldday, "Wh", 0);
    addField(totalObj, "YieldTotal", inverter_data[0].ac_data.yieldtotal, "kWh", 3);

    JsonObject hintObj = root["hints"].to<JsonObject>();
    hintObj["time_sync"] = false;
    hintObj["radio_problem"] = false;
    hintObj["default_password"] = false;
    hintObj["pin_mapping_issue"] = false;
}

void WebApiWsLiveClass::generateInverterCommonJsonResponse(JsonObject& invObj, InverterData *inv)
{
    invObj["serial"] = inv->serial;
    invObj["name"] = inv->name;
    invObj["order"] = 0;
    invObj["data_age"] = (millis() - inv->t_lastUpdate) / 1000;
    invObj["data_age_ms"] = millis() - inv->t_lastUpdate;
    invObj["poll_enabled"] = true;
    invObj["reachable"] = true;
    invObj["producing"] = true;
    invObj["limit_relative"] = 100.0;
    invObj["limit_absolute"] = 1500.0;
    invObj["radio_stats"]["tx_request"] = 0;
    invObj["radio_stats"]["tx_re_request"] = 0;
    invObj["radio_stats"]["rx_success"] = 0;
    invObj["radio_stats"]["rx_fail_nothing"] = 0;
    invObj["radio_stats"]["rx_fail_partial"] = 0;
    invObj["radio_stats"]["rx_fail_corrupt"] = 0;
    invObj["radio_stats"]["rssi"] = 0;
}

void WebApiWsLiveClass::generateInverterChannelJsonResponse(JsonObject& invObj, InverterData *inv)
{
    auto acObj = invObj["AC"]["0"].to<JsonObject>();
    addField(acObj, "Power", inv->ac_data.power, "W", 1);
    addField(acObj, "Voltage", inv->ac_data.voltage, "V", 1);
    addField(acObj, "Current", inv->ac_data.current, "A", 2);
    addField(acObj, "Frequency", inv->ac_data.frequency, "Hz", 2);
    addField(acObj, "PowerFactor", inv->ac_data.powerfactor, "", 3);
    addField(acObj, "ReactivePower", inv->ac_data.reactivepower, "var", 1);

    auto dcObj = invObj["DC"].to<JsonObject>();
    // Loop all channels
    for (auto &t : inv->dc_data) {
        int i = &t - inv->dc_data;
        auto chanObj = dcObj[String(static_cast<uint8_t>(i+1))].to<JsonObject>();
        addField(chanObj, "Power", t.power, "W", 1);
        addField(chanObj, "Voltage", t.voltage, "V", 1);
        addField(chanObj, "Current", t.current, "A", 1);
        addField(chanObj, "YieldDay", t.yieldday, "Wh", 0);
        addField(chanObj, "YieldTotal", t.yieldtotal, "kWh", 3);
    }

    auto invObj = invObj["INV"].to<JsonObject>();
    addField(invObj, "Power DC", inv->ac_data.powerdc, "W", 1);
    addField(invObj, "Yield Day", inv->ac_data.yieldday, "Wh", 0);
    addField(invObj, "Yield Total", inv->ac_data.yieldtotal, "kWh", 3);
    addField(invObj, "Temperature", inv->ac_data.temperature, "°C", 1);
    addField(invObj, "Efficiency", inv->ac_data.efficiency, "%", 3);

    invObj["events"] = -1;
}

void WebApiWsLiveClass::addField(JsonObject& parent, const String& name, const float value, const String& unit, const uint8_t digits)
{
    parent[name]["v"] = value;
    parent[name]["u"] = unit;
    parent[name]["d"] = digits;
}

void WebApiWsLiveClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        ESP_LOGD(TAG, "Websocket: [%s][%" PRIu32 "] connect", server->url(), client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        ESP_LOGD(TAG, "Websocket: [%s][%" PRIu32 "] disconnect", server->url(), client->id());
    }
}

void WebApiWsLiveClass::onLivedataStatus(AsyncWebServerRequest* request)
{
    try {
        std::lock_guard<std::mutex> lock(_mutex);
        AsyncJsonResponse* response = new AsyncJsonResponse();
        auto& root = response->getRoot();
        auto invArray = root["inverters"].to<JsonArray>();
        auto serial = WebApi.parseSerialFromRequest(request);

        if (serial > 0) {
            auto inv = Hoymiles.getInverterBySerial(serial);
            if (inv != nullptr) {
                JsonObject invObject = invArray.add<JsonObject>();
                generateInverterCommonJsonResponse(invObject, inv);
                generateInverterChannelJsonResponse(invObject, inv);
            }
        } else {
            // Loop all inverters
            for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
                auto inv = Hoymiles.getInverterByPos(i);
                if (inv == nullptr) {
                    continue;
                }

                JsonObject invObject = invArray.add<JsonObject>();
                generateInverterCommonJsonResponse(invObject, inv);
            }
        }

        generateCommonJsonResponse(root);

        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    } catch (const std::bad_alloc& bad_alloc) {
        ESP_LOGE(TAG, "Call to /api/livedata/status temporarely out of resources. Reason: \"%s\".", bad_alloc.what());
        WebApi.sendTooManyRequests(request);
    } catch (const std::exception& exc) {
        ESP_LOGE(TAG, "Unknown exception in /api/livedata/status. Reason: \"%s\".", exc.what());
        WebApi.sendTooManyRequests(request);
    }
}
