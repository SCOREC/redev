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

  //TODO consider moving the ClassPtn source to another file
  ClassPtn::ClassPtn() {}

  ClassPtn::ClassPtn(const redev::LOs& ranks_, const ModelEntVec& ents) {
    assert(ranks_.size() == ents.size());
    if( ! ModelEntDimsValid(ents) ) exit(EXIT_FAILURE);
    for(auto i=0; i<ranks_.size(); i++) {
      modelEntToRank[ents[i]] = ranks_[i];
    }
  }

  bool ClassPtn::ModelEntDimsValid(const ClassPtn::ModelEntVec& ents) const {
    auto badDim = [](ModelEnt e){ return (e.first<0 || e.first>3); };
    auto res = std::find_if(begin(ents), end(ents), badDim);
    if (res != std::end(ents)) {
      std::stringstream ss;
      ss << "ERROR: a ModelEnt contains an invalid dimension: "
         << res->first << "... exiting\n";
      std::cerr << ss.str();
    }
    return (res == std::end(ents));
  }

  redev::LO ClassPtn::GetRank(ModelEnt ent) const {
    REDEV_FUNCTION_TIMER;
    REDEV_ALWAYS_ASSERT(ent.first>=0 && ent.first <=3); //check for valid dimension
    assert(modelEntToRank.size());
    assert(modelEntToRank.count(ent));
    return modelEntToRank.at(ent);
  }

  redev::LOs ClassPtn::GetRanks() const {
    REDEV_FUNCTION_TIMER;
    redev::LOs ranks(modelEntToRank.size());
    int i=0;
    for(const auto& iter : modelEntToRank) {
      ranks[i++]=iter.second;
    }
    return ranks;
  }

  ClassPtn::ModelEntVec ClassPtn::GetModelEnts() const {
    REDEV_FUNCTION_TIMER;
    ModelEntVec ents(modelEntToRank.size());
    int i=0;
    for(const auto& iter : modelEntToRank) {
      ents[i++]=iter.first;
    }
    return ents;
  }

  redev::LOs ClassPtn::SerializeModelEntsAndRanks() const {
    REDEV_FUNCTION_TIMER;
    const auto stride = 3;
    const auto numEnts = modelEntToRank.size();
    redev::LOs entsAndRanks;
    entsAndRanks.reserve(numEnts*stride);
    for(const auto& iter : modelEntToRank) {
      auto ent = iter.first;
      entsAndRanks.push_back(ent.first);   //dim
      entsAndRanks.push_back(ent.second);  //id
      entsAndRanks.push_back(iter.second); //rank
    }
    REDEV_ALWAYS_ASSERT(entsAndRanks.size()==numEnts*stride);
    return entsAndRanks;
  }

  ClassPtn::ModelEntToRank ClassPtn::DeserializeModelEntsAndRanks(const redev::LOs& serialized) const {
    REDEV_FUNCTION_TIMER;
    const auto stride = 3;
    REDEV_ALWAYS_ASSERT(serialized.size()%stride==0);
    ModelEntToRank me2r;
    for(size_t i=0; i<serialized.size(); i+=stride) {
      const auto dim=serialized[i];
      REDEV_ALWAYS_ASSERT(dim>=0 && dim<=3);
      const auto id=serialized[i+1];
      const auto rank=serialized[i+2];
      ModelEnt ent(dim,id);
      if(me2r.count(ent))
        REDEV_ALWAYS_ASSERT(me2r[ent] == rank);
      me2r[ModelEnt(dim,id)] = rank;
    }
    return me2r;
  }

  void ClassPtn::Write(adios2::Engine& eng, adios2::IO& io) {
    REDEV_FUNCTION_TIMER;
    auto serialized = SerializeModelEntsAndRanks();
    const auto len = serialized.size();
    auto entsAndRanksVar = io.DefineVariable<redev::LO>(entsAndRanksVarName,{},{},{len});
    eng.Put(entsAndRanksVar, serialized.data());
    eng.PerformPuts();
  }

  void ClassPtn::Read(adios2::Engine& eng, adios2::IO& io) {
    REDEV_FUNCTION_TIMER;
    const auto step = eng.CurrentStep();
    auto entsAndRanksVar = io.InquireVariable<redev::LO>(entsAndRanksVarName);
    assert(entsAndRanksVar);

    auto blocksInfo = eng.BlocksInfo(entsAndRanksVar,step);
    assert(blocksInfo.size()==1);
    entsAndRanksVar.SetBlockSelection(blocksInfo[0].BlockID);
    redev::LOs serialized;
    eng.Get(entsAndRanksVar, serialized);
    eng.PerformGets(); //default read mode is deferred

    modelEntToRank = DeserializeModelEntsAndRanks(serialized);
  }

  void ClassPtn::Gather(MPI_Comm comm, int root) {
    REDEV_FUNCTION_TIMER;
    int rank, commSize;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &commSize);
    auto degree = !rank ? redev::LOs(commSize+1) : redev::LOs();
    auto serialized = SerializeModelEntsAndRanks();
    int len = static_cast<int>(serialized.size());
    MPI_Gather(&len,1,MPI_INT,degree.data(),1,MPI_INT,root,comm);
    if(root==rank) {
      auto offset = redev::LOs(commSize+1);
      std::exclusive_scan(degree.begin(), degree.end(), offset.begin(), redev::LO(0));
      auto allSerialized = redev::LOs(offset.back());
      MPI_Gatherv(serialized.data(), len, MPI_INT, allSerialized.data(),
          degree.data(), offset.data(), MPI_INT, root, MPI_COMM_WORLD);
      modelEntToRank = DeserializeModelEntsAndRanks(allSerialized);
    } else {
      MPI_Gatherv(serialized.data(), len, MPI_INT, NULL, NULL, NULL, MPI_INT, root, MPI_COMM_WORLD);
    }
  }

  void ClassPtn::Broadcast(MPI_Comm comm, int root) {
    REDEV_FUNCTION_TIMER;
    int rank;
    MPI_Comm_rank(comm, &rank);
    auto serialized = SerializeModelEntsAndRanks();
    int len = static_cast<int>(serialized.size());
    redev::Broadcast(&len, 1, root, comm);
    if(root != rank) {
      serialized.resize(len);
    }
    redev::Broadcast(serialized.data(), serialized.size(), root, comm);
    if(root != rank) {
      modelEntToRank = DeserializeModelEntsAndRanks(serialized);
    }
  }


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

  redev::LO RCBPtn::GetRank(std::array<redev::Real,3>& pt) const { //TODO better name?
    REDEV_FUNCTION_TIMER;
    assert(ranks.size() && cuts.size());
    assert(dim>0 && dim<=3);
    const auto len = cuts.size();
    const auto levels = std::log2(len);
    auto lvl = 0;
    auto idx = 1;
    auto d = 0;
    while(lvl < levels) {
      if(pt[d]<cuts.at(idx))
        idx = idx*2;
      else
        idx = idx*2+1;
      ++lvl;
      d = (d + 1) % dim;
    }
    auto rankIdx = idx-std::pow(2,lvl);
    assert(rankIdx < ranks.size());
    return ranks.at(rankIdx);
  }

  std::vector<redev::LO> RCBPtn::GetRanks() const {
    REDEV_FUNCTION_TIMER;
    return ranks;
  }

  std::vector<redev::Real> RCBPtn::GetCuts() const {
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
    eng.PerformPuts();
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
    int rank;
    MPI_Comm_rank(comm, &rank);
    int count = ranks.size();
    redev::Broadcast(&count, 1, root, comm);
    if(root != rank) {
      ranks.resize(count);
      cuts.resize(count);
    }
    redev::Broadcast(ranks.data(), ranks.size(), root, comm);
    redev::Broadcast(cuts.data(), cuts.size(), root, comm);
  }

  // BP4 support
  // - with a rendezvous + non-rendezvous application pair
  // - with only a rendezvous application for debugging/testing
  // - in streaming and non-streaming modes; non-streaming requires 'waitForEngineCreation'
  void Redev::openEnginesBP4(bool noParticipant) {
    REDEV_FUNCTION_TIMER;
    //create the engine writers at the same time - BP4 does not wait for the readers (SST does)
    if(isRendezvous) {
      fromEng = fromIo.Open(bpFromName, adios2::Mode::Write);
      assert(fromEng);
    } else {
      toEng = toIo.Open(bpToName, adios2::Mode::Write);
      assert(toEng);
    }
    waitForEngineCreation(fromIo);
    waitForEngineCreation(toIo);
    //create engines for reading
    if(isRendezvous) {
      if(noParticipant==false) { //support unit testing
        toEng = toIo.Open(bpToName, adios2::Mode::Read);
        assert(toEng);
      }
    } else {
      fromEng = fromIo.Open(bpFromName, adios2::Mode::Read);
      assert(fromEng);
    }
  }

  // SST support
  // - with a rendezvous + non-rendezvous application pair
  // - with only a rendezvous application for debugging/testing
  void Redev::openEnginesSST(bool noParticipant) {
    REDEV_FUNCTION_TIMER;
    //create one engine's reader and writer pair at a time - SST blocks on open(read)
    if(isRendezvous) {
      fromEng = fromIo.Open(bpFromName, adios2::Mode::Write);
    } else {
      fromEng = fromIo.Open(bpFromName, adios2::Mode::Read);
    }
    assert(fromEng);
    if(isRendezvous) {
      if(noParticipant==false) { //support unit testing
        toEng = toIo.Open(bpToName, adios2::Mode::Read);
        assert(toEng);
      }
    } else {
      toEng = toIo.Open(bpToName, adios2::Mode::Write);
      assert(toEng);
    }
  }

  Redev::Redev(MPI_Comm comm_, Partition& ptn_, bool isRendezvous_, bool noParticipant)
    : comm(comm_), adios("adios2.yaml", comm), ptn(ptn_), isRendezvous(isRendezvous_) {
    REDEV_FUNCTION_TIMER;
    int isInitialized = 0;
    MPI_Initialized(&isInitialized);
    REDEV_ALWAYS_ASSERT(isInitialized);
    MPI_Comm_rank(comm, &rank);
    if(!rank) {
      std::cout << "Redev Git Hash: " << redevGitHash << "\n";
    }
    fromIo = adios.DeclareIO("fromRendezvous");
    toIo = adios.DeclareIO("toRendezvous");
    REDEV_ALWAYS_ASSERT(fromIo.EngineType() == toIo.EngineType());
    if(noParticipant) {
      //SST hangs if there is no reader
      fromIo.SetEngine("BP4");
      toIo.SetEngine("BP4");
    }
    if( isSameCi(fromIo.EngineType(), "SST") ) {
      openEnginesSST(noParticipant);
    } else if( isSameCi(fromIo.EngineType(), "BP4") ) {
      openEnginesBP4(noParticipant);
    } else {
      if(!rank) {
        std::cerr << "ERROR: redev does not support ADIOS2 engine " << fromIo.EngineType() << "\n";
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
    auto status = eng.BeginStep();
    REDEV_ALWAYS_ASSERT(status == adios2::StepStatus::OK);
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
        REDEV_ALWAYS_ASSERT(inHash == redevGitHash);
      }
    }
    eng.EndStep();
  }

  void Redev::Setup() {
    REDEV_FUNCTION_TIMER;
    CheckVersion(fromEng,fromIo);
    auto status = fromEng.BeginStep();
    REDEV_ALWAYS_ASSERT(status == adios2::StepStatus::OK);
    //rendezvous app rank 0 writes partition info and other apps read
    if(!rank) {
      if(isRendezvous)
        ptn.Write(fromEng,fromIo);
      else
        ptn.Read(fromEng,fromIo);
    }
    fromEng.EndStep();
    ptn.Broadcast(comm);
  }

  void Redev_Assert_Fail(const char* msg) {
    fprintf(stderr, "%s", msg);
    abort();
  }
}
