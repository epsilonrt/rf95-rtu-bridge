#include <Arduino.h>

#include "RHPcf8574Pin.h"

RHPcf8574Pin::RHPcf8574Pin (int pinNumber, Pcf8574 & ctrl) :
  RHPin (false), m_pinNumber (pinNumber), m_ctrl (ctrl) {
}

//Effectue les opérations d'initialisation des ressources matérielles.
void RHPcf8574Pin::begin() {
  m_ctrl.pinMode (m_pinNumber, OUTPUT);
  setState (false);
}

//Lecture de l'état binaire de la led.
//true si allumée.
bool RHPcf8574Pin::state() const {
  return m_ctrl.digitalRead (m_pinNumber) == polarity();
}

//Modifie l'état allumée / éteinte
//true pour allumée.
void RHPcf8574Pin::setState (bool on) {
  m_ctrl.digitalWrite (m_pinNumber, on ^ ! polarity());
}

//Bascule de l'état binaire d'une led.
//Si elle était éteinte, elle s'allume.
//Si elle était allumée, elle s'éteint.

void RHPcf8574Pin::toggleState() {
  setState (!state());
}
