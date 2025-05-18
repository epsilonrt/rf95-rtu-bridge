// rf95_modbus_bridge
// Modbus RTU bridge between serial port and RF95 radio
//
// rf95_rtu_bridge -h
//
// rf95_rtu_bridge [OPTION]... serial_port
//   serial_port:  serial port path, eg /dev/ttyUSB0, /dev/tnt1...
// Allowed options:
//   -h, --help                   produce help message
//   -v, --verbose                be verbose
//   -D, --daemon                 be daemonized
//   -q, --quiet                  be quiet, if set, there is no output on the console
//   -c, --cs-pin arg             sets the chip select pin number, must be set ! > use `pido readall` to find the pin number
//   -d, --dio0-pin arg           sets the dio0 pin number, must be set !        > use `pido readall` to find the pin number
//   -l, --led-pin arg            sets the Rx/Tx led pin number (optionnal)      > use `pido readall` to find the pin number
//   -y, --wire-bus arg           sets the wire bus where a PCF8574 wich Rx/Tx led is connected
//   -b, --baudrate arg (=38400)  sets serial baudrate
//   -k, --key arg                sets the secret key for AES128 encryption, must be 16 characters long
//   -p, --tx-power arg           sets the transmitter power output level in dBm (5..23, default 13)
//   -s, --spreading-factor arg   sets the radio spreading factor (6..12, default 7)
//   -w, --bandwidth arg          sets the radio signal bandwidth in Hz (62500, 125000, 250000, 500000, default 125000)
//   -r, --coding-rate arg        sets the coding rate to 4/5, 4/6, 4/7 or 4/8 (denominator 5..8, default 5)
#include <Piduino.h>  // All the magic is here ;-)
#include <SPI.h>
#include <RH_RF95.h>
#include <RHPcf8574Pin.h>
#include <RHGpioPin.h>
#include <RHEncryptedDriver.h>
#include <AES.h>

// ---------------------------
// --cs-pin and --dio0-pin options must be set
// The CS pin and the DIO0 pin must be set with the Arduino pin number
// The CS pin is used to select the RF95 module
// The DIO0 pin is used to detect the end of the transmission
// Use the following command to find the pin number:
// $ pido readall

// NanoPi /dev/spidev0.0
// csPin   = 10
// dio0Pin = 0

// LoRasSpi /dev/spidev0.0
// csPin   = 10
// dio0Pin = 6

float frequency = 868.0;
// ---------------------------
// End of configuration

RH_RF95 *rf95 = nullptr;  //  Pointer on the RF95 driver
RHEncryptedDriver *encryptDrv = nullptr;  //  Driver which encrypts the data
RHGenericDriver *driver = nullptr; //  Generic driver which can be RF95 or encrypted
AES128 cipher;                               // cipher AES128

// We use a Piduino serial port, because the Arduino serial port introduces
// delays that do not allow to respect the Modbus RTU delays
Piduino::SerialPort serial;

// Led controler on NanoPi4DinBox
// cf https://github.com/epsilonrt/poo-toolbox
Pcf8574 pcf8574;

//  Tx and Rx led which are used to indicate the transmission and reception of data
RHPin *led = nullptr; // Pointer on the led Tx (GPIO pin or PCF8574)

unsigned long charInterval; // maximum time  between 2 characters (1.5c)
unsigned long frameInterval; // minimum time between 2 frames (3.5c)
unsigned long t0; // time of the last request
uint8_t txlen; // length of the string from the serial port
bool isEncrypted = false;
bool isQuiet = false; // if true, no output on the console

using namespace std;

// Affiche un message modbus sur la console en Hexa
// msg:  buffer contenant le message
// len:  taille du message
// req:  true les octets sont encadrés par [], false par <>
void printModbusMessage (const uint8_t *msg, uint8_t len, bool req = true);

// Calcule le CRC d'une trame
word calcCrc (byte address, byte *pduFrame, byte pduLen);

