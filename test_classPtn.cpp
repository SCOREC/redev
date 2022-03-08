#include "redev.h"

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  const auto expectedRanks = redev::LOs({0,1,2,3});
  const auto expectedClassIds = redev::LOs({2,1,0,3});
  std::map<redev::LO, redev::LO> expC2R;
  for(int i=0; i<expectedRanks.size(); i++)
    expC2R[expectedClassIds[i]] = expectedRanks[i];

  auto ranks = rank==0 ? expectedRanks : redev::LOs();
  auto classIds = rank==0 ? expectedClassIds : redev::LOs();
  auto ptn = redev::ClassPtn(ranks,classIds);
  ptn.Broadcast(MPI_COMM_WORLD);

  auto ptnRanks = ptn.GetRanks();
  REDEV_ALWAYS_ASSERT(ptnRanks.size() == expectedRanks.size());
  auto ptnClassIds = ptn.GetClassIds();
  REDEV_ALWAYS_ASSERT(ptnClassIds.size() == expectedClassIds.size());
  std::map<redev::LO, redev::LO> c2r;
  for(int i=0; i<ptnRanks.size(); i++)
    c2r[ptnClassIds[i]] = ptnRanks[i];
  REDEV_ALWAYS_ASSERT(c2r == expC2R);

  MPI_Finalize();
  return 0;
}
