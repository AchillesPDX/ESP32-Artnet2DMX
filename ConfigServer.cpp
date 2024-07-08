#include "ConfigServer.h"

ConfigServer::ConfigServer() {
  m_settings_changed = false;
}

ConfigServer::~ConfigServer() {
}

void ConfigServer::Init() {
  if( !this->SettingsLoad() ) {
    Serial.println( "Settings failed to load - Resetting to default." );
    this->ResetConfigToDefault();
  }

  m_settings_changed = true;
}

void ConfigServer::ResetConfigToDefault() {
  // Remove existing settings file.
  if( LittleFS.begin( false ) ) {
    LittleFS.remove( CONFIG_FILENAME );
  }

  this->ResetWiFiToDefault();
  this->ResetESP32PinsToDefault();
  this->ResetArtnet2DMXToDefault();
}

void ConfigServer::ResetWiFiToDefault() {
  m_is_connected_to_wifi = false;

  m_wifi_ssid   = "";
  m_wifi_pass   = "";
  m_wifi_ip     = "";
  m_wifi_subnet = "";
}

void ConfigServer::ResetESP32PinsToDefault() {
  m_gpio_enable      = 21;
  m_gpio_transmit    = 33;
  m_gpio_receive     = 38;
}

void ConfigServer::ResetArtnet2DMXToDefault() {
  m_artnet_source_ip       = "255.255.255.255";
  m_artnet_universe        = 1;
  m_artnet_timeout_ms      = 3000;
  m_dmx_update_interval_ms = 23;
}

void ConfigServer::SettingsSave() {
  DynamicJsonDocument doc( 32768 );
  doc[ "wifi_ssid" ]              = m_wifi_ssid;
  doc[ "wifi_pass" ]              = m_wifi_pass;
  doc[ "wifi_ip" ]                = m_wifi_ip;
  doc[ "wifi_subnet" ]            = m_wifi_subnet;
  doc[ "gpio_enable" ]            = m_gpio_enable;
  doc[ "gpio_transmit" ]          = m_gpio_transmit;
  doc[ "gpio_receive" ]           = m_gpio_receive;
  doc[ "artnet_source_ip" ]       = m_artnet_source_ip;
  doc[ "artnet_universe" ]        = m_artnet_universe;
  doc[ "artnet_timeout_ms" ]      = m_artnet_timeout_ms;
  doc[ "dmx_update_interval_ms" ] = m_dmx_update_interval_ms;

  if( !LittleFS.begin( false ) ) {
    Serial.println( "LittleFS failed.  Attempting format." );
    if( !LittleFS.begin( true ) ) {
      Serial.println( "LittleFS failed format. Config saving aborted." );
      return;     
    } else {
      Serial.println( "LittleFS: Formatted" );
    }
  }

  File config_file = LittleFS.open( CONFIG_FILENAME, "w" );
  serializeJson( doc, config_file );
  config_file.close();

  m_settings_changed = true;
}

bool ConfigServer::SettingsLoad() {
  if( !LittleFS.begin( false ) ) {
    Serial.println( "Failed to start LittleFS" );
    return false;
  }

  DynamicJsonDocument doc( 32768 );
  File config_file = LittleFS.open( CONFIG_FILENAME, "r" );

  if( !config_file ) {
    Serial.println( "Failed to load file" );
    return false;
  }

  deserializeJson( doc, config_file );
  config_file.close();

  m_wifi_ssid              = doc[ "wifi_ssid" ].as<String>();
  m_wifi_pass              = doc[ "wifi_pass" ].as<String>();
  m_wifi_ip                = doc[ "wifi_ip" ].as<String>();
  m_wifi_subnet            = doc[ "wifi_subnet" ].as<String>();
  m_gpio_enable            = doc[ "gpio_enable" ];
  m_gpio_transmit          = doc[ "gpio_transmit" ];
  m_gpio_receive           = doc[ "gpio_receive" ];
  m_artnet_source_ip       = doc[ "artnet_source_ip" ].as<String>();
  m_artnet_universe        = doc[ "artnet_universe" ];
  m_artnet_timeout_ms      = doc[ "artnet_timeout_ms" ];
  m_dmx_update_interval_ms = doc[ "dmx_update_interval_ms" ];

  LoadDMXRoutingConfigs();

  return true;
}

