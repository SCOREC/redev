#pragma once
#include "redev.h"
#include <numeric> // accumulate, esclusive_scan
#include <type_traits> // is_same

namespace redev {

//TODO I think there is a cleaner way
template<class T>
MPI_Datatype getMpiType(T) {
  MPI_Datatype mpitype;
  //determine the type based on what is being sent
  if( std::is_same<T, redev::Real>::value ) {
    mpitype = MPI_DOUBLE;
  } else if ( std::is_same<T, redev::CV>::value ) {
    //https://www.mpi-forum.org/docs/mpi-3.1/mpi31-report/node48.htm
    mpitype = MPI_CXX_DOUBLE_COMPLEX;
  } else if ( std::is_same<T, redev::GO>::value ) {
    mpitype = MPI_INT64_T;
  } else if ( std::is_same<T, redev::LO>::value ) {
    mpitype = MPI_INT32_T;
  } else {
    assert(false);
    fprintf(stderr, "Unknown type in %s... exiting\n", __func__);
    exit(EXIT_FAILURE);
  }
  return mpitype;
}

template<typename T>
void Broadcast(T* data, int count, int root, MPI_Comm comm) {
  //begin_func();
  auto type = getMpiType(T());
  MPI_Bcast(data, count, type, root, comm);
  //end_func();
}

template<typename T>
class Communicator {
  public:
    virtual void Pack(LOs& dest, LOs& offsets, T* msgs) = 0;
    virtual void Send() = 0;
    virtual void Unpack(GOs& rdvRanks, GOs& offsets, T*& msgs) = 0;
};

template<typename T>
class AdiosComm : public Communicator<T> {
  public:
    AdiosComm(MPI_Comm comm_, int rdvRanks_, adios2::Engine& eng_, adios2::IO& io_, std::string name_)
      : comm(comm_), rdvRanks(rdvRanks_), eng(eng_), io(io_), name(name_) {
    }
    void Pack(LOs& dest_, LOs& offsets_, T* msgs_) {
      Msg m(dest_, offsets_, msgs_);
      packed.push_back(m);
    }
    void Send() {
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
      for( auto p : packed ) {
        for( auto i=0; i<p.dest.size(); i++) {
          auto destRank = p.dest[i];
          assert(destRank < rdvRanks);
          degree[destRank] += p.offsets[i+1] - p.offsets[i];
        }
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
      const size_t gDegreeTot = static_cast<size_t>(std::accumulate(gDegree.begin(), gDegree.end(), 0));

      GOs gStart(rdvRanks,0);
      std::exclusive_scan(gDegree.begin(), gDegree.end(), gStart.begin(), 0);

      //The messages array has a different length on each rank ('irregular') so we don't
      //define local size and count here.
      adios2::Dims shape{static_cast<size_t>(gDegreeTot)};
      adios2::Dims start{};
      adios2::Dims count{};
      auto var = io.DefineVariable<T>(name, shape, start, count);
      assert(var);
      const auto srcRanksName = name+"_srcRanks";
      //The source rank offsets array is the same on each process ('regular').
      adios2::Dims srShape{static_cast<size_t>(commSz*rdvRanks)};
      adios2::Dims srStart{static_cast<size_t>(rdvRanks*rank)};
      adios2::Dims srCount{static_cast<size_t>(rdvRanks)};
      auto srcRanksVar = io.DefineVariable<redev::GO>(srcRanksName, srShape, srStart, srCount);
      assert(srcRanksVar);
      eng.BeginStep();

      //send source rank offsets array 'rdvRankStart'
      eng.Put<redev::GO>(srcRanksVar, rdvRankStart.data());

      //assume one call to pack from each rank for now
      assert(packed.size() == 1);
      auto p = packed[0];
      for( auto i=0; i<p.dest.size(); i++ ) {
        const auto destRank = p.dest[i];
        const auto lStart = gStart[destRank]+rdvRankStart[destRank];
        const auto lCount = p.offsets[i+1]-p.offsets[i];
        if( lCount > 0 ) {
          start = adios2::Dims{static_cast<size_t>(lStart)};
          count = adios2::Dims{static_cast<size_t>(lCount)};
          var.SetSelection({start,count});
          eng.Put<T>(var, p.msgs);
        }
      }

      //send dest rank offsets array from rank 0
      auto offsets = gStart;
      offsets.push_back(gDegreeTot);
      if(!rank) {
        const auto offsetsName = name+"_offsets";
        const auto shape = offsets.size();
        const auto start = 0;
        const auto count = offsets.size();
        auto offsetsVar = io.DefineVariable<redev::GO>(offsetsName,{shape},{start},{count});
        eng.Put<redev::GO>(offsetsVar, offsets.data());
      }
      eng.PerformPuts();
      eng.EndStep();
    }
    void Unpack(GOs& rdvSrcRanks, GOs& offsets, T*& msgs) {
      int rank, commSz;
      MPI_Comm_rank(comm, &rank);
      MPI_Comm_size(comm, &commSz);
      eng.BeginStep();

      auto rdvRanksVar = io.InquireVariable<redev::GO>(name+"_srcRanks");
      assert(rdvRanksVar);
      auto offsetsVar = io.InquireVariable<redev::GO>(name+"_offsets");
      assert(offsetsVar);

      auto offsetsShape = offsetsVar.Shape();
      assert(offsetsShape.size() == 1);
      const auto offSz = offsetsShape[0];
      offsets.resize(offSz);
      offsetsVar.SetSelection({{0}, {offSz}});
      eng.Get(offsetsVar, offsets.data());

      auto rdvRanksShape = rdvRanksVar.Shape();
      assert(rdvRanksShape.size() == 1);
      const auto rsrSz = rdvRanksShape[0];
      rdvSrcRanks.resize(rsrSz);
      rdvRanksVar.SetSelection({{0},{rsrSz}});
      eng.Get(rdvRanksVar, rdvSrcRanks.data());

      eng.PerformGets();

      auto msgsVar = io.InquireVariable<T>(name);
      assert(msgsVar);
      const auto start = static_cast<size_t>(offsets[rank]);
      const auto count = static_cast<size_t>(offsets[rank+1]-start);
      msgs = new T[count];
      msgsVar.SetSelection({{start}, {count}});
      eng.Get(msgsVar, msgs);

      eng.PerformGets();
      eng.EndStep();
    }
  private:
    MPI_Comm comm;
    int rdvRanks;
    adios2::Engine eng;
    adios2::IO io;
    std::string name;
    //support only one call to pack for now...
    struct Msg {
      Msg(LOs& dest_, LOs& offsets_, T* msgs_)
        : dest(dest_), offsets(offsets_), msgs(msgs_) { }
      LOs& dest;
      LOs& offsets;
      T* msgs;
    };
    std::vector<Msg> packed;
};

}
