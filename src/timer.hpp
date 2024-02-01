#pragma once
#include <arduino-timer-cpp17.hpp>
#include <cstdint>
#include <functional>

class ITimer {
 public:
  virtual void tick() = 0;
  virtual void in(uint32_t milliseconds, std::function<Timers::HandlerResult()> &&callback) = 0;
};

ITimer *getTimer();