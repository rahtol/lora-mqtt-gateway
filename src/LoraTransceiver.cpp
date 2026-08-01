#include "LoraTransceiver.h"
#include <NtpSettings.h>
#include <MessageOutput.h>
#include "FifoBuffer.h"
#include "MqttSettings.h"

#define ss 12
#define rst 14
#define dio0 2

unsigned t0 = 0;
unsigned t1 = 0;

FifoBufferClass<uint8_t> receivedPackets(8, 258);

typedef struct _DC_data {
    float voltage;
    float current;
    float power;
    float yieldday;
    float yieldtotal;
    float irradiation;
} DC_data;

typedef struct _AC_data {
    float voltage;
    float current;
    float power;
    float powerdc;
    float powerfactor;
    float frequency;
    float reactivepower;
    float temperature;
    float yieldday;
    float yieldtotal;
    float efficiency;
} AC_data;

float decode_uint16(uint8_t *data, int divisor) {
    uint16_t val = (data[0] << 8) | data[1];
    float tmp = (float) val;
//    MessageOutput.logf("decode_uint16: data[0]=%02x, data[1]=%02x, val=%u, divisor=%d, tmp=%f", data[0], data[1], val, divisor, tmp);
    return tmp / divisor;
}

float decode_uint32(uint8_t *data, int divisor) {
    uint32_t val = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    float tmp = (float) val;
//    MessageOutput.logf("decode_uint32: data[0]=%02x, data[1]=%02x, data[2]=%02x, data[3]=%02x, val=%u, divisor=%d, tmp=%f", data[0], data[1], data[2], data[3], val, divisor, tmp);
    return tmp / divisor;
}

float decode_int16(uint8_t *data, int divisor) {
    int16_t val = (data[0] << 8) | data[1];
    float tmp = (float) val;
//    MessageOutput.logf("decode_int16: data[0]=%02x, data[1]=%02x, val=%d, divisor=%d, tmp=%f", data[0], data[1], val, divisor, tmp);
    return tmp / divisor;
}

void decode_dc_data(uint8_t *data, DC_data *dc_data0, DC_data *dc_data1) {
    dc_data0->voltage = decode_uint16(data + 0, 10);
    dc_data1->voltage = dc_data0->voltage;
    dc_data0->current = decode_uint16(data + 2, 100);
    dc_data1->current = decode_uint16(data + 4, 100);
    dc_data0->power = decode_uint16(data + 6, 10);
    dc_data1->power = decode_uint16(data + 8, 10);
    dc_data0->yieldtotal = decode_uint32(data + 10, 1000);
    dc_data1->yieldtotal = decode_uint32(data + 14, 1000);
    dc_data0->yieldday = decode_uint16(data + 18, 1);
    dc_data1->yieldday = decode_uint16(data + 20, 1);
    dc_data0->irradiation = 0.0;
    dc_data1->irradiation = 0.0;
}

void decode_ac_data(uint8_t *data, AC_data *ac_data) {
    ac_data->voltage = decode_uint16(data + 46, 10);
    ac_data->current = decode_uint16(data + 54, 100);
    ac_data->power = decode_uint16(data + 50, 10);
    ac_data->reactivepower = decode_int16(data + 52, 10);
    ac_data->powerdc = 0.0;
    ac_data->powerfactor = decode_uint16(data + 56, 1000);
    ac_data->frequency = decode_uint16(data + 48, 100);
    ac_data->temperature = decode_int16(data + 52, 10);
    ac_data->yieldday = 0.0;
    ac_data->yieldtotal = 0.0;
    ac_data->efficiency = 0.0;
}

void decode_statistics(uint8_t *data, DC_data *dc_data, AC_data *ac_data) {
    decode_dc_data(data + 2, &dc_data[0], &dc_data[1]);
    decode_dc_data(data + 24, &dc_data[2], &dc_data[3]);
    decode_ac_data(data, ac_data);
}

void publish_dc_data(DC_data *dc_data) {
    for (int i = 0; i < 4; i++) {
        String subtopic = String(i+1);
        MqttSettings.publish(subtopic + "/voltage", String(dc_data[i].voltage, 1));
        MqttSettings.publish(subtopic + "/current", String(dc_data[i].current, 2));
        MqttSettings.publish(subtopic + "/power", String(dc_data[i].power, 1));
        MqttSettings.publish(subtopic + "/yieldday", String(dc_data[i].yieldday, 0));
        MqttSettings.publish(subtopic + "/yieldtotal", String(dc_data[i].yieldtotal, 3));
        MqttSettings.publish(subtopic + "/irradiation", String(dc_data[i].irradiation,2));
    }
}

