#pragma once
#include <adios2.h>
#include <mpi.h>

namespace redev {

class Redev {
  public:
    Redev(MPI_Comm comm, bool isRendezvous=false);
    void Setup(std::vector<int>& ranks, std::vector<double>& cuts);
  private:
    bool isRendezvous;
    adios2::ADIOS adios;
    adios2::IO io;
    int rank;
};

}
