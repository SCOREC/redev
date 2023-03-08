#pragma once
#include "redev_types.h"
#include "redev_assert.h"
#include "redev_profile.h"
#include "redev_assert.h"
#include "redev_exclusive_scan.h"
#include <numeric> // accumulate, exclusive_scan
#include <type_traits> // is_same
#include <stddef.h>

namespace {
void checkStep(adios2::StepStatus status) {
  REDEV_ALWAYS_ASSERT(status == adios2::StepStatus::OK);
}
}

namespace redev {

  namespace detail {
    template <typename... T> struct dependent_always_false : std::false_type {};
  }

template<class T>
[[ nodiscard ]]
constexpr MPI_Datatype getMpiType(T) noexcept {
  if constexpr (std::is_same_v<T, char>) { return MPI_CHAR; }
  else if constexpr (std::is_same_v<T, signed short int>) { return MPI_SHORT; }
  else if constexpr (std::is_same_v<T, signed int>) { return MPI_INT; }
  else if constexpr (std::is_same_v<T, signed long>) { return MPI_LONG; }
  else if constexpr (std::is_same_v<T, signed long long>) { return MPI_LONG_LONG; }
  else if constexpr (std::is_same_v<T, signed char>) { return MPI_SIGNED_CHAR; }
  else if constexpr (std::is_same_v<T, unsigned char>) { return MPI_UNSIGNED_CHAR; }
  else if constexpr (std::is_same_v<T, unsigned short>) { return MPI_UNSIGNED_SHORT; }
  else if constexpr (std::is_same_v<T, unsigned int>) { return MPI_UNSIGNED; }
  else if constexpr (std::is_same_v<T, unsigned long>) { return MPI_UNSIGNED_LONG; }
  else if constexpr (std::is_same_v<T, unsigned long long>) { return MPI_UNSIGNED_LONG_LONG; }
  else if constexpr (std::is_same_v<T, float>) { return MPI_FLOAT; }
  else if constexpr (std::is_same_v<T, double>) { return MPI_DOUBLE; }
  else if constexpr (std::is_same_v<T, long double>) { return MPI_LONG_DOUBLE; }
  else if constexpr (std::is_same_v<T, wchar_t>) { return MPI_WCHAR; }
  else if constexpr (std::is_same_v<T, int8_t>) { return MPI_INT8_T; }
  else if constexpr (std::is_same_v<T, int16_t>) { return MPI_INT16_T; }
  else if constexpr (std::is_same_v<T, int32_t>) { return MPI_INT32_T; }
  else if constexpr (std::is_same_v<T, int64_t>) { return MPI_INT64_T; }
  else if constexpr (std::is_same_v<T, uint8_t>) { return MPI_UINT8_T; }
  else if constexpr (std::is_same_v<T, uint16_t>) { return MPI_UINT16_T; }
  else if constexpr (std::is_same_v<T, uint32_t>) { return MPI_UINT32_T; }
  else if constexpr (std::is_same_v<T, uint64_t>) { return MPI_UINT64_T; }
  else if constexpr (std::is_same_v<T, bool>) { return MPI_CXX_BOOL; }
  else if constexpr (std::is_same_v<T, std::complex<float>>) { return MPI_CXX_FLOAT_COMPLEX; }
  else if constexpr (std::is_same_v<T, std::complex<double>>) { return MPI_CXX_DOUBLE_COMPLEX; }
  else if constexpr (std::is_same_v<T, std::complex<long double>>) { return MPI_CXX_LONG_DOUBLE_COMPLEX; }
  else{ static_assert(detail::dependent_always_false<T>::value, "type has unkown map to MPI_Type"); return {}; }
  // empty return statement needed to avoid compiler warning
  return {};
}

template<typename T>
void Broadcast(T* data, int count, int root, MPI_Comm comm) {
  REDEV_FUNCTION_TIMER;
  auto type = getMpiType(T());
  MPI_Bcast(data, count, type, root, comm);
}

/**
 * The InMessageLayout struct contains the arrays defining the arrangement of
 * data in the array returned by Communicator::Recv.
 */
struct InMessageLayout {
  /**
   * Array of source ranks sized NumberOfClientRanks*NumberOfServerRanks.  Each
   * rank reads the entire array once at the start of a communication round.
   * A communication round is defined as a series of sends and receives using
   * the same message layout.
   */
  redev::GOs srcRanks;
  /**
   * Array of size NumberOfReceiverRanks+1 that indicates the segment of the
   * messages array each server rank should read. NumberOfReceiverRanks is
   * defined as the number of ranks calling Communicator::Recv.
   */
  redev::GOs offset;
  /**
   * Set to true if Communicator::Recv has been called and the message layout data set;
   * false otherwise.
   */
  bool knownSizes;
  /**
   * Index into the messages array (returned by Communicator::Recv) where the current process should start
   * reading.
   */
  size_t start;
  /**
   * Number of items (of the user specified type passed to the template
   * parameter of AdiosComm) that should be read from the messages array
   * (returned by Communicator::Recv).
   */
  size_t count;
};

/**
 * The Communicator class provides an abstract interface for sending and
 * receiving messages to/from the client and server.
 * TODO: Split Communicator into Send/Recieve Communicators, bidirectional constructed by composition and can perform both send and receive
 */
template<typename T>
class Communicator {
  public:
    /**
     * Set the arrangement of data in the messages array so that its segments,
     * defined by the offsets array, are sent to the correct destination ranks,
     * defined by the dest array.
     * @param[in] dest array of integers specifying the destination rank for a
     * portion of the msgs array
     * @param[in] offsets array of length |dest|+1 defining the segment of the
     * msgs array (passed to the Send function) being sent to each destination rank.
     * the segment [ msgs[offsets[i]] : msgs[offsets[i+1]] } is sent to rank dest[i]
     */
    virtual void SetOutMessageLayout(LOs& dest, LOs& offsets) = 0;
    /**
     * Send the array.
     * @param[in] msgs array of data to be sent according to the layout specified
     *            with SetOutMessageLayout
     */
    virtual void Send(T* msgs) = 0;
    /**
     * Receive an array. Use AdiosComm's GetInMessageLayout to retreive
     * an instance of the InMessageLayout struct containing the layout of
     * the received array.
     */
    virtual std::vector<T> Recv() = 0;

