#include <SPI.h>
#include <UIPEthernet.h>

#define ETHERNET_CS_PIN 10

byte mac_address[] = { 0x02, 0x52, 0x4C, 0x59, 0x01, 0x02 };
EthernetServer server(80);

void print_ip(const __FlashStringHelper* label, IPAddress address) {
  Serial.print(label);
  Serial.print(address[0]);
  Serial.print('.');
  Serial.print(address[1]);
  Serial.print('.');
  Serial.print(address[2]);
  Serial.print('.');
  Serial.println(address[3]);
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("ENC28J60 minimal Ethernet test"));
  Serial.println(F("Initializing ENC28J60 on CS pin 10..."));

  Ethernet.init(ETHERNET_CS_PIN);
  int dhcp_result = Ethernet.begin(mac_address);

  Serial.print(F("DHCP result: "));
  Serial.println(dhcp_result);
  Serial.print(F("Hardware status: "));
  Serial.println((int)Ethernet.hardwareStatus());
  Serial.print(F("Link status: "));
  Serial.println((int)Ethernet.linkStatus());
  print_ip(F("IP address: "), Ethernet.localIP());
  print_ip(F("Gateway: "), Ethernet.gatewayIP());
  print_ip(F("Subnet: "), Ethernet.subnetMask());
  print_ip(F("DNS: "), Ethernet.dnsServerIP());

  server.begin();
  Serial.println(F("HTTP test server started on port 80"));
}

void loop() {
  EthernetClient client = server.available();
  if (!client) {
    return;
  }

  bool current_line_is_empty = true;
  while (client.connected()) {
    if (!client.available()) {
      continue;
    }

    char character = client.read();
    if (character == '\n' && current_line_is_empty) {
      client.println(F("HTTP/1.0 200 OK"));
      client.println(F("Content-Type: text/plain"));
      client.println(F("Connection: close"));
      client.println();
      client.println(F("ENC28J60 Ethernet test OK"));
      break;
    }
    if (character == '\n') {
      current_line_is_empty = true;
    } else if (character != '\r') {
      current_line_is_empty = false;
    }
  }
  delay(1);
  client.stop();
}
