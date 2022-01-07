#include <redev.h>
#include <cassert>
#include <type_traits> // is_same
#include "redev_git_version.h"

namespace {
  void begin_func() {
  }
  void end_func() {
  }

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
      mpitype = MPI_UNSIGNED_LONG;
    } else if ( std::is_same<T, redev::LO>::value ) {
      mpitype = MPI_INT;
    } else {
      assert(false);
      fprintf(stderr, "Unknown type in %s... exiting\n", __func__);
      exit(EXIT_FAILURE);
    }
    return mpitype;
  }

  template<typename T>
  void Broadcast(T* data, int count, int root, MPI_Comm comm) {
    begin_func();
    auto type = getMpiType(T());
    MPI_Bcast(data, count, type, root, comm);
    end_func();
  }

}

namespace redev {

  //TODO consider moving the RCBPtn source to another file
  RCBPtn::RCBPtn() {}

  RCBPtn::RCBPtn(std::vector<int>& ranks_, std::vector<double>& cuts_)
    : ranks(ranks_), cuts(cuts_) {}

  redev::LO RCBPtn::GetRank(redev::Real coords[3]) {
    begin_func();
    assert(ranks.size() && cuts.size());
    assert(false); //TODO implement this
    end_func();
    return 0;
  }

  std::vector<redev::LO>& RCBPtn::GetRanks() {
    return ranks;
  }

  std::vector<redev::Real>& RCBPtn::GetCuts() {
    return cuts;
  }

  void RCBPtn::Write(adios2::Engine& eng, adios2::IO& io) {
    const auto len = ranks.size();
    assert(len>=1);
    auto ranksVar = io.DefineVariable<redev::LO>(ranksVarName,{},{},{len});
    auto cutsVar = io.DefineVariable<redev::Real>(cutsVarName,{},{},{len-1});
    eng.Put(ranksVar, ranks.data());
    eng.Put(cutsVar, cuts.data());
  }

  void RCBPtn::Read(adios2::Engine& eng, adios2::IO& io) {
    const auto step = eng.CurrentStep();
    auto ranksVar = io.InquireVariable<redev::LO>(ranksVarName);
    auto cutsVar = io.InquireVariable<redev::Real>(cutsVarName);
    assert(ranksVar && cutsVar);

    auto blocksInfo = eng.BlocksInfo(ranksVar,step);
    assert(blocksInfo.size()==1);
    ranksVar.SetBlockSelection(blocksInfo[0].BlockID);
    eng.Get(ranksVar, ranks);

    blocksInfo = eng.BlocksInfo(ranksVar,step);
    assert(blocksInfo.size()==1);
    ranksVar.SetBlockSelection(blocksInfo[0].BlockID);
    eng.Get(cutsVar, cuts);
    eng.PerformGets(); //default read mode is deferred
  }

  void RCBPtn::Broadcast(MPI_Comm comm, int root) {
    ::Broadcast(ranks.data(), ranks.size(), root, comm);
    ::Broadcast(cuts.data(), cuts.size(), root, comm);
  }

  Redev::Redev(MPI_Comm comm_, Partition& ptn_, bool isRendezvous_)
    : comm(comm_), adios(comm), ptn(ptn_), isRendezvous(isRendezvous_) {
    begin_func();
    int isInitialized = 0;
    MPI_Initialized(&isInitialized);
    assert(isInitialized);
    MPI_Comm_rank(comm, &rank);
    if(!rank) {
      std::cout << "Redev Git Hash: " << redevGitHash << "\n";
    }
    io = adios.DeclareIO("rendezvous"); //this will likely change
    const auto bpName = "rendevous.bp";
    const auto mode = isRendezvous ? adios2::Mode::Write : adios2::Mode::Read;
    auto eng = io.Open(bpName, mode);
    CheckVersion(eng,io);
    end_func();
  }

  void Redev::CheckVersion(adios2::Engine& eng, adios2::IO& io) {
    const auto hashVarName = "redev git hash";
    eng.BeginStep();
    //rendezvous app writes the version it has and other apps read
    if(isRendezvous) {
      auto varVersion = io.DefineVariable<std::string>(hashVarName);
      if(!rank)
        eng.Put(varVersion, std::string(redevGitHash));
    }
    else {
      auto varVersion = io.InquireVariable<std::string>(hashVarName);
      std::string inHash;
      if(varVersion && !rank) {
        eng.Get(varVersion, inHash);
        eng.PerformGets(); //default read mode is deferred
        std::cout << "inHash " << inHash << "\n";
        assert(inHash == redevGitHash);
      }
    }
    eng.EndStep();
  }

  void Redev::Setup() {
    begin_func();
    eng.BeginStep();
    //rendezvous app rank 0 writes partition info and other apps read
    if(!rank) {
      if(isRendezvous)
        ptn.Write(eng,io);
      else
        ptn.Read(eng,io);
    }
    eng.EndStep();
    eng.Close();
    ptn.Broadcast(comm);
    end_func();
  }

}