    virtual InMessageLayout GetInMessageLayout() = 0;
    virtual ~Communicator() = default;
};

template <typename T>
class NoOpComm : public Communicator<T> {
    void SetOutMessageLayout(LOs& dest, LOs& offsets) final {};
    void Send(T* msgs) final {};
    std::vector<T> Recv() final { return {}; }
    InMessageLayout GetInMessageLayout() final { return {}; }
};


/**
 * The AdiosComm class implements the Communicator interface to support sending
 * messages between the clients and server via ADIOS2.  The BP4 and SST ADIOS2
 * engines are currently supported.
 * One AdiosComm object is required for each communication link direction.  For
 * example, for a client and server to both send and receive messages one
 * AdiosComm for client->server messaging and another AdiosComm for
 * server->client messaging are needed. Redev::BidirectionalComm is a helper
 * class for this use case.
 */
template<typename T>
class AdiosComm : public Communicator<T> {
  public:
    /**
     * Create an AdiosComm object.  Collective across sender and receiver ranks.
     * Calls to the constructor from the sender and receiver ranks must be in
     * the same order (i.e., first creating the client-to-server object then the
     * server-to-client link).
     * @param[in] comm_ MPI communicator for sender ranks
     * @param[in] recvRanks_ number of ranks in the receivers MPI communicator
     * @param[in] eng_ ADIOS2 engine for writing on the sender side
     * @param[in] io_ ADIOS2 IO associated with eng_
     * @param[in] name_ unique name among AdiosComm objects
     */
    AdiosComm(MPI_Comm comm_, int recvRanks_, adios2::Engine& eng_, adios2::IO& io_, std::string name_)
      : comm(comm_), recvRanks(recvRanks_), eng(eng_), io(io_), name(name_), verbose(0) {
        inMsg.knownSizes = false;
    }
    
