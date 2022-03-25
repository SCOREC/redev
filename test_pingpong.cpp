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
  redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv);
  rdv.Setup();
  redev::AdiosComm<redev::LO> commA2R(MPI_COMM_WORLD, ranks.size(), rdv.getToEngine(), rdv.getIO(), "foo_A2R");
  redev::AdiosComm<redev::LO> commR2A(MPI_COMM_WORLD, ranks.size(), rdv.getFromEngine(), rdv.getIO(), "foo_R2A");
  // the non-rendezvous app sends to the rendezvous app
  if(!isRdv) {
    redev::LOs dest = redev::LOs{0};
    redev::LOs offsets = redev::LOs{0,1};
    redev::LOs msgs = redev::LOs(1,42);
    commA2R.Pack(dest, offsets, msgs.data());
    commA2R.Send();
  } else {
    redev::LO* msgs;
    redev::GOs rdvSrcRanks;
    redev::GOs offsets;
    size_t msgStart, msgCount;
    const bool knownSizes = false;
    commA2R.Unpack(rdvSrcRanks,offsets,msgs,msgStart,msgCount,knownSizes);
    REDEV_ALWAYS_ASSERT(offsets == redev::GOs({0,1}));
    REDEV_ALWAYS_ASSERT(rdvSrcRanks == redev::GOs({0}));
    REDEV_ALWAYS_ASSERT(msgStart == 0);
    REDEV_ALWAYS_ASSERT(msgCount == 1);
    REDEV_ALWAYS_ASSERT(msgs[0] == 42);
    delete [] msgs;
  }
  // the rendezvous app sends to the non-rendezvous app
  if(isRdv) {
    redev::LOs dest = redev::LOs{0};
    redev::LOs offsets = redev::LOs{0,1};
    redev::LOs msgs = redev::LOs(1,1337);
    commR2A.Pack(dest, offsets, msgs.data());
    commR2A.Send();
  } else {
    redev::LO* msgs;
    redev::GOs rdvSrcRanks;
    redev::GOs offsets;
    size_t msgStart, msgCount;
    const bool knownSizes = false;
    commR2A.Unpack(rdvSrcRanks,offsets,msgs,msgStart,msgCount,knownSizes);
    REDEV_ALWAYS_ASSERT(offsets == redev::GOs({0,1}));
    REDEV_ALWAYS_ASSERT(rdvSrcRanks == redev::GOs({0}));
    REDEV_ALWAYS_ASSERT(msgStart == 0);
    REDEV_ALWAYS_ASSERT(msgCount == 1);
    REDEV_ALWAYS_ASSERT(msgs[0] == 1337);
    delete [] msgs;
  }
  }
  MPI_Finalize();
  return 0;
}
