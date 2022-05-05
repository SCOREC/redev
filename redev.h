#pragma once
#include <adios2.h>
#include <mpi.h>
#include "redev_types.h"
#include "redev_assert.h"
#include "redev_comm.h"
#include "redev_strings.h"
#include <array>          // std::array

namespace redev {

/**
 * The Partition class provides an abstract interface that is used by Redev for
 * sharing partition information among the server and client processes.
 * Instances of the Partition class define an interface (i.e., GetRank(...)) to query
 * which process owns a point in the domain (RCBPtn) or a given geometric model entity
 * (ClassPtn).
 * Defining the cut tree for RCB or the assignment of ranks to geometric model entities
 * is not the purpose/responsiblity of this class or the RCBPtn and ClassPtn
 * derived classes.
 */
class Partition {
  public:
    /**
     * Write the partition to the provided ADIOS engine and io.
     * @param[in] eng an ADIOS2 Engine opened in write mode
     * @param[in] io the ADIOS2 IO object that created eng
     */
    virtual void Write(adios2::Engine& eng, adios2::IO& io) = 0;
    /**
     * Read the partition to the provided ADIOS engine/io.
     * @param[in] eng an ADIOS2 Engine opened in read mode
     * @param[in] io the ADIOS2 IO object that created eng
     */
    virtual void Read(adios2::Engine& eng, adios2::IO& io) = 0;
    /**
     * Send the partition information from the root rank to all other ranks in comm.
     * @param[in] comm MPI communicator containing the ranks that need the partition information
     * @param[in] root the source rank that sends the partition information
     */
    virtual void Broadcast(MPI_Comm comm, int root=0) = 0;
};

/**
 * The ClassPtn class supports a domain partition defined by the ownership of geometric model entities.
 * The user passes to the constructor the assignment of ranks to geometric model entities.
 * The 'Class' in the name is from the 'classification' term used to define the
 * association of mesh entities with a geometric model entity.
 * The concepts of classification are described in Section 2.2.2 of the PUMI
 * Users Guide (v2.1) https://www.scorec.rpi.edu/pumi/PUMI.pdf.
 */
class ClassPtn : public Partition {
  public:
    /**
     * Pair of integers (dimension, id) that uniquely identify a geometric model entity by their
     * dimension and id.  The id for an entity within a dimension is unique.
     */
    typedef std::pair<redev::LO, redev::LO> ModelEnt;
    /**
     * Vector of geometric model entities.
     */
    typedef std::vector<ModelEnt> ModelEntVec;
    /**
     * Map of geometric model entities to the process that owns them.
     */
    typedef std::map<ModelEnt,redev::LO> ModelEntToRank;
    ClassPtn();
    /**
     * Create a ClassPtn object from a vector of owning ranks and geometric model entities.
     * @param[in] comm MPI communicator containing the ranks that need the partition information
     * @param[in] ranks vector of ranks owning each geometric model entity
     * @param[in] ents vector of geometric model entities
     */
    ClassPtn(MPI_Comm comm, const redev::LOs& ranks, const ModelEntVec& ents);
    /**
     * Return the rank owning the given geometric model entity.
     * @param[in] ent the geometric model entity
     */
    redev::LO GetRank(ModelEnt ent) const;
    void Write(adios2::Engine& eng, adios2::IO& io);
    void Read(adios2::Engine& eng, adios2::IO& io);
    void Broadcast(MPI_Comm comm, int root=0);
    /**
     * Return the vector of owning ranks for all geometric model entity.
     */
    redev::LOs GetRanks() const;
    /**
     * Return the vector of all geometric model entities.
     */
    ModelEntVec GetModelEnts() const;
  private:
    const std::string entsAndRanksVarName = "class partition ents and ranks";
    /**
     * return a vector with the contents of the ModelEntToRank map
     * the map is serialized as [dim_0, id_0, rank_0, dim_1, id_1, rank_1, ..., dim_n-1, id_n-1, rank_n-1]
     */
    void Gather(MPI_Comm comm, int root=0);
    /**
     * return a vector with the contents of the ModelEntToRank map
     * the map is serialized as [dim_0, id_0, rank_0, dim_1, id_1, rank_1, ..., dim_n-1, id_n-1, rank_n-1]
     */
    redev::LOs SerializeModelEntsAndRanks() const;
    /**
     * Given a vector that contains the owning ranks and geometric model entities
     * [dim_0, id_0, rank_0, dim_1, id_1, rank_1, ..., dim_n-1, id_n-1, rank_n-1]
     * construct the ModelEntToRank map
     */
    ModelEntToRank DeserializeModelEntsAndRanks(const redev::LOs& serialized) const;
    /**
     * Ensure that the dimensions of the model ents is [0:3]
     */
    bool ModelEntDimsValid(const ModelEntVec& ents) const;
    /**
     * The map of geometric model entities to their owning rank
     */
    ModelEntToRank modelEntToRank;
};

/**
 * The RCBPtn class supports recursive coordinate bisection domain partitions.
 * The user passes to the constructor the definition of the cut tree and the ranks owning each
 * sub-domain.
 * The non-leaf levels of the partition tree have alternating cut dimensions associated
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
class RCBPtn : public Partition {
  public:
    RCBPtn();
    /**
     * Create a RCBPtn object with only the dimension specified.  The cuts and ranks will be filled
     * in during Redev creation.
     * @param[in] dim the dimension of the domain (2=2d,3=3d); defines the length of each cut vector
     */
    RCBPtn(redev::LO dim);
    /**
     * Create a RCBPtn object from a vector of owning ranks and cut vectors.
     * @param[in] dim the dimension of the domain (2=2d,3=3d); defines the length of each cut vector
     * @param[in] ranks vector of ranks owning each sub-domain in the cut tree
     * @param[in] cuts vector of 2-vectors or 3-vectors, for 2d and 3d domains respectively, defining the cut tree.
     *             The 2-vectors (3-vectors) are stored in the vector as (x0,y0(,z0),x1,y1(,z1),...xN-1,yN-1(,zN-1))
     */
    RCBPtn(redev::LO dim, std::vector<int>& ranks, std::vector<double>& cuts);
    /**
     * Return the rank owning the given point.
     * @param[in] pt the cartesian point in space. The 3rd value is ignored for 2d domains.
     */
    redev::LO GetRank(std::array<redev::Real,3>& pt) const;
    void Write(adios2::Engine& eng, adios2::IO& io);
    void Read(adios2::Engine& eng, adios2::IO& io);
    void Broadcast(MPI_Comm comm, int root=0);
    /**
     * Return the vector of owning ranks for each sub-domain of the cut tree.
     */
    std::vector<redev::LO> GetRanks() const;
    /**
     * Return the vector of cuts defining the cut tree. The 2-vectors (3-vectors) are stored in the vector as (x0,y0(,z0),x1,y1(,z1),...xN-1,yN-1(,zN-1))
     */
    std::vector<redev::Real> GetCuts() const;
  private:
    const std::string ranksVarName = "rcb partition ranks";
    const std::string cutsVarName = "rcb partition cuts";
    redev::LO dim;
    std::vector<redev::LO> ranks;
    std::vector<redev::Real> cuts;
};