void publish_ac_data(AC_data *ac_data) {
    MqttSettings.publish("0/voltage", String(ac_data->voltage, 1));
    MqttSettings.publish("0/current", String(ac_data->current, 2));
    MqttSettings.publish("0/power", String(ac_data->power, 1));
    MqttSettings.publish("0/reactivepower", String(ac_data->reactivepower, 1));
    MqttSettings.publish("0/powerdc", String(ac_data->powerdc, 1));
    MqttSettings.publish("0/powerfactor", String(ac_data->powerfactor, 3));
    MqttSettings.publish("0/frequency", String(ac_data->frequency, 2));
    MqttSettings.publish("0/temperature", String(ac_data->temperature, 1));
    MqttSettings.publish("0/yieldday", String(ac_data->yieldday, 0));
    MqttSettings.publish("0/yieldtotal", String(ac_data->yieldtotal, 3));
    MqttSettings.publish("0/efficiency", String(ac_data->efficiency, 3));
}

String decode_payload_as_string(uint8_t *data, int len) {
    String payload = "";
    for (int i = 0; i < len; i++) {
        String s = String(data[i], HEX);
        if (s.length() < 2) {
            s = "0" + s;
        };
        payload += s;
        if (i < len - 1) {
            payload += " ";
        }
    }
    return payload;
}


void onTxDone()
{
  t1 = millis();
}

void onReceive(int packetSize)
{
    // Validate packet size (buffer has 258 bytes, 2 are reserved for header)
    if (packetSize > 256 || packetSize <= 0) {
        MessageOutput.logf("Invalid packet size: %d, dropping packet", packetSize);
        return;
    }
    
    uint8_t *bf = receivedPackets.getNextHeadBufferPtr();
    if (bf == nullptr) {
        MessageOutput.logf("FIFO buffer error: no buffer available");
        return;
    }
    
    bf[0] = (uint8_t) packetSize; // store packet size in the first byte
    bf[1] = (uint8_t) LoRa.packetRssi(); // store RSSI in the second byte
    int snr = 0; // LoRa.packetSnr();
//    MessageOutput.logf("snr=%d", snr);
    bf[2] = (uint8_t) snr;
    for (int i = 0; i < packetSize; i++) {
        bf[i+3] = LoRa.read();
    }
}

LoraTransceiverClass::LoraTransceiverClass() :
    _mode(0),
    period(10000),
    packet_length(0),
    packet_length_min(64),
    packet_length_max(252),
    seqnr(-1),
    no_sent_packets(0),
    no_received_packets(0),
    no_misssed_packets(0),
    no_corrupted_packets(0)
{
}

void LoraTransceiverClass::init() 
{
    MessageOutput.logf("LoRa Transceiver initializing... ");

    LoRa.setPins(ss, rst, dio0); 

    while (!LoRa.begin(433E6))     //433E6 - Asia, 866E6 - Europe, 915E6 - North America
    {
        MessageOutput.logf(".");
        delay(500);
    }

    LoRa.onTxDone(onTxDone);
    LoRa.onReceive(onReceive);
//    LoRa.setSyncWord(0xA5);
//    LoRa.enableCrc();
    MessageOutput.logf("LoRa Transceiver initializing OK!");
}