void ConfigServer::LoadDMXRoutingConfigs() {
  DynamicJsonDocument doc(32768);
  File config_file = LittleFS.open(CONFIG_FILENAME, "r");

  if (config_file) {
    deserializeJson(doc, config_file);
    config_file.close();

    JsonArray routing_configs = doc["dmx_routing_configs"].as<JsonArray>();
    for (JsonObject routing_config : routing_configs) {
      DMXRoutingConfig config;
      config.input_channel = routing_config["input_channel"];
      for (JsonVariant output_channel : routing_config["output_channels"].as<JsonArray>()) {
        config.output_channels.push_back(output_channel.as<uint8_t>());
      }
      m_dmx_routing_configs.push_back(config);
    }
  }
}

void ConfigServer::SaveDMXRoutingConfigs() {
  DynamicJsonDocument doc(32768);
  JsonArray routing_configs = doc.createNestedArray("dmx_routing_configs");

  for (const DMXRoutingConfig& config : m_dmx_routing_configs) {
    JsonObject routing_config = routing_configs.createNestedObject();
    routing_config["input_channel"] = config.input_channel;
    JsonArray output_channels = routing_config.createNestedArray("output_channels");
    for (uint8_t output_channel : config.output_channels) {
      output_channels.add(output_channel);
    }
  }

  File config_file = LittleFS.open(CONFIG_FILENAME, "w");
  serializeJson(doc, config_file);
  config_file.close();
}

void ConfigServer::AddDMXRoutingConfig(uint8_t input_channel, const std::vector<uint8_t>& output_channels) {
  DMXRoutingConfig config;
  config.input_channel = input_channel;
  config.output_channels = output_channels;
  m_dmx_routing_configs.push_back(config);
  SaveDMXRoutingConfigs();
}

void ConfigServer::ClearDMXRoutingConfigs() {
  m_dmx_routing_configs.clear();
  SaveDMXRoutingConfigs();
}

void ConfigServer::StartWebServer( WebServer* ptr_WebServer ) {
  m_ptr_WebServer = ptr_WebServer;
  m_ptr_WebServer->begin();
}

bool ConfigServer::ConnectToWiFi() {
  if( m_is_connected_to_wifi ) {
    WiFi.disconnect();
    m_is_connected_to_wifi = false;
  }

  IPAddress ip( 192, 168, 1, 1 );
  IPAddress subnet( 255, 255, 255, 0 );

  if( m_wifi_ssid.length() != 0 ) {
    WiFi.mode( WIFI_STA );
    WiFi.begin( m_wifi_ssid, m_wifi_pass );
  
    int timeout = 20;
    Serial.print( "Connecting to WiFi" );
    while( WiFi.status() != WL_CONNECTED && timeout-- > 0 ) {
      delay( 1000 );
      Serial.print( "." );
    }

    if( timeout > 0 ) {
      if( m_wifi_ip.length() > 0 ) {
        ip.fromString( m_wifi_ip );
        subnet.fromString( m_wifi_subnet );

        WiFi.config( ip, ip, subnet );
      }
      Serial.printf( "\\nConnected to WiFi. IP = " );
      Serial.println( WiFi.localIP() );
      m_is_connected_to_wifi = true;
      return true;
    }
  }

  m_wifi_ssid = "";

  Serial.println( "No config found or WiFi timeout." );
  WiFi.mode( WIFI_AP );
  WiFi.softAP( HOTSPOT_SSID, HOTSPOT_PASS );  
  WiFi.softAPConfig( ip, ip, subnet );
  
  Serial.printf( "WiFi started in AP mode with IP = " );
  Serial.println( ip );
  
  m_is_connected_to_wifi = false;

  return false;
}

bool ConfigServer::IsConnectedToWiFi() {
  return m_is_connected_to_wifi;
}