/**
 * A CommPair is a pair of communication objects using ADIOS2.
 * s2c is the object for server-to-client communications.
 * c2s is the object for client-to-server communications.
 * The 'server' is the set of processes with the rendezvous partition.
 * A 'client' is a set of processes that queries the rendezvous partition.
 */
template <class T>
struct CommPair {
  redev::AdiosComm<T> s2c;
  redev::AdiosComm<T> c2s;
};

/**
 * The Redev class exercises the Partition class APIs to setup the rendezvous
 * partition on the server/rendezvous processes, communicate the partition to
 * the client processes.  It also provides an API to create objects that enable
 * communication with the clients.
 * One instance of Redev can support multiple clients.
 */
class Redev {
  public:
    /**
     * Create a Redev object
     * @param[in] comm MPI communicator containing the ranks that are part of the server/client
     * @param[in] ptn Partition object defining the redezvous domain partition
     * @param[in] isRendezvous true for the server processes and false otherwise
     * @param[in] noClients for testing without any clients present
     */
    Redev(MPI_Comm comm, Partition& ptn, bool isRendezvous=false, bool noClients=false);
    /**
     * Create a ADIOS2-based CommPair between the server and one client
     * @param[in] name name for the communication channel, each CommPair must have a unique name
     * @param[in] params list of ADIOS2 parameters controlling IO and Engine creation, see
     * https://adios2.readthedocs.io/en/latest/engines/engines.html for the list
     * of applicable parameters for the SST and BP4 engines
     * @param[in] isSST by default the BP4 Engine is used, setting this to true
     * enables use of the SST engine
     */
    template<typename T> CommPair<T> CreateAdiosClient(std::string_view name, adios2::Params params, bool isSST=false);
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

template<typename T>
CommPair<T> Redev::CreateAdiosClient(std::string_view name, adios2::Params params, bool isSST) {
  auto s2cName = std::string(name)+"_s2c";
  auto c2sName = std::string(name)+"_c2s";
  auto s2cIO = adios.DeclareIO(s2cName);
  auto c2sIO = adios.DeclareIO(c2sName);
  s2cIO.SetEngine( isSST ? "SST" : "BP4");
  c2sIO.SetEngine( isSST ? "SST" : "BP4");
  s2cIO.SetParameters(params);
  c2sIO.SetParameters(params);
  REDEV_ALWAYS_ASSERT(s2cIO.EngineType() == c2sIO.EngineType());
  if(noClients) {
    //SST hangs if there is no reader
    s2cIO.SetEngine("BP4");
    c2sIO.SetEngine("BP4");
  }
  adios2::Engine s2cEngine;
  adios2::Engine c2sEngine;
  if( isSameCi(s2cIO.EngineType(), "SST") ) {
    openEnginesSST(noClients,s2cName,c2sName,
        s2cIO,c2sIO,s2cEngine,c2sEngine);
  } else if( isSameCi(s2cIO.EngineType(), "BP4") ) {
    openEnginesBP4(noClients,s2cName+".bp",c2sName+".bp",
        s2cIO,c2sIO,s2cEngine,c2sEngine);
  } else {
    if(!rank) {
      std::cerr << "ERROR: redev does not support ADIOS2 engine " << s2cIO.EngineType() << "\n";
    }
    exit(EXIT_FAILURE);
  }
  Setup(s2cIO,s2cEngine);
  const auto serverRanks = GetServerCommSize(s2cIO,s2cEngine);
  const auto clientRanks = GetClientCommSize(c2sIO,c2sEngine);
  return CommPair<T>{
    AdiosComm<T>(comm, clientRanks, s2cEngine, s2cIO, std::string(name)+"_s2c"),
    AdiosComm<T>(comm, serverRanks, c2sEngine, c2sIO, std::string(name)+"_c2s")};
}

}
