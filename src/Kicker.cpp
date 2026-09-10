// By: github.com/dimasragaa — IG: @dmsragaa
#include "Kicker.h"
#include "Config.h"
#include "State.h"

void kickerSetup(){
  pinMode(KICK_PIN, OUTPUT);
  digitalWrite(KICK_PIN, LOW);
}

void handleKick(bool trigger){
  if (trigger && !kickActive){
    kickActive = true; kickStart = millis(); digitalWrite(KICK_PIN, HIGH);
  }
  if (kickActive && (millis() - kickStart >= KICK_MS)){
    kickActive = false; digitalWrite(KICK_PIN, LOW);
  }
}