bool ConfigServer::Update() {

  m_ptr_WebServer->handleClient();

  if( m_settings_changed ) {
    m_settings_changed = false;
    return true;
  }

  return false;
}

void ConfigServer::SendSetupMenuPage() {
  String mac_address = "Device MAC = " + WiFi.macAddress();

  m_WebpageBuilder.StartPage();
  m_WebpageBuilder.AddTitle( "Artnet2DMX Setup Page" );
  m_WebpageBuilder.StartBody();
  m_WebpageBuilder.StartCenter();
  m_WebpageBuilder.AddHeading( "Artnet2DMX Setup Page" );

  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddButtonActionForm( "settings_wifi", "WiFi" );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddButtonActionForm( "settings_esp32pins", "ESP32 Pins" );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddButtonActionForm( "settings_artnet2dmx", "Art-Net 2 DMX" );

  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddButtonActionForm( "settings_dmx_routing", "DMX Routing" );

  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddButtonActionForm( "reset_all", "RESET ALL SETTINGS TO DEFAULT" );

  m_WebpageBuilder.EndCenter();
  m_WebpageBuilder.EndBody();
  m_WebpageBuilder.EndPage();

  m_ptr_WebServer->send( 200, "text/html", m_WebpageBuilder.m_html );
}

void ConfigServer::SendWiFiSetupPage() {
  String mac_address = "Device MAC = " + WiFi.macAddress();

  m_WebpageBuilder.StartPage();
  m_WebpageBuilder.AddTitle( "Artnet2DMX Setup Page" );
  m_WebpageBuilder.StartBody();
  m_WebpageBuilder.StartCenter();
  m_WebpageBuilder.AddHeading( "WiFi Setup" );

  m_WebpageBuilder.AddText( mac_address );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddFormAction( "/setup_wifi", "POST" );
  m_WebpageBuilder.AddLabel( "wifi_ssid", "WiFi ssid : " );
  m_WebpageBuilder.AddInputType( "text", "wifi_ssid", "wifi_ssid", "", "", true );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddLabel( "wifi_pass", "Password : " );
  m_WebpageBuilder.AddInputType( "password", "wifi_pass", "wifi_pass", "", "", true );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddLabel( "ip", "IP (Leave blank if DHCP assigned) : " );
  m_WebpageBuilder.AddInputType( "text", "ip", "ip", "", String( "xxx.xxx.xxx.xxx" ), false );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddLabel( "subnet", "Subnet (Leave blank if DHCP assigned) : " );
  m_WebpageBuilder.AddInputType( "text", "subnet", "subnet", "", String( "xxx.xxx.xxx.xxx" ), false );
  m_WebpageBuilder.AddBreak( 3 );

  m_WebpageBuilder.AddBreak( 3 );
  m_WebpageBuilder.AddButton( "submit", "SUBMIT" );
  m_WebpageBuilder.EndFormAction();

  m_WebpageBuilder.AddBreak( 3 );
  m_WebpageBuilder.AddButtonActionForm( "/", "CANCEL" );

  m_WebpageBuilder.AddBreak( 3 );
  m_WebpageBuilder.AddButtonActionForm( "reset_wifi", "RESET WiFi BACK TO HOTSPOT" );

  m_WebpageBuilder.EndCenter();
  m_WebpageBuilder.EndBody();
  m_WebpageBuilder.EndPage();

  m_ptr_WebServer->send( 200, "text/html", m_WebpageBuilder.m_html );
}

