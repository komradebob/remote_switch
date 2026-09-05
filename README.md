# rem_sw Arduino relay web server

Software to control one to 6 relays. My original project was a 6x4 RF switch, this has grown/devolved from that initially to control a 
Single Pole 6 Throw (SP6T) coaxial RF switch used with an ICOM IC-905 to switch the outputs of the radio to the correct antennas at the
top of the mast using only a single RF cable and Cat6 Ethernet cable. It has since found use with a SP3T coaxial switch steering the RF 
output of an IF radio used to drive transverters for 902, 3400, and 24GHz. 

The original hand crafted software was dramatically reworked using CoPilot in early September 2026. 

The initial implementations include firmware for a simple HTTP server suitable for an Arduino Pro Mini and ENC28J60 Ethernet module. 
The server exposes a dashboard showing the current state of 1-6 relays underneath buttons used to toggle the state of any relay. 
There are also two configuration pages, one for the network information such as a static IP address or the use of DHCP. The header 
page allows you to set the number of relays, the exclusivity (N separate relays or one of N), the labels for the dashboard controls. 
Each page also has a footer with buttons to take you to the configuration pages, clear the relays (sets them all to off), 
save the current state to EEPROM, and to restore the state. NOTE:If you change anything, it is good practice to save the config after
Just To Make Sure.

To keep you informed, the header on the dashboard is green when the state is saved, red when it is not.

## NOTE:  The MAC Address is hardcoded! If you are going to put more than one of these on your network, change the last octet!!!

I tried to implement selection of a random MAC every time the system booted, however it was causing instability and I ran out of time. 
If you know of a solution or fix it, please let me know!!!


## Libraries

Install the stock `UIPEthernet` library through the Arduino Library Manager. The sketch uses the Pro Mini hardware SPI pins for the 
ENC28J60 and the library's default chip-select pin.

Typical ENC28J60 wiring:

- `VCC` and `GND` to the module's required supply voltage and ground. NOTE: The ENC28J60 is a 3.3v chip. Set VCC appropriately and
   use level shifters or a 3.3V Arduino
- `SCK`, `MISO`, and `MOSI` to the Pro Mini hardware SPI pins (11,12,13)
- `CS` to digital pin 10
- `INT` is not required by this sketch

Check the exact voltage requirements of the specific ENC28J60 module before powering it.

## Build and upload

Open `rem_sw.ino` in the Arduino IDE, select the Arduino Pro or Pro Mini board and the correct processor/clock, select the serial port, 
then install `UIPEthernet` and upload. The sketch uses DHCP or the static values in `network_config` depending on the EEPROM configuration.

The dashboard uses DHCP by default, six relay controls, EEPROM-backed headers, and the tested UIPEthernet server loop. The fallback static 
address is `192.168.1.50`. **Configure** opens a chooser with separate short pages for network/relay mode and labels/headers, avoiding oversized c
onfiguration requests. EEPROM actions are available as colored buttons in the page footer.

NOTE:
This code is VERY sensitive to using more SRAM. You will note many of the long strings are stored in flash via the F() macro. 
This is to save SRAM. Adding even a few additional int to the code causes it to fail in interesting ways, usually rebooting 
constantly, not finding an IP address, or adding HTML that isn't supposed to be there to the output stream. 

## Hardware extension points

- `update_relays()` currently drives output pins 2 through 7, active HIGH.
- `read_relay_status()` reads feedback from pins 14, 15, 16, 17, A6, and A7, least significant bit first. Digital inputs use active-low
   `INPUT_PULLUP`; analog inputs use active-low threshold 512.
- Customize the page header in `page_header()` and footer actions in `page_footer()`.

The dashboard reads feedback when a page is requested and displays it below the relay command buttons. Relay command state is stored 
separately from feedback state. Relay buttons use direct links and reload the dashboard after a command. If your relays do not have 
confirmation outputs, these will always read 'OFF'. This should get fixed. Someday.

## Hardware tests

`simple_relay_test/simple_relay_test.ino` is a serial-only mapping test. It drives each output on pins 2 through 7 ON and OFF, waits 100 ms for the relay indicator to settle, samples inputs 14, 15, 16, 17, A6, and A7 five times, and reports which inputs changed. It runs at 115200 baud and leaves all outputs OFF.

`ethernet_relay_test/ethernet_relay_test.ino` adds UIPEthernet to the same mapping test. Upload it, read the DHCP address at 115200 baud, then open `/cycle` on the device to run the sequence over Ethernet. The cycle results are printed on the serial port and all outputs are left OFF afterward.


## Final Notes

If you find this useful, please let me know. If you use it, even in part, in a commercial product, please give credit.
