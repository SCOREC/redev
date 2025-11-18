#ifndef REDEV_REDEV_ADIOS_CHANNEL_H
#define REDEV_REDEV_ADIOS_CHANNEL_H
#include "redev_assert.h"
#include "redev_profile.h"
#include <adios2.h>

namespace redev {

class AdiosChannel {
public:
  AdiosChannel(adios2::ADIOS &adios, MPI_Comm comm, std::string name,
               adios2::Params params, TransportType transportType,
               ProcessType processType, Partition &partition, std::string path,
               bool noClients = false)
      : comm_(comm), process_type_(processType), partition_(partition)

  {
    REDEV_FUNCTION_TIMER;
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
        partition_(o.partition_) {REDEV_FUNCTION_TIMER;}
  AdiosChannel operator=(AdiosChannel &&) = delete;
  // FIXME IMPL RULE OF 5
  ~AdiosChannel() {
    REDEV_FUNCTION_TIMER;
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
  [[nodiscard]] BidirectionalComm<T> CreateComm(std::string name, MPI_Comm comm,
                                                CommType ctype){
    REDEV_FUNCTION_TIMER;
    // TODO, remove s2c/c2s distinction on variable names then use std::move
    // name
    if (comm != MPI_COMM_NULL) {
      std::unique_ptr<Communicator<T>> s2c, c2s;
      switch (ctype) {
        case CommType::Ptn:
          s2c = std::make_unique<AdiosPtnComm<T>>(comm, num_client_ranks_,
                                                  s2c_engine_, s2c_io_, name);
              c2s = std::make_unique<AdiosPtnComm<T>>(comm, num_server_ranks_,
                                                      c2s_engine_, c2s_io_, name);
              break;
        case CommType::Global:
          s2c = std::make_unique<AdiosGlobalComm<T>>(comm, s2c_engine_, s2c_io_,
                                                     name);
              c2s = std::make_unique<AdiosGlobalComm<T>>(comm, c2s_engine_, c2s_io_,
                                                         name);
              break;
      }
      switch (process_type_) {
        case ProcessType::Client: return {std::move(c2s), std::move(s2c)};
        case ProcessType::Server: return {std::move(s2c), std::move(c2s)};
      }
    }
    return {std::make_unique<NoOpComm<T>>(), std::make_unique<NoOpComm<T>>()};
  }

  // TODO s2c/c2s Engine/IO -> send/receive Engine/IO. This removes need for all
  // the switch statements...
  void BeginSendCommunicationPhase() {
    REDEV_FUNCTION_TIMER;
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
  }
  void EndSendCommunicationPhase() {
    switch (process_type_) {
    case ProcessType::Client:
      c2s_engine_.EndStep();
      break;
    case ProcessType::Server:
      s2c_engine_.EndStep();
      break;
    }
  }
  void BeginReceiveCommunicationPhase() {
    REDEV_FUNCTION_TIMER;
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
  }
  void EndReceiveCommunicationPhase() {
    REDEV_FUNCTION_TIMER;
    switch (process_type_) {
    case ProcessType::Client:
      s2c_engine_.EndStep();
      break;
    case ProcessType::Server:
      c2s_engine_.EndStep();
      break;
    }
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
};
} // namespace redev

#endif // REDEV__REDEV_ADIOS_CHANNEL_H
