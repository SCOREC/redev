#pragma once
#include <adios2.h>
#include <mpi.h>
#include "redev_types.h"
#include "redev_assert.h"
#include "redev_comm.h"
#include <array>          // std::array

namespace redev {

class Partition {
  public:
    virtual void Write(adios2::Engine& eng, adios2::IO& io) = 0;
    virtual void Read(adios2::Engine& eng, adios2::IO& io) = 0;
    virtual void Broadcast(MPI_Comm comm, int root=0) = 0;
};

class ClassPtn : public Partition {
  public:
    typedef std::pair<redev::LO, redev::LO> ModelEnt; //dim, id
    typedef std::vector<ModelEnt> ModelEntVec;
    typedef std::map<ModelEnt,redev::LO> ModelEntToRank;
    ClassPtn();
    ClassPtn(MPI_Comm comm, const redev::LOs& ranks, const ModelEntVec& ents);
    redev::LO GetRank(ModelEnt ent) const;
    void Write(adios2::Engine& eng, adios2::IO& io);
    void Read(adios2::Engine& eng, adios2::IO& io);
    void Broadcast(MPI_Comm comm, int root=0);
    redev::LOs GetRanks() const;
    ModelEntVec GetModelEnts() const;
  private:
    void Gather(MPI_Comm comm, int root=0);
    const std::string entsAndRanksVarName = "class partition ents and ranks";
    /**
     * return a vector that writes the ModelEntToRank map as:
     * [dim_0, id_0, rank_0, dim_1, id_1, rank_1, ..., dim_n-1, id_n-1, rank_n-1]
     */
    redev::LOs SerializeModelEntsAndRanks() const;
    /**
     * Given a vector that contains:
     * [dim_0, id_0, rank_0, dim_1, id_1, rank_1, ..., dim_n-1, id_n-1, rank_n-1]
     * construct the ModelEntToRank map
     */
    ModelEntToRank DeserializeModelEntsAndRanks(const redev::LOs& serialized) const;
    /**
     * Ensure that the dimensions of the model ents is [0:3]
     */
    bool ModelEntDimsValid(const ModelEntVec& ents) const;
    ModelEntToRank modelEntToRank;
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
     * cuts: Array specifying the coordinates associated with the non-leaf nodes
     * in a breath-first traversal order starting at the root and
     * visiting the child nodes at each level from left to right.
     * The root of the cut tree is stored at index 1 and index 0 is unused.
     * See test_query.cpp for examples.
     */
    RCBPtn(redev::LO dim);
    RCBPtn(redev::LO dim, std::vector<int>& ranks, std::vector<double>& cuts);
    redev::LO GetRank(std::array<redev::Real,3>& pt) const;
    void Write(adios2::Engine& eng, adios2::IO& io);
    void Read(adios2::Engine& eng, adios2::IO& io);
    void Broadcast(MPI_Comm comm, int root=0);
    std::vector<redev::LO> GetRanks() const;
    std::vector<redev::Real> GetCuts() const;
  private:
    const std::string ranksVarName = "rcb partition ranks";
    const std::string cutsVarName = "rcb partition cuts";
    redev::LO dim;
    std::vector<redev::LO> ranks;
    std::vector<redev::Real> cuts;
};

class Redev {
  public:
    Redev(MPI_Comm comm, Partition& ptn, bool isRendezvous=false, bool noClients=false);
    template <class T>
    using CommPair = std::pair<redev::Communicator<T>, redev::Communicator<T>>;
    template<typename T> CommPair<T> CreateAdiosClient(std::string_view name);
  private:
    void Setup(adios2::IO& s2cIO, adios2::Engine& s2cEngine);
    void CheckVersion(adios2::Engine& eng, adios2::IO& io);
    bool isRendezvous; // true: the rendezvous application, false: otherwise
    bool noClients; // true: no clients will be connected, false: otherwise
    void openEnginesBP4(bool noClients,
        std::string s2cName, std::string c2sName,
        adios2::IO& s2cIO, adios2::IO& c2sIO,
        adios2::Engine& s2cEngine, adios2::Engine& c2sEngine);
    void openEnginesSST(bool noClients,
        std::string s2cName, std::string c2sName,
        adios2::IO& s2cIO, adios2::IO& c2sIO,
        adios2::Engine& s2cEngine, adios2::Engine& c2sEngine);
    redev::LO GetServerCommSize(adios2::IO& s2cIO, adios2::Engine& s2cEngine);
    redev::LO GetClientCommSize(adios2::IO& c2sIO, adios2::Engine& c2sEngine);
    MPI_Comm comm;
    adios2::ADIOS adios;
    int rank;
    Partition& ptn;
};

}
