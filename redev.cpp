#include <redev.h>
#include <cassert>
#include "redev_git_version.h"
#include "redev_comm.h"
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds

namespace {
  void begin_func() {
  }
  void end_func() {
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

  redev::LO RCBPtn::GetRank(redev::Real pt[3]) { //TODO better name?
    //begin_func(); //TODO fix this by creating a hook object that goes out of scope
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
    return ranks;
  }

  std::vector<redev::Real>& RCBPtn::GetCuts() {
    return cuts;
  }

  void RCBPtn::Write(adios2::Engine& eng, adios2::IO& io) {
    const auto len = ranks.size();
    assert(len>=1);
    assert(len==cuts.size());
    auto ranksVar = io.DefineVariable<redev::LO>(ranksVarName,{},{},{len});
    auto cutsVar = io.DefineVariable<redev::Real>(cutsVarName,{},{},{len});
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
    redev::Broadcast(ranks.data(), ranks.size(), root, comm);
    redev::Broadcast(cuts.data(), cuts.size(), root, comm);
  }

  Redev::Redev(MPI_Comm comm_, Partition& ptn_, bool isRendezvous_, bool noParticipant)
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
    //engine for data sent from rendezvous
    auto bpName = "fromRendevous.bp";
    auto mode = isRendezvous ? adios2::Mode::Write : adios2::Mode::Read;
    //wait for the file to be created by the writer
    if(!isRendezvous) std::this_thread::sleep_for(std::chrono::seconds(1));
    fromEng = io.Open(bpName, mode);
    assert(fromEng);

    if(noParticipant) {
      end_func();
      return;
    }

    //engine for data sent to rendezvous
    bpName = "toRendevous.bp";
    mode = isRendezvous ? adios2::Mode::Read : adios2::Mode::Write;
    //wait for the file to be created by the writer
    if(isRendezvous) std::this_thread::sleep_for(std::chrono::seconds(2)); //better way?
    toEng = io.Open(bpName, mode);
    assert(toEng);
    end_func();
  }

  Redev::~Redev() {
    if(toEng)
      toEng.Close();
    if(fromEng)
      fromEng.Close();
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
    end_func();
  }

}
