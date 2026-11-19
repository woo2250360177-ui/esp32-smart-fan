#pragma once
#include <Arduino.h>

namespace CloudBridge {
struct Command {
  int code;
  uint32_t receivedAt, generation;
};
struct State {
  float temperature;
  int rpm, pwmPercent, mode, level, alarms;
  bool sensorValid;
};

void begin();
bool take(Command& command);
bool current(const Command& command);
void publish(const State& state);
}
