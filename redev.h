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
    /* The non-leaf levels of the partition tree have alternating cut dimensions associated
     * with the level starting with 'x'.
     * Each non-leaf node has a coordinate for the position of the cut along the dimension
     * specified by its level.
     * Non-leaf tree node n with cut dimension d and cut coordinate v along dimension d has
     * a left child that covers the sub-domain d < v and a right child that
     * covers the sub-domain d >= v.
     * ranks: labels the leaf nodes in the partition tree from left to right
     * cuts: Specifies the coordinates associated with the non-leaf nodes
     * in a breath-first traversal order starting at the root and
     * visiting the child nodes at each level from left to right.
     * The root of the cut tree is stored at index 1.
     */
    RCBPtn(redev::LO dim, std::vector<int>& ranks, std::vector<double>& cuts);
    redev::LO GetRank(redev::Real coords[3]);
    void Write(adios2::Engine& eng, adios2::IO& io);
    void Read(adios2::Engine& eng, adios2::IO& io);
    void Broadcast(MPI_Comm comm, int root);
    std::vector<redev::LO>& GetRanks();
    std::vector<redev::Real>& GetCuts();
  private:
    const std::string ranksVarName = "rcb partition ranks";
    const std::string cutsVarName = "rcb partition cuts";
    redev::LO dim;
    std::vector<redev::LO> ranks;
    std::vector<redev::Real> cuts;
};

class Redev {
  public:
    Redev(MPI_Comm comm, Partition& ptn, bool isRendezvous=false);
    void Setup();
  private:
    void CheckVersion(adios2::Engine& eng, adios2::IO& io);
    bool isRendezvous; // true: the rendezvous application, false: otherwise
    MPI_Comm comm;
    adios2::ADIOS adios;
    adios2::Engine eng;
    adios2::IO io;
    int rank;
    Partition& ptn;
};

}
