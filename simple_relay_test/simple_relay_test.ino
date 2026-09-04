#define RELAY_COUNT 6
#define SETTLE_TIME_MS 100
#define SAMPLE_COUNT 5
#define SAMPLE_INTERVAL_MS 5

static const byte output_pins[RELAY_COUNT] = { 2, 3, 4, 5, 6, 7 };
static const byte input_pins[RELAY_COUNT] = { 14, 15, 16, 17, A6, A7 };

unsigned char read_inputs() {
  unsigned char input_word = 0;
  for (byte index = 0; index < RELAY_COUNT; index++) {
    bool input_bit = index < 4 ? digitalRead(input_pins[index]) == LOW : analogRead(input_pins[index]) <= 512;
    input_word |= input_bit << index;
  }
  return input_word;
}

unsigned char stable_input_sample() {
  unsigned int input_totals[RELAY_COUNT] = { 0, 0, 0, 0, 0, 0 };
  for (byte sample = 0; sample < SAMPLE_COUNT; sample++) {
    unsigned char input_word = read_inputs();
    for (byte index = 0; index < RELAY_COUNT; index++) input_totals[index] += (input_word >> index) & 1;
    delay(SAMPLE_INTERVAL_MS);
  }
  unsigned char stable_word = 0;
  for (byte index = 0; index < RELAY_COUNT; index++) {
    if (input_totals[index] >= (SAMPLE_COUNT + 1) / 2) stable_word |= 1 << index;
  }
  return stable_word;
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
  bool any_change = false;
  Serial.print(F("Changed inputs: "));
  for (byte index = 0; index < RELAY_COUNT; index++) {
    if (((before >> index) & 1) != ((after >> index) & 1)) {
      if (any_change) Serial.print(F(", "));
      Serial.print(input_pins[index]);
      any_change = true;
    }
  }
  if (!any_change) Serial.print(F("none"));
  Serial.println();
}

void test_relay(byte relay_index) {
  unsigned char before = stable_input_sample();
  Serial.print(F("Relay ")); Serial.print(relay_index + 1); Serial.print(F(" output pin ")); Serial.print(output_pins[relay_index]); Serial.println(F(" ON"));
  digitalWrite(output_pins[relay_index], HIGH);
  delay(SETTLE_TIME_MS);
  unsigned char after_on = stable_input_sample();
  print_input_word(F("Inputs after ON:  "), after_on);
  print_changed_inputs(before, after_on);

  Serial.print(F("Relay ")); Serial.print(relay_index + 1); Serial.print(F(" output pin ")); Serial.print(output_pins[relay_index]); Serial.println(F(" OFF"));
  digitalWrite(output_pins[relay_index], LOW);
  delay(SETTLE_TIME_MS);
  unsigned char after_off = stable_input_sample();
  print_input_word(F("Inputs after OFF: "), after_off);
  print_changed_inputs(after_on, after_off);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  for (byte index = 0; index < RELAY_COUNT; index++) {
    pinMode(output_pins[index], OUTPUT);
    if (index < 4) pinMode(input_pins[index], INPUT_PULLUP);
  }
  for (byte index = 0; index < RELAY_COUNT; index++) digitalWrite(output_pins[index], LOW);
  Serial.println(F("Serial relay input/output mapping test"));
  Serial.println(F("Outputs: 2,3,4,5,6,7"));
  Serial.println(F("Inputs LSB-first: 14,15,16,17,A6,A7"));
  Serial.println(F("LOW or analog <=512 is reported as ON"));
  Serial.println();
  for (byte relay_index = 0; relay_index < RELAY_COUNT; relay_index++) test_relay(relay_index);
  for (byte index = 0; index < RELAY_COUNT; index++) digitalWrite(output_pins[index], LOW);
  Serial.println(F("Test complete; outputs left OFF."));
}

void loop() {
}
