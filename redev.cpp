#include <redev.h>
#include <cassert>

namespace redev {
  Redev::Redev(MPI_Comm comm) : adios(comm) {
    int isInitialized = 0;
    MPI_Initialized(&isInitialized);
    assert(isInitialized);
  }
}