void setup() {

  // Setting up command line options and parameters, cf
  // https://github.com/epsilonrt/popl/blob/master/example/popl_example.cpp
  Piduino::OptionParser &op = CmdLine;
  auto help_option = op.get_option<Piduino::Switch> ('h'); // declared in Piduino.h
  auto verbose_option = op.get_option<Piduino::Switch> ('v'); // declared in Piduino.h

  auto quiet_option = op.add<Piduino::Switch> ("q", "quiet", "be quiet, if set, there is no output on the console");
  auto cspin_option = op.add<Piduino::Value<int>> ("c", "cs-pin",     "sets the chip select pin number, must be set ! > use `pido readall` to find the pin number");
  auto dio0pin_option = op.add<Piduino::Value<int>> ("d", "dio0-pin", "sets the dio0 pin number, must be set !        > use `pido readall` to find the pin number");
  auto led_option = op.add<Piduino::Value<int>> ("l", "led-pin",      "sets the Rx/Tx led pin number (optionnal)      > use `pido readall` to find the pin number");
  auto wirebus_option = op.add<Piduino::Value<int>> ("y", "wire-bus", "sets the wire bus where a PCF8574 wich Rx/Tx led is connected");
  auto baudrate_option = op.add<Piduino::Value<unsigned long>> ("b", "baudrate", "sets serial baudrate", 38400);
  auto key_option = op.add<Piduino::Value<std::string>> ("k", "key", "sets the secret key for AES128 encryption, must be 16 characters long");
  auto txpower_option = op.add<Piduino::Value<int>> ("p", "tx-power", "sets the transmitter power output level in dBm (5..23, default 13)");
  auto spfactor_option = op.add<Piduino::Value<int>> ("s", "spreading-factor", "sets the radio spreading factor (6..12, default 7)");
  auto bw_option = op.add<Piduino::Value<int>> ("w", "bandwidth", "sets the radio signal bandwidth in Hz (62500, 125000, 250000, 500000, default 125000)");
  auto codrate_option = op.add<Piduino::Value<int>> ("r", "coding-rate", "sets the coding rate to 4/5, 4/6, 4/7 or 4/8 (denominator 5..8, default 5)");
  op.parse (argc, argv);

  if (help_option->is_set()) {

    std::cout << Piduino::System::progName() << " [OPTION]... serial_port" << endl;
    std::cout << "  serial_port:\tserial port path, eg /dev/ttyUSB0, /dev/tnt1..." << endl;
    std::cout << op << endl;
    exit (EXIT_SUCCESS);
  }

    if (op.non_option_args().size() < 1) {

    cerr << "A serial port must be specified!" << endl << op << endl;
    exit (EXIT_FAILURE);
  }

  if (! cspin_option->is_set() || ! dio0pin_option->is_set()) {
    cerr << "The --cs-pin and --dio0-pin options must be set!" << endl << op << endl;
    exit (EXIT_FAILURE);
  }


  isQuiet = quiet_option->is_set();
  if (isQuiet) {

    verbose_option->set_value (false);
  }

  int csPin = cspin_option->value();
  int dio0Pin = dio0pin_option->value();

  // TODO: detect the default spidev device with Piduino::System::findSpidev()
  rf95 = new RH_RF95 (csPin, dio0Pin); // Pointeur sur le driver RF95

  if (key_option->is_set()) {
    string  key = key_option->value();

    if (cipher.setKey (reinterpret_cast<const uint8_t *> (key.data()), key.size())) {

      encryptDrv = new RHEncryptedDriver (*rf95, cipher); // Driver qui assemble chiffreur et transmetteur RF
      if (!isQuiet) {
        std::cout <<  "Encryption enabled" << endl;
      }
      driver = encryptDrv;
      isEncrypted = true;
    }
    else {

      cerr << "Unable to set secret key !" << endl << op << endl;
      exit (EXIT_FAILURE);
    }
  }
  else {

    driver = rf95;
  }

  if (led_option->is_set()) {
    // process the --led option
    // if --wirebus is not set, we use GPIO pins

    if (wirebus_option->is_set()) {

      int wireBus = wirebus_option->value();

      // Open the I2C bus where the PCF8574 is connected
      Wire.begin (wireBus);
      if (! pcf8574.begin()) {

        cerr << "Unable to connect to PCF8574. Please check the wiring, slave address, and bus ID!" << endl;
        exit (EXIT_FAILURE);
      }
      if (led_option->is_set()) {
        int ledPin = led_option->value();

        led = new RHPcf8574Pin (ledPin, pcf8574); // Led Tx
        if (verbose_option->is_set()) {
          std::cout << Piduino::System::progName() << ": " << "Use PCF8574 on bus " << wireBus << " for Rx/Tx led " << ledPin << endl;
        }
        rf95->setTxLed (*led);
        rf95->setRxLed (*led);
      }
    }
    else {

      if (led_option->is_set()) {
        int ledPin = led_option->value();

        led = new RHGpioPin (ledPin); // Led Tx
        if (verbose_option->is_set()) {
          std::cout << Piduino::System::progName() << ": " << "Use GPIO pin " << ledPin << " for Rx/Tx led" << endl;
        }
        rf95->setTxLed (*led);
        rf95->setRxLed (*led);
      }
    }
  }

  string portName = op.non_option_args() [0];
  unsigned long baudrate = baudrate_option->value();
  // end of command line options

  //  Open the serial port
  serial.setPortName (portName);
  serial.setParity (Piduino::SerialPort::EvenParity);
  serial.setBaudRate (baudrate);

  if (!serial.open (Piduino::IoDevice::ReadWrite | Piduino::IoDevice::Binary)) {

    cerr << "Unable to open " <<  portName << endl;
    exit (EXIT_FAILURE);
  }

  // RTU Modbus timing, the silence between two frames must be at least 3.5T
  // and the time between two characters must be less than 1.5T
  if (baudrate > 19200UL) {
    // if baudrate > 19200, we use a delay of 750us between two characters
    // and 1750us between two frames
    charInterval = 750;
    frameInterval = 1750;
  }
  else {

    charInterval = 16500000UL / baudrate; // 1T * 1.5 = T1.5, 1T = 11 bits
    frameInterval = 38500000UL / baudrate; // 1T * 3.5 = T3.5, 1T = 11 bits
  }
  if (!isQuiet) {
    std::cout << Piduino::System::progName() << ": " << portName << ", " << baudrate << " bd, " << frameInterval << "us" << endl;
  }

  if (verbose_option->is_set()) {

    const Piduino::SpiDev::Info &spiInfo = Piduino::SpiDev::Info::defaultBus();
    std::cout << Piduino::System::progName() << ": " << "Connecting the RFM95 to the SPI bus " << spiInfo.path() << endl;
  }

  // Defaults after init are 434.0MHz, 13dBm,
  // Bw = 125 kHz, Cr = 5 (4/5), Sf = 7 (128chips/symbol), CRC on

  if (!rf95->init()) {
    cerr << "RF95 init failed !" << endl;
    exit (EXIT_FAILURE);
  }

  // Setup ISM frequency
  rf95->setFrequency (frequency);

  if (txpower_option->is_set()) {
    int txPower = txpower_option->value();

    if (txPower < 5 || txPower > 23) {
      cerr << "Invalid Tx power, must be between 5 and 23 dBm" << endl;
      exit (EXIT_FAILURE);
    }
    rf95->setTxPower (txPower);
  }

  if (spfactor_option->is_set()) {
    int spFactor = spfactor_option->value();

    if (spFactor < 6 || spFactor > 12) {
      cerr << "Invalid Spreading factor, must be between 6 and 12" << endl;
      exit (EXIT_FAILURE);
    }
    rf95->setSpreadingFactor (spFactor);
  }

  if (bw_option->is_set()) {
    int sbw = bw_option->value();

    if (sbw < 62500 || sbw > 500000) {
      cerr << "Invalid Bandwidth, must be between 62500 and 500000 Hz" << endl;
      exit (EXIT_FAILURE);
    }
    rf95->setSignalBandwidth (sbw);
  }

  if (codrate_option->is_set()) {
    int cdrate = codrate_option->value();

    if (cdrate < 5 || cdrate > 8) {
      cerr << "Invalid Coding rate, must be between 5 and 8" << endl;
      exit (EXIT_FAILURE);
    }
    rf95->setCodingRate4 (cdrate);
  }

  // rf95->printRegisters (Console);
  if (!isQuiet) {
    std::cout << "Waiting for incoming messages...." << endl;
  }
  if (isDaemon) {

    Piduino::Syslog.open();
  }

}