    //rule of 5 en.cppreference.com/w/cpp/language/rule_of_three
    /// destructor to close the engine
    ~AdiosComm() {
      eng.Close();
    }
    /// We are explicitly not allowing copy/move constructor/assignment as we don't
    /// know if the ADIOS2 Engine and IO objects can be safely copied/moved.
    AdiosComm(const AdiosComm& other) = delete;
    AdiosComm(AdiosComm&& other) = delete;
    AdiosComm& operator=(const AdiosComm& other) = delete;
    AdiosComm& operator=(AdiosComm&& other) = delete;

    void SetOutMessageLayout(LOs& dest_, LOs& offsets_) {
      REDEV_FUNCTION_TIMER;
      outMsg = OutMessageLayout{dest_, offsets_};
    }
    void Send(T* msgs) {
      REDEV_FUNCTION_TIMER;
      int rank, commSz;
      MPI_Comm_rank(comm, &rank);
      MPI_Comm_size(comm, &commSz);
      GOs degree(recvRanks,0); //TODO ideally, this would not be needed
      for( auto i=0; i<outMsg.dest.size(); i++) {
        auto destRank = outMsg.dest[i];
        assert(destRank < recvRanks);
        degree[destRank] += outMsg.offsets[i+1] - outMsg.offsets[i];
      }
      GOs rdvRankStart(recvRanks,0);
      auto ret = MPI_Exscan(degree.data(), rdvRankStart.data(), recvRanks,
          getMpiType(redev::GO()), MPI_SUM, comm);
      assert(ret == MPI_SUCCESS);
      if(!rank) {
        //on rank 0 the result of MPI_Exscan is undefined, set it to zero
        rdvRankStart = GOs(recvRanks,0);
      }

      GOs gDegree(recvRanks,0);
      ret = MPI_Allreduce(degree.data(), gDegree.data(), recvRanks,
          getMpiType(redev::GO()), MPI_SUM, comm);
      assert(ret == MPI_SUCCESS);
      const size_t gDegreeTot = static_cast<size_t>(std::accumulate(gDegree.begin(), gDegree.end(), redev::GO(0)));

      GOs gStart(recvRanks,0);
      redev::exclusive_scan(gDegree.begin(), gDegree.end(), gStart.begin(), redev::GO(0));

      //The messages array has a different length on each rank ('irregular') so we don't
      //define local size and count here.
      adios2::Dims shape{static_cast<size_t>(gDegreeTot)};
      adios2::Dims start{};
      adios2::Dims count{};
      if(!rdvVar) {
        rdvVar = io.DefineVariable<T>(name, shape, start, count);
      }
      assert(rdvVar);
      const auto srcRanksName = name+"_srcRanks";
      //The source rank offsets array is the same on each process ('regular').
      adios2::Dims srShape{static_cast<size_t>(commSz*recvRanks)};
      adios2::Dims srStart{static_cast<size_t>(recvRanks*rank)};
      adios2::Dims srCount{static_cast<size_t>(recvRanks)};
      checkStep(eng.BeginStep());

      //send dest rank offsets array from rank 0
      auto offsets = gStart;
      offsets.push_back(gDegreeTot);
      if(!rank) {
        const auto offsetsName = name+"_offsets";
        const auto oShape = offsets.size();
        const auto oStart = 0;
        const auto oCount = offsets.size();
        if(!offsetsVar) {
          offsetsVar = io.DefineVariable<redev::GO>(offsetsName,{oShape},{oStart},{oCount});
          eng.Put<redev::GO>(offsetsVar, offsets.data());
        }
      }

      //send source rank offsets array 'rdvRankStart'
      if(!srcRanksVar) {
        srcRanksVar = io.DefineVariable<redev::GO>(srcRanksName, srShape, srStart, srCount);
        assert(srcRanksVar);
        eng.Put<redev::GO>(srcRanksVar, rdvRankStart.data());
      }

      //assume one call to pack from each rank for now
      for( auto i=0; i<outMsg.dest.size(); i++ ) {
        const auto destRank = outMsg.dest[i];
        const auto lStart = gStart[destRank]+rdvRankStart[destRank];
        const auto lCount = outMsg.offsets[i+1]-outMsg.offsets[i];
        if( lCount > 0 ) {
          start = adios2::Dims{static_cast<size_t>(lStart)};
          count = adios2::Dims{static_cast<size_t>(lCount)};
          rdvVar.SetSelection({start,count});
          eng.Put<T>(rdvVar, &(msgs[outMsg.offsets[i]]));
        }
      }

      eng.PerformPuts();
      eng.EndStep();
    }
    std::vector<T> Recv() {
      REDEV_FUNCTION_TIMER;
      int rank, commSz;
      MPI_Comm_rank(comm, &rank);
      MPI_Comm_size(comm, &commSz);
      auto t1 = redev::getTime();
      checkStep(eng.BeginStep());

      if(!inMsg.knownSizes) {
        auto rdvRanksVar = io.InquireVariable<redev::GO>(name+"_srcRanks");
        assert(rdvRanksVar);
        auto offsetsVar = io.InquireVariable<redev::GO>(name+"_offsets");
        assert(offsetsVar);

        auto offsetsShape = offsetsVar.Shape();
        assert(offsetsShape.size() == 1);
        const auto offSz = offsetsShape[0];
        inMsg.offset.resize(offSz);
        offsetsVar.SetSelection({{0}, {offSz}});
        eng.Get(offsetsVar, inMsg.offset.data());

        auto rdvRanksShape = rdvRanksVar.Shape();
        assert(rdvRanksShape.size() == 1);
        const auto rsrSz = rdvRanksShape[0];
        inMsg.srcRanks.resize(rsrSz);
        rdvRanksVar.SetSelection({{0},{rsrSz}});
        eng.Get(rdvRanksVar, inMsg.srcRanks.data());

        eng.PerformGets();
        inMsg.start = static_cast<size_t>(inMsg.offset[rank]);
        inMsg.count = static_cast<size_t>(inMsg.offset[rank+1]-inMsg.start);
        inMsg.knownSizes = true;
      }
      auto t2 = redev::getTime();

      auto msgsVar = io.InquireVariable<T>(name);
      assert(msgsVar);
      std::vector<T> msgs(inMsg.count);
      if(inMsg.count) {
        //only call Get with non-zero sized reads
        msgsVar.SetSelection({{inMsg.start}, {inMsg.count}});
        eng.Get(msgsVar, msgs.data());
      }

      eng.PerformGets();
      eng.EndStep();
      auto t3 = redev::getTime();
      std::chrono::duration<double> r1 = t2-t1;
      std::chrono::duration<double> r2 = t3-t2;
      if(!rank && verbose) {
        fprintf(stderr, "recv knownSizes %d r1(sec.) r2(sec.) %f %f\n",
            inMsg.knownSizes, r1.count(), r2.count());
      }
      return msgs;
    }
    /**
     * Return the InMessageLayout object.
     * @todo should return const object
     */
    InMessageLayout GetInMessageLayout() {
      return inMsg;
    }
    /**
     * Control the amount of output from AdiosComm functions.  The higher the value the more output is written.
     * @param[in] lvl valid values are [0:5] where 0 is silent and 5 is produces
     *                the most output
     */
    void SetVerbose(int lvl) {
      assert(lvl>=0 && lvl<=5);
      verbose = lvl;
    }
  private:
    MPI_Comm comm;
    int recvRanks;
    adios2::Engine& eng;
    adios2::IO& io;
    adios2::Variable<T> rdvVar;
    adios2::Variable<redev::GO> srcRanksVar;
    adios2::Variable<redev::GO> offsetsVar;
    std::string name;
    //support only one call to pack for now...
    struct OutMessageLayout {
      LOs dest;
      LOs offsets;
    } outMsg;
    int verbose;
    //receive side state
    InMessageLayout inMsg;
};

}
