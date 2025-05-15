# rf95-rtu-bridge
Serial Modbus RTU to RFM95 LoRa bridge

This project demonstrates how to perform MODBUS message transmission over LoRa using RFM95 modules.

Here we are using a NanoPi or Raspberry Pi board which transmits to an other board equipped with an RFM95 module. The latter is connected to a Modbus RTU slave device (for example, a PLC or a sensor). The Pi board acts as a Modbus master and communicates with the slave device or vice versa.  

The libmodbuspp library can be used to perform Modbus RTU communication tests on the Pi board by connecting to the virtual serial port connected to the bridge (see below).

## Dependencies

On Pi, you must install the following libraries:  
- [epsilonrt/RadioHead](https://github.com/epsilonrt/RadioHead)  
- [epsilonrt/piduino](https://github.com/epsilonrt/piduino) and  
- [libmodbuspp](https://github.com/epsilonrt/libmodbuspp) for the example of Modbus RTU master/slave on the Pi board.  
and [tty0tty](https://github.com/epsilonrt/tty0tty) to create virtual serial ports.

On Arduino, you must install the following libraries (available in the Arduino library manager, or PlatformIO):  
- [Modbus-Radio](https://github.com/epsilonrt/modbus-radio)  
- [RadioHead](https://github.com/epsilonrt/RadioHead), this one and not the one available in the library manager!  
- [Crypto](https://rweather.github.io/arduinolibs/crypto.html), available in the library manager  

It will be necessary to **activate encryption in RadioHead** as indicated at the beginning of the sketch rf95_modbus_slave_tester!

## Install on the Pi board

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

## Build and Execute on the Pi board

You must modify the `rf95_rtu_bridge.cpp` file to match your hardware configuration.  
`CsPin` is the chip select pin of the RFM95 module, and `Dio0Pin` is the interrupt pin.

```cpp
// Choose or modify the following lines to match your hardware configuration
// ---------------------------

// NanoPi /dev/spidev0.0
// uint8_t CsPin = 10;
// uint8_t Dio0Pin = 0;

// LoRasSpi /dev/spidev0.0
uint8_t CsPin = 10;
uint8_t Dio0Pin = 6;

float frequency = 868.0;
```

It is important to know that **the SPI port used is the first in the list of available SPI ports displayed by the `pinfo`** command (see below).

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

After installing  [tty0tty](https://github.com/epsilonrt/tty0tty) for virtual ports, build and run  :

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

If you want to enable AES128 encryption, you can supply an encryption key with the `-k` option:  

    rf95_rtu_bridge --key 1234567890abcdef /dev/tnt0
    
It will of course be necessary to ensure that the same encryption key is configured in the Arduino sketch!!

![rf95_rtu_bridge](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/rf95_rtu_bridge.png)

The command has a help page:

```
$ rf95_rtu_bridge -h
rf95_rtu_bridge [OPTION]... serial_port
  serial_port:  serial port path, eg /dev/ttyUSB0, /dev/tnt1...
Allowed options:
  -h, --help                   produce help message
  -v, --verbose                be verbose
  -D, --daemon                 be daemonized
  -b, --baudrate arg (=38400)  set serial baudrate
  -t, --txled arg              set the Tx led pin number (Ino number if --wirebus not used)
  -r, --rxled arg              set the Rx led pin number (Ino number if --wirebus not used)
  -w, --wirebus arg            set the wire bus where a PCF8574 wich Tx and/or Rx led is connected
  -k, --key arg                secret key for AES128 encryption, must be 16 characters long
```

## Build and Execute on the Arduino board

Upload the Arduino/rf95_modbus_lamp sketch to an Arduino board equipped with an RFM95 shield.

![rf95_modbus_lamp](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/rf95_modbus_lamp.png)

Don't forget to modify the encryption key and pass isEncrypted to true if you use encryption.

## Send Modbus messages from Pi Board

You can then use mbpoll to send a test modbus command :

     mbpoll -m rtu -b38400 -a20 -t0 -r1 -c3 -v /dev/tnt1

![mbpoll](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/mbpoll.png)
