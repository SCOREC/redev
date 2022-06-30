#include <iostream>
#include <cstdlib>
#include "redev.h"
#include "redev_comm.h"

//If the name of the variable being sent ("foo"), the communication
//pattern, or data being sent is changed then also update the cmake
//test using the adios2 utility bpls to check the array.

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
  fprintf(stderr, "rank %d isRdv %d\n", rank, isRdv);
  if(isRdv && nproc != 4) {
      std::cerr << "There must be exactly 4 rendezvous processes for this test.\n";
      exit(EXIT_FAILURE);
  }
  if(!isRdv && nproc != 3) {
      std::cerr << "There must be exactly 3 non-rendezvous processes for this test.\n";
      exit(EXIT_FAILURE);
  }
  {
  //dummy partition vector data
  const auto dim = 2;
  auto ranks = isRdv ? redev::LOs({0,1,2,3}) : redev::LOs(4);
  auto cuts = isRdv ? redev::Reals({0,0.5,0.75,0.25}) : redev::Reals(4);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(MPI_COMM_WORLD,ptn,static_cast<redev::ProcessType>(isRdv));
  std::string name = "foo";
  const bool isSST = false;
  adios2::Params params{ {"Streaming", "On"}, {"OpenTimeoutSecs", "2"}};
  auto commPair = rdv.CreateAdiosClient<redev::LO>(name,params,isSST);
  // the non-rendezvous app sends to the rendezvous app
  if(!isRdv) {
    redev::LOs dest;
    redev::LOs offsets;
    redev::LOs msgs;
    if(rank==0) {
      dest = redev::LOs{0,2};
      offsets = redev::LOs{0,2,6};
      msgs = redev::LOs(6,0); //write the src rank as the msg for now
    } else if (rank==1) {
      dest = redev::LOs{0,1,2,3};
      offsets = redev::LOs{0,1,4,8,10};
      msgs = redev::LOs(10,1);
    } else if (rank==2) {
      dest = redev::LOs{0,1,2,3};
      offsets = redev::LOs{0,4,5,7,11};
      msgs = redev::LOs(11,2);
    }
    commPair.c2s.SetOutMessageLayout(dest, offsets);
    commPair.c2s.Send(msgs.data());
  }
  }
  MPI_Finalize();
  return 0;
}
