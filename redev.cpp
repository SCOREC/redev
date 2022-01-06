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
    std::string varName = "redev git hash";
    std::string bpName = "redev_version.bp";
    //rendezvous app writes the version it has and others read
    if(isRendezvous) {
      auto writer = io.Open(bpName, adios2::Mode::Write);
      auto varVersion = io.DefineVariable<std::string>(varName);
      if(!rank)
        writer.Put(varVersion, std::string(redevGitHash));
      writer.Close();
    }
    else {
      auto reader = io.Open(bpName, adios2::Mode::Read);
      auto varVersion = io.InquireVariable<std::string>(varName);
      std::string inHash;
      if(varVersion && !rank) {
        reader.Get(varVersion, inHash);
        std::cout << "inHash " << inHash << "\n";
        assert(inHash == redevGitHash);
      }
      reader.Close();
    }
    end_func();
  }
}
