// Remote Switch Controller v0.1.0 (base checkpoint: 4f06f7b)
#include <SPI.h>
#include <EEPROM.h>
#include <UIPEthernet.h>

#define RELAY_COUNT 6
#define EEPROM_MAGIC 0x5250
#define ETHERNET_CS_PIN 10
#define COUNT_MASK 0x07
#define DHCP_FLAG 0x08
#define EXCLUSIVE_FLAG 0x10
#define COLUMNS_FLAG 0x20
#define HEADER_SIZE 9
#define LABEL_SIZE 21
static byte mac_address[] = { 0x02, 0x52, 0x4C, 0x59, 0x01, 0x02 };

EthernetServer server(80);
unsigned char relay_status = 0;
unsigned char feedback_status = 0;
bool relay_state_dirty = false;

struct NetworkConfig {
  uint16_t magic;
  byte options;
  byte ip[4];
  byte gateway[4];
  byte dns[4];
  byte subnet[4];
};

#define HEADER_EEPROM_ADDRESS (sizeof(NetworkConfig))
#define LABEL_EEPROM_ADDRESS (HEADER_EEPROM_ADDRESS + RELAY_COUNT * HEADER_SIZE)
#define RELAY_EEPROM_ADDRESS (LABEL_EEPROM_ADDRESS + LABEL_SIZE)

NetworkConfig network_config = {
  EEPROM_MAGIC, DHCP_FLAG | EXCLUSIVE_FLAG | COLUMNS_FLAG | RELAY_COUNT,
  { 192, 168, 1, 50 }, { 192, 168, 1, 1 },
  { 192, 168, 1, 1 }, { 255, 255, 255, 0 }
};

// Returns whether a relay command bit is ON.
bool relay_is_on(byte index) { return (relay_status & (1 << index)) != 0; }

// Returns whether DHCP is enabled.
bool use_dhcp() { return (network_config.options & DHCP_FLAG) != 0; }
// Returns whether exclusive relay mode is enabled.
bool exclusive_relays() { return (network_config.options & EXCLUSIVE_FLAG) != 0; }
// Returns the configured number of active relays.
byte active_relay_count() { byte count = network_config.options & COUNT_MASK; return count < 1 || count > RELAY_COUNT ? RELAY_COUNT : count; }
// Declares the EEPROM default label/header initializer.
void save_default_text();

// Drives output pins 2 through 7 from relay_status.
void update_relays() {
  for (byte index = 0; index < RELAY_COUNT; index++) {
    digitalWrite(2 + index, relay_is_on(index) ? HIGH : LOW);
  }
}

// Samples active-low relay feedback into one packed byte.
unsigned char* read_relay_status() {
  feedback_status = 0;
  for (byte index = 0; index < RELAY_COUNT; index++) {
    bool input_bit = index < 4 ? digitalRead(14 + index) == LOW : analogRead(index == 4 ? A6 : A7) <= 512;
    feedback_status |= input_bit << index;
  }
  return &feedback_status;
}

// Sets or clears one relay command bit.
void set_relay(byte index, bool enabled) {
  if (enabled) relay_status |= 1 << index;
  else relay_status &= ~(1 << index);
}

// Restores factory network, relay, label, and header defaults.
void load_defaults() {
  network_config = { EEPROM_MAGIC, DHCP_FLAG | EXCLUSIVE_FLAG | COLUMNS_FLAG | RELAY_COUNT, { 192, 168, 1, 50 }, { 192, 168, 1, 1 }, { 192, 168, 1, 1 }, { 255, 255, 255, 0 } };
  relay_status = 0;
  save_default_text();
}

// Loads saved configuration and relay state from EEPROM.
void load_eeprom() {
  EEPROM.get(0, network_config);
  if (network_config.magic != EEPROM_MAGIC) load_defaults();
  EEPROM.get(RELAY_EEPROM_ADDRESS, relay_status);
}

// Saves configuration and commanded relay state to EEPROM.
void save_eeprom() {
  EEPROM.put(0, network_config);
  EEPROM.put(RELAY_EEPROM_ADDRESS, relay_status);
  relay_state_dirty = false;
}

// Restores configuration and commanded relay state from EEPROM.
void restore_eeprom() {
  EEPROM.get(0, network_config);
  if (network_config.magic == EEPROM_MAGIC) {
    EEPROM.get(RELAY_EEPROM_ADDRESS, relay_status);
    update_relays();
    relay_state_dirty = false;
  }
}

