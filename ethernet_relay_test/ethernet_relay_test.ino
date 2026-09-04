#include <SPI.h>
#include <UIPEthernet.h>

#define RELAY_COUNT 6
#define ETHERNET_CS_PIN 10
#define SETTLE_TIME_MS 100
#define SAMPLE_COUNT 5
#define SAMPLE_INTERVAL_MS 5

static const byte output_pins[RELAY_COUNT] = { 2, 3, 4, 5, 6, 7 };
static const byte input_pins[RELAY_COUNT] = { 14, 15, 16, 17, A6, A7 };
static byte mac_address[] = { 0x02, 0x52, 0x4C, 0x59, 0x01, 0x03 };
EthernetServer server(80);

unsigned char read_inputs() {
  unsigned char input_word = 0;
  for (byte index = 0; index < RELAY_COUNT; index++) {
    bool input_bit = index < 4 ? digitalRead(input_pins[index]) == LOW : analogRead(input_pins[index]) <= 512;
    input_word |= input_bit << index;
  }
  return input_word;
}

unsigned char stable_input_sample() {
  unsigned int totals[RELAY_COUNT] = { 0, 0, 0, 0, 0, 0 };
  for (byte sample = 0; sample < SAMPLE_COUNT; sample++) {
    unsigned char input_word = read_inputs();
    for (byte index = 0; index < RELAY_COUNT; index++) totals[index] += (input_word >> index) & 1;
    delay(SAMPLE_INTERVAL_MS);
  }
  unsigned char result = 0;
  for (byte index = 0; index < RELAY_COUNT; index++) if (totals[index] >= (SAMPLE_COUNT + 1) / 2) result |= 1 << index;
  return result;
}

void print_input_word(const __FlashStringHelper* label, unsigned char input_word) {
  Serial.print(label);
  for (byte index = 0; index < RELAY_COUNT; index++) {
    Serial.print((input_word >> index) & 1);
    if (index < RELAY_COUNT - 1) Serial.print(',');
  }
  Serial.println();
}

void print_changed_inputs(unsigned char before, unsigned char after) {
  bool changed = false;
  Serial.print(F("Changed inputs: "));
  for (byte index = 0; index < RELAY_COUNT; index++) {
    if (((before >> index) & 1) != ((after >> index) & 1)) {
      if (changed) Serial.print(F(", "));
      Serial.print(input_pins[index]);
      changed = true;
    }
  }
  if (!changed) Serial.print(F("none"));
  Serial.println();
}

void cycle_relays() {
  for (byte relay_index = 0; relay_index < RELAY_COUNT; relay_index++) {
    unsigned char before = stable_input_sample();
    digitalWrite(output_pins[relay_index], HIGH);
    delay(SETTLE_TIME_MS);
    unsigned char after_on = stable_input_sample();
    Serial.print(F("Relay ")); Serial.print(relay_index + 1); Serial.println(F(" ON"));
    print_input_word(F("Inputs: "), after_on);
    print_changed_inputs(before, after_on);
    digitalWrite(output_pins[relay_index], LOW);
    delay(SETTLE_TIME_MS);
    unsigned char after_off = stable_input_sample();
    Serial.print(F("Relay ")); Serial.print(relay_index + 1); Serial.println(F(" OFF"));
    print_input_word(F("Inputs: "), after_off);
    print_changed_inputs(after_on, after_off);
  }
  Serial.println(F("Ethernet cycle complete; outputs left OFF."));
}

void send_page(EthernetClient& client) {
  client.println(F("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n"));
  client.println(F("<!doctype html><html><body><h2>Ethernet relay test</h2><p><a href='/cycle'><button>Cycle relays 1-6</button></a></p><p>Outputs: 2,3,4,5,6,7</p><p>Inputs: 14,15,16,17,A6,A7</p></body></html>"));
}

void setup() {
  Serial.begin(115200);
  for (byte index = 0; index < RELAY_COUNT; index++) {
    pinMode(output_pins[index], OUTPUT);
    if (index < 4) pinMode(input_pins[index], INPUT_PULLUP);
    digitalWrite(output_pins[index], LOW);
  }
  Ethernet.init(ETHERNET_CS_PIN);
  int dhcp_result = Ethernet.begin(mac_address);
  server.begin();
  Serial.print(F("DHCP result: ")); Serial.println(dhcp_result);
  Serial.print(F("Ethernet test IP: ")); Serial.println(Ethernet.localIP());
  Serial.println(F("Open /cycle to run the relay test."));
}

void loop() {
  EthernetClient client = server.available();
  if (!client) return;
  char request[96];
  byte length = 0;
  unsigned long deadline = millis() + 500;
  while (client.connected() && millis() < deadline) {
    if (client.available() == 0) continue;
    char character = client.read();
    if (character == '\n') break;
    if (character != '\r' && length < sizeof(request) - 1) request[length++] = character;
  }
  request[length] = '\0';
  if (strncmp(request, "GET /cycle", 10) == 0) cycle_relays();
  send_page(client);
  client.stop();
}
