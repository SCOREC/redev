#pragma once
#include <adios2.h>
#include "redev_types.h"
#include <mpi.h>

namespace redev {

class Partition {
  public:
    virtual redev::LO GetRank(redev::Real coords[3]) = 0;
    virtual void Write(adios2::Engine& eng, adios2::IO& io) = 0;
    virtual void Read(adios2::Engine& eng, adios2::IO& io) = 0;
    virtual void Broadcast(MPI_Comm comm, int root=0) = 0;
};

class RCBPtn : public Partition {
  public:
    RCBPtn();
    RCBPtn(std::vector<int>& ranks, std::vector<double>& cuts);
    redev::LO GetRank(redev::Real coords[3]);
    void Write(adios2::Engine& eng, adios2::IO& io);
    void Read(adios2::Engine& eng, adios2::IO& io);
    void Broadcast(MPI_Comm comm, int root);
    std::vector<redev::LO>& GetRanks();
    std::vector<redev::Real>& GetCuts();
  private:
    const std::string ranksVarName = "rcb partition ranks";
    const std::string cutsVarName = "rcb partition cuts";
    std::vector<redev::LO> ranks;
    std::vector<redev::Real> cuts;
};

class Redev {
  public:
    Redev(MPI_Comm comm, Partition& ptn, bool isRendezvous=false);
    void Setup();
  private:
    bool CheckVersion();
    bool isRendezvous; // true: the rendezvous application, false: otherwise
    MPI_Comm comm;
    adios2::ADIOS adios;
    adios2::IO io;
    int rank;
    Partition& ptn;
};

}
