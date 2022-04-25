#include "redev_assert.h"
#include <cstdio>
#include <cstdlib>
namespace redev {
void Redev_Assert_Fail(const char* msg) {
  fprintf(stderr, "%s", msg);
  abort();
}
}
