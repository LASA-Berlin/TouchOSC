// ###  TOUCH OSC + UDP BUFFER UTILS  ###
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <WiFiClient.h>

#include "Touch_OSC.h"

#define ARRARY_SIZE 256

// Control-Typ für jeden Parameter,
// 0 = t_none, 1 = t_page, 2 = t_label, 3 = t_led, 4 = t_toggle, 5 = t_push, 6 = t_fader,
// 7 = t_rotary, 8 = t_multitoggle, 9 = t_multipush, 10 = t_multifader, 11 = t_x


char UDP_send_buffer[ARRARY_SIZE];      // zu sendende Daten
char UDP_rcv_buffer[ARRARY_SIZE];       // hier kommen UDP-Daten an, koennte noch private werden

unsigned long last_millis_1s, last_millis_100ms, last_millis_10ms;  // Für Timer-Ticks gebraucht

IPAddress CurrentClientIP(0, 0, 0, 0);  // Default, kann sich nach Empfang ändern
const IPAddress c_UnsetIP(0, 0, 0, 0);
const int c_clients_max = 4;            // Setzt die maximal Anzahl an verbunden Clients

IPAddress ClientIPs[c_clients_max];
int ClientIPs_timer[c_clients_max];
int ClientIPs_idx = 0;




TouchOSC::TouchOSC(int port) {          // Port für eingehende UDP-Packete
  initNetwork(port);
}

void TouchOSC::read(String *controlTyp, int *parameternummer, int *idx1, int *idx2, int *val_1, int *val_2) {
  // Liest länge des UDP-Packets und gibt Adressen an parser_udp() weiter
  // Lässt runtime() laufen für Hintergrundsprozzese
  int packet_size, udp_buf_len;
  packet_size = udp.parsePacket();
  if (packet_size) {
    udp.read(UDP_rcv_buffer, packet_size);  // read the packet into the buffer
    parse_udp(packet_size, controlTyp, parameternummer, idx1, idx2, val_1, val_2);
  } else {
    // MON_MSGLN("kein Packet empfangen");
  }
  runtime();
}

void TouchOSC::send(int type, int param, int idx_1, int idx_2, int val_1, int val_2) {
  // ruft nötige Funktion zum senden in der entsprechenden Reihenfolge auf und gibt Argumente weiter
  clear_udpsendbuf();
  int bufferLength = setup_udp_send_buffer(type, param, idx_1, idx_2, val_1, val_2);
  send_udp_buffer(bufferLength);
  delay(13);
}

void TouchOSC::sendText(int type, int param, String textstr) {
  // ruft nötige Funktion zum senden in der entsprechenden Reihenfolge auf und gibt Argumente weiter
  clear_udpsendbuf();
  int bufferLength = setup_udp_send_buffer_text(type, param, textstr);
  send_udp_buffer(bufferLength);
}

void TouchOSC::readNonStop() {
  // Liest UDP Pakete damit alles einkommenden Werte auf dem Seriellen Monitor zu sehen sind
  String controlTyp;
  int parameternummer, idx1, idx2, val_1, val_2;
  read(&controlTyp, &parameternummer, &idx1, &idx2, &val_1, &val_2);
}

void TouchOSC::runtime() {    
  // Hintergrundsprozese
  check_for_new_client();
  chores_and_timeouts();
}

float TouchOSC::udprcvbuf_extract_float(int idx) {
  // holt Float-Bytes aus UDP_rcv_buffer, wandelt sie in Integer und dreht die byte reihenfolge
  union {
    byte b[4];
    float f;
  } u;
  int n;
  for ( n = 0 ; n < 4 ; n++ ) u.b[n] = UDP_rcv_buffer[idx + 3 - n];
  return (u.f); // in Integer 0..127 wandeln
}

int TouchOSC::udprcvbuf_extract_int(int idx) {
  // holt Int-Bytes aus UDP_rcv_buffer, wandelt sie in Integer und dreht die byte reihenfolge
  union {
    byte b[4];
    int32 i;
  } u;
  int n;
  for ( n = 0 ; n < 4 ; n++ ) u.b[n] = UDP_rcv_buffer[idx + 3 - n];
  return (u.i); // in Integer 0..127 wandeln
}

