#ifndef TOUCH_OSC__H
#define TOUCH_OSC__H
#include <Arduino.h>
#include <WiFiUdp.h>

// Control-Typ für jeden Parameter,
// 0 = t_none, 1 = t_page, 2 = t_label, 3 = t_led, 4 = t_toggle, 5 = t_push, 6 = t_fader,
// 7 = t_rotary, 8 = t_multitoggle, 9 = t_multipush, 10 = t_multifader, 11 = t_xypad
enum {
    t_none,
    t_page,
    t_label,
    t_led,
    t_toggle,
    t_push,
    t_fader,
    t_rotary,
    t_multitoggle,
    t_multipush,
    t_multifader,
    t_xypad
};

struct EEPromData {
    char password[50];
    char ssid[50];
    int udp_delay; // Delay nach seriellem Empfang
    int udp_timeout; // Sekunden nach 1. Anmeldung
    int udp_fb_others; // Feedback an andere Clients
    int udp_fb_self; // Feedback an sendenden Client
    int ap_mode;
    IPAddress station_ip; // Dummy!
    char password_station[50];
    char ssid_station[50];
    char flag;
};

class TouchOSC {
public:
    TouchOSC(int port);
    void read(String* controlTyp, int* parameternummer, int* idx1, int* idx2, int* val_1, int* val_2);
    void send(int type, int param, int idx_1, int idx_2, int val_1, int val_2);
    void sendText(int type, int param, String textstr);

    void readNonStop();

private:
    char UDP_send_buffer[256];
    char UDP_rcv_buffer[256];
    const char ControlNames[12][16] = {
        "/none", "/page", "/label", "/led", "/toggle", "/push", "/fader",
        "/rotary", "/multitoggle", "/multipush", "/multifader", "/xypad"
    };

    float udprcvbuf_extract_float(int idx);
    int udprcvbuf_extract_int(int idx);
    String extract_value(String data, char separator, int index);

    void int_to_udpsendbuf(int* idx, int val);
    void float_to_udpsendbuf(int* idx, float val);
    void format_to_udpsendbuf(int* idx, int count, char format_char);
    void clear_udpsendbuf();

    int setup_udp_send_buffer(int type, int param, int idx_1, int idx_2, int val_1, int val_2);
    int setup_udp_send_buffer_text(int type, int param, String textstr);
    void send_udp_buffer(int bytes_to_send);

    void parse_udp(int packet_size,
        String* controlStrPtr, int* parameterPtr,
        int* idx1ptr, int* idx2ptr,
        int* value1ptr, int* value2ptr);

    void runtime(); //in read() aufgerufen damit sie regelmäßig läuft
    void chores_and_timeouts();
    void init_eeprom_ap();
    void write_eeprom();
    void initNetwork(int udpport);
    void check_for_new_client();

    WiFiUDP udp;
    EEPromData eePromData;
};

int val_in_range(int value, int min_val, int max_val); // Wert zwischen min und max?

#endif //TOUCH_OSC__H

// für genauere Ausgabe im seriellen Monitor Kommentare entfernen
//#define DEBUG
//#define DEBUG_MON

#ifdef DEBUG
#define COM_MSGLN(...) Serial.println(__VA_ARGS__)
#define COM_MSG(...) Serial.print(__VA_ARGS__)
#else
#define COM_MSGLN(...)
#define COM_MSG(...)
#endif

#ifdef DEBUG_MON
#define MON_MSGLN(...) Serial.println(__VA_ARGS__)
#define MON_MSG(...) Serial.print(__VA_ARGS__)
#else
#define MON_MSGLN(...)
#define MON_MSG(...)
#endif
