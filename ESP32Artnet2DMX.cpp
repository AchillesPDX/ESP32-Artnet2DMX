#include "Print.h"
#include "ESP32Artnet2DMX.h"

ESP32Artnet2DMX::ESP32Artnet2DMX() {
  memset( m_dmx_buffer, 0, sizeof( m_dmx_buffer ) );

  m_artnet_source_ipaddress_any.fromString( "255.255.255.255" );

  m_is_started = false;
}

ESP32Artnet2DMX::~ESP32Artnet2DMX() {
  this->Stop();
}

void ESP32Artnet2DMX::Init( WebServer* ptr_WebServer ) {

  m_ConfigServer.Init();

  m_ConfigServer.ConnectToWiFi();

  m_ConfigServer.StartWebServer( ptr_WebServer );

  m_is_started = false;
}

bool ESP32Artnet2DMX::Start() {

  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_personality_t personalities[] = {};
  int personality_count = 0;

  dmx_driver_install( DMX_NUM_1, &config, personalities, personality_count );

  dmx_set_pin( DMX_NUM_1, m_ConfigServer.m_gpio_transmit, m_ConfigServer.m_gpio_receive, m_ConfigServer.m_gpio_enable );

  if( !m_WiFiUDP.begin( ARTNET_UDP_PORT ) ) {
    Serial.print("Failed to create Art-Net network socket on UDP port 6464\n");
    return false;
  }

  m_artnet_source_ipaddress.fromString( m_ConfigServer.m_artnet_source_ip );

  m_dmx_update_time_next_ms = millis();

  if( m_ConfigServer.m_artnet_timeout_ms == 0 ) {
    m_artnet_timeout_next_ms = 0;
  } else {
    m_artnet_timeout_next_ms = m_dmx_update_time_next_ms + m_ConfigServer.m_artnet_timeout_ms;
  }

  m_is_started = true;

  return m_is_started;
}

void ESP32Artnet2DMX::Stop() {
  if( dmx_driver_is_installed( DMX_NUM_1 ) ) {
    dmx_driver_delete( DMX_NUM_1 ) ;
  }

  m_WiFiUDP.stop();

  m_is_started = false;
  return;
}

bool ESP32Artnet2DMX::IsStarted() {
  return m_is_started;
}

void ESP32Artnet2DMX::Update() {

  if( m_ConfigServer.Update() ) {
    this->Stop();
    this->Start();
  }

  this->CheckForArtNetData();

  if( millis() >= m_dmx_update_time_next_ms ) {
    this->SendDMX();
  }

  if( ( m_artnet_timeout_next_ms != 0 ) && ( millis() >= m_artnet_timeout_next_ms ) ) {
    m_artnet_timeout_next_ms = 0;
    memset( m_dmx_buffer, 0, sizeof( m_dmx_buffer ) );
    this->SendDMX();
  }
}

void ESP32Artnet2DMX::HandleWebServerData() {
  m_ConfigServer.HandleWebServerData();
}

void ESP32Artnet2DMX::CheckForArtNetData() {
  int packet_size_in_bytes = m_WiFiUDP.parsePacket();

  if( packet_size_in_bytes == 0 ) {
    return;
  }

  m_WiFiUDP.read( m_data_buffer, ARTNET_PACKET_MAXSIZE );

  if( packet_size_in_bytes < ARTNET_PACKET_MINSIZE_HEADER ) {
    if( packet_size_in_bytes != 0 ) {
      Serial.printf( "Packet ignored with data length = %i\n", packet_size_in_bytes );
    }
    return;
  }

  if( m_artnet_source_ipaddress != m_artnet_source_ipaddress_any ) {
    if( m_artnet_source_ipaddress != m_WiFiUDP.remoteIP() ) {
      Serial.printf( "Packet ignored from unexpected source IP.\n" );
      return;
    }
  }

  ArtNetPacketHeader* ptr_header = (ArtNetPacketHeader*)&m_data_buffer[ 0 ];

  String art_net = String( (char*)ptr_header->m_ID );
  if( !art_net.equals( ARTNET_HEADER_ID ) ) {
    Serial.printf( "Header ID failed = %i\n", packet_size_in_bytes );
    return;
  }

  switch( ptr_header->m_OpCode ) {
    case ARTNET_OPCODE_DMX: {
      this->HandleArtNetDMX( (ArtNetPacketDMX*)&m_data_buffer[ ARTNET_PACKET_PAYLOAD_START ] );
      break;
    }
    case ARTNET_OPCODE_POLL: {
      break;
    }
    case ARTNET_OPCODE_POLLREPLY: {
      break;
    }
    default: {
      Serial.printf( "Unhandled OpCode %i\n", ptr_header->m_OpCode );
      break;
    }
  }
}

void ESP32Artnet2DMX::HandleArtNetDMX(ArtNetPacketDMX* ptr_packetdmx) {
  uint16_t protocol = ptr_packetdmx->m_ProtocolLo | ptr_packetdmx->m_ProtocolHi << 8;
  uint16_t universe_in = ptr_packetdmx->m_SubUni | ptr_packetdmx->m_Net << 8;
  uint16_t number_of_channels = ptr_packetdmx->m_Length | ptr_packetdmx->m_LengthHi << 8;

  if (m_ConfigServer.m_artnet_timeout_ms != 0) {
    m_artnet_timeout_next_ms = millis() + m_ConfigServer.m_artnet_timeout_ms;
  }

  if (universe_in != m_ConfigServer.m_artnet_universe) {
    return;
  }

  // Copy incoming Art-Net data to the corresponding DMX channels
  for (int i = 0; i < number_of_channels; i++) {
    m_dmx_buffer[i + 1] = ptr_packetdmx->m_Data[i];
  }

  // Apply routing configurations
  for (const DMXRoutingConfig& config : m_ConfigServer.m_dmx_routing_configs) {
    if (config.input_channel <= number_of_channels) {
      uint8_t value = ptr_packetdmx->m_Data[config.input_channel - 1];
      for (uint8_t output_channel : config.output_channels) {
        if (output_channel > 0 && output_channel < 513) {
          // Merge new value with existing value
          if (m_dmx_buffer[output_channel] == 0) {
            m_dmx_buffer[output_channel] = value;
          } else {
            m_dmx_buffer[output_channel] = std::max(m_dmx_buffer[output_channel], value);
          }
        }
      }
    }
  }
}


void ESP32Artnet2DMX::SendDMX()
{
  dmx_write( DMX_NUM_1, m_dmx_buffer, DMX_PACKET_SIZE );
  dmx_send_num( DMX_NUM_1, DMX_PACKET_SIZE );
  dmx_wait_sent( DMX_NUM_1, DMX_TIMEOUT_TICK );
  m_dmx_update_time_next_ms += m_ConfigServer.m_dmx_update_interval_ms;
}
