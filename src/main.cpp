#include <Arduino.h>



void setup() {

  //Debug-Port
  Serial.begin(115200);
  Serial.setDebugOutput(true);  

  //Debug Infos
  Serial.printf("Current CPU Frequency: %dMHz\n", getCpuFrequencyMhz());
  
}

void loop() {
  // put your main code here, to run repeatedly:


}

