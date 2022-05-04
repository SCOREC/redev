#pragma once
#include <cassert>

#define REDEV_ALWAYS_ASSERT(cond) do {          \
  if (! (cond)) {                               \
    char omsg[2048];                            \
    sprintf(omsg, "%s failed at %s + %d \n",    \
      #cond, __FILE__, __LINE__);               \
    redev::Redev_Assert_Fail(omsg);             \
  }                                             \
} while (0)


namespace redev {
//from scorec/core/pcu_fail.h
void Redev_Assert_Fail(const char* msg) __attribute__ ((noreturn));
}