void ConfigServer::SendESP32PinsSetupPage() {
  String mac_address = "Device MAC = " + WiFi.macAddress();

  m_WebpageBuilder.StartPage();
  m_WebpageBuilder.AddTitle( "Artnet2DMX Setup Page" );
  m_WebpageBuilder.StartBody();
  m_WebpageBuilder.StartCenter();
  m_WebpageBuilder.AddHeading( "ESP32 Pin Setup" );

  m_WebpageBuilder.AddFormAction( "/setup_esp32pins", "POST" );
  m_WebpageBuilder.AddLabel( "gpio_enable", "GPIO - Enable : Connnect to DE & RE on MAX485." );
  m_WebpageBuilder.AddBreak( 1 );
  m_WebpageBuilder.AddInputType( "number", "GPIO Enable", "gpio_enable", String( m_gpio_enable ), "", true );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddLabel( "gpio_transmit", "GPIO - Transmit : Connnect to DI on MAX485." );
  m_WebpageBuilder.AddBreak( 1 );
  m_WebpageBuilder.AddInputType( "number", "GPIO Transmit", "gpio_transmit", String( m_gpio_transmit ), "", true );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddLabel( "gpio_receive", "GPIO - Receive : Ensure GPIO is not connected." );
  m_WebpageBuilder.AddBreak( 1 );
  m_WebpageBuilder.AddInputType( "number", "GPIO Receive", "gpio_receive", String( m_gpio_receive ), "", true );

  m_WebpageBuilder.AddBreak( 3 );
  m_WebpageBuilder.AddButton( "submit", "SUBMIT" );
  m_WebpageBuilder.EndFormAction();

  m_WebpageBuilder.AddBreak( 3 );
  m_WebpageBuilder.AddButtonActionForm( "/", "CANCEL" );

  m_WebpageBuilder.AddBreak( 3 );
  m_WebpageBuilder.AddButtonActionForm( "reset_esp32pins", "RESET ALL ESP32 PINS TO DEFAULT" );

  m_WebpageBuilder.EndCenter();
  m_WebpageBuilder.EndBody();
  m_WebpageBuilder.EndPage();

  m_ptr_WebServer->send( 200, "text/html", m_WebpageBuilder.m_html );
}

void ConfigServer::SendArtnet2DMXSetupPage() {
  m_WebpageBuilder.StartPage();
  m_WebpageBuilder.AddTitle( "Artnet2DMX Setup Page" );
  m_WebpageBuilder.StartBody();
  m_WebpageBuilder.StartCenter();
  m_WebpageBuilder.AddHeading( "Art-Net to DMX Setup" );

  m_WebpageBuilder.AddFormAction( "/setup_artnet2dmx", "POST" );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddLabel( "source ip", "Source IP that will send Art-Net packets. Use 255.255.255.255 if any." );
  m_WebpageBuilder.AddBreak( 1 );
  m_WebpageBuilder.AddInputType( "text", "source ip", "artnet_source_ip", String( m_artnet_source_ip ), "xxx.xxx.xxx.xxx", true );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddLabel( "Art-Net Universe", "Art-Net Universe : The Art-Net universe to translate into DMX. All other universes are ignored." );
  m_WebpageBuilder.AddBreak( 1 );
  m_WebpageBuilder.AddInputType( "number", "Art-Net universe", "artnet_universe", String( m_artnet_universe ), "", true );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddLabel( "Art-Net timeout in ms", "Art-Net timeout in ms.  If no data received after this time then everything is turned off.  Use 0 to disable." );
  m_WebpageBuilder.AddBreak( 1 );
  m_WebpageBuilder.AddInputType( "number", "Art-Net timeout in ms", "artnet_timeout_ms", String( m_artnet_timeout_ms ), "", true );
  m_WebpageBuilder.AddBreak( 2 );
  m_WebpageBuilder.AddLabel( "DMX update interval in ms", "DMX interval update in milliseconds.  Only change this if you know what you're doing." );
  m_WebpageBuilder.AddBreak( 1 );
  m_WebpageBuilder.AddInputType( "number", "DMX update interval in ms", "dmx_update_ms", String( m_dmx_update_interval_ms ), "", true );

  m_WebpageBuilder.AddBreak( 3 );
  m_WebpageBuilder.AddButton( "submit", "SUBMIT" );
  m_WebpageBuilder.EndFormAction();

  m_WebpageBuilder.AddBreak( 3 );
  m_WebpageBuilder.AddButtonActionForm( "/", "CANCEL" );

  m_WebpageBuilder.AddBreak( 3 );
  m_WebpageBuilder.AddButtonActionForm( "reset_artnew2dmx", "RESET ALL ART-NET TO DMX SETTINGS TO DEFAULT" );

  m_WebpageBuilder.EndCenter();
  m_WebpageBuilder.EndBody();
  m_WebpageBuilder.EndPage();

  m_ptr_WebServer->send( 200, "text/html", m_WebpageBuilder.m_html );
}

