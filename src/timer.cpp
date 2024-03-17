/*
  MitsuQTT Copyright (c) 2024 floatplane

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU
  Lesser General Public License as published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version. This library is distributed in the hope that
  it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License along with this library;
  if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02110-1301 USA
*/

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