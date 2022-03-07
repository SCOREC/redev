#include <cassert>
#include "redev.h"

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  const auto expectedRanks = redev::LOs({0,1,2,3});
  const auto expectedClassIds = redev::LOs({2,1,0,3});
  auto ranks = rank==0 ? expectedRanks : redev::LOs();
  auto classIds = rank==0 ? expectedClassIds : redev::LOs();
  auto ptn = redev::ClassPtn(ranks,classIds);
  ptn.Broadcast(MPI_COMM_WORLD);
  assert(ptn.GetRanks() == expectedRanks);
  assert(ptn.GetCuts() == expectedClassIds);
  MPI_Finalize();
  return 0;
}