void ConfigServer::SendDMXRoutingSetupPage() {
  m_WebpageBuilder.AddDMXRoutingConfigTable(m_dmx_routing_configs);
  m_ptr_WebServer->send(200, "text/html", m_WebpageBuilder.m_html);
}

bool ConfigServer::HandleSetupDMXRouting() {
  uint8_t input_channel = 0;
  std::vector<uint8_t> output_channels;

  for (int i = 0; i < m_ptr_WebServer->args(); i++) {
    if (m_ptr_WebServer->argName(i) == "input_channel") {
      input_channel = m_ptr_WebServer->arg(i).toInt();
    } else if (m_ptr_WebServer->argName(i) == "output_channels") {
      String output_channels_str = m_ptr_WebServer->arg(i);
      int start = 0;
      int end = output_channels_str.indexOf(',');
      while (end != -1) {
        output_channels.push_back(output_channels_str.substring(start, end).toInt());
        start = end + 1;
        end = output_channels_str.indexOf(',', start);
      }
      output_channels.push_back(output_channels_str.substring(start).toInt());
    }
  }

  AddDMXRoutingConfig(input_channel, output_channels);
  SendDMXRoutingSetupPage();
  return true;
}

bool ConfigServer::HandleEditDMXRouting() {
  int index = -1;

  for (int i = 0; i < m_ptr_WebServer->args(); i++) {
    if (m_ptr_WebServer->argName(i) == "index") {
      index = m_ptr_WebServer->arg(i).toInt();
    }
  }

  if (index >= 0 && index < m_dmx_routing_configs.size()) {
    const auto& config = m_dmx_routing_configs[index];

    m_WebpageBuilder.StartPage();
    m_WebpageBuilder.AddTitle("Edit DMX Routing Configuration");
    m_WebpageBuilder.StartBody();
    m_WebpageBuilder.StartCenter();
    m_WebpageBuilder.AddHeading("Edit DMX Routing Configuration");

    m_WebpageBuilder.AddFormAction("/update_dmx_routing", "POST");
    m_WebpageBuilder.AddLabel("input_channel", "Input DMX Channel:");
    m_WebpageBuilder.AddInputType("number", "input_channel", "input_channel", String(config.input_channel), "", true);
    m_WebpageBuilder.AddBreak(2);
    m_WebpageBuilder.AddLabel("output_channels", "Output DMX Channels (comma-separated):");
    String output_channels_str;
    for (size_t j = 0; j < config.output_channels.size(); ++j) {
      output_channels_str += String(config.output_channels[j]);
      if (j < config.output_channels.size() - 1) {
        output_channels_str += ",";
      }
    }
    m_WebpageBuilder.AddInputType("text", "output_channels", "output_channels", output_channels_str, "", true);
    m_WebpageBuilder.AddBreak(2);
    m_WebpageBuilder.AddInputType("hidden", "index", "index", String(index), "", false);
    m_WebpageBuilder.AddBreak(3);

    m_WebpageBuilder.AddButton("submit", "UPDATE");
    m_WebpageBuilder.EndFormAction();
    m_WebpageBuilder.AddBreak(3);
    m_WebpageBuilder.AddButtonActionForm("/", "CANCEL");

    m_WebpageBuilder.EndCenter();
    m_WebpageBuilder.EndBody();
    m_WebpageBuilder.EndPage();

    m_ptr_WebServer->send(200, "text/html", m_WebpageBuilder.m_html);
    return true;
  }

  return false;
}

bool ConfigServer::HandleDeleteDMXRouting() {
  int index = -1;

  for (int i = 0; i < m_ptr_WebServer->args(); i++) {
    if (m_ptr_WebServer->argName(i) == "index") {
      index = m_ptr_WebServer->arg(i).toInt();
    }
  }

  if (index >= 0 && index < m_dmx_routing_configs.size()) {
    m_dmx_routing_configs.erase(m_dmx_routing_configs.begin() + index);
    SaveDMXRoutingConfigs();
    SendDMXRoutingSetupPage();
    return true;
  }

  return false;
}

