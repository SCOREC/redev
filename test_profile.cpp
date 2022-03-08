#include <iostream>
#include <cstdlib>
#include <chrono>
#include <thread>
#include "redev.h"
#include "redev_profile.h"

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  auto isRdv = true;
  auto noParticipant = true;
  {
    const auto dim = 1;
    std::vector<redev::LO> ranks = {0,1,2,3};
    std::vector<redev::Real> cuts = {0,0.5,0.25,0.75};
    auto ptn = redev::RCBPtn(dim,ranks,cuts);
    redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv,noParticipant);
    rdv.Setup();
    std::array<redev::Real,3> pt{0.6, 0.0, 0.0};
    const int numCalls = 10;
    for(auto i=0; i<numCalls; i++)
      ptn.GetRank(pt);

    auto prof = redev::Profiling::GetInstance();
    prof->Write(std::cout);
    auto t = prof->GetTime("Setup");
    auto cnt = prof->GetCallCount("Setup");
    REDEV_ALWAYS_ASSERT((t > 0 && t < 1) && cnt == 1);

    t = prof->GetTime("Redev");
    cnt = prof->GetCallCount("Redev");
    REDEV_ALWAYS_ASSERT((t > 0 && t < 1) && cnt == 1);

    t = prof->GetTime("GetRank");
    cnt = prof->GetCallCount("GetRank");
    REDEV_ALWAYS_ASSERT((t > 0 && t < 1) && cnt == numCalls);
  }
  MPI_Finalize();
  return 0;
}
