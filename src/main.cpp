// rf95_modbus_bridge
// Pont Série <-> LoRa avec un RFM95
//
// rf95_modbus_bridge -h
//
//  rf95_modbus_bridge [OPTION]... serial_port
//          serial_port:    serial port path, eg /dev/ttyUSB0, /dev/tnt1...
//  Allowed options:
//    -h, --help                   produce help message
//    -v, --verbose                be verbose
//    -D, --daemon                 be daemonized
//    -b, --baudrate arg (=38400)  set serial baudrate
//
// This example code is in the public domain.
#include <Piduino.h>  // All the magic is here ;-)
#include <SPI.h>
#include <RH_RF95.h>
#include <RHPcf8574Pin.h>
#include <RHEncryptedDriver.h>
#include <AES.h>

// Modifier les valeurs ci-dessous en fonction de la carte utilisée
// ---------------------------
const int LedPin = 0;
const int WireBus = 2;
const uint8_t CsPin = 10;
const uint8_t Dio0Pin = 0;

const float frequency = 868.0;
// ---------------------------
// End of configuration

// Singleton instance of the radio driver
RH_RF95 rf95 (CsPin, Dio0Pin);
AES128 cipher;                               // Chiffreur AES128 (lib Crypto)
RHEncryptedDriver encryptDrv (rf95, cipher); // Driver qui assemble chiffreur et transmetteur RF
RHGenericDriver *driver; // Pointeur qui permettra de manipuler de la même façon, driver rf95 ou encrypt

// On utilise un port série Piduino, car le port série Arduino introduit des
// délais qui ne permettent pas de respecter les délais Modbus RTU
Piduino::SerialPort serial;

// Contrôleur des leds NanoPi4DinBox
// cf https://github.com/epsilonrt/poo-toolbox
Pcf8574 pcf8574;
RHPcf8574Pin led (LedPin, pcf8574); // Led Rx/Tx

unsigned long charInterval; // temps pour transmettre un octect en microsecondes (1.5c)
unsigned long frameInterval; // temps qui doit séparer 2 trames modbus (3.5c)
unsigned long t0; // temps précédent en microsecondes
uint8_t txlen; // nombre de caractères reçus de la liaison série
bool isEncrypted = false;

using namespace std;

// Affiche un message modbus sur la console en Hexa
// msg:  buffer contenant le message
// len:  taille du message
// req:  true les octets sont encadrés par [], false par <>
void printModbusMessage (const uint8_t * msg, uint8_t len, bool req = true);

// Calcule le CRC d'une trame
word calcCrc (byte address, byte* pduFrame, byte pduLen);

void setup() {

  // Configure les options et paramètres passés par la ligne de commande
  // https://github.com/epsilonrt/popl/blob/master/example/popl_example.cpp
  Piduino::OptionParser & op = CmdLine;
  auto help_option = op.get_option<Piduino::Switch> ('h');

  auto verbose_option = op.get_option<Piduino::Switch> ('v');
  auto baudrate_option = op.add<Piduino::Value<unsigned long>> ("b", "baudrate", "set serial baudrate", 38400);
  auto key_option = op.add<Piduino::Value<std::string>> ("k", "key", "secret key for AES128 encryption, must be 16 characters long");
  op.parse (argc, argv);


  if (help_option->is_set()) {

    cout << Piduino::System::progName() << " [OPTION]... serial_port" << endl;
    cout << "  serial_port:\tserial port path, eg /dev/ttyUSB0, /dev/tnt1..." << endl;
    cout << op << endl;
    exit (EXIT_SUCCESS);
  }


  if (op.non_option_args().size() < 1) {

    cerr << "Serial port must be provided !" << endl << op << endl;
    exit (EXIT_FAILURE);
  }

  if (key_option->is_set()) {
    string  key = key_option->value();

    if (cipher.setKey (reinterpret_cast<const uint8_t *> (key.data()), key.size())) {

      cout << "Encryption enabled" << endl;
      driver = & encryptDrv;
      isEncrypted = true;
    }
    else {

      cerr << "Unable to set secret key !" << endl << op << endl;
      exit (EXIT_FAILURE);
    }
  }
  else {

    driver = & rf95;
  }

  string portName = op.non_option_args() [0];
  unsigned long baudrate = baudrate_option->value();
  // Fin de traitement de la ligne de commande

  // Configuration du port série
  serial.setPortName (portName);
  serial.setParity (Piduino::SerialPort::EvenParity);
  serial.setBaudRate (baudrate);

  if (!serial.open (Piduino::IoDevice::ReadWrite | Piduino::IoDevice::Binary)) {

    cerr << "Unable to open " <<  portName << endl;
    exit (EXIT_FAILURE);
  }

  // Calcul des temps pour la trame
  if (baudrate > 19200UL) {
    
    charInterval = 750;
    frameInterval = 1750;
  }
  else {
    
    charInterval = 16500000UL / baudrate; // 1T * 1.5 = T1.5, 1T = 11 bits
    frameInterval = 38500000UL / baudrate; // 1T * 3.5 = T3.5, 1T = 11 bits
  }

  cout << Piduino::System::progName() << ": " << portName << ", " << baudrate << " bd, " << frameInterval << "us" << endl;

  // Ouverture du bus I2c où se trouve le PCF8574 qui commande les leds
  Wire.begin (WireBus);
  if (! pcf8574.begin()) {

    cerr << "Unable to connect to PCF8574, check wiring, slave address and bus id !" << endl;
    exit (EXIT_FAILURE);
  }

  rf95.setTxLed (led);
  rf95.setRxLed (led);

  // Defaults after init are 434.0MHz, 13dBm,
  // Bw = 125 kHz, Cr = 5 (4/5), Sf = 7 (128chips/symbol), CRC on
  if (!rf95.init()) {
    cerr << "RF95 init failed !" << endl;
    exit (EXIT_FAILURE);
  }

  // Setup ISM frequency
  rf95.setFrequency (frequency);


  // rf95.printRegisters (Console);
  cout << "Waiting for incoming messages...." << endl;
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

        t0 = micros(); // on mémorise le temps pour le calcul du temps de réponse
        driver->send (txbuf, txlen);
      }
      else {

        cerr << "CRC Error ! > ";
      }
    }
    else {

      // message trop court
      cout << "Message flushed ! > ";
    }
    // On affiche le message et on rétablit le compteur txlen
    printModbusMessage (txbuf, txlen);
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
        printModbusMessage (rxbuf, rxlen, false);
        cout << "Reply time: " << dt / 1000UL << "ms" << endl;
      }
    }
  }
}

// Affiche un message modbus sur la console en Hexa
void printModbusMessage (const uint8_t * msg, uint8_t len, bool req) {

  if (len) {
    char str[3]; // buffer pour sprintf
    for (uint8_t i = 0; i < len; i++) {

      sprintf (str, "%02X", msg[i]); // str contient la chaîne de caractères de la représentation hexa de l'octet
      cout << (req ? '[' : '<') << str << (req ? ']' : '>');
    }
    cout <<  endl;
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

word calcCrc (byte address, byte* pduFrame, byte pduLen) {
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