bool ConfigServer::HandleUpdateDMXRouting() {
  int index = -1;
  uint8_t input_channel = 0;
  std::vector<uint8_t> output_channels;

  for (int i = 0; i < m_ptr_WebServer->args(); i++) {
    if (m_ptr_WebServer->argName(i) == "index") {
      index = m_ptr_WebServer->arg(i).toInt();
    } else if (m_ptr_WebServer->argName(i) == "input_channel") {
      input_channel = m_ptr_WebServer->arg(i).toInt();
    } else if (m_ptr_WebServer->argName(i) == "output_channels") {
      String output_channels_str = m_ptr_WebServer->arg(i);
      int start = 0;
      int end = output_channels_str.indexOf(',');
      while (end != -1) {
        output_channels.push_back(output_channels_str.substring(start, end).toInt());
        start = end + 1;
        end = output_channels_str.indexOf(',', start);
      }
      output_channels.push_back(output_channels_str.substring(start).toInt());
    }
  }

  if (index >= 0 && index < m_dmx_routing_configs.size()) {
    m_dmx_routing_configs[index].input_channel = input_channel;
    m_dmx_routing_configs[index].output_channels = output_channels;
    SaveDMXRoutingConfigs();
    SendDMXRoutingSetupPage();
    return true;
  }

  return false;
}


bool ConfigServer::HandleWebGet() {
  if (m_ptr_WebServer->uri() == "/settings_wifi") {
    this->SendWiFiSetupPage();
  } else if (m_ptr_WebServer->uri() == "/settings_esp32pins") {
    this->SendESP32PinsSetupPage();
  } else if (m_ptr_WebServer->uri() == "/settings_artnet2dmx") {
    this->SendArtnet2DMXSetupPage();
  } else if (m_ptr_WebServer->uri() == "/settings_dmx_routing") {
    this->SendDMXRoutingSetupPage();
  } else {
    this->SendSetupMenuPage();
  }

  return true;
}

bool ConfigServer::HandleWebPost() {
  if (m_ptr_WebServer->uri() == "/setup_wifi") {
    if (this->HandleSetupWiFi()) {
      Serial.printf("Restarting WiFi\n");
      this->ConnectToWiFi();
      return true;
    }
  } else if (m_ptr_WebServer->uri() == "/setup_esp32pins") {
    this->HandleSetupESP32Pins();
    this->SendSetupMenuPage();
    return true;
  } else if (m_ptr_WebServer->uri() == "/setup_artnet2dmx") {
    this->HandleSetupArtnet2DMX();
    this->SendSetupMenuPage();
    return true;
  } else if (m_ptr_WebServer->uri() == "/setup_dmx_routing") {
    this->HandleSetupDMXRouting();
    return true;
  } else if (m_ptr_WebServer->uri() == "/edit_dmx_routing") {
    this->HandleEditDMXRouting();
    return true;
  } else if (m_ptr_WebServer->uri() == "/delete_dmx_routing") {
    this->HandleDeleteDMXRouting();
    return true;
  } else if (m_ptr_WebServer->uri() == "/update_dmx_routing") {
    this->HandleUpdateDMXRouting();
    return true;
  }

  return false;
}


