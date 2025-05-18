# Serial Modbus RTU to RFM95 LoRa Bridge

This project demonstrates how to transmit MODBUS messages over LoRa using RFM95 modules.

A NanoPi or Raspberry Pi board is used to communicate with another board equipped with an RFM95 module. This second board is connected to a Modbus RTU slave device, such as a PLC or sensor. The Pi board acts as a Modbus master and communicates with the slave device, or vice versa.

The libmodbuspp library can be used to test Modbus RTU communication on the Pi board by connecting to the virtual serial port linked to the bridge (see below).

## Dependencies

On the Pi, install the following libraries:  
- [epsilonrt/piduino](https://github.com/epsilonrt/piduino)  
- [epsilonrt/RadioHead](https://github.com/epsilonrt/RadioHead)  
- [tty0tty](https://github.com/epsilonrt/tty0tty) to create virtual serial ports
- [libmodbuspp](https://github.com/epsilonrt/libmodbuspp) for Modbus RTU master/slave examples  

You may need to install the piduino.org repository:  
```bash
wget -O- http://www.piduino.org/piduino-key.asc | sudo gpg --dearmor --yes --output /usr/share/keyrings/piduino-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/piduino-archive-keyring.gpg] http://apt.piduino.org $(lsb_release -c -s) piduino" | sudo tee /etc/apt/sources.list.d/piduino.list
sudo apt update
```

Then, install all the dependencies:  
```bash
sudo apt install libpiduino-dev piduino-utils tty0tty-dkms radiohead radiohead-dev radiohead-doc libmodbuspp-dev libmodbuspp-doc 
```

On Arduino, install these libraries (available via the Arduino library manager or PlatformIO):  
- [Modbus-Radio](https://github.com/epsilonrt/modbus-radio)  
- [RadioHead](https://github.com/epsilonrt/RadioHead) (use this version, not the one in the library manager)  
- [Crypto](https://rweather.github.io/arduinolibs/crypto.html), available in the library manager  

**Remember to enable encryption in RadioHead** as described at the beginning of the `rf95_modbus_slave_tester` sketch!

## Installation on the Pi Board

```bash
git clone --recurse-submodules https://github.com/epsilonrt/rf95-rtu-bridge
cd rf95-rtu-bridge
git submodule init
git submodule update
mkdir build
cd build
cmake ..
make
sudo make install
```

## CS and DIO0 Pins

The CS and DIO0 pins are used to communicate with the RFM95 module.  
The CS pin is the chip select pin, and the DIO0 pin is the interrupt pin.

**You must pass the CS and DIO0 pin numbers to the `rf95_rtu_bridge` program with the `-c` and `-d` options, respectively**.

To find the pin numbers, use the `pido` command and your schematic of the RFM95 board.

```bash
epsilonrt@cm4:~ $ pido readall
                                           J8 (#1)
+-----+-----+----------+------+------+---+----++----+---+------+------+----------+-----+-----+
| sOc | iNo |   Name   | Mode | Pull | V | Ph || Ph | V | Pull | Mode |   Name   | iNo | sOc |
+-----+-----+----------+------+------+---+----++----+---+------+------+----------+-----+-----+
|     |     |     3.3V |      |      |   |  1 || 2  |   |      |      | 5V       |     |     |
|   2 |   8 |     SDA1 | ALT0 |   UP | 1 |  3 || 4  |   |      |      | 5V       |     |     |
|   3 |   9 |     SCL1 | ALT0 |   UP | 1 |  5 || 6  |   |      |      | GND      |     |     |
|   4 |   7 |    GPIO4 |   IN |   UP | 1 |  7 || 8  | 1 | OFF  | ALT5 | TXD1     | 15  | 14  |
|     |     |      GND |      |      |   |  9 || 10 | 1 | UP   | ALT5 | RXD1     | 16  | 15  |
|  17 |   0 |   GPIO17 |  OUT | DOWN | 1 | 11 || 12 | 1 | DOWN | OUT  | GPIO18   | 1   | 18  |
|  27 |   2 |   GPIO27 |   IN | DOWN | 0 | 13 || 14 |   |      |      | GND      |     |     |
|  22 |   3 |   GPIO22 |   IN | DOWN | 1 | 15 || 16 | 0 | OFF  | OUT  | GPIO23   | 4   | 23  |
|     |     |     3.3V |      |      |   | 17 || 18 | 0 | DOWN | IN   | GPIO24   | 5   | 24  |
|  10 |  12 | SPI0MOSI | ALT0 | DOWN | 0 | 19 || 20 |   |      |      | GND      |     |     |
|   9 |  13 | SPI0MISO | ALT0 | DOWN | 0 | 21 || 22 | 1 | DOWN | IN   | GPIO25   | 6   | 25  |
|  11 |  14 | SPI0SCLK | ALT0 | DOWN | 0 | 23 || 24 | 1 | OFF  | OUT  | GPIO8    | 10  | 8   |
|     |     |      GND |      |      |   | 25 || 26 | 1 | UP   | OUT  | GPIO7    | 11  | 7   |
|   0 |  30 |    GPIO0 |   IN |   UP | 1 | 27 || 28 | 1 | UP   | IN   | GPIO1    | 31  | 1   |
|   5 |  21 |    GPIO5 |   IN |   UP | 1 | 29 || 30 |   |      |      | GND      |     |     |
|   6 |  22 |    GPIO6 |   IN |   UP | 1 | 31 || 32 | 0 | DOWN | IN   | GPIO12   | 26  | 12  |
|  13 |  23 |   GPIO13 |   IN | DOWN | 0 | 33 || 34 |   |      |      | GND      |     |     |
|  19 |  24 | SPI1MISO | ALT4 | DOWN | 0 | 35 || 36 | 1 | DOWN | OUT  | STAT_LED | 27  | 16  |
|  26 |  25 |   GPIO26 |   IN | DOWN | 0 | 37 || 38 | 0 | DOWN | ALT4 | SPI1MOSI | 28  | 20  |
|     |     |      GND |      |      |   | 39 || 40 | 0 | DOWN | ALT4 | SPI1SCLK | 29  | 21  |
+-----+-----+----------+------+------+---+----++----+---+------+------+----------+-----+-----+
| sOc | iNo |   Name   | Mode | Pull | V | Ph || Ph | V | Pull | Mode |   Name   | iNo | sOc |
+-----+-----+----------+------+------+---+----++----+---+------+------+----------+-----+-----+
```

This command will display the GPIO pin numbers and their corresponding names. Look for the CS and DIO0 pins in the output.

For example, the LoRasPi board uses GPIO 8 for CS and GPIO 25 for DIO0 on a Raspberry Pi, which corresponds to the iNo numbers 10 for CS and 6 for DIO0 in the output above.  

The `rf95_rtu_bridge` command will look like this:

```bash
rf95_rtu_bridge -c10 -d6 /dev/tnt0
```

If you want to turn on the LED of the LoRasPi module, you can use GPIO 23 (iNo 4) for the Rx/Tx LED. The command will be:

```bash
rf95_rtu_bridge -c10 -d6 -l4 /dev/tnt0
```


Note that **the SPI port used is the first one listed by the `pinfo` command (see below).**

```bash
epsilonrt@cm4:~/src/rf95-rtu-bridge/Arduino $ pinfo
Name            : Raspberry Pi Compute Module 4
Model           : RaspberryPi Compute Module 4
Family          : RaspberryPi
Database Id     : 68
Manufacturer    : Sony UK
Board Revision  : 0xd03141
SoC             : Bcm2711 (Broadcom)
Memory          : 8192MB
GPIO Id         : 3
PCB Revision    : 1.1
I2C Buses       : /dev/i2c-1,/dev/i2c-20,/dev/i2c-21
SPI Buses       : /dev/spidev0.0,/dev/spidev0.1
Serial Ports    : /dev/ttyS0
```


To enable AES128 encryption, provide an encryption key with the `-k` option:

```bash
rf95_rtu_bridge  -c10 -d6 --key 1234567890abcdef /dev/tnt0
```

Make sure the same encryption key is set in the Arduino sketch!

![rf95_rtu_bridge](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/rf95_rtu_bridge.png)

The command includes a help page:

```bash
$ rf95_rtu_bridge -h
rf95_rtu_bridge [OPTION]... serial_port
  serial_port:  serial port path, eg /dev/ttyUSB0, /dev/tnt1...
Allowed options:
  -h, --help                   produce help message
  -v, --verbose                be verbose
  -D, --daemon                 be daemonized
  -q, --quiet                  be quiet, if set, there is no output on the console
  -c, --cs-pin arg             sets the chip select pin number, must be set ! > use `pido readall` to find the pin number
  -d, --dio0-pin arg           sets the dio0 pin number, must be set !        > use `pido readall` to find the pin number
  -l, --led-pin arg            sets the Rx/Tx led pin number (optionnal)      > use `pido readall` to find the pin number
  -y, --wire-bus arg           sets the wire bus where a PCF8574 wich Rx/Tx led is connected
  -b, --baudrate arg (=38400)  sets serial baudrate
  -k, --key arg                sets the secret key for AES128 encryption, must be 16 characters long
  -p, --tx-power arg           sets the transmitter power output level in dBm (5..23, default 13)
  -s, --spreading-factor arg   sets the radio spreading factor (6..12, default 7)
  -w, --bandwidth arg          sets the radio signal bandwidth in Hz (62500, 125000, 250000, 500000, default 125000)
  -r, --coding-rate arg        sets the coding rate to 4/5, 4/6, 4/7 or 4/8 (denominator 5..8, default 5)
```

## Use an Arduino Board with RFM95 Shield to Test the Bridge

You can use an Arduino board with an RFM95 shield to test the bridge.

Upload the `Arduino/rf95_modbus_lamp` sketch to an Arduino board with an RFM95 shield.

![rf95_modbus_lamp](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/rf95_modbus_lamp.png)

If you use encryption, update the encryption key and set `isEncrypted` to `true`.

## Sending Modbus Messages from the Pi Board

You can use `mbpoll` to send a test Modbus command (turn on the LED) to the Arduino board with the RFM95 shield:

```bash
mbpoll -m rtu -b38400 -a10 -t0 -v /dev/tnt1 1
```

![mbpoll](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/mbpoll.png)

## More Complex Scenarios

Combined with [libmodbuspp](https://github.com/epsilonrt/libmodbuspp), you can create more complex scenarios. For example, you can build a bridge between multiple Modbus networks using LoRa.

In the example below, a Raspberry Pi (host `cm4.naboo.lan`) is used as a gateway between a LoRa network and a Modbus RTU network. The [`router-json`](https://github.com/epsilonrt/libmodbuspp/tree/master/examples/router/router-json) program from libmodbuspp is used to route messages between the LoRa network and the RS485 network.  
`router-json` listens for requests on TCP port 1502 and forwards them either to the virtual serial port `/dev/tnt0` for LoRa or to `/dev/ttyUSB0` for the RS485 network, depending on the slave ID specified in the request.

This corresponds to the diagram below:

![router-ibd](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/router-ibd.png)

In which `HF Modem` refers to the `rf95_rtu_bridge` program connected to a LoRa RFM95 module.

In the image below, you can see:

- Top left: the [`mbpoll`](https://github.com/epsilonrt/mbpoll) command sends a request from a PC to the Raspberry Pi's outside port to set a coil (LED) to 1 on a slave with ID 10 connected to the LoRa Modbus network.  
- Bottom left: the `router-json` program on `cm4` receives the command on TCP port 1502 and forwards it to the virtual serial port `/dev/tnt0` (connected to the `rf95_rtu_bridge` program). You can also see a slave with ID 40 on the RS485 network at `/dev/ttyUSB0`.  
- Top right: the `rf95_rtu_bridge` program receives the command on the virtual serial port `/dev/tnt1` and forwards it to the LoRa RFM95 module. The response took 87ms to arrive, which is quite fast for LoRa transmission.
- Bottom right: the serial output of the `rf95_modbus_lamp` program running on an Arduino board with a LoRa RFM95 module. The command was received and the lamp is turned on.

![router-rf95-write-coil-lamp](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/router-rf95-write-coil-lamp.png)

The image below shows the same setup, but with a slave connected to the Raspberry Pi's RS485 network (ID 40), responding to a request to read 2 input registers:

![router-rf95-read-ireg-master-station](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/router-rf95-read-ireg-master-station.png)

The value read, 420, corresponds to a voltage of 4.2V

Finally, this scenario can be extended to several LoRa stations. In the image below, you can see a second slave at address 16 on the LoRa network. It is physically located 1km from the Raspberry Pi on another LoRa station managed by a NanoPi board running the `rf95_rtu_bridge` program connected to `router-json` to route messages to its RS485 network, where the actual Modbus RTU slave (@16) is located:

![router-rf95-write-coil-slave-station](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/router-rf95-write-coil-slave-station.png)

You can see that the request sent by mbpoll from the PC sets a coil to 1, which activates a relay.
