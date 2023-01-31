#pragma once
#include <adios2.h>
#include <mpi.h>
#include "redev_types.h"
#include "redev_assert.h"
#include "redev_comm.h"
#include "redev_strings.h"
#include <array>          // std::array
#include <variant>

namespace redev {

/**
 * \page concepts
 *
 * The Rendezvous Algorithm
 * ========================
 *
 * The Rendezvous algorithm 
 * > enables scalable algorithms which are most useful when processors neither know which other processors
 * > to send data to, nor which other processors will be sending data to them [1].
 * It is used in LAMMPS [1], the PUMI unstructured mesh library for loading
 * multi-billion element meshes from file [2,3], and in the Data Transfer
 * Toolkit [4].
 *
 * The following figures from [1] depict two unstructured meshes of the same domain
 * where information from one mesh is needed by the other but they have a
 * different partitioning across the same set of processes.
 * In Figure 4 the colored ovals depict the domain owned by a given process in
 * the two meshes.
 * Since the domains are not intersecting the Rendezvous method is used to
 * efficiently exchange data between the processes that do have the intersecting
 * portion of the domain.
 * A common domain partition, depicted by the red structured grid in the bottom
 * two figures, allows processes that own data within a given cell in grid to
 * 'rendezvous' with the other processes that own data in that cell and perform
 * an exchange.
 *
 * \image xml redevPartition.png
 *
 * The key to the Rendezvous algorithm is a partitioning of the common portion
 * of the domain that (1) has a relatively low memory usage and (2) supports computationally
 * efficient queries for membership within the partition given a entity 
 * within the domain (e.g., a mesh element or vertex).
 * The structured grid from the above figure is one possible partition that
 * satisfies these requirements.
 *
 * WDMApp Use Case
 * ===============
 *
 * The [WDMApp project](https://wdmapp.readthedocs.io/en/latest/) combines an
 * application that evolves the physics near the 'edge' of a fusion device 
 * (i.e., D3D, ITER, etc.) with another application that
 * is responsible for the inner 'core' portion of the device near the
 * magnetic axis.
 *
 * The following figure depicts a conceptual division of the D3D domain into
 * core and edge sub-domains where the respective applications are
 * operating and an overlap sub-domain where fields on the mesh are exchanged
 * and transformed.
 *
 * \image xml overlap500.png
 *
 * The WDMApp Coupler is a set of procedures to support field transformations in
 * the overlap sub-domain between application meshes and partitions that are
 * different.
 * For example, the following figure depicts a 16 process partition created with
 * a geometric recursive bisection method on the left and a four process
 * partition using groupings of geometric model faces.
 * The Rendezvous algorithm allows the WDMApp Coupler to define its own
 * partition of the domain and efficiently collect, and transform, field data
 * from the core and edge applications.
 *
 * \image xml partitions400.png
 *
 * Redev Design
 * ============
 *
 * Redev is an implementation of the Rendezvous algorithm that is currently
 * focused on supporting the WDMApp Coupler and its use of independent sets of
 * MPI processes for the core and edge applications.
 * Specifically, each of these applications is executed with its own `mpirun` (or
 * equivalent batch scheduler) command.
 * Transferring data between these sets of processes is directly supported by
 * the [ADIOS2](https://adios2.readthedocs.io/en/latest/) library.
 *
 * Key to the Redev design is the concept of a Server and Clients.
 * A Client is a set of processes that owns some portion of the overlap sub-domain
 * and needs to exchange data in that domain with another Client.
 * Clients query the Redev API to (1) determine which processes in the Server the data
 * they own needs to be sent, (2) send the data, and (3) receive data back from
 * the server in their portion of the domain.
 * The Server is a set of processes that owns the Rendezvous partition of the
 * overlap sub-domain, receives data from Clients, and supports sending the data
 * back to the Clients.
 * 
 * The following figure depicts an example Redev workflow for exchanging data between
 * two applications (Clients) and the Coupler (the Server).
 * The Rendezvous partition in this example is formed from groups of adjacent
 * geometric model entities referred to as a 'Feature-based PartitionInterface'.
 * Red colored boxes and text are operations supported by Redev.
 * The phases of the workflow are described below.
 * - _App. Setup_: The applications initialize its domain data
 *   by loading meshes, and associated partition data.
 *   The Coupler loads the geometric model of the domain.
 * - _Rdv. Setup_: The coupler forms the 'Feature-based PartitionInterface' used for Rendezvous, and
 *   sends partition information to an instance of Redev embedded
 *   into each application.
 * - _Mesh Setup_: Each application queries the rendezvous partition in a loop
 *   over their mesh entities in the overlap sub-domain and create a map of mesh
 *   entities to their destination rank in the Coupler.
 *   Using a communication object established for the connection between
 *   each application and the Coupler a **Forward** send is performed from the
 *   applications to the Coupler.
 *   This initial **Forward** send, and the meta data associated with it that
 *   describes which application rank sent each portion of the data, enables the
 *   construction of a **Reverse** send from the Coupler back to the appropriate
 *   application processes.
 * - _Field Transfer_: The **Forward** and **Reverse** sends are
 *   repeatedly executed until the coupled simulation completes.
 *   The figure depicts one of portion of a communication round that performs
 *   a **Forward** send from application 'A' to the Coupler,
 *   execution of a field transformation procedure marked 'mesh-to-mesh', and a
 *   **Reverse** send from the Coupler to application 'B'.
 *   The communication round would be completed with the mirror image of this
 *   process; a **Forward** send from 'B' to the Coupler, another transformation, and
 *   a **Reverse** send from the Coupler to 'A'.
 *
 * \image xml redevWorkflow500.png
 * 
 * References
 * ----------
 *
 * - [[1] “Rendezvous  algorithms for large-scale modeling and simulation”, Plimpton, et.al.,  JPDC, 2021.](https://www.sciencedirect.com/science/article/abs/pii/S0743731520303622)
 * - [[2] PUMI  Github  Issue](https://github.com/SCOREC/core/issues/205)
 * - [[3] "Optimizing  CAD and Mesh Generation  Workflow for  SeisSol", Rettenberger, et.al.,  SC14, 2014.](http://sc14.supercomputing.org/sites/all/themes/sc14/files/archive/tech_poster/poster_files/post175s2-file3.pdf)
 * - [[4] "The  Data Transfer  Kit: A geometric rendezvous-based  tool  for  multiphysicsdata transfer", Slattery et.al.,  M&C * 2013](https://www.osti.gov/biblio/22212795)
 */

/**
 * The PartitionInterface class provides an abstract interface that is used by
 * Redev for sharing partition information among the server and client
 * processes. Instances of the PartitionInterface class define an interface
 * (i.e., GetRank(...)) to query which process owns a point in the domain
 * (RCBPtn) or a given geometric model entity (ClassPtn). Defining the cut tree
 * for RCB or the assignment of ranks to geometric model entities is not the
 * purpose/responsiblity of this class or the RCBPtn and ClassPtn derived
 * classes.
 * @note this class is only used to enforce the partition interface and
 * can/should be replaced with concept in C++20
 */
class PartitionInterface {
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
class ClassPtn : public PartitionInterface {
  public:
    /**
     * Pair of integers (dimension, id) that uniquely identify a geometric model entity by their
     * dimension and id.  The id for an entity within a dimension is unique.
     */
    using ModelEnt = std::pair<redev::LO, redev::LO>;
    /**
     * Vector of geometric model entities.
     */
    using ModelEntVec = std::vector<ModelEnt>;
    /**
     * Map of geometric model entities to the process that owns them.
     */
    using ModelEntToRank = std::map<ModelEnt,redev::LO>;
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
    [[nodiscard]]
    redev::LO GetRank(ModelEnt ent) const;
    void Write(adios2::Engine& eng, adios2::IO& io) final;
    void Read(adios2::Engine& eng, adios2::IO& io) final;
    void Broadcast(MPI_Comm comm, int root=0) final;
    /**
     * Return the vector of owning ranks for all geometric model entity.
     */
    [[nodiscard]]
    redev::LOs GetRanks() const;
    /**
     * Return the vector of all geometric model entities.
     */
     [[nodiscard]]
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
    [[nodiscard]]
    redev::LOs SerializeModelEntsAndRanks() const;
    /**
     * Given a vector that contains the owning ranks and geometric model entities
     * [dim_0, id_0, rank_0, dim_1, id_1, rank_1, ..., dim_n-1, id_n-1, rank_n-1]
     * construct the ModelEntToRank map
     */
    [[nodiscard]]
    ModelEntToRank DeserializeModelEntsAndRanks(const redev::LOs& serialized) const;
    /**
     * Ensure that the dimensions of the model ents is [0:3]
     */
     [[nodiscard]]
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
class RCBPtn : public PartitionInterface {
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
    void Write(adios2::Engine& eng, adios2::IO& io) final;
    void Read(adios2::Engine& eng, adios2::IO& io) final;
    void Broadcast(MPI_Comm comm, int root=0) final;
    /**
     * Return the vector of owning ranks for each sub-domain of the cut tree.
     */
    [[nodiscard]]
    std::vector<redev::LO> GetRanks() const;
    /**
     * Return the vector of cuts defining the cut tree. The 2-vectors (3-vectors) are stored in the vector as (x0,y0(,z0),x1,y1(,z1),...xN-1,yN-1(,zN-1))
     */
    [[nodiscard]]
    std::vector<redev::Real> GetCuts() const;
  private:
    const std::string ranksVarName = "rcb partition ranks";
    const std::string cutsVarName = "rcb partition cuts";
    redev::LO dim;
    std::vector<redev::LO> ranks;
    std::vector<redev::Real> cuts;
};

using Partition = std::variant<ClassPtn,RCBPtn>;

/**
 * A BidirectionalComm is a communicator that can send and receive data
 * If you are on a client rank sending sends data to the server and receiving
 * retrieves data from server.
 * If you are on a server rank sending sends data to the client and receiving
 * retrieves data from client.
 */
template <class T>
class BidirectionalComm {
public:
  BidirectionalComm() = default;
  BidirectionalComm(std::unique_ptr<Communicator<T>> sender_,
                    std::unique_ptr<Communicator<T>> receiver_)
      : sender(std::move(sender_)), receiver(std::move(receiver_)) {
    REDEV_ALWAYS_ASSERT(sender != nullptr);
    REDEV_ALWAYS_ASSERT(receiver != nullptr);
  }
  void SetOutMessageLayout(LOs &dest, LOs &offsets) {
    sender->SetOutMessageLayout(dest, offsets);
  }
  InMessageLayout GetInMessageLayout() {
    return receiver->GetInMessageLayout();
  }

