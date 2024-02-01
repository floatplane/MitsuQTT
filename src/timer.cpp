#include "timer.hpp"

#include <arduino-timer-cpp17.hpp>

class TimerWrapper : public ITimer {
 public:
  TimerWrapper() {
  }
  ~TimerWrapper() {
  }

  void tick() override {
    timerset.tick();
  }

  void in(uint32_t milliseconds, Timers::Handler &&callback) override {
    timerset.in(milliseconds, std::move(callback));
  }

 private:
  Timers::TimerSet<10> timerset;
};

ITimer *getTimer() {
  static TimerWrapper timer;
  return &timer;
}