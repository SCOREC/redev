#include "redev.h"

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  const redev::LOs expectedRanks = {0,1,2,3};
  const redev::ClassPtn::ModelEntVec expectedEnts {{0,0},{1,0},{2,0},{2,1}};
  REDEV_ALWAYS_ASSERT(expectedRanks.size() == expectedEnts.size());
  typedef std::map<redev::ClassPtn::ModelEnt,redev::LO> EntToRank;
  EntToRank expectedE2R;
  for(int i=0; i<expectedRanks.size(); i++)
    expectedE2R[expectedEnts[i]] = expectedRanks[i];

  auto ranks = rank==0 ? expectedRanks : redev::LOs();
  auto ents = rank==0 ? expectedEnts : redev::ClassPtn::ModelEntVec();
  auto partition = redev::ClassPtn(MPI_COMM_WORLD,ranks,ents);
  partition.Broadcast(MPI_COMM_WORLD);

  auto p_ranks = partition.GetRanks();
  REDEV_ALWAYS_ASSERT(p_ranks.size() == expectedRanks.size());
  auto p_modelEnts = partition.GetModelEnts();
  REDEV_ALWAYS_ASSERT(p_modelEnts.size() == expectedEnts.size());
  EntToRank e2r;
  for(int i=0; i<p_modelEnts.size(); i++)
    e2r[p_modelEnts[i]] = p_ranks[i];
  REDEV_ALWAYS_ASSERT(e2r == expectedE2R);

  MPI_Finalize();
  return 0;
}
