#include <redev.h>
#include <cassert>
#include "git_version.h"

namespace {
  void begin_func() {
  }
  void end_func() {
  }
}

namespace redev {
  Redev::Redev(MPI_Comm comm) : adios(comm) {
    begin_func();
    std::cout << "Redev Git Hash: " << kGitHash << "\n";
    int isInitialized = 0;
    MPI_Initialized(&isInitialized);
    assert(isInitialized);
    end_func();
  }
}
