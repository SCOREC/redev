#include "redev_time.h"

namespace redev {
  TimeType getTime() {
    return std::chrono::steady_clock::now();
  }
}
