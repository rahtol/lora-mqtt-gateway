#include <vector>
#include "MqttSettings.h"
#include "LoRa.h"

class LoraTransceiverClass {
public:
    LoraTransceiverClass();
    void init();
    void loop();
    void init_subscriptions();
    void onMqttMessage(String subtopic, String payload);

    void set_mode_idle();
    void set_mode_receiving();
    void set_mode_transmitting(int period, int packet_length_min, int packet_length_max);

    int get_mode() {
        return _mode;
    };
    int get_period() {
        return period;
    };
    int get_packet_length_max() {
        return packet_length_max;
    };

    int get_no_sent_packets() {
        return no_sent_packets;
    };

    int get_no_received_packets() {
        return no_received_packets;
    };

    int get_no_misssed_packets() {
        return no_misssed_packets;
    };

    int get_no_corrupted_packets() {
        return no_corrupted_packets;
    };

    void exec_cmd(String cmd);

private:
    int _mode; // 0=idle, 1=receiving, 2=transmitting
    int period;
    int packet_length;
    int packet_length_min;
    int packet_length_max;
    int seqnr;
    int no_sent_packets;
    int no_received_packets;
    int no_misssed_packets;
    int no_corrupted_packets;
};

extern LoraTransceiverClass LoraTransceiver;
