# rf95-rtu-bridge
Serial Modbus RTU to RFM95 LoRa bridge

This project demonstrates how to perform MODBUS message transmission over LoRa using RFM95 modules.

Here we are using a NanoPi or Raspberry Pi board which transmits to an Arduino MEGA board.
The 2 boards are equipped with RFM95 shields.

## Dependencies

On Pi, you must install the following libraries:  
- [epsilonrt/RadioHead](https://github.com/epsilonrt/RadioHead)  
- [epsilonrt/piduino](https://github.com/epsilonrt/piduino)  

On Arduino, you must install the following libraries:  
- [epsilonrt/RadioHead](https://github.com/epsilonrt/RadioHead), this one and not the one available in the library manager!  
- [Crypto](https://rweather.github.io/arduinolibs/crypto.html), available in the library manager  

It will be necessary to **activate encryption in RadioHead** as indicated at the beginning of the sketch rf95_modbus_slave_tester!

## Install on the Pi board

```
git clone --recurse-submodules https://github.com/epsilonrt/rf95-rtu-bridge
cd rf95-rtu-bridge
git submodule init
git submodule update
codelite &
```

## Build and Execute on the Pi board

Possibly modify the constants at the beginning of main.cpp according to your board :

```cpp
const int LedPin = 0;
const int WireBus = 2;
const uint8_t CsPin = 10;
const uint8_t Dio0Pin = 0;

const float frequency = 868.0;
```

You can use [tty0tty](https://github.com/epsilonrt/tty0tty) to install virtual serial ports.

After installing virtual ports, build with codelite and run as root :

    cd Release
    sudo ./rf95_rtu_bridge /dev/tnt0

If you want to enable AES128 encryption, you can supply an encryption key with the `-k` option:  

    sudo ./rf95_rtu_bridge --key 1234567890abcdef /dev/tnt0
    
It will of course be necessary to ensure that the same encryption key is configured in the Arduino sketch!!

![rf95_rtu_bridge](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/rf95_rtu_bridge.png)

The command has a help page:

```
$ ./rf95_rtu_bridge -h
rf95_rtu_bridge [OPTION]... serial_port
  serial_port:  serial port path, eg /dev/ttyUSB0, /dev/tnt1...
Allowed options:
  -h, --help                   produce help message
  -v, --verbose                be verbose
  -D, --daemon                 be daemonized
  -b, --baudrate arg (=38400)  set serial baudrate
  -k, --key arg                secret key for AES128 encryption, must be 16 characters long
```

## Build and Execute on the Arduino board


Upload the Arduino/rf95_modbus_slave_tester sketch to an Arduino board equipped with an RFM95 shield.

![rf95_modbus_slave_tester](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/rf95_modbus_slave_tester.png)

Don't forget to modify the encryption key and pass isEncrypted to true if you use encryption.

## Send Modbus messages from Pi Board

You can then use mbpoll to send a test modbus command :

     mbpoll -m rtu -b38400 -a20 -t0 -r1 -c3 -v /dev/tnt1

![mbpoll](https://github.com/epsilonrt/rf95-rtu-bridge/blob/main/doc/images/mbpoll.png)
