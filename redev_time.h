#ifndef REDEV_TIME_H
#define REDEV_TIME_H
#include <chrono> //steady_clock

namespace redev {
  using TimeType = std::chrono::steady_clock::time_point;
  TimeType getTime();
}

#endif