// Dont put this on the stack:
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];

void loop() {

  txlen = 0;

  while (serial.available() > txlen) {

    txlen = serial.available();
    delayMicroseconds (charInterval);
  }

  if (txlen > 0) {

    if (serial.read (reinterpret_cast<char *> (&txbuf[0]), txlen) >= 4) {

      // Last two bytes = crc
      word crc = ( (txbuf[txlen - 2] << 8) | txbuf[txlen - 1]);

      // CRC Check
      if (crc == calcCrc (txbuf[0], txbuf + 1, txlen - 3)) {

        t0 = micros(); // we save the time of the last message for calculating the delay between the request and the response
        driver->send (txbuf, txlen);
      }
      else {

        cerr << "CRC Error ! > ";
      }
    }
    else {

      // message trop court
      if (!isQuiet) {
        std::cout << Piduino::System::progName() << ": " << "Message flushed ! > ";
      }
    }
    // On affiche le message et on rétablit le compteur txlen
    if (!isQuiet) {
      printModbusMessage (txbuf, txlen);
    }
    txlen = 0;
  }

  if (driver->available()) {
    // On a reçu une trame
    uint8_t rxlen = sizeof (rxbuf);
    // Should be a message for us now
    if (driver->recv ( (uint8_t *) rxbuf, &rxlen)) {

      if (rxlen >= 4) {
        // le message est suffisament long, on l'envoie sur la liaisons série
        serial.write (reinterpret_cast<char *> (&rxbuf[0]), rxlen);
        unsigned long dt = micros() - t0;
        serial.flush(); // on vide le buffer interne pour forcer l'envoi
        // On affiche le message reçu et le temps entre émission et réception
        if (!isQuiet) {
          printModbusMessage (rxbuf, rxlen, false);
          std::cout << "Reply time: " << dt / 1000UL << "ms" << endl;
        }
      }
    }
  }
}

