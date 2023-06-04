// rf95_modbus_slave_tester
//
// <WARNING>
// You must uncomment #define RH_ENABLE_ENCRYPTION_MODULE at the bottom of RadioHead.h
// But ensure you have installed the Crypto directory from arduinolibs first !
//
// Réponds à la trame modbus transmise par LoRa :
//    [14][01][00][00][03][7E][CE]
// par la réponse modbus :
//    <14><01><01><00><55><84>
//
// Cela correspond à la lecture de 3 coils {1,2,3} de l'esclave à l'adresse 20
// Voilà la commande mbpoll correspondant :
//
// mbpoll -m rtu -b38400 -a20 -t0 -r1 -c3 -v /dev/tnt1
// -- Polling slave 20... Ctrl-C to stop)
// [14][01][00][00][00][03][7E][CE]
// Waiting for a confirmation...
// <14><01><01><00><55><84>
// [1]:    0
// [2]:    0
// [3]:    0
//
// This example code is in the public domain.
#include <SPI.h>
#include <RH_RF95.h>
#include <RHGpioPin.h>
#include <RHEncryptedDriver.h>
#include <AES.h>

// Configuration
// ---------------------------
// Defines the serial port as the console on the Arduino platform
#define Console Serial
const int LedPin = LED_BUILTIN;
const float frequency = 868.0;
// Paquet requête Modbus qui doit être reçu
const uint8_t req[] = { 0x14, 0x01, 0x00, 0x00, 0x00, 0x03, 0x7E, 0xCE };
// Paquet de réponse Modbus à renvoyer
const uint8_t resp[] = { 0x14, 0x01, 0x01, 0x00, 0x55, 0x84 };

// Activation du cryptage, mettre true ci-dessous pour activer
const bool isEncrypted = true;

// Clé de cryptage, la même clé doit être configurée sur tous les éléments comuniquants
const uint8_t encryptKey[16] = { '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'a', 'b', 'c', 'd', 'e', 'f' };

// ---------------------------
// End of configuration

// Singleton instance of the radio driver
RH_RF95 rf95;
AES128 cipher;                               // Chiffreur AES128 (lib Crypto)
RHEncryptedDriver encryptDrv (rf95, cipher); // Driver qui assemble chiffreur et transmetteur RF
RHGenericDriver *driver; // Pointeur qui permettra de manipuler de la même façon, driver rf95 ou encrypt

RHGpioPin led (LedPin); // Led qui s'allume à la réception et à l'emission LoRa

// Affiche un message modbus sur la console en Hexa
// msg:  buffer contenant le message
// len:  taille du message
// req:  true les octets sont encadrés par [], false par <>
void printModbusMessage (const uint8_t *msg, uint8_t len, bool req = true);

void setup() {
  Console.begin (115200);
  Console.println ("rf95_modbus_slave_tester");

  // Defaults after init are 434.0MHz, 13dBm,
  // Bw = 125 kHz, Cr = 5 (4/5), Sf = 7 (128chips/symbol), CRC on
  if (!rf95.init()) {
    Console.println ("init failed");
    exit (EXIT_FAILURE);
  }

  rf95.setTxLed (led);
  rf95.setRxLed (led);

  // Setup ISM frequency
  rf95.setFrequency (frequency);

  if (isEncrypted) {

    if (cipher.setKey (encryptKey, sizeof (encryptKey))) {
      Console.println ("Encryption enabled");
      driver = &encryptDrv;
    }
    else {
      Console.println ("Unable to set secret key !");
      exit (EXIT_FAILURE);
    }
  }
  else {

    driver = &rf95;
  }

  // rf95.printRegisters (Console);
  Console.println ("Waiting for incoming messages....");
}

// Dont put this on the stack:
char buf[RH_RF95_MAX_MESSAGE_LEN];

void loop() {

  // Wait for a packet
  if (driver->waitAvailableTimeout (3000)) {
    uint8_t len = sizeof (buf);
    // Should be a message for us now
    if (driver->recv ( (uint8_t *) buf, &len)) {

      printModbusMessage (buf, len);
      if (buf[0] == req[0]) {  // si l'adresse esclave correspond

        // Send a reply
        driver->send (resp, sizeof (resp));
        driver->waitPacketSent();
        printModbusMessage (resp, sizeof (resp), false);
      }
    }
  }
}

void printModbusMessage (const uint8_t *m, uint8_t len, bool req) {

  if (len) {
    char str[3];  // buffer pour sprintf
    for (uint8_t i = 0; i < len; i++) {

      sprintf (str, "%02X", m[i]); // str contient la chaîne de caractères de la représentation hexa de l'octet
      Console.write (req ? '[' : '<');
      Console.print (str);
      Console.write (req ? ']' : '>');
    }
    Console.println ("");
  }
}
