#pragma once 
#include <adios2.h>
#include <mpi.h>

namespace redev {

class Redev {
  public:
    Redev(MPI_Comm comm);
  private:
    adios2::ADIOS adios;
};

}
