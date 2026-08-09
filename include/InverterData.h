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
    String name;
    String serial;
    unsigned long t_lastUpdate;
    DC_data dc_data[4];
    AC_data ac_data;
};

extern InverterData inverter_data[INV_MAX_COUNT];
