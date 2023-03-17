#pragma once
#include "redev_assert.h"
#include "redev_comm.h"
#include "redev_strings.h"
#include "redev_types.h"
#include <adios2.h>
#include <array> // std::array
#include <mpi.h>
#include <optional>
#include <utility>
#include <variant>

#include "redev_bidirectional_comm.h"
#include "redev_channel.h"
#include "redev_partition.h"

namespace redev {

/**
 * \page concepts
 *
 * The Rendezvous Algorithm
 * ========================
 *
 * The Rendezvous algorithm
 * > enables scalable algorithms which are most useful when processors neither
 * know which other processors > to send data to, nor which other processors
 * will be sending data to them [1]. It is used in LAMMPS [1], the PUMI
 * unstructured mesh library for loading multi-billion element meshes from file
 * [2,3], and in the Data Transfer Toolkit [4].
 *
 * The following figures from [1] depict two unstructured meshes of the same
 * domain where information from one mesh is needed by the other but they have a
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
 * of the domain that (1) has a relatively low memory usage and (2) supports
 * computationally efficient queries for membership within the partition given a
 * entity within the domain (e.g., a mesh element or vertex). The structured
 * grid from the above figure is one possible partition that satisfies these
 * requirements.
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
 * Specifically, each of these applications is executed with its own `mpirun`
 * (or equivalent batch scheduler) command. Transferring data between these sets
 * of processes is directly supported by the
 * [ADIOS2](https://adios2.readthedocs.io/en/latest/) library.
 *
 * Key to the Redev design is the concept of a Server and Clients.
 * A Client is a set of processes that owns some portion of the overlap
 * sub-domain and needs to exchange data in that domain with another Client.
 * Clients query the Redev API to (1) determine which processes in the Server
 * the data they own needs to be sent, (2) send the data, and (3) receive data
 * back from the server in their portion of the domain. The Server is a set of
 * processes that owns the Rendezvous partition of the overlap sub-domain,
 * receives data from Clients, and supports sending the data back to the
 * Clients.
 *
 * The following figure depicts an example Redev workflow for exchanging data
 * between two applications (Clients) and the Coupler (the Server). The
 * Rendezvous partition in this example is formed from groups of adjacent
 * geometric model entities referred to as a 'Feature-based PartitionInterface'.
 * Red colored boxes and text are operations supported by Redev.
 * The phases of the workflow are described below.
 * - _App. Setup_: The applications initialize its domain data
 *   by loading meshes, and associated partition data.
 *   The Coupler loads the geometric model of the domain.
 * - _Rdv. Setup_: The coupler forms the 'Feature-based PartitionInterface' used
 * for Rendezvous, and sends partition information to an instance of Redev
 * embedded into each application.
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
 *   process; a **Forward** send from 'B' to the Coupler, another
 * transformation, and a **Reverse** send from the Coupler to 'A'.
 *
 * \image xml redevWorkflow500.png
 *
 * References
 * ----------
 *
 * - [[1] “Rendezvous  algorithms for large-scale modeling and simulation”,
 * Plimpton, et.al.,  JPDC,
 * 2021.](https://www.sciencedirect.com/science/article/abs/pii/S0743731520303622)
 * - [[2] PUMI  Github  Issue](https://github.com/SCOREC/core/issues/205)
 * - [[3] "Optimizing  CAD and Mesh Generation  Workflow for  SeisSol",
 * Rettenberger, et.al.,  SC14,
 * 2014.](http://sc14.supercomputing.org/sites/all/themes/sc14/files/archive/tech_poster/poster_files/post175s2-file3.pdf)
 * - [[4] "The  Data Transfer  Kit: A geometric rendezvous-based  tool  for
 * multiphysicsdata transfer", Slattery et.al.,  M&C *
 * 2013](https://www.osti.gov/biblio/22212795)
 */



class AdiosChannel {
public:
  AdiosChannel(adios2::ADIOS &adios, MPI_Comm comm, std::string name,
               adios2::Params params, TransportType transportType,
               ProcessType processType, Partition &partition, std::string path,
               bool noClients = false)
      : comm_(comm), process_type_(processType), partition_(partition),
        send_communication_phase_active_(false),
        receive_communication_phase_active_(false)

