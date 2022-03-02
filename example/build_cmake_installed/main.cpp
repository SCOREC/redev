#include <iostream>
#include <cstdlib>
#include <cassert>
#include "redev.h"

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  auto isRdv = true;
  auto noParticipant = true;
  std::cout << "comm rank " << rank << " size " << nproc << " isRdv " << isRdv << "\n";

  const auto dim = 1;
  std::vector<redev::LO> ranks = {0,1,2,3};
  std::vector<redev::Real> cuts = {0,0.5,0.25,0.75};
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv,noParticipant);
  rdv.Setup();
  std::array<redev::Real,3> pt{0.6, 0.0, 0.0};
  pt[0] = 0.6;   assert(2 == ptn.GetRank(pt));
  pt[0] = 0.01;  assert(0 == ptn.GetRank(pt));
  pt[0] = 0.5;   assert(2 == ptn.GetRank(pt));
  pt[0] = 0.751; assert(3 == ptn.GetRank(pt));

  MPI_Finalize();
  return 0;
}
