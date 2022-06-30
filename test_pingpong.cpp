#include <iostream>
#include <cstdlib>
#include "redev.h"
#include "redev_comm.h"

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  if(argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <1=isRendezvousApp,0=isParticipant>\n";
    exit(EXIT_FAILURE);
  }
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  auto isRdv = atoi(argv[1]);
  fprintf(stderr, "rank %d isRdv %d\n", rank, isRdv);
  if(nproc != 1) {
      std::cerr << "There must be exactly 1 rendezvous and 1 non-rendezvous processes for this test.\n";
      exit(EXIT_FAILURE);
  }
  {
  //dummy partition vector data
  const auto dim = 1;
  auto ranks = isRdv ? redev::LOs({0}) : redev::LOs(1);
  auto cuts = isRdv ? redev::Reals({0}) : redev::Reals(1);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(MPI_COMM_WORLD,ptn,static_cast<redev::ProcessType>(isRdv));
  std::string name = "foo";
  const bool isSST = false;
  adios2::Params params{ {"Streaming", "On"}, {"OpenTimeoutSecs", "2"}};
  auto commPair = rdv.CreateAdiosClient<redev::LO>(name,params,isSST);
  for(int iter=0; iter<3; iter++) {
    // the non-rendezvous app sends to the rendezvous app
    if(!isRdv) {
      if(iter==0) {
        redev::LOs dest = redev::LOs{0};
        redev::LOs offsets = redev::LOs{0,1};
        commPair.c2s.SetOutMessageLayout(dest, offsets);
      }
      redev::LOs msgs = redev::LOs(1,42);
      commPair.c2s.Send(msgs.data());
    } else {
      auto msgs = commPair.c2s.Recv();
      if(iter == 0) {
        auto inMsg = commPair.c2s.GetInMessageLayout();
        REDEV_ALWAYS_ASSERT(inMsg.offset == redev::GOs({0,1}));
        REDEV_ALWAYS_ASSERT(inMsg.srcRanks == redev::GOs({0}));
        REDEV_ALWAYS_ASSERT(inMsg.start == 0);
        REDEV_ALWAYS_ASSERT(inMsg.count == 1);
      }
      REDEV_ALWAYS_ASSERT(msgs[0] == 42);
    }
    // the rendezvous app sends to the non-rendezvous app
    if(isRdv) {
      if(iter==0) {
        redev::LOs dest = redev::LOs{0};
        redev::LOs offsets = redev::LOs{0,1};
        commPair.s2c.SetOutMessageLayout(dest, offsets);
      }
      redev::LOs msgs = redev::LOs(1,1337);
      commPair.s2c.Send(msgs.data());
    } else {
      auto msgs = commPair.s2c.Recv();
      if(iter==0) {
        auto inMsg = commPair.s2c.GetInMessageLayout();
        REDEV_ALWAYS_ASSERT(inMsg.offset == redev::GOs({0,1}));
        REDEV_ALWAYS_ASSERT(inMsg.srcRanks == redev::GOs({0}));
        REDEV_ALWAYS_ASSERT(inMsg.start == 0);
        REDEV_ALWAYS_ASSERT(inMsg.count == 1);
      }
      REDEV_ALWAYS_ASSERT(msgs[0] == 1337);
    }
  }
  }
  MPI_Finalize();
  return 0;
}
