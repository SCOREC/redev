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
}



namespace redev {
  Redev::Redev(MPI_Comm comm_, bool isRendezvous_)
    : comm(comm_), adios(comm), isRendezvous(isRendezvous_) {
    begin_func();
    int isInitialized = 0;
    MPI_Initialized(&isInitialized);
    assert(isInitialized);
    MPI_Comm_rank(comm, &rank);
    if(!rank) {
      std::cout << "Redev Git Hash: " << redevGitHash << "\n";
    }
    end_func();
  }
  void Redev::Setup(std::vector<int>& ranks, std::vector<double>& cuts) {
    begin_func();
    io = adios.DeclareIO("rendezvous"); //this will likely change
    const auto hashVarName = "redev git hash";
    const auto bpName = "rendevous.bp";
    const auto mode = isRendezvous ? adios2::Mode::Write : adios2::Mode::Read;
    auto eng = io.Open(bpName, mode);
    //rendezvous app writes the version it has and other apps read
    eng.BeginStep();
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
    eng.BeginStep();
    //rendezvous app writes the partition vector and other apps read
    auto ranksVarName = "partition vector ranks";
    auto cutsVarName = "partition vector cuts";
    std::vector<redev::LO> inRanks;
    std::vector<redev::Real> inCuts;
    if(isRendezvous && !rank) {
      const auto len = ranks.size();
      std::cout << "writing " << len << "\n";
      auto ranksVar = io.DefineVariable<redev::LO>(ranksVarName,{},{},{len});
      auto cutsVar = io.DefineVariable<redev::Real>(cutsVarName,{},{},{len-1});
      eng.Put(ranksVar, ranks.data());
      eng.Put(cutsVar, cuts.data());
    }
    else { //all ranks need the partition vector; read on rank 0 then broadcast
      if(!rank) {
        const auto step = eng.CurrentStep();
        auto ranksVar = io.InquireVariable<redev::LO>(ranksVarName);
        auto cutsVar = io.InquireVariable<redev::Real>(cutsVarName);
        assert(ranksVar && cutsVar);

        auto blocksInfo = eng.BlocksInfo(ranksVar,step);
        assert(blocksInfo.size()==1);
        ranksVar.SetBlockSelection(blocksInfo[0].BlockID);
        eng.Get(ranksVar, inRanks);

        blocksInfo = eng.BlocksInfo(ranksVar,step);
        assert(blocksInfo.size()==1);
        ranksVar.SetBlockSelection(blocksInfo[0].BlockID);
        eng.Get(cutsVar, inCuts);

        eng.PerformGets(); //default read mode is deferred
        //TODO check if ranks and cuts are valid before the asserts
        assert(inRanks == ranks);
        assert(inCuts == cuts);
      }
    }
    eng.EndStep();
    eng.Close();
    Broadcast(inRanks.data(), inRanks.size(), 0);
    end_func();
  }

  template<typename T>
  void Redev::Broadcast(T* data, int count, int root) {
    begin_func();
    auto type = getMpiType(T());
    MPI_Bcast(data, count, type, root, comm);
    end_func();
  }
}
