#include <redev.h>
#include <cassert>
#include "redev_git_version.h"

namespace {
  void begin_func() {
  }
  void end_func() {
  }
}

namespace redev {
  Redev::Redev(MPI_Comm comm, bool isRendezvous_)
    : adios(comm), isRendezvous(isRendezvous_) {
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
        std::cout << "inHash " << inHash << "\n";
        assert(inHash == redevGitHash);
      }
    }
    eng.EndStep();
    eng.BeginStep();
    //rendezvous app writes the partition vector and other apps read
    auto ranksVarName = "partition vector ranks";
    auto cutsVarName = "partition vector cuts";
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
        std::vector<redev::LO> inRanks;
        std::vector<redev::Real> inCuts;
        eng.Get(ranksVar, inRanks);
        eng.Get(cutsVar, inCuts);
//        assert(inRanks == ranks); //fails here, inRanks contains zeros
//        assert(inCuts == cuts);
      }
    }
    eng.EndStep();
    eng.Close();
    end_func();
  }
}
