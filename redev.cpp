#include <redev.h>
#include <cassert>
#include "redev_git_version.h"

namespace {
  void begin_func() {
  }
  void end_func() {
  }
}

namespace redev {
  Redev::Redev(MPI_Comm comm) : adios(comm) {
    begin_func();
    std::cout << "Redev Git Hash: " << redevGitHash << "\n";
    int isInitialized = 0;
    MPI_Initialized(&isInitialized);
    assert(isInitialized);
    end_func();
  }
}