  void Send(T *msgs) { sender->Send(msgs); }
  std::vector<T> Recv() { return receiver->Recv(); }

private:
  std::unique_ptr<Communicator<T>> sender;
  std::unique_ptr<Communicator<T>> receiver;
};

enum class ProcessType {
  Client = 0,
  Server = 1
};
enum class TransportType {
  BP4 = 0,
  SST = 1
};
/**
 * The Redev class exercises the PartitionInterface class APIs to setup the rendezvous
 * partition on the server/rendezvous processes, communicate the partition to
 * the client processes.  It also provides an API to create objects that enable
 * communication with the clients.
 * One instance of Redev can support multiple clients.
 */
class Redev {
  public:
    /**
     * Create a Redev server
     * @param[in] comm MPI communicator containing the ranks that are part of the server/client
     * @param[in] ptn PartitionInterface object defining the redezvous domain partition (see note below)
     * @param[in] processProcess enum for if the server is a client, server
     * @param[in] noClients for testing without any clients present
     * The client processes should pass in an empty PartitionInterface object.
     * The server will send the client the needed partition information during
     * the call to CreateAdiosClient.
     */
    Redev(MPI_Comm comm, Partition ptn, ProcessType processType = ProcessType::Server, bool noClients= false);
    /**
     * Create a Redev client
     * @param[in] comm MPI communicator containing the ranks that are part of the server/client
     * @param[in] processProcess enum for if the server is a client, server
     * @param[in] noClients for testing without any clients present
     * The client processes should pass in an empty PartitionInterface object.
     * The server will send the client the needed partition information during
     * the call to CreateAdiosClient.
     */
    Redev(MPI_Comm comm, ProcessType processType = ProcessType::Client, bool noClients= false);
    /**
     * Create a ADIOS2-based BidirectionalComm between the server and one client
     * @param[in] name name for the communication channel, each BidirectionalComm must have a unique name
     * @param[in] params list of ADIOS2 parameters controlling IO and Engine creation, see
     * https://adios2.readthedocs.io/en/latest/engines/engines.html for the list
     * of applicable parameters for the SST and BP4 engines
     * @param[in] transportType by default the BP4 Engine is used, other transport
     * types are available in the TransportType enum
     */
    template<typename T>
    BidirectionalComm<T> CreateAdiosClient(std::string_view name, adios2::Params params,
                                  TransportType transportType = TransportType::BP4, std::string_view path = {});
    [[nodiscard]]
    ProcessType GetProcessType() const;
    [[nodiscard]]
    const Partition & GetPartition() const;

