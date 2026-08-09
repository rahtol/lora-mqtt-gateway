#include <Arduino.h>

#define INV_MAX_COUNT 1

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


class InverterData {
public:
    InverterData() : 
        name("Garage"), 
        serial("116183777340"), 
        serial_u(0x116183777340), 
        t_lastUpdate(0), 
        last_seqnr(0xFFFFFFFF), 
        last_packet_rssi(0),
        no_received_packets(0),
        no_misssed_packets(0),
        no_corrupted_packets(0) {}

    String name;
    String serial;
    uint64_t serial_u;
    unsigned long t_lastUpdate;
    uint32_t last_seqnr;
    int last_packet_rssi;
    uint32_t no_received_packets;
    uint32_t no_misssed_packets;
    uint32_t no_corrupted_packets;
    DC_data dc_data[4];
    AC_data ac_data;
};

extern InverterData inverter_data[INV_MAX_COUNT];