  {
    MPI_Comm_rank(comm, &rank_);
    auto s2cName = path + name + "_s2c";
    auto c2sName = path + name + "_c2s";
    s2c_io_ = adios.DeclareIO(s2cName);
    c2s_io_ = adios.DeclareIO(c2sName);
    if (transportType == TransportType::SST && noClients == true) {
      // TODO log message here
      transportType = TransportType::BP4;
    }
    std::string engineType;
    switch (transportType) {
    case TransportType::BP4:
      engineType = "BP4";
      s2cName = s2cName + ".bp";
      c2sName = c2sName + ".bp";
      break;
    case TransportType::SST:
      engineType = "SST";
      break;
      // no default case. This will cause a compiler error if we do not handle a
      // an engine type that has been defined in the TransportType enum.
      // (-Werror=switch)
    }
    s2c_io_.SetEngine(engineType);
    c2s_io_.SetEngine(engineType);
    s2c_io_.SetParameters(params);
    c2s_io_.SetParameters(params);
    REDEV_ALWAYS_ASSERT(s2c_io_.EngineType() == c2s_io_.EngineType());
    switch (transportType) {
    case TransportType::SST:
      openEnginesSST(noClients, s2cName, c2sName, s2c_io_, c2s_io_, s2c_engine_,
                     c2s_engine_);
      break;
    case TransportType::BP4:
      openEnginesBP4(noClients, s2cName, c2sName, s2c_io_, c2s_io_, s2c_engine_,
                     c2s_engine_);
      break;
    }
    // TODO pull begin/end step out of Setup/SendReceive metadata functions
    // begin step
    // send metadata
    Setup(s2c_io_, s2c_engine_);
    num_server_ranks_ = SendServerCommSizeToClient(s2c_io_, s2c_engine_);
    num_client_ranks_ = SendClientCommSizeToServer(c2s_io_, c2s_engine_);
    // end step
  }
  // don't allow copying of class because it creates
  AdiosChannel(const AdiosChannel &) = delete;
  AdiosChannel operator=(const AdiosChannel &) = delete;
  // FIXME
  AdiosChannel(AdiosChannel &&o)
      : s2c_io_(std::exchange(o.s2c_io_, adios2::IO())),
        c2s_io_(std::exchange(o.c2s_io_, adios2::IO())),
        c2s_engine_(std::exchange(o.c2s_engine_, adios2::Engine())),
        s2c_engine_(std::exchange(o.s2c_engine_, adios2::Engine())),
        num_client_ranks_(o.num_client_ranks_),
        num_server_ranks_(o.num_server_ranks_),
        comm_(std::exchange(o.comm_, MPI_COMM_NULL)),
        process_type_(o.process_type_), rank_(o.rank_),
        partition_(o.partition_),
        send_communication_phase_active_(
            std::exchange(o.send_communication_phase_active_, false)),
        receive_communication_phase_active_(
            std::exchange(o.receive_communication_phase_active_, false)) {
  }
  AdiosChannel operator=(AdiosChannel &&) = delete;
  // FIXME IMPL RULE OF 5
  ~AdiosChannel() {
    // NEED TO CHECK that the engine exists before trying to close it because it
    // could be in a moved from state
    if (s2c_engine_) {
      s2c_engine_.Close();
    }
    if (c2s_engine_) {
      c2s_engine_.Close();
    }
  }
  template <typename T>
  [[nodiscard]] BidirectionalComm<T> CreateComm(std::string name) {
    // TODO, remove s2c/c2s destinction on variable names then use std::move
    // name
    auto s2c = std::make_unique<AdiosComm<T>>(comm_, num_client_ranks_,
                                              s2c_engine_, s2c_io_, name);
    auto c2s = std::make_unique<AdiosComm<T>>(comm_, num_server_ranks_,
                                              c2s_engine_, c2s_io_, name);
    switch (process_type_) {
    case ProcessType::Client:
      return {std::move(c2s), std::move(s2c)};
    case ProcessType::Server:
      return {std::move(s2c), std::move(c2s)};
    }
    // Quash compiler warnings
    return {nullptr, nullptr};
  }
  template <typename T> [[nodiscard]] BidirectionalComm<T> CreateNoOpComm() {
    return {std::make_unique<NoOpComm<T>>(), std::make_unique<NoOpComm<T>>()};
  }

