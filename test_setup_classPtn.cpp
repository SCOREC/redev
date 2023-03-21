#include <iostream>
#include <cstdlib>
#include "redev.h"

void classPtnTest(int rank, bool isRdv) {
  //dummy partition vector data: class partition
  const redev::LOs expectedRanks = {0,1,2,3};
  const redev::ClassPtn::ModelEntVec expectedEnts {{0,0},{1,0},{2,0},{2,1}};
  typedef std::map<redev::ClassPtn::ModelEnt,redev::LO> EntToRank;
  EntToRank expectedE2R;
  for(int i=0; i<expectedRanks.size(); i++)
    expectedE2R[expectedEnts[i]] = expectedRanks[i];
  auto ranks = isRdv ? expectedRanks : redev::LOs();
  auto ents = isRdv ? expectedEnts : redev::ClassPtn::ModelEntVec();
  redev::Redev rdv(MPI_COMM_WORLD,redev::Partition{std::in_place_type<redev::ClassPtn>, MPI_COMM_WORLD,ranks,ents},static_cast<redev::ProcessType>(isRdv));
  adios2::Params params{ {"Streaming", "On"}, {"OpenTimeoutSecs", "2"}};
  auto channel = rdv.CreateAdiosChannel("foo", params,
                                                    redev::TransportType::BP4);
  auto commPair = channel.CreateComm<redev::LO>("foo", MPI_COMM_WORLD);
  if(!isRdv) {
    const auto& partition = std::get<redev::ClassPtn>(rdv.GetPartition());
    auto p_ranks = partition.GetRanks();
    auto p_modelEnts = partition.GetModelEnts();
    REDEV_ALWAYS_ASSERT(p_ranks.size() == 4);
    REDEV_ALWAYS_ASSERT(p_modelEnts.size() == 4);
    EntToRank e2r;
    for(int i=0; i<p_ranks.size(); i++)
      e2r[p_modelEnts[i]] = p_ranks[i];
    REDEV_ALWAYS_ASSERT(e2r == expectedE2R);
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
  std::cerr << "done\n";
  MPI_Finalize();
  return 0;
}
