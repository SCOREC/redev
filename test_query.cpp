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
  std::cout << "comm rank " << rank << " size " << nproc << " isRdv " << isRdv << "\n";
  { //1D
    const auto dim = 1;
    std::vector<redev::LO> ranks = {0,1,2,3};
    std::vector<redev::Real> cuts = {0,0.5,0.25,0.75};
    auto ptn = redev::RCBPtn(dim,ranks,cuts);
    redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv);
    rdv.Setup();
    redev::Real pt[3] = {0.6, 0.0, 0.0};
    auto expectedRank = 2;
    assert(expectedRank == ptn.GetRank(pt));
    pt[0] = 0.01;
    expectedRank = 0;
    assert(expectedRank == ptn.GetRank(pt));
    pt[0] = 0.5;
    expectedRank = 2;
    assert(expectedRank == ptn.GetRank(pt));
    pt[0] = 0.751;
    expectedRank = 3;
    assert(expectedRank == ptn.GetRank(pt));
  }
  MPI_Finalize();
  return 0;
}