bool ConfigServer::HandleSetupWiFi() {  
  String wifi_ssid_new   = "";
  String wifi_pass_new   = "";
  String wifi_ip_new     = "";
  String wifi_subnet_new = "";

  for( int i = 0; i < m_ptr_WebServer->args(); i++ ) {
    if( m_ptr_WebServer->argName( i ) == "wifi_ssid" ) {
      wifi_ssid_new = m_ptr_WebServer->arg( i );
    } else if( m_ptr_WebServer->argName( i ) == "wifi_pass" ) {
      wifi_pass_new = m_ptr_WebServer->arg( i );
    } else if( m_ptr_WebServer->argName( i ) == "ip" ) {
      wifi_ip_new = m_ptr_WebServer->arg( i );
    } else if( m_ptr_WebServer->argName( i ) == "subnet" ) {
      wifi_subnet_new = m_ptr_WebServer->arg( i );
    }
  }

  if( wifi_ssid_new.length() > 0 && wifi_pass_new.length() > 0 ) {
    m_wifi_ssid = wifi_ssid_new;
    m_wifi_pass = wifi_pass_new;
    m_wifi_ip   = wifi_ip_new;
    if( m_wifi_ip.length() > 0 && wifi_subnet_new.length() == 0 ) {
      m_wifi_subnet = "255.255.255.0";
    } else {
      m_wifi_subnet = wifi_subnet_new;
    }

    m_ptr_WebServer->send( 200, "text/plain", "Attempting to connect to WiFi. On failure hotspot will re-appear." );
     
    if( this->ConnectToWiFi() ) {
      this->SettingsSave();
    }
    return true;
  }
  return false;
}

bool ConfigServer::HandleSetupESP32Pins() {  

  for( int i = 0; i < m_ptr_WebServer->args(); i++ ) {
    if( m_ptr_WebServer->argName( i ) == "gpio_enable" ) {
      m_gpio_enable = m_ptr_WebServer->arg( i ).toInt();
    } else if( m_ptr_WebServer->argName( i ) == "gpio_transmit" ) {
      m_gpio_transmit = m_ptr_WebServer->arg( i ).toInt();
    } else if( m_ptr_WebServer->argName( i ) == "gpio_receive" ) {
      m_gpio_receive = m_ptr_WebServer->arg( i ).toInt();
    }
  }

  this->SettingsSave();

  return true;
}

bool ConfigServer::HandleSetupArtnet2DMX() {  
  for( int i = 0; i < m_ptr_WebServer->args(); i++ ) {
    if( m_ptr_WebServer->argName( i ) == "artnet_source_ip" ) {
      m_artnet_source_ip = m_ptr_WebServer->arg( i );
    } else if( m_ptr_WebServer->argName( i ) == "artnet_universe" ) {
      m_artnet_universe = m_ptr_WebServer->arg( i ).toInt();
    } else if( m_ptr_WebServer->argName( i ) == "dmx_update_ms" ) {
      m_dmx_update_interval_ms = m_ptr_WebServer->arg( i ).toInt();
    } else if( m_ptr_WebServer->argName( i ) == "artnet_timeout_ms" ) {
      m_artnet_timeout_ms = m_ptr_WebServer->arg( i ).toInt();
    }
  }

  this->SettingsSave();

  return true;
}

void ConfigServer::HandleWebServerData() {
  bool handled = false;
  
  if( m_ptr_WebServer->uri() == String( "/reset_all" ) ) {
    m_ptr_WebServer->send( 200, "text/plain", "Resetting everything to defaults - Reconnect to hotspot to setup WiFi." );
    delay( 2000 );
    this->ResetConfigToDefault();
    this->SettingsSave();
    this->ConnectToWiFi();
    return;
  } else if( m_ptr_WebServer->uri() == String( "/reset_wifi" ) ) {
    m_ptr_WebServer->send( 200, "text/plain", "Resetting to WiFi defaults - Reconnect to hotspot to setup WiFi." );
    delay( 2000 );
    this->ResetWiFiToDefault();
    this->SettingsSave();
    this->ConnectToWiFi();
    return;
  } else if( m_ptr_WebServer->uri() == String( "/reset_esp32pins" ) ) {
    this->ResetESP32PinsToDefault();
    this->SettingsSave();
    this->SendESP32PinsSetupPage();
    return;
  } else if( m_ptr_WebServer->uri() == String( "/reset_artnew2dmx" ) ) {
    this->ResetArtnet2DMXToDefault();
    this->SettingsSave();
    this->SendArtnet2DMXSetupPage();
    return;
  }

  if( m_ptr_WebServer->method() == HTTP_GET ) {
    handled = this->HandleWebGet();
  } else {
    handled = this->HandleWebPost();
  }

  if( !handled ) {
    m_ptr_WebServer->send( 200, "text/plain", "Not found!" );
  }
}
