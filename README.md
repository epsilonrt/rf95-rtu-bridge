# rf95-rtu-bridge
Serial Modbus RTU to RFM95 LoRa bridge

```
git clone --recurse-submodules https://github.com/epsilonrt/rf95-rtu-bridge
cd rf95-rtu-bridge
git submodule init
git submodule update
codelite &
```

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

Upload the Arduino/rf95_modbus_slave_tester sketch to an Arduino board equipped with an RFM95 shield.

You can then use mbpoll to send a test modbus command :

     mbpoll -m rtu -b38400 -a20 -t0 -r1 -c3 -v /dev/tnt1
