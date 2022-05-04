#pragma once
#include "redev.h"
#include "redev_profile.h"
#include "redev_assert.h"
#include <numeric> // accumulate, exclusive_scan
#include <type_traits> // is_same

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
  if constexpr (std::is_same_v<T, double>) { return MPI_DOUBLE; }
  else if constexpr (std::is_same_v<T, std::complex<double>>) { return MPI_DOUBLE_COMPLEX; }
  else if constexpr (std::is_same_v<T, int64_t>) { return MPI_INT64_T; }
  else if constexpr (std::is_same_v<T, int32_t>) { return MPI_INT32_T; }
  else{ static_assert(detail::dependent_always_false<T>::value, "type has unkown map to MPI_Type"); return {}; }
}

template<typename T>
void Broadcast(T* data, int count, int root, MPI_Comm comm) {
  REDEV_FUNCTION_TIMER;
  auto type = getMpiType(T());
  MPI_Bcast(data, count, type, root, comm);
}

template<typename T>
class Communicator {
  public:
    virtual void SetOutMessageLayout(LOs& dest, LOs& offsets) = 0;
    virtual void Send(T* msgs) = 0;
    virtual std::vector<T> Recv() = 0;
};

struct InMessageLayout {
  redev::GOs srcRanks;
  redev::GOs offset;
  bool knownSizes;
  size_t start;
  size_t count;
};

template<typename T>
class AdiosComm : public Communicator<T> {
  public:
    AdiosComm(MPI_Comm comm_, int rdvRanks_, adios2::Engine& eng_, adios2::IO& io_, std::string name_)
      : comm(comm_), rdvRanks(rdvRanks_), eng(eng_), io(io_), name(name_), verbose(0) {
        inMsg.knownSizes = false;
    }
    void SetOutMessageLayout(LOs& dest_, LOs& offsets_) {
      REDEV_FUNCTION_TIMER;
      outMsg = OutMessageLayout{dest_, offsets_};
    }
    void Send(T* msgs) {
      REDEV_FUNCTION_TIMER;
      int rank, commSz;
      MPI_Comm_rank(comm, &rank);
      MPI_Comm_size(comm, &commSz);
      // The assumption here is that the communicator
      // used by the rendezvous application is orders
      // of magnitude smaller than the communicator
      // used by the largest application.  For example,
      // XGC may use 1024 ranks, GENE 16, and the coupler
      // (the rendezvous application) 16. Given this,
      // allocating an array with length equal to the
      // rendevous communicator size is acceptable.
      GOs degree(rdvRanks,0);
      for( auto i=0; i<outMsg.dest.size(); i++) {
        auto destRank = outMsg.dest[i];
        assert(destRank < rdvRanks);
        degree[destRank] += outMsg.offsets[i+1] - outMsg.offsets[i];
      }
      GOs rdvRankStart(rdvRanks,0);
      auto ret = MPI_Exscan(degree.data(), rdvRankStart.data(), rdvRanks,
          getMpiType(redev::GO()), MPI_SUM, comm);
      assert(ret == MPI_SUCCESS);
      if(!rank) {
        //on rank 0 the result of MPI_Exscan is undefined, set it to zero
        rdvRankStart = GOs(rdvRanks,0);
      }

      GOs gDegree(rdvRanks,0);
      ret = MPI_Allreduce(degree.data(), gDegree.data(), rdvRanks,
          getMpiType(redev::GO()), MPI_SUM, comm);
      assert(ret == MPI_SUCCESS);
      const size_t gDegreeTot = static_cast<size_t>(std::accumulate(gDegree.begin(), gDegree.end(), redev::GO(0)));

      GOs gStart(rdvRanks,0);
      std::exclusive_scan(gDegree.begin(), gDegree.end(), gStart.begin(), redev::GO(0));

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
      adios2::Dims srShape{static_cast<size_t>(commSz*rdvRanks)};
      adios2::Dims srStart{static_cast<size_t>(rdvRanks*rank)};
      adios2::Dims srCount{static_cast<size_t>(rdvRanks)};
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
    InMessageLayout GetInMessageLayout() {
      return inMsg;
    }
    //the higher the value the more output is written
    //verbose=0 is silent
    void SetVerbose(int lvl) {
      assert(lvl>=0 && lvl<=5);
      verbose = lvl;
    }
  private:
    MPI_Comm comm;
    int rdvRanks;
    adios2::Engine eng;
    adios2::IO io;
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
