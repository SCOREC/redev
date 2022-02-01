#include <redev.h>
#include <cassert>
#include "redev_git_version.h"
#include "redev_comm.h"
#include "redev_profile.h"
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include <string>         // std::stoi
#include <algorithm>      // std::transform

namespace {
  //Ci = case insensitive
  bool isSameCi(std::string s1, std::string s2) {
    REDEV_FUNCTION_TIMER;
    std::transform(s1.begin(), s1.end(), s1.begin(), ::toupper);
    std::transform(s2.begin(), s2.end(), s2.begin(), ::toupper);
    return s1 == s2;
  }

  //Wait for the file to be created by the writer.
  //Assuming that if 'Streaming' and 'OpenTimeoutSecs' are set then we are in
  //BP4 mode.  SST blocks on Open by default.
  void waitForEngineCreation(adios2::IO& io) {
    REDEV_FUNCTION_TIMER;
    auto params = io.Parameters();
    bool isStreaming = params.count("Streaming") && isSameCi(params["Streaming"],"ON");
    bool timeoutSet = params.count("OpenTimeoutSecs") && std::stoi(params["OpenTimeoutSecs"]) > 0;
    bool isSST = isSameCi(io.EngineType(),"SST");
    if( (isStreaming && timeoutSet) || isSST ) return;
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
}

namespace redev {

  //TODO consider moving the RCBPtn source to another file
  RCBPtn::RCBPtn() {}

  RCBPtn::RCBPtn(redev::LO dim_)
    : dim(dim_) {
    assert(dim>0 && dim<=3);
  }

  RCBPtn::RCBPtn(redev::LO dim_, std::vector<int>& ranks_, std::vector<double>& cuts_)
    : dim(dim_), ranks(ranks_), cuts(cuts_) {
    assert(dim>0 && dim<=3);
  }

  redev::LO RCBPtn::GetRank(std::array<redev::Real,3>& pt) { //TODO better name?
    REDEV_FUNCTION_TIMER;
    assert(ranks.size() && cuts.size());
    assert(dim>0 && dim<=3);
    const auto len = cuts.size();
    const auto levels = std::log2(len);
    auto lvl = 0;
    auto idx = 1;
    auto d = 0;
    while(lvl < levels) {
      if(pt[d]<cuts[idx])
        idx = idx*2;
      else
        idx = idx*2+1;
      ++lvl;
      d = (d + 1) % dim;
    }
    auto rankIdx = idx-std::pow(2,lvl);
    assert(rankIdx < ranks.size());
    return ranks[rankIdx];
  }

  std::vector<redev::LO>& RCBPtn::GetRanks() {
    REDEV_FUNCTION_TIMER;
    return ranks;
  }

  std::vector<redev::Real>& RCBPtn::GetCuts() {
    REDEV_FUNCTION_TIMER;
    return cuts;
  }

  void RCBPtn::Write(adios2::Engine& eng, adios2::IO& io) {
    REDEV_FUNCTION_TIMER;
    const auto len = ranks.size();
    assert(len>=1);
    assert(len==cuts.size());
    auto ranksVar = io.DefineVariable<redev::LO>(ranksVarName,{},{},{len});
    auto cutsVar = io.DefineVariable<redev::Real>(cutsVarName,{},{},{len});
    eng.Put(ranksVar, ranks.data());
    eng.Put(cutsVar, cuts.data());
  }

  void RCBPtn::Read(adios2::Engine& eng, adios2::IO& io) {
    REDEV_FUNCTION_TIMER;
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
    REDEV_FUNCTION_TIMER;
    redev::Broadcast(ranks.data(), ranks.size(), root, comm);
    redev::Broadcast(cuts.data(), cuts.size(), root, comm);
  }

  // SST support
  // - with a rendezvous + non-rendezvous application pair
  // - with only a rendezvous application for debugging/testing
  void Redev::openEnginesBP4(bool noParticipant) {
    REDEV_FUNCTION_TIMER;
    //create the engine writers at the same time - BP4 does not wait for the readers (SST does)
    if(isRendezvous) {
      fromEng = io.Open(bpFromName, adios2::Mode::Write);
      assert(fromEng);
    } else {
      toEng = io.Open(bpToName, adios2::Mode::Write);
      assert(toEng);
    }
    waitForEngineCreation(io);
    //create engines for reading
    if(isRendezvous) {
      if(noParticipant==false) { //support unit testing
        toEng = io.Open(bpToName, adios2::Mode::Read);
        assert(toEng);
      }
    } else {
      fromEng = io.Open(bpFromName, adios2::Mode::Read);
      assert(fromEng);
    }
  }

  // BP4 support
  // - with a rendezvous + non-rendezvous application pair
  // - with only a rendezvous application for debugging/testing
  // - in streaming and non-streaming modes; non-streaming requires 'waitForEngineCreation'
  void Redev::openEnginesSST(bool noParticipant) {
    REDEV_FUNCTION_TIMER;
    //create one engine's reader and writer pair at a time - SST blocks on open(read)
    if(isRendezvous) {
      fromEng = io.Open(bpFromName, adios2::Mode::Write);
    } else {
      fromEng = io.Open(bpFromName, adios2::Mode::Read);
    }
    assert(fromEng);
    if(isRendezvous) {
      if(noParticipant==false) { //support unit testing
        toEng = io.Open(bpToName, adios2::Mode::Read);
        assert(toEng);
      }
    } else {
      toEng = io.Open(bpToName, adios2::Mode::Write);
      assert(toEng);
    }
  }

  Redev::Redev(MPI_Comm comm_, Partition& ptn_, bool isRendezvous_, bool noParticipant)
    : comm(comm_), adios("adios2.yaml", comm), ptn(ptn_), isRendezvous(isRendezvous_) {
    REDEV_FUNCTION_TIMER;
    int isInitialized = 0;
    MPI_Initialized(&isInitialized);
    assert(isInitialized);
    MPI_Comm_rank(comm, &rank);
    if(!rank) {
      std::cout << "Redev Git Hash: " << redevGitHash << "\n";
    }
    io = adios.DeclareIO("rendezvous"); //this will likely change
    if(noParticipant) io.SetEngine("BP4"); //SST hangs if there is no reader
    if( isSameCi(io.EngineType(), "SST") ) {
      openEnginesSST(noParticipant);
    } else if( isSameCi(io.EngineType(), "BP4") ) {
      openEnginesBP4(noParticipant);
    } else {
      if(!rank) {
        std::cerr << "ERROR: redev does not support ADIOS2 engine " << io.EngineType() << "\n";
      }
      exit(EXIT_FAILURE);
    }
  }

  Redev::~Redev() {
    REDEV_FUNCTION_TIMER;
    if(toEng)
      toEng.Close();
    if(fromEng)
      fromEng.Close();
  }

  void Redev::CheckVersion(adios2::Engine& eng, adios2::IO& io) {
    REDEV_FUNCTION_TIMER;
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
    REDEV_FUNCTION_TIMER;
    CheckVersion(fromEng,io);
    fromEng.BeginStep();
    //rendezvous app rank 0 writes partition info and other apps read
    if(!rank) {
      if(isRendezvous)
        ptn.Write(fromEng,io);
      else
        ptn.Read(fromEng,io);
    }
    fromEng.EndStep();
    ptn.Broadcast(comm);
  }

}
