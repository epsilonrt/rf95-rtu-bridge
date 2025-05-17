# Serial Modbus RTU to RFM95 LoRa Bridge

This project demonstrates how to transmit MODBUS messages over LoRa using RFM95 modules.

A NanoPi or Raspberry Pi board is used to communicate with another board equipped with an RFM95 module. This second board is connected to a Modbus RTU slave device, such as a PLC or sensor. The Pi board acts as a Modbus master and communicates with the slave device, or vice versa.

The libmodbuspp library can be used to test Modbus RTU communication on the Pi board by connecting to the virtual serial port linked to the bridge (see below).

## Dependencies

On the Pi, install the following libraries:  
- [epsilonrt/RadioHead](https://github.com/epsilonrt/RadioHead)  
- [epsilonrt/piduino](https://github.com/epsilonrt/piduino)  
- [libmodbuspp](https://github.com/epsilonrt/libmodbuspp) for Modbus RTU master/slave examples  
- [tty0tty](https://github.com/epsilonrt/tty0tty) to create virtual serial ports

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

## Building and Running on the Pi Board

Edit the `rf95_rtu_bridge.cpp` file to match your hardware configuration.  
`CsPin` is the chip select pin for the RFM95 module, and `Dio0Pin` is the interrupt pin.

```cpp
// Adjust these lines to match your hardware configuration
// ---------------------------

// NanoPi /dev/spidev0.0
// uint8_t CsPin = 10;
// uint8_t Dio0Pin = 0;

// LoRasSpi /dev/spidev0.0
uint8_t CsPin = 10;
uint8_t Dio0Pin = 6;

float frequency = 868.0;
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

After installing [tty0tty](https://github.com/epsilonrt/tty0tty) for virtual ports, build and run:

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

To enable AES128 encryption, provide an encryption key with the `-k` option:

```bash
rf95_rtu_bridge --key 1234567890abcdef /dev/tnt0
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
  -b, --baudrate arg (=38400)  sets serial baudrate
  -t, --tx-led arg             sets the Tx led pin number (Ino number if --wirebus not used)
  -r, --rx-led arg             sets the Rx led pin number (Ino number if --wirebus not used)
  -y, --wire-bus arg           sets the wire bus where a PCF8574 wich Tx and/or Rx led is connected
  -k, --key arg                sets the secret key for AES128 encryption, must be 16 characters long
  -p, --tx-power arg           sets the transmitter power output level (5..23, default 13 dBm)
  -s, --spreading-factor arg   sets the radio spreading factor (6..12, default 7)
  -w, --bandwidth arg          sets the radio signal bandwidth in Hz (62500, 125000, 250000, 500000, default 125000)
  -c, --coding-rate arg        sets the coding rate to 4/5, 4/6, 4/7 or 4/8 (denominator 5..8, default 5)
```

## Building and Running on the Arduino Board

Upload the `Arduino/rf95_modbus_lamp` sketch to an Arduino board with an RFM95 shield.

![rf95_modbus_lamp](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/rf95_modbus_lamp.png)

If you use encryption, update the encryption key and set `isEncrypted` to `true`.

## Sending Modbus Messages from the Pi Board

You can use `mbpoll` to send a test Modbus command:


```bash
mbpoll -m rtu -b38400 -a10 -t0 -r1 -c3 -v /dev/tnt1
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
