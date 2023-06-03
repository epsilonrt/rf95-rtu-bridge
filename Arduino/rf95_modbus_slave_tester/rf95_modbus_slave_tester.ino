// rf95_modbus_slave_tester
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

// Configuration
// ---------------------------
// Defines the serial port as the console on the Arduino platform
#define Console Serial
const int LedPin = LED_BUILTIN;
const float frequency = 868.0;
const uint8_t req[] = { 0x14, 0x01, 0x00, 0x00, 0x00, 0x03, 0x7E, 0xCE };
const uint8_t resp[] = { 0x14, 0x01, 0x01, 0x00, 0x55, 0x84 };

// ---------------------------
// End of configuration

// Singleton instance of the radio driver
RH_RF95 rf95;

RHGpioPin led(LedPin);  // Led qui s'allume à la réception et à l'emission LoRa

// Affiche un message modbus sur la console en Hexa
// msg:  buffer contenant le message
// len:  taille du message
// req:  true les octets sont encadrés par [], false par <>
void printModbusMessage(const uint8_t * msg, uint8_t len, bool req = true);

void setup() {
  Console.begin(115200);
  Console.println("rf95_server");

  // Defaults after init are 434.0MHz, 13dBm,
  // Bw = 125 kHz, Cr = 5 (4/5), Sf = 7 (128chips/symbol), CRC on
  if (!rf95.init()) {
    Console.println("init failed");
    exit(EXIT_FAILURE);
  }

  rf95.setTxLed(led);
  rf95.setRxLed(led);

  // Setup ISM frequency
  rf95.setFrequency(frequency);

  // rf95.printRegisters (Console);
  Console.println("Waiting for incoming messages....");
}

// Dont put this on the stack:
char buf[RH_RF95_MAX_MESSAGE_LEN];

void loop() {

  // Wait for a packet
  if (rf95.waitAvailableTimeout(3000)) {
    uint8_t len = sizeof(buf);
    // Should be a message for us now
    if (rf95.recv((uint8_t *)buf, &len)) {

      printModbusMessage(buf, len);
      if (buf[0] == req[0]) { // si l'adresse esclave correspond

        // Send a reply
        rf95.send(resp, sizeof(resp));
        rf95.waitPacketSent();
        printModbusMessage(resp, sizeof(resp), false);
      }
    }
  }
}

void printModbusMessage(const uint8_t *m, uint8_t len, bool req) {

  if (len) {
    char str[3]; // buffer pour sprintf
    for (uint8_t i = 0; i < len; i++) {

      sprintf(str, "%02X", m[i]); // str contient la chaîne de caractères de la représentation hexa de l'octet
      Console.write(req ? '[' : '<');
      Console.print(str);
      Console.write(req ? ']' : '>');
    }
    Console.println("");
  }
}