// Print modbus message on console in Hexa
void printModbusMessage (const uint8_t *msg, uint8_t len, bool req) {

  if (len) {
    char str[3]; // buffer for the hexadecimal representation of the byte
    for (uint8_t i = 0; i < len; i++) {

      sprintf (str, "%02X", msg[i]); // str store the string with the hexadecimal representation of the byte
      std::cout << (req ? '[' : '<') << str << (req ? ']' : '>');
    }
    std::cout <<  endl;
  }
}

/* Table of CRC values for highorder byte */
const byte _auchCRCHi[] = {
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
  0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
  0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
  0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
  0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
  0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
  0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
  0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
  0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
  0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
  0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
  0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
  0x40
};

/* Table of CRC values for loworder byte */
const byte _auchCRCLo[] = {
  0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
  0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
  0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
  0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
  0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
  0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
  0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
  0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
  0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
  0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
  0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
  0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
  0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
  0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
  0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
  0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
  0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
  0x40
};

// return the CRC of the message
// address: 1st byte of the message
// pduFrame: pointer to the message
// pduLen: length of the message
// CRC is calculated on the address and the PDU frame
// CRC is 2 bytes long, high byte first
word calcCrc (byte address, byte *pduFrame, byte pduLen) {
  byte CRCHi = 0xFF, CRCLo = 0x0FF, Index;

  Index = CRCHi ^ address;
  CRCHi = CRCLo ^ _auchCRCHi[Index];
  CRCLo = _auchCRCLo[Index];

  while (pduLen--) {
    Index = CRCHi ^ *pduFrame++;
    CRCHi = CRCLo ^ _auchCRCHi[Index];
    CRCLo = _auchCRCLo[Index];
  }

  return (CRCHi << 8) | CRCLo;
}
