#ifndef _CONFIGSERVER_H_
#define _CONFIGSERVER_H_

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "WebpageBuilder.h"

const String HOTSPOT_SSID = "ESP32_ArtNet2DMX";
const String HOTSPOT_PASS = "1234567890";  // Has to be minimum 10 digits?

const String CONFIG_FILENAME = "/config.json";

struct DMXRoutingConfig {
  uint8_t input_channel;
  std::vector<uint8_t> output_channels;
};

class ConfigServer {
public:
  ConfigServer();

  ~ConfigServer();
  
  void Init();

  bool ConnectToWiFi();
  
  bool IsConnectedToWiFi();

  void StartWebServer(WebServer* ptr_WebServer);

  bool Update();
  
  void HandleWebServerData();

  String m_wifi_ssid;
  String m_wifi_pass;
  String m_wifi_ip;
  String m_wifi_subnet;

  int m_gpio_enable;
  int m_gpio_transmit;
  int m_gpio_receive;

  String m_artnet_source_ip;
  int m_artnet_universe;
  unsigned long m_artnet_timeout_ms;
  unsigned long m_dmx_update_interval_ms;

  std::vector<DMXRoutingConfig> m_dmx_routing_configs;

  void LoadDMXRoutingConfigs();
  void SaveDMXRoutingConfigs();
  void AddDMXRoutingConfig(uint8_t input_channel, const std::vector<uint8_t>& output_channels);
  void ClearDMXRoutingConfigs();

  // New functions for edit and delete functionality
  bool HandleEditDMXRouting();
  bool HandleDeleteDMXRouting();
  bool HandleUpdateDMXRouting();

private:
  void ResetConfigToDefault();
  void ResetWiFiToDefault();
  void ResetESP32PinsToDefault();
  void ResetArtnet2DMXToDefault();  

  void SettingsSave();
  bool SettingsLoad();

  void SendSetupMenuPage();
  void SendWiFiSetupPage();
  void SendESP32PinsSetupPage();
  void SendArtnet2DMXSetupPage();
  void SendDMXRoutingSetupPage();

  bool HandleWebGet();
  bool HandleWebPost();

  bool HandleSetupWiFi();
  bool HandleSetupESP32Pins();
  bool HandleSetupArtnet2DMX();
  bool HandleSetupDMXRouting();

  WebServer* m_ptr_WebServer;
  WebpageBuilder m_WebpageBuilder;

  bool m_settings_changed;
  bool m_is_connected_to_wifi;
};

#endif
