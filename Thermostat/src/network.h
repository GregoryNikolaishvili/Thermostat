#ifndef _NETWORK_H
#define _NETWORK_H

#include "main.h"

#ifndef SIMULATION_MODE

#include <Ethernet.h>

#else

#include <ESP8266WiFi.h>

// Wi-Fi and MQTT credentials
#define WIFI_SSID "GIO_IoT"
#define WIFI_PASSWORD "batobato"
#endif

#include <ArduinoHA.h>

#define MQTT_BROKER "192.168.68.25"
#define MQTT_USERNAME "cha"
#define MQTT_PASSWORD "BatoBato02@"

#ifndef SIMULATION_MODE
void initNetwork(HADevice &device, EthernetClient &Client);
#else
void initNetwork(HADevice &device);
bool reconnectWifi();
#endif

#endif