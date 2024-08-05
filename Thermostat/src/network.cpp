#include "network.h"

#ifndef SIMULATION_MODE
IPAddress ip(192, 168, 68, 8);
#else
IPAddress ip(192, 168, 68, 18);
#endif
IPAddress gateway(192, 168, 68, 1);
IPAddress subnet(255, 255, 252, 0);

#ifndef SIMULATION_MODE

byte mac[] = {0x54, 0x34, 0x41, 0x30, 0x30, 0x08};

#include "utility/w5100.h"

void initNetwork(HADevice &device, EthernetClient &client)
{
	Serial.println(F("Starting ethernet.."));

	Ethernet.begin(mac, ip, gateway, gateway, subnet);
	client.setConnectionTimeout(2000);

	Ethernet.setRetransmissionTimeout(250);
	Ethernet.setRetransmissionCount(4);

	Serial.print(F("IP Address: "));
	Serial.println(Ethernet.localIP());

	device.setUniqueId(mac, sizeof(mac));
}

#else

byte mac[] = {0x56, 0x34, 0x41, 0x30, 0x30, 0x07};

bool printWifiInfo = true;
bool doLog = true;

void initNetwork(HADevice &device)
{
	Serial.println(F("Starting wifi.."));
	WiFi.mode(WIFI_STA);

	// Configures static IP address
	if (!WiFi.config(ip, gateway, subnet))
	{
		Serial.println("STA Failed to configure");
	}
	device.setUniqueId(mac, sizeof(mac));

	reconnectWifi();
}

void outputWifiInfo()
{
	Serial.print(F("IP Address: "));
	Serial.println(WiFi.localIP());

	long rssi = WiFi.RSSI();
	Serial.print("Signal strength (RSSI): ");
	Serial.print(rssi);
	Serial.println(" dBm");
	WiFi.printDiag(Serial);
}

bool reconnectWifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    if (printWifiInfo)
    {
      printWifiInfo = false;
      outputWifiInfo();
    }
    return true;
  }

  WiFi.disconnect();
  Serial.print(F("Connecting to: "));
  Serial.print(WIFI_SSID);
  Serial.print(F("... "));

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  for (int i = 0; i < 12; i++)
  {
    delay(5000);
    if (WiFi.status() == WL_CONNECTED)
      break;

    Serial.print('.');
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(F("connected"));
    printWifiInfo = false;
    outputWifiInfo();
    return true;
  }

  Serial.print(F("failed, rc="));
  Serial.println(WiFi.status());
  return false;
}

#endif