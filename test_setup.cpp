#include <iostream>
#include <cstdlib>
#include <cassert>
#include "redev.h"

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
  //dummy partition vector data
  std::vector<redev::LO> ranks = {0,1,2,3};
  std::vector<redev::Real> cuts = {0.5,0.5,0.5};
  const auto dim = 2;
  auto ptn = isRdv ? redev::RCBPtn(dim,ranks,cuts) : redev::RCBPtn();
  redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv);
  rdv.Setup();
  if(!isRdv) {
    auto ptnRanks = ptn.GetRanks();
    auto ptnCuts = ptn.GetCuts();
    assert(ptnRanks == ranks);
    assert(ptnCuts == cuts);
  }
  MPI_Finalize();
  return 0;
}
