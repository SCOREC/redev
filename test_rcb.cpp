#include <cassert>
#include "redev.h"

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  const auto expectedRanks = redev::LOs({0,1,2,3});
  const auto expectedCuts = redev::Reals({0,0.5,0.75,0.25});
  auto ranks = rank==0 ? expectedRanks : redev::LOs();
  auto cuts = rank==0 ? expectedCuts : redev::Reals();
  const auto dim = 2;
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  ptn.Broadcast(MPI_COMM_WORLD);
  assert(ptn.GetRanks() == expectedRanks);
  assert(ptn.GetCuts() == expectedCuts);
  MPI_Finalize();
  return 0;
}