// Turns off all commanded relays.
void clear_relays() {
  relay_status = 0;
  update_relays();
  relay_state_dirty = true;
}

// Formats a four-byte address as dotted-decimal text.
void print_ip(char* destination, size_t size, const byte address[4]) {
  snprintf(destination, size, "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
}

// Writes a bounded text field to EEPROM.
void write_text(int address, const char* value, byte size) {
  for (byte index = 0; index < size; index++) {
    EEPROM.update(address + index, value[index]);
    if (value[index] == '\0') break;
  }
  EEPROM.update(address + size - 1, '\0');
}

// Reads a bounded EEPROM text field and terminates it.
void read_text(int address, char* value, byte size) {
  for (byte index = 0; index < size - 1; index++) {
    value[index] = EEPROM.read(address + index);
    if (value[index] == '\0' || (byte)value[index] == 0xFF) {
      value[index] = '\0';
      break;
    }
  }
  value[size - 1] = '\0';
}

// Writes default label and column headers to EEPROM.
void save_default_text() {
  const char* defaults[RELAY_COUNT] = { "One", "Two", "Three", "Four", "Five", "Six" };
  for (byte index = 0; index < RELAY_COUNT; index++) write_text(HEADER_EEPROM_ADDRESS + index * HEADER_SIZE, defaults[index], HEADER_SIZE);
  write_text(LABEL_EEPROM_ADDRESS, "RF Matrix Switch", LABEL_SIZE);
}

// Extracts one bounded URL query value.
void query_value(const char* request, const char* key, char* value, byte size) {
  value[0] = '\0';
  const char* start = strstr(request, key);
  if (start == NULL) return;
  start += strlen(key);
  byte length = 0;
  while (*start && *start != '&' && *start != ' ' && length < size - 1) {
    if (*start == '+' || (*start == '%' && start[1] == '2' && start[2] == '0')) {
      value[length++] = ' ';
      start += *start == '+' ? 1 : 3;
    } else value[length++] = *start++;
  }
  value[length] = '\0';
}

// Applies network and relay-mode settings from a request.
void apply_network_config(const char* request) {
  char key[4], value[16], header[HEADER_SIZE], label[LABEL_SIZE];
  query_value(request, "count=", value, sizeof(value));
  byte count = (byte)atoi(value);
  if (count < 1) count = 1;
  if (count > RELAY_COUNT) count = RELAY_COUNT;
  network_config.options = (network_config.options & ~COUNT_MASK) | count;
  query_value(request, "exclusive=", value, sizeof(value));
  if (strcmp(value, "1") == 0) network_config.options |= EXCLUSIVE_FLAG;
  else network_config.options &= ~EXCLUSIVE_FLAG;
  query_value(request, "dhcp=", value, sizeof(value));
  if (strcmp(value, "1") == 0) network_config.options |= DHCP_FLAG;
  else network_config.options &= ~DHCP_FLAG;
  const char* keys[] = { "ip=", "gateway=", "dns=", "subnet=" };
  byte* addresses[] = { network_config.ip, network_config.gateway, network_config.dns, network_config.subnet };
  for (byte index = 0; index < 4; index++) {
    query_value(request, keys[index], value, sizeof(value));
    int parts[4];
    if (sscanf(value, "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]) == 4) {
      for (byte part = 0; part < 4; part++) addresses[index][part] = (byte)parts[part];
    }
  }
  EEPROM.put(0, network_config);
}

// Applies the page label and relay headers from a request.
void apply_headers_config(const char* request) {
  char key[4], header[HEADER_SIZE], label[LABEL_SIZE];
  if (strstr(request, "label=") != NULL) {
    query_value(request, "label=", label, sizeof(label));
    write_text(LABEL_EEPROM_ADDRESS, label, LABEL_SIZE);
  }
  for (byte index = 0; index < RELAY_COUNT; index++) {
    snprintf(key, sizeof(key), "h%u=", index);
    header[0] = '\0';
    query_value(request, key, header, sizeof(header));
    if (strstr(request, key) != NULL) write_text(HEADER_EEPROM_ADDRESS + index * HEADER_SIZE, header, HEADER_SIZE);
  }
}

// Sends standard HTTP headers for HTML content.
void send_http_headers(EthernetClient& client) {
  client.println(F("HTTP/1.0 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));
  client.println();
}

// Renders the shared page header and opening HTML.
void page_header(EthernetClient& client) {
  char label[LABEL_SIZE];
  read_text(LABEL_EEPROM_ADDRESS, label, sizeof(label));
  send_http_headers(client);
  client.println(F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width'><title>Remote Switch Controller - KI2L</title><style>body{font:16px sans-serif;max-width:900px;margin:auto;padding:12px;text-align:center}header,footer{padding:12px 0;text-align:center}main{display:flex;justify-content:center}table{border-collapse:collapse;margin:20px auto}td,th{border:1px solid #ddd;padding:10px;text-align:center}button{padding:10px 14px;font-size:14px}.relay{width:100%;height:100%}.on{background:#8f8}.off{background:#f8b0b0}.header{background-color: "));
  client.print(relay_state_dirty ? F("#ff6666") : F("#66cc66"));
  client.print(F(";}a{margin-right:14px}.logo{display:block;margin:0 auto}</style></head><body><header class='header'><h3>Remote Switch Controller - KI2L</h3><img class='logo' src='https://w2sz.org/images/W2SZ_small.gif' alt='W2SZ'><h3>"));
  client.print(label);
  client.println(F("</h3></header>"));
}

// Renders action buttons and closes the HTML document.
void page_footer(EthernetClient& client) {
  client.println(F("<footer><a href='/ee_clear'><button style='background:#aaa'>Clear</button></a><a href='/ee_save'><button style='background:#37f'>Save</button></a><a href='/ee_restore'><button style='background:#fc3'>Restore</button></a><a href='/network'><button style='background:#6c3'>Network</button></a><a href='/headers'><button style='background:#6c3'>Headers</button></a><hr></footer></body></html>"));
}

// Routes a request, performs its action, and renders a page.
void send_page(EthernetClient& client, const char* request) {
  if (strstr(request, "GET /ee_clear") != NULL) clear_relays();
  if (strstr(request, "GET /ee_save") != NULL) save_eeprom();
  if (strstr(request, "GET /ee_restore") != NULL) restore_eeprom();
  read_relay_status();
  bool network_saved = strstr(request, "GET /network?") != NULL;
  bool headers_saved = strstr(request, "GET /headers?") != NULL;
  if (network_saved) apply_network_config(request);
  if (headers_saved) apply_headers_config(request);
  if (strstr(request, "GET /config") != NULL) {
    page_header(client);
    client.println(F("<main><h2>Configure System</h2><p><a href='/network'><button>Network</button></a></p><p><a href='/headers'><button>Headers</button></a></p><p><a href='/'><button>Cancel</button></a></p></main>"));
    page_footer(client);
    return;
  }
  if (strstr(request, "GET /network") != NULL && !network_saved) {
    char ip[16], gateway[16], dns[16], subnet[16];
    print_ip(ip, sizeof(ip), network_config.ip); print_ip(gateway, sizeof(gateway), network_config.gateway); print_ip(dns, sizeof(dns), network_config.dns); print_ip(subnet, sizeof(subnet), network_config.subnet);
    page_header(client);
    client.print(F("<main><h2>Network</h2><form action='/network' method='get'>Relays <select name='count'>"));
    for (byte count = 1; count <= RELAY_COUNT; count++) { client.print(F("<option value='")); client.print(count); if (count == active_relay_count()) client.print(F("' selected>")); else client.print(F("'>")); client.print(count); client.println(F("</option>")); }
    client.print(F("</select><br>Exclusive <input type='checkbox' name='exclusive' value='1'")); if (exclusive_relays()) client.print(F(" checked")); client.print(F("><br>DHCP <input type='checkbox' name='dhcp' value='1'")); if (use_dhcp()) client.print(F(" checked")); client.print(F("><br>IP <input name='ip' value='")); client.print(ip); client.print(F("'><br>Gateway <input name='gateway' value='")); client.print(gateway); client.print(F("'><br>DNS <input name='dns' value='")); client.print(dns); client.print(F("'><br>Subnet <input name='subnet' value='")); client.print(subnet); client.println(F("'><br><button type='submit'>Save network</button> <a href='/'><button type='button'>Cancel</button></a></form></main>"));
    page_footer(client);
    return;
  }
  if (strstr(request, "GET /headers") != NULL && !headers_saved) {
    char label[LABEL_SIZE], header[HEADER_SIZE];
    read_text(LABEL_EEPROM_ADDRESS, label, sizeof(label));
    page_header(client);
    client.print(F("<main><h2>Headers</h2><form action='/headers' method='get'>Label <input maxlength='20' name='label' value='")); client.print(label); client.println(F("'><br>"));
    for (byte index = 0; index < RELAY_COUNT; index++) { read_text(HEADER_EEPROM_ADDRESS + index * HEADER_SIZE, header, sizeof(header)); client.print(F("Column ")); client.print(index + 1); client.print(F(" <input maxlength='8' name='h")); client.print(index); client.print(F("' value='")); client.print(header); client.println(F("'><br>")); }
    client.println(F("<button type='submit'>Save headers</button> <a href='/'><button type='button'>Cancel</button></a></form></main>"));
    page_footer(client);
    return;
  }
  page_header(client);
  read_relay_status();
  client.println(F("<main><table><tr>"));
  for (byte index = 0; index < active_relay_count(); index++) {
    char header[HEADER_SIZE];
    read_text(HEADER_EEPROM_ADDRESS + index * HEADER_SIZE, header, sizeof(header));
    client.print(F("<th>")); client.print(header); client.print(F("</th>"));
  }
  client.println(F("</tr><tr>"));
  for (byte index = 0; index < active_relay_count(); index++) {
    client.print(F("<td><a href='/toggle?relay=")); client.print(index); client.print(F("'><button class='relay ")); client.print(relay_is_on(index) ? F("on") : F("off")); client.print(F("' type='button'>")); client.print(relay_is_on(index) ? F("ON") : F("OFF")); client.println(F("</button></a></td>"));
  }
  client.println(F("</tr><tr>"));
  for (byte index = 0; index < active_relay_count(); index++) {
    client.print((feedback_status & (1 << index)) ? F("<td><span class='relay on'>ON</span></td>") : F("<td><span class='relay off'>OFF</span></td>"));
  }
  client.println(F("</tr></table></main>"));
  page_footer(client);
}

// Toggles the selected relay command and applies exclusive mode.
void toggle_relay(const char* request) {
  const char* parameter = strstr(request, "relay=");
  if (parameter == NULL) return;
  byte index = (byte)atoi(parameter + 6);
  if (index >= RELAY_COUNT) return;
  bool was_on = relay_is_on(index);
  if (exclusive_relays()) relay_status = 0;
  set_relay(index, !was_on);
  relay_state_dirty = true;
  update_relays();
}

// Initializes persistent state, I/O, Ethernet, and the server.
void setup() {
  Serial.begin(115200);
  load_eeprom();
  for (byte index = 0; index < RELAY_COUNT; index++) {
    pinMode(2 + index, OUTPUT);
    if (index < 4) pinMode(14 + index, INPUT_PULLUP);
  }
  update_relays();
  Ethernet.init(ETHERNET_CS_PIN);
  if (use_dhcp()) Ethernet.begin(mac_address);
  else Ethernet.begin(mac_address, network_config.ip, network_config.dns, network_config.gateway, network_config.subnet);
  server.begin();
  IPAddress address = Ethernet.localIP();
  Serial.print(use_dhcp() ? F("DHCP IP: ") : F("Static IP: "));
  Serial.print(address[0]); Serial.print('.'); Serial.print(address[1]); Serial.print('.'); Serial.print(address[2]); Serial.print('.'); Serial.println(address[3]);
}

// Receives one HTTP request, responds, then closes the connection.
void loop() {
  if (EthernetClient client = server.available()) {
    char request[160];
    byte length = 0;
    unsigned long deadline = millis() + 500;
    bool request_complete = false;
    while (client.connected() && millis() < deadline && !request_complete) {
      if (client.available() == 0) continue;
      char character = client.read();
      if (character == '\n') request_complete = true;
      else if (character != '\r' && length < sizeof(request) - 1) request[length++] = character;
    }
    if (request_complete) {
      request[length] = '\0';
      if (strncmp(request, "GET /toggle", 11) == 0) toggle_relay(request);
      send_page(client, request);
    }
    while (client.available() > 0) client.read();
    client.stop();
  }
}
