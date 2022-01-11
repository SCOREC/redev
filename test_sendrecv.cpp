#include <iostream>
#include <cstdlib>
#include <cassert>
#include "redev.h"
#include "redev_comm.h"

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
  {
  //dummy partition vector data
  const auto dim = 2;
  std::vector<redev::LO> ranks = {0,1,2,3};
  std::vector<redev::Real> cuts = {0,0.5,0.75,0.25};
  auto ptn = isRdv ? redev::RCBPtn(dim,ranks,cuts) : redev::RCBPtn();
  redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv);
  rdv.Setup();
  std::string name = "foo";
  redev::AdiosComm<redev::LO> comm(MPI_COMM_WORLD, ranks.size(), rdv.getEngine(), rdv.getIO(), name);
  redev::LOs dest;
  redev::LOs offsets;
  redev::LOs msgs;
  if(rank==0) {
    dest = redev::LOs{0,2};
    offsets = redev::LOs{0,10,20};
    msgs = redev::LOs(20);
  } else if (rank==1) {
    dest = redev::LOs{0,1,2,3};
    offsets = redev::LOs{0,20,23,43,45};
    msgs = redev::LOs(45);
  } else if (rank==2) {
    dest = redev::LOs{0,1,2,3};
    offsets = redev::LOs{0,25,40,65,75};
    msgs = redev::LOs(75);
  }

  comm.Pack(dest, offsets, msgs.data());
  comm.Send();
  }
  MPI_Finalize();
  return 0;
}
