#include "redev.h"

//less typing...
using MdlEntVec = redev::ClassPtn::ModelEntVec;
typedef std::map<redev::ClassPtn::ModelEnt,redev::LO> EntToRank;

void check(const redev::ClassPtn& partition, const EntToRank& expectedE2R) {
  //check that is matches expected result
  auto p_ranks = partition.GetRanks();
  REDEV_ALWAYS_ASSERT(p_ranks.size() == expectedE2R.size());
  auto p_modelEnts = partition.GetModelEnts();
  REDEV_ALWAYS_ASSERT(p_modelEnts.size() == expectedE2R.size());
  EntToRank e2r;
  for(int i=0; i<p_modelEnts.size(); i++)
    e2r[p_modelEnts[i]] = p_ranks[i];
  REDEV_ALWAYS_ASSERT(e2r == expectedE2R);
}

/**
 * partition data split across two ranks
 */
void test1(const int rank, const EntToRank& expectedE2R) {
  auto ranks = rank==0 ? redev::LOs({0,1}) : redev::LOs({2,3});
  auto ents = rank==0 ? MdlEntVec({{0,0},{1,0}}) : MdlEntVec({{2,0},{2,1}}) ;
  auto partition = redev::ClassPtn(MPI_COMM_WORLD,ranks,ents);
  if(!rank) check(partition,expectedE2R);
}

/**
 * partition data all on rank 0
 */
void test2(const int rank, const EntToRank& expectedE2R) {
  auto ranks = rank==0 ? redev::LOs({0,1,2,3}) : redev::LOs();
  auto ents = rank==0 ? MdlEntVec({{0,0},{1,0},{2,0},{2,1}}) : MdlEntVec() ;
  auto partition = redev::ClassPtn(MPI_COMM_WORLD,ranks,ents);
  if(!rank) check(partition,expectedE2R);
}

/**
 * partition data all on rank 1
 */
void test3(const int rank, const EntToRank& expectedE2R) {
  auto ranks = rank!=0 ? redev::LOs({0,1,2,3}) : redev::LOs();
  auto ents = rank!=0 ? MdlEntVec({{0,0},{1,0},{2,0},{2,1}}) : MdlEntVec() ;
  auto partition = redev::ClassPtn(MPI_COMM_WORLD,ranks,ents);
  if(!rank) check(partition,expectedE2R);
}

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  REDEV_ALWAYS_ASSERT(nproc == 2);

  //expected result
  const redev::LOs expectedRanks = {0,1,2,3};
  const MdlEntVec expectedEnts {{0,0},{1,0},{2,0},{2,1}};
  REDEV_ALWAYS_ASSERT(expectedRanks.size() == expectedEnts.size());
  EntToRank expectedE2R;
  for(int i=0; i<expectedRanks.size(); i++)
    expectedE2R[expectedEnts[i]] = expectedRanks[i];

  test1(rank, expectedE2R);
  test2(rank, expectedE2R);
  test3(rank, expectedE2R);

  MPI_Finalize();
  return 0;
}
