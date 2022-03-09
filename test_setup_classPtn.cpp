#include <iostream>
#include <cstdlib>
#include "redev.h"

void classPtnTest(int rank, bool isRdv) {
  //dummy partition vector data: class partition
  const auto expectedRanks = redev::LOs({0,1,2,3});
  const auto expectedClassIds = redev::LOs({2,1,0,3});
  std::map<redev::LO, redev::LO> expC2R;
  for(int i=0; i<expectedRanks.size(); i++)
    expC2R[expectedClassIds[i]] = expectedRanks[i];
  auto ranks = isRdv ? expectedRanks : redev::LOs();
  auto classIds = isRdv ? expectedClassIds : redev::LOs();
  auto ptn = redev::ClassPtn(ranks,classIds);
  redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv);
  rdv.Setup();
  if(!isRdv) {
    auto ptnRanks = ptn.GetRanks();
    auto ptnClassIds = ptn.GetClassIds();
    REDEV_ALWAYS_ASSERT(ptnRanks.size() == 4);
    REDEV_ALWAYS_ASSERT(ptnClassIds.size() == 4);
    std::map<redev::LO, redev::LO> c2r;
    for(int i=0; i<ptnRanks.size(); i++)
      c2r[ptnClassIds[i]] = ptnRanks[i];
    REDEV_ALWAYS_ASSERT(c2r == expC2R);
  }
}

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
  std::cout << "comm rank " << rank << " size " << nproc << " isRdv " << isRdv << "\n";
  classPtnTest(rank,isRdv);
  MPI_Finalize();
  return 0;
}
