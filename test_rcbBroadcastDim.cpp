#include "redev.h"
#include <array>

// Ensure that a default constructed RCB partition receives dim
static void check(redev::LO dim, redev::LOs ranks, redev::Reals cuts,
                  int rank) {
  auto ptn = rank == 0 ? redev::RCBPtn(dim, ranks, cuts) : redev::RCBPtn();
  ptn.Broadcast(MPI_COMM_WORLD);
  redev::LOs routed;
  for (int i = 0; i < 10; ++i)
    for (int j = 0; j < 10; ++j)
      for (int k = 0; k < 10; ++k) {
        std::array<redev::Real, 3> pt = {(i + .5) / 10, (j + .5) / 10,
                                         (k + .5) / 10};
        routed.push_back(ptn.GetRank(pt));
      }
  auto rootRouted = routed;
  MPI_Bcast(rootRouted.data(), rootRouted.size(), MPI_INT, 0, MPI_COMM_WORLD);
  REDEV_ALWAYS_ASSERT(routed == rootRouted);
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  int rank, nproc;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  REDEV_ALWAYS_ASSERT(nproc == 2);
  const redev::Reals cuts = {0, .5, .25, .75, .1, .3, .6, .9};
  check(2, {0, 1, 2, 3, 4, 5, 6, 7}, cuts, rank);
  check(3, {0, 1, 2, 3, 4, 5, 6, 7}, cuts, rank);
  MPI_Finalize();
  return 0;
}