  // TODO s2c/c2s Engine/IO -> send/receive Engine/IO. This removes need for all
  // the switch statements...
  void BeginSendCommunicationPhase() {
    REDEV_ALWAYS_ASSERT(InSendCommunicationPhase() == false);
    adios2::StepStatus status;
    switch (process_type_) {
    case ProcessType::Client:
      status = c2s_engine_.BeginStep();
      break;
    case ProcessType::Server:
      status = s2c_engine_.BeginStep();
      break;
    }
    REDEV_ALWAYS_ASSERT(status == adios2::StepStatus::OK);
    send_communication_phase_active_ = true;
  }
  void EndSendCommunicationPhase() {
    REDEV_ALWAYS_ASSERT(InSendCommunicationPhase() == true);
    switch (process_type_) {
    case ProcessType::Client:
      c2s_engine_.EndStep();
      break;
    case ProcessType::Server:
      s2c_engine_.EndStep();
      break;
    }
    send_communication_phase_active_ = false;
  }
  void BeginReceiveCommunicationPhase() {
    REDEV_ALWAYS_ASSERT(InReceiveCommunicationPhase() == false);
    adios2::StepStatus status;
    switch (process_type_) {
    case ProcessType::Client:
      status = s2c_engine_.BeginStep();
      break;
    case ProcessType::Server:
      status = c2s_engine_.BeginStep();
      break;
    }
    REDEV_ALWAYS_ASSERT(status == adios2::StepStatus::OK);
    receive_communication_phase_active_ = true;
  }
  void EndReceiveCommunicationPhase() {
    REDEV_ALWAYS_ASSERT(InReceiveCommunicationPhase() == true);
    switch (process_type_) {
    case ProcessType::Client:
      s2c_engine_.EndStep();
      break;
    case ProcessType::Server:
      c2s_engine_.EndStep();
      break;
    }
    receive_communication_phase_active_ = false;
  }
  [[nodiscard]] bool InSendCommunicationPhase() const noexcept {
    return send_communication_phase_active_;
  }
  [[nodiscard]] bool InReceiveCommunicationPhase() const noexcept {
    return receive_communication_phase_active_;
  }

private:
  void openEnginesBP4(bool noClients, std::string s2cName, std::string c2sName,
                      adios2::IO &s2cIO, adios2::IO &c2sIO,
                      adios2::Engine &s2cEngine, adios2::Engine &c2sEngine);
  void openEnginesSST(bool noClients, std::string s2cName, std::string c2sName,
                      adios2::IO &s2cIO, adios2::IO &c2sIO,
                      adios2::Engine &s2cEngine, adios2::Engine &c2sEngine);
  [[nodiscard]] redev::LO SendServerCommSizeToClient(adios2::IO &s2cIO,
                                                     adios2::Engine &s2cEngine);
  [[nodiscard]] redev::LO SendClientCommSizeToServer(adios2::IO &c2sIO,
                                                     adios2::Engine &c2sEngine);
  [[nodiscard]] std::size_t
  SendPartitionTypeToClient(adios2::IO &s2cIO, adios2::Engine &s2cEngine);
  void Setup(adios2::IO &s2cIO, adios2::Engine &s2cEngine);
  void CheckVersion(adios2::Engine &eng, adios2::IO &io);
  void ConstructPartitionFromIndex(size_t partition_index);

  adios2::IO s2c_io_;
  adios2::IO c2s_io_;
  adios2::Engine s2c_engine_;
  adios2::Engine c2s_engine_;
  redev::LO num_client_ranks_;
  redev::LO num_server_ranks_;
  MPI_Comm comm_;
  ProcessType process_type_;
  int rank_;
  Partition &partition_;
  bool send_communication_phase_active_;
  bool receive_communication_phase_active_;
};

/**
 * The Redev class exercises the PartitionInterface class APIs to setup the
 * rendezvous partition on the server/rendezvous processes, communicate the
 * partition to the client processes.  It also provides an API to create objects
 * that enable communication with the clients. One instance of Redev can support
 * multiple clients.
 */
class Redev {
public:
  /**
   * Create a Redev server
   * @param[in] comm MPI communicator containing the ranks that are part of the
   * server/client
   * @param[in] ptn PartitionInterface object defining the redezvous domain
   * partition (see note below)
   * @param[in] processProcess enum for if the server is a client, server
   * @param[in] noClients for testing without any clients present
   * The client processes should pass in an empty PartitionInterface object.
   * The server will send the client the needed partition information during
   * the call to CreateAdiosChannel.
   */
  Redev(MPI_Comm comm, Partition ptn,
        ProcessType processType = ProcessType::Server, bool noClients = false);
  /**
   * Create a Redev client
   * @param[in] comm MPI communicator containing the ranks that are part of the
   * server/client
   * @param[in] processProcess enum for if the server is a client, server
   * @param[in] noClients for testing without any clients present
   * The client processes should pass in an empty PartitionInterface object.
   * The server will send the client the needed partition information during
   * the call to CreateAdiosChannel.
   */
  Redev(MPI_Comm comm, ProcessType processType = ProcessType::Client,
        bool noClients = false);
  // FIXME UPDATE DOCS
  /**
   * Create a ADIOS2-based BidirectionalComm between the server and one client
   * @param[in] name name for the communication channel, each BidirectionalComm
   * must have a unique name
   * @param[in] params list of ADIOS2 parameters controlling IO and Engine
   * creation, see https://adios2.readthedocs.io/en/latest/engines/engines.html
   * for the list of applicable parameters for the SST and BP4 engines
   * @param[in] transportType by default the BP4 Engine is used, other transport
   * types are available in the TransportType enum
   */
  [[nodiscard]] Channel
  CreateAdiosChannel(std::string name, adios2::Params params,
                     TransportType transportType = TransportType::BP4,
                     std::string path = {}) {
    return AdiosChannel{
        adios,       comm, std::move(name), std::move(params), transportType,
        processType, ptn,  std::move(path), noClients};
  }
  [[nodiscard]] ProcessType GetProcessType() const noexcept;
  [[nodiscard]] const Partition &GetPartition() const noexcept;
  [[nodiscard]] bool RankParticipates() const noexcept;

private:
  ProcessType processType;
  bool noClients; // true: no clients will be connected, false: otherwise
  MPI_Comm comm;
  adios2::ADIOS adios;
  int rank;
  Partition ptn;
};

} // namespace redev