String TouchOSC::extract_value(String data, char separator, int index) {
  // Werte-Strings extrahieren, Trennzeichen in "separator"
  // OSC-Message mit Trenner "/": /<1>/<2>/<3>/..., da erstes Trennzeichen am Anfang
  // Text-Message mit Trenner "=": <0>=<1>
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || data.charAt(i) == 0 || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void TouchOSC::int_to_udpsendbuf(int *idx, int val) {
  // zum Umsetzen von Int32 in Bytes, schreibt in UDP_send_buffer
  // Reihenfolge umdrehen
  union {
    byte b[4];
    int32 i32;
  } u;
  int i;
  u.i32 = val;
  for ( i = 0 ; i < 4 ; i++ ) UDP_send_buffer[*idx + 3 - i] = u.b[i];
  *idx += 4;
}

void TouchOSC::float_to_udpsendbuf(int *idx, float val) {
  // zum Umsetzen von Float in Bytes, schreibt in UDP_send_buffer
  // Reihenfolge umdrehen
  union {
    byte b[4];
    float f32;
  } u;
  int i;
  u.f32 = val;
  for ( i = 0 ; i < 4 ; i++ ) UDP_send_buffer[*idx + 3 - i] = u.b[i];
  *idx += 4;
}

void TouchOSC::format_to_udpsendbuf(int *idx, int count, char format_char) {
  // schreibt Format-Info in UDP_send_buffer
  if (count > 0) {
    UDP_send_buffer[*idx] = ',';
    UDP_send_buffer[*idx + 1] = format_char;
    if (count > 1)
      UDP_send_buffer[*idx + 2] = format_char;
    UDP_send_buffer[*idx + 3] = 0;
    *idx += 4;
  }
}

void TouchOSC::clear_udpsendbuf() {
  // Setzt alles im Buffer auf 0
  int i;
  for (i = 0; i < ARRARY_SIZE; i++)
    UDP_send_buffer[i] = 0;
}

void TouchOSC::parse_udp(int packet_size,
                         String * controlTypPtr, int *parameterPtr,
                         int *idx1ptr, int *idx2ptr,
                         int *value1ptr, int *value2ptr) {
  // Wertet UDP-Packete in Teile und werte diese aus
  int i, j, idx, send_udp, udp_buf_len;
  String  tempstr;
  float val_1 = 0;
  float val_2 = 0;
  int arg_count = 0;
  int arg_count_f = 0;
  int arg_count_i = 0;

  // Position der Argumente, Komma suchen
  for (i = 0; (i <= packet_size) && (UDP_rcv_buffer[i] != 0x2C); i++);  // ","
  idx = i;

  if (UDP_rcv_buffer[idx + 1] == 'f') arg_count_f++;
  if (UDP_rcv_buffer[idx + 1] == 'i') arg_count_i++;
  if (UDP_rcv_buffer[idx + 2] == 'f') arg_count_f++;  // "ff"
  if (UDP_rcv_buffer[idx + 2] == 'i') arg_count_i++;  // "ii"
  arg_count = arg_count_f + arg_count_i;

  if (arg_count) {
    // erster Teil des Strings gibt Typ des Controls an, z.B. "fader" bei "/fader/1000/9"
    *controlTypPtr = extract_value(UDP_rcv_buffer, '/', 1);
    COM_MSG(*controlTypPtr);
    // Parameter-Nummer
    tempstr = extract_value(UDP_rcv_buffer, '/', 2);
    *parameterPtr = tempstr.toInt();
    COM_MSG(" Param: ");
    COM_MSG(*parameterPtr);
    // erster Index bei Multi-Fadern und Y-Position bei Multi-Buttons, sonst 0
    tempstr = extract_value(UDP_rcv_buffer, '/', 3);  // evt. nicht vorhanden, dann 0
    *idx1ptr = tempstr.toInt();
    if (*idx1ptr) {
      COM_MSG("  Idx_1: ");
      COM_MSG(*idx1ptr);
    }
    // X-Position bei Multi-Buttons, somst 0
    tempstr = extract_value(UDP_rcv_buffer, '/', 4);  // evt. nicht vorhanden, dann 0
    *idx2ptr = tempstr.toInt();
    if (*idx2ptr) {
      COM_MSG("  Idx_2: ");
      COM_MSG(*idx2ptr);
    }
    idx += 4;
    if (arg_count_f)
      val_1 = udprcvbuf_extract_float(idx); // Reihenfolge Bytes umkehren
    else
      val_1 = (float)udprcvbuf_extract_int(idx);

    COM_MSG("  Val_1: ");
    COM_MSG(val_1);
    if (arg_count_f == 2) {
      // Zweiter Wert X (!), auf Anfang 2. Float
      idx += 4;
      val_2 = udprcvbuf_extract_float(idx); // Reihenfolge Bytes umkehren
      COM_MSG("  Val_2: ");
      COM_MSG(val_2);

    }
    if (arg_count_i == 2) {
      // Zweiter Wert, auf Anfang 2. Float
      idx += 4;
      val_2 = (float)udprcvbuf_extract_int(idx);
      COM_MSG(", ");
      COM_MSG(val_2);
    }
  }
  COM_MSGLN("");

  *value1ptr = (int)val_1;
  *value2ptr = (int)val_2;
}

int TouchOSC::setup_udp_send_buffer(int type, int param, int idx_1, int idx_2, int val_1, int val_2) {
  // Buffer für UDP Send aufbereiten, liefert endgültige Länge des UDP-Buffers
  String sendstr = ControlNames[type];
  int buf_len = 0;
  int val_count = 1;
  if (val_2 >= 0)
    val_count++;
  if (param >= 0)
    sendstr += "/" + String(param);
  // Indexe bei Multibuttons und Multifadern
  if (idx_1 >= 1)
    sendstr += "/" + String(idx_1);
  if (idx_2 >= 1)
    sendstr += "/" + String(idx_2);
  COM_MSG("WIF->UDP: " + sendstr);

  buf_len = sendstr.length();
  sendstr.toCharArray(UDP_send_buffer, buf_len + 1);
  // auf Long-Grenze bringen
  buf_len = buf_len + 4 - (buf_len % 4);
  // String ist im Buffer, nun ",i" und Integer einbauen
  format_to_udpsendbuf(&buf_len, val_count, 'i'); // 2 Werte wenn XY
  int_to_udpsendbuf(&buf_len, val_1); // umgek. Reihenfolge in Send-Buffer
  if (val_2 >= 0) {
    int_to_udpsendbuf(&buf_len, val_2); // nur wenn val_2 >= 0
    COM_MSGLN(" = " + String(val_1) + ", " + String(val_2));
  } else
    COM_MSGLN(" = " + String(val_1));
  return (buf_len);
}

int TouchOSC::setup_udp_send_buffer_text(int type, int param, String textstr) {
  // UDP_send_buffer nach Text-Befehl in HX3_text_buffer vorbereiten
  String sendstr = ControlNames[type];
  int i;
  int buf_len = 0;
  if (param >= 0)
    sendstr += "/" + String(param);
  COM_MSG("WIF->UDP: " + sendstr);
  COM_MSGLN(" = " + textstr);
  buf_len = sendstr.length();
  sendstr.toCharArray(UDP_send_buffer, buf_len + 1);
  // auf Long-Grenze bringen
  buf_len = buf_len + 4 - (buf_len % 4);
  format_to_udpsendbuf(&buf_len, 1, 's'); // 1 String
  // String in den Send-Buffer
  for (i = 0; i < textstr.length(); i++) {
    UDP_send_buffer[buf_len] = textstr.charAt(i);
    buf_len++;
  }
  // auf Long-Grenze bringen
  buf_len = buf_len + 4 - (buf_len % 4);
  return (buf_len);
}

void TouchOSC::send_udp_buffer(int bytes_to_send) {
  // Werte an alle angemeldete Clients senden
  int i;
  for (i = 0; i < 4; i++)
    if (ClientIPs_timer[i]) {
      udp.beginPacket(ClientIPs[i], 9000);
      udp.write(UDP_send_buffer, bytes_to_send); // Send data to Client
      udp.endPacket();
    }
  delay(eePromData.udp_delay);
}

void TouchOSC::init_eeprom_ap() {
  // Bereitet eeprom vor
  strcpy(eePromData.ssid, "TouchOSC Bridge");
  strcpy(eePromData.password, "password");
  eePromData.udp_fb_others = 1; // default
  eePromData.udp_fb_self = 1;
  eePromData.udp_delay = 2;
  eePromData.udp_timeout = 300; // 5 Minuten
  eePromData.ap_mode = 1;     // ESP8266 ist selbst Access Point
}

void TouchOSC::initNetwork(int udpport) {
  // setzt das Netzwerk auf
  int i, temp_ap_mode, len;
  char line_buf[100];
  int idx;
  String temp, ssid_list;

  // Read EEPROM
  EEPROM.begin(sizeof(EEPromData));
  EEPROM.get(0, eePromData);
  EEPROM.end();
  ssid_list = "";
  int ssids_found = WiFi.scanNetworks();
  if (ssids_found) {
    MON_MSGLN("Found SSIDs: ");
    for (i = 0; i < ssids_found; i++) {
      temp = WiFi.SSID(i);
      MON_MSGLN(temp);
      if (i) ssid_list += ",\r\n";  // mehr als eine Zeile?
      else ssid_list += "\r\n";
      ssid_list += "\"" + temp + "\"";
    }
  }

  if (eePromData.flag != 0x55) {
    init_eeprom_ap();
    strcpy(eePromData.ssid_station, "Your Network");  // "Your Network"
    strcpy(eePromData.password_station, "password");  // "password"
    eePromData.flag = 0x55;     // initialized
    write_eeprom();
    MON_MSGLN("WiFi name reset to SSID 'Make TouchOSC Bridge' and PW 'password'");
    SPIFFS.format();
    MON_MSGLN("SPIFFS formatted, upload HTML/CSS files!");
  }

  temp_ap_mode = eePromData.ap_mode;
  if (temp_ap_mode) {
    // ESP8266 ist selbst Access Point
    MON_MSGLN("WiFi Acces Point Mode, IP 192.168.4.1");
    WiFi.mode(WIFI_AP);     // Only Access point
    WiFi.softAP(eePromData.ssid, eePromData.password);  // access point
  } else {
    // Externen Access Point bzw. Router verwenden
    MON_MSGLN("WiFi Station Mode ");
    WiFi.mode(WIFI_STA);
    WiFi.begin(eePromData.ssid_station, eePromData.password_station);
    i = 0;
    MON_MSG("Connecting ...");
    while (WiFi.status() != WL_CONNECTED) {
      // 10 Sekunden Warten auf Verbindung
      delay(333);
      COM_MSG(".");
      i++;
      if (i > 30) {
        temp_ap_mode = 1;
        break;
      }
    }
    MON_MSGLN("");
    if (temp_ap_mode) {
      MON_MSGLN("Station mode failed, will setup Acces Point Mode, IP 192.168.4.1");
      WiFi.mode(WIFI_AP);     // Only Access point
      WiFi.softAP(eePromData.ssid, eePromData.password);  // access point
    } else {
      MON_MSG("Connected with IP ");
      MON_MSGLN(WiFi.localIP());
    }
  }

  //Start UDP
  udp.begin(udpport);
  MON_MSG("UDP started, local port: ");
  MON_MSGLN(udp.localPort());

  //initWebserver();

  MON_MSGLN("HTTP started");

  for (i = 0; i < c_clients_max; i++) {
    ClientIPs[i] = c_UnsetIP;
    ClientIPs_timer[i] = eePromData.udp_timeout;
  }
}

void TouchOSC::check_for_new_client() {
  // setzt ClientIPs_idx neu, wenn neuer Client gefunden
  int i, param, val_i, udp_buf_len;
  int found = 0;
  boolean send_info = false;
  String sendstr;
  IPAddress udp_client_ip = udp.remoteIP(); // IP der aktuellen Verbindung

  // War IP schon einmal verbunden?
  for (i = 0; i < c_clients_max; i++)
    if (ClientIPs[i] == udp_client_ip) {
      found = 1;
      if (ClientIPs_timer[i] == 0) {
        MON_MSG("WIF: Known client reconnect,  IP: ");
        MON_MSG(udp_client_ip);
        MON_MSG(", index ");
        MON_MSGLN(i);
        send_info = true;
      }
      ClientIPs_timer[i] = eePromData.udp_timeout;
      break;
    }
  // IP noch nicht bekannt? Dann bis zu 4 Clients neu eintragen
  if (!found) {
    ClientIPs[ClientIPs_idx] = udp_client_ip;
    ClientIPs_timer[ClientIPs_idx] = eePromData.udp_timeout;
    MON_MSG("WIF: New client, IP: ");
    MON_MSG(udp_client_ip);
    MON_MSG(", index ");
    MON_MSGLN(ClientIPs_idx);
  }
}

#define C_1S 1000;
#define C_10MS 10;
#define C_100MS 100;
void TouchOSC::chores_and_timeouts() {
  // Timeouts in 10ms und 1000ms
  int udp_buf_len, adc_val;
  int i;
  int current_millis = millis();
  String sendstr;

  if (current_millis >= last_millis_1s) {  // Timeout-Zähler für Clients etc. + Uptime
    last_millis_1s = current_millis + C_1S;

    for (i = 0; i < c_clients_max; i++) {
      if (ClientIPs_timer[i] > 0) {
        ClientIPs_timer[i]--;
      }
      if (ClientIPs_timer[i] == 1) {
        MON_MSG("WIF: Client timed out: #");
        MON_MSGLN(i);
      }
    }
  }
}

void TouchOSC::write_eeprom() {
  EEPROM.begin(sizeof(EEPromData));
  EEPROM.put(0, eePromData);
  EEPROM.commit();    // Only needed for ESP8266 to get data written
  EEPROM.end();
  delay(200);
}
