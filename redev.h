#pragma once
#include <adios2.h>
#include <mpi.h>
#include <cassert>
#include "redev_types.h"
#include <array>          // std::array

#define REDEV_ALWAYS_ASSERT(cond) do {          \
  if (! (cond)) {                               \
    char omsg[2048];                            \
    sprintf(omsg, "%s failed at %s + %d \n",    \
      #cond, __FILE__, __LINE__);               \
    redev::Redev_Assert_Fail(omsg);             \
  }                                             \
} while (0)

namespace redev {

//from scorec/core/pcu_fail.h
void Redev_Assert_Fail(const char* msg) __attribute__ ((noreturn));

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
    ClassPtn(const redev::LOs& ranks, const ModelEntVec& ents);
    redev::LO GetRank(ModelEnt ent) const;
    void Write(adios2::Engine& eng, adios2::IO& io);
    void Read(adios2::Engine& eng, adios2::IO& io);
    void Broadcast(MPI_Comm comm, int root=0);
    void Gather(MPI_Comm comm, int root=0);
    redev::LOs GetRanks() const;
    ModelEntVec GetModelEnts() const;
  private:
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
    Redev(MPI_Comm comm, Partition& ptn, bool isRendezvous=false, bool noParticipant=false);
    ~Redev();
    adios2::Engine& getToEngine() { return toEng; }
    adios2::Engine& getFromEngine() { return fromEng; }
    adios2::IO& getToIO() { return toIo; }
    adios2::IO& getFromIO() { return fromIo; }
  private:
    void Setup();
    void CheckVersion(adios2::Engine& eng, adios2::IO& io);
    bool isRendezvous; // true: the rendezvous application, false: otherwise
    void openEnginesBP4(bool noParticipant);
    void openEnginesSST(bool noParticipant);
    const char* bpFromName = "fromRendezvous.bp";
    const char* bpToName = "toRendezvous.bp";
    MPI_Comm comm;
    adios2::ADIOS adios;
    adios2::Engine fromEng;
    adios2::Engine toEng;
    adios2::IO fromIo;
    adios2::IO toIo;
    int rank;
    Partition& ptn;
};

}
