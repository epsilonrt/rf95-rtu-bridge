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

// On utilise un port série Piduino, car le port série Arduino introduit des
// délais qui ne permettent pas de respecter les délais Modbus RTU
Piduino::SerialPort serial;

// Contrôleur des leds NanoPi4DinBox
// cf https://github.com/epsilonrt/poo-toolbox
Pcf8574 pcf8574;
RHPcf8574Pin led (LedPin, pcf8574); // Led Rx/Tx

unsigned long charTime; // temps pour transmettre un octect en microsecondes (1c)
unsigned long frameInterval; // temps qui doit séparer 2 trames modbus (3.5c)
unsigned long t0; // temps précédent en microsecondes
uint8_t txlen; // nombre de caractères reçus de la liaison série

using namespace std;

// Affiche un message modbus sur la console en Hexa
// msg:  buffer contenant le message
// len:  taille du message
// req:  true les octets sont encadrés par [], false par <>
void printModbusMessage (const uint8_t * msg, uint8_t len, bool req = true);

void setup() {

  // Configure les options et paramètres passés par la ligne de commande
  // https://github.com/epsilonrt/popl/blob/master/example/popl_example.cpp
  Piduino::OptionParser & op = CmdLine;
  auto help_option = op.get_option<Piduino::Switch> ('h');

  auto verbose_option = op.get_option<Piduino::Switch> ('v');
  auto baudrate_option = op.add<Piduino::Value<unsigned long>> ("b", "baudrate", "set serial baudrate", 38400);

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
  charTime = (1000000UL * 11UL) / baudrate;
  frameInterval = (charTime * 7UL) / 2UL;

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

  t0 = micros(); // temps initial
}

// Dont put this on the stack:
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];

void loop() {
  int pktLen; // taille du paquet reçu sur liaison série

  unsigned long t1 = micros(); // temps actuel en microsecondes
  // Calcul du temps entre 2 paquets sur liaison série
  unsigned long dt = t1 - t0;
  bool isStarted = (txlen != 0); // indique si une partie de la trame a déjà été reçue

  if ( (pktLen = serial.available()) > 0) {
    // réception d'un paquet sur liaison série
    t0 = t1;
    txlen += serial.read (reinterpret_cast<char *> (&txbuf[txlen]), pktLen);
    cout << "t:" << dt << " l:" << pktLen << " n:" << isStarted << endl;
  }


  if (isStarted && (dt >= frameInterval))  {
    // si la trame a débuté et que le temps depuis la dernière transmission a
    // dépassé 3.5c
    if (txlen >= 4) {

      t0 = micros(); // on mémorise le temps pour le calcul du temps de réponse
      rf95.send (txbuf, txlen);
    }
    else {

      // message trop court
      cout << "Message flushed ! > ";
    }
    // On affiche le message et on rétablit le compteur txlen
    printModbusMessage (txbuf, txlen);
    txlen = 0;
  }

  if (rf95.available()) {
    // On a reçu une trame
    uint8_t rxlen = sizeof (rxbuf);
    // Should be a message for us now
    if (rf95.recv ( (uint8_t *) rxbuf, &rxlen)) {

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
