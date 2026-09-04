# rem_sw Arduino relay web server

Web server firmware for an Arduino Pro Mini and ENC28J60 Ethernet module. The dashboard exposes configurable relay controls, feedback status boxes, and EEPROM actions.

## Libraries

Install the stock `UIPEthernet` library through the Arduino Library Manager. The sketch uses the Pro Mini hardware SPI pins for the ENC28J60 and the library's default chip-select pin.

Typical ENC28J60 wiring:

- `VCC` and `GND` to the module's required supply voltage and ground
- `SCK`, `MISO`, and `MOSI` to the Pro Mini hardware SPI pins
- `CS` to digital pin 10
- `INT` is not required by this sketch

Check the exact voltage requirements of the specific ENC28J60 module before powering it.

## Build and upload

Open `rem_sw.ino` in the Arduino IDE, select the Arduino Pro or Pro Mini board and the correct processor/clock, select the serial port, then install `UIPEthernet` and upload. The sketch uses DHCP or the static values in `network_config` depending on the EEPROM configuration.

The dashboard uses DHCP by default, six relay controls, EEPROM-backed headers, and the tested UIPEthernet server loop. The fallback static address is `192.168.1.50`. **Configure** opens a chooser with separate short pages for network/relay mode and labels/headers, avoiding oversized configuration requests. EEPROM actions are available as colored buttons in the page footer.

## Hardware extension points

- Implement `update_relays()` to write the six physical relay outputs.
- `update_relays()` currently drives output pins 2 through 7, active HIGH.
- `read_relay_status()` reads feedback pins 14, 15, 16, 17, A6, and A7, least significant bit first. Digital inputs use active-low `INPUT_PULLUP`; analog inputs use active-low threshold 512.
- Customize the page header in `page_header()` and footer actions in `page_footer()`.

The dashboard reads feedback when a page is requested and displays it below the relay command buttons. Relay command state is stored separately from feedback state. Relay buttons use direct links and reload the dashboard after a command.

## Hardware tests

`simple_relay_test/simple_relay_test.ino` is a serial-only mapping test. It drives each output on pins 2 through 7 ON and OFF, waits 100 ms for the relay indicator to settle, samples inputs 14, 15, 16, 17, A6, and A7 five times, and reports which inputs changed. It runs at 115200 baud and leaves all outputs OFF.

`ethernet_relay_test/ethernet_relay_test.ino` adds UIPEthernet to the same mapping test. Upload it, read the DHCP address at 115200 baud, then open `/cycle` on the device to run the sequence over Ethernet. The cycle results are printed on the serial port and all outputs are left OFF afterward.