  private:
    void Setup(adios2::IO& s2cIO, adios2::Engine& s2cEngine);
    void CheckVersion(adios2::Engine& eng, adios2::IO& io);
    ProcessType processType;
    bool noClients; // true: no clients will be connected, false: otherwise
    void openEnginesBP4(bool noClients,
        std::string s2cName, std::string c2sName,
        adios2::IO& s2cIO, adios2::IO& c2sIO,
        adios2::Engine& s2cEngine, adios2::Engine& c2sEngine);
    void openEnginesSST(bool noClients,
        std::string s2cName, std::string c2sName,
        adios2::IO& s2cIO, adios2::IO& c2sIO,
        adios2::Engine& s2cEngine, adios2::Engine& c2sEngine);
    redev::LO SendServerCommSizeToClient(adios2::IO& s2cIO, adios2::Engine& s2cEngine);
    redev::LO SendClientCommSizeToServer(adios2::IO& c2sIO, adios2::Engine& c2sEngine);
    std::size_t SendPartitionTypeToClient(adios2::IO& s2cIO, adios2::Engine& s2cEngine);
    MPI_Comm comm;
    adios2::ADIOS adios;
    int rank;
    Partition ptn;
    void ConstructPartitionFromIndex(size_t partition_index);
};

template<typename T>
BidirectionalComm<T> Redev::CreateAdiosClient(std::string_view name, adios2::Params params,
                                     TransportType transportType, std::string_view path) {
  auto s2cName = std::string(name)+"_s2c";
  auto c2sName = std::string(name)+"_c2s";
  auto s2cIO = adios.DeclareIO(s2cName);
  auto c2sIO = adios.DeclareIO(c2sName);
  if(transportType == TransportType::SST && noClients == true) {
    // TODO log message here
    transportType = TransportType::BP4;
  }
  std::string engineType;
  switch (transportType) {
    case TransportType::BP4 :
      engineType = "BP4";
      break;
    case TransportType::SST:
      engineType = "SST";
      break;
    // no default case. This will cause a compiler error if we do not handle a
    // an engine type that has been defined in the TransportType enum. (-Werror=switch)
  }
  s2cIO.SetEngine(engineType);
  c2sIO.SetEngine(engineType);
  s2cIO.SetParameters(params);
  c2sIO.SetParameters(params);
  REDEV_ALWAYS_ASSERT(s2cIO.EngineType() == c2sIO.EngineType());
  adios2::Engine s2cEngine;
  adios2::Engine c2sEngine;
  std::stringstream S2CEngineName;
  S2CEngineName << path << s2cName;
  std::stringstream C2SEngineName;
  C2SEngineName << path << c2sName;
  switch (transportType) {
    case TransportType::BP4 :
      C2SEngineName << ".bp";
      S2CEngineName << ".bp";
      openEnginesBP4(noClients,S2CEngineName.str(),C2SEngineName.str(),
                     s2cIO,c2sIO,s2cEngine,c2sEngine);
      break;
    case TransportType::SST:
      openEnginesSST(noClients,S2CEngineName.str(),C2SEngineName.str(),
                     s2cIO,c2sIO,s2cEngine,c2sEngine);
      break;
    // no default case. This will cause a compiler error if we do not handle a
    // an engine type that has been defined in the TransportType enum. (-Werror=switch)
  }
  Setup(s2cIO,s2cEngine);
  const auto serverRanks = SendServerCommSizeToClient(s2cIO, s2cEngine);
  const auto clientRanks = SendClientCommSizeToServer(c2sIO, c2sEngine);
  auto s2c = std::make_unique<AdiosComm<T>>(comm, clientRanks, s2cEngine, s2cIO, std::string(name)+"_s2c");
  auto c2s = std::make_unique<AdiosComm<T>>(comm, serverRanks, c2sEngine, c2sIO, std::string(name)+"_c2s");
  switch (processType) {
    case ProcessType::Client:
      return {std::move(c2s), std::move(s2c)};
    case ProcessType::Server:
      return {std::move(s2c), std::move(c2s)};
  }
  REDEV_ALWAYS_ASSERT(false);  //we should never get here
  return {nullptr, nullptr}; //silence compiler warning
}

}