void LoraTransceiverClass::loop() {
    static unsigned long t_last_duty_cycle = 0;
    static unsigned long t_last_mqtt_cycle = 0;
    unsigned long t_current = millis();
    if (_mode == 1) {
        // receiving
        if (!receivedPackets.isEmpty()) {
            uint8_t *packet = receivedPackets.pop(false);
            int packetSize = packet[0];
            int rssi = (int8_t)packet[1];
            int snr = (int8_t)packet[2];
            uint32_t seqnr = *((uint32_t *) (packet + 3));
            receivedPackets.pop(true);

            MessageOutput.logf("Received packet len='%d', seqnr='%d' with RSSI %d, SNR=%d", packetSize, seqnr, rssi, snr);
            MessageOutput.logf("Payload: %s", decode_payload_as_string(packet + 7, packetSize - 4).c_str());

            no_received_packets++;
            if (packetSize != 68) {
                MessageOutput.logf("Packet format error: seqnr or len not found or in wrong order");
                no_corrupted_packets++;
            }
            else
            {
                DC_data dc_data[4];
                AC_data ac_data;
                decode_statistics(packet + 7, dc_data, &ac_data);

                ac_data.powerdc = dc_data[0].power + dc_data[1].power + dc_data[2].power + dc_data[3].power;
                ac_data.yieldday = dc_data[0].yieldday + dc_data[1].yieldday + dc_data[2].yieldday + dc_data[3].yieldday;
                ac_data.yieldtotal = dc_data[0].yieldtotal + dc_data[1].yieldtotal + dc_data[2].yieldtotal + dc_data[3].yieldtotal;
                ac_data.efficiency = (ac_data.powerdc != 0) ? (ac_data.power / ac_data.powerdc) * 100 : 0;

                publish_dc_data(dc_data);
                publish_ac_data(&ac_data);
            }
        }
    }
    else if (_mode == 2) {
        // transmitting
        if (t_current - t_last_duty_cycle > period) {
            t_last_duty_cycle = t_current;
            packet_length = random(packet_length_min, packet_length_max);
            String payload = "seqnr=" + String(seqnr) + ", len=" + String(packet_length) + ", millis=" + String(millis()) + " dt=" + String(t1 - t0);
            MessageOutput.logf("Transmitting packet: %s", payload.c_str());
            payload += " ";
            while (payload.length() < packet_length) {
                payload += "x";
            }
            t0 = millis();
            LoRa.beginPacket();
            LoRa.print(payload);
            LoRa.endPacket(true);
            seqnr++;
            no_sent_packets++;
        }
    }

    // MQTT loop every 15 seconds
    if (t_current - t_last_mqtt_cycle > 15000) {
        t_last_mqtt_cycle = t_current;
        if (_mode == 1) {
            MqttSettings.publish("no_received_packets", String(no_received_packets));
            MqttSettings.publish("no_misssed_packets", String(no_misssed_packets));
            MqttSettings.publish("no_corrupted_packets", String(no_corrupted_packets));
        }
        else if (_mode == 2) {
            MqttSettings.publish("no_sent_packets", String(no_sent_packets));
        }
    }
}

void LoraTransceiverClass::init_subscriptions() {
}

void LoraTransceiverClass::onMqttMessage(String subtopic, String payload) {
}

void LoraTransceiverClass::set_mode_idle() {
    _mode = 0;
    LoRa.idle();
}

void LoraTransceiverClass::set_mode_receiving() {
    _mode = 1;
    seqnr = -1;
    LoRa.receive();
}

void LoraTransceiverClass::set_mode_transmitting(int period, int packet_length_min, int packet_length_max) {
    _mode = 2;
    if (seqnr < 0) seqnr = 0;
    this->period = period;
    this->packet_length_min = packet_length_min < 64 ? 64 : packet_length_min;
    this->packet_length_max = packet_length_max < this->packet_length_min ? this->packet_length_min : packet_length_max;
}

void LoraTransceiverClass::exec_cmd(String cmd)
{
    cmd.toLowerCase();
    cmd.trim();
    if (cmd == String("restart")) {
        MessageOutput.logf("Restarting...");
        delay(3000);
        ESP.restart();
    }
     else if (cmd == String("idle")) {
        set_mode_idle();
    }
    else if (cmd.startsWith("receive")) {
        set_mode_receiving();
    }
    else if (cmd.startsWith("transmit")) {
        String params = cmd.substring(String("transmit").length());
        int period = 10000;
        int packet_length_min = 64;
        int packet_length_max = 224;
        int idx1 = params.indexOf(',');
        if (idx1 > 0) {
            period = params.substring(0, idx1).toInt();
            int idx2 = params.indexOf(',', idx1 + 1);
            if (idx2 > 0) {
                packet_length_min = params.substring(idx1 + 1, idx2).toInt();
                packet_length_max = params.substring(idx2 + 1).toInt();
            }
            else {
                packet_length_min = params.substring(idx1 + 1).toInt();
            }
        }
        set_mode_transmitting(period, packet_length_min, packet_length_max);
    }
    else if (cmd.startsWith("period")) {
        int new_period = period;
        String params = cmd.substring(String("period").length());
        if (params.length() > 0) {
            new_period = params.toInt();
            if (new_period < 1000) new_period = period;
            if (new_period > 60000) new_period = period;
        }
        if (new_period != period) {
            MessageOutput.logf("Set period to %d ms", new_period);
            period = new_period;
        }
        else {
            MessageOutput.logf("Current period: %d ms", period);
        }
    }
    else {
        MessageOutput.logf("Unknown command: %s", cmd.c_str());
    }
}

LoraTransceiverClass LoraTransceiver;
