#pragma once
#include <adios2.h>
#include "redev_types.h"
#include <mpi.h>

namespace redev {

class Redev {
  public:
    Redev(MPI_Comm comm, bool isRendezvous=false);
    void Setup(std::vector<int>& ranks, std::vector<double>& cuts);
    template<typename T>
    void Broadcast(T* data, int count, int root);
  private:
    bool isRendezvous;
    MPI_Comm comm;
    adios2::ADIOS adios;
    adios2::IO io;
    int rank;
};

}
