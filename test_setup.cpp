#include <iostream>
#include <cstdlib>
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
  redev::Redev rdv(MPI_COMM_WORLD,isRdv);
  //dummy partition vector data
  std::vector<redev::LO> ranks = {0,1,2,3};
  std::vector<redev::Real> cuts = {0.5,0.5,0.5};
  rdv.Setup(ranks,cuts);
  MPI_Finalize();
  return 0;
}
