#include <Arduino.h>

void setup() {
    Serial.begin(115200);
}

void loop() {
    Serial.println("ESP32 smart fan: serial OK");
    delay(1000);
}
