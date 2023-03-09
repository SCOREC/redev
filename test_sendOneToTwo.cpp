#include <iostream>
#include <cstdlib>
#include "redev.h"

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
  if(isRdv && nproc != 2) {
      std::cerr << "There must be exactly 2 rendezvous processes for this test.\n";
      exit(EXIT_FAILURE);
  }
  if(!isRdv && nproc != 1) {
      std::cerr << "There must be exactly 1 non-rendezvous processes for this test.\n";
      exit(EXIT_FAILURE);
  }
  {
  //dummy partition vector data
  const auto dim = 2;
  auto ranks = isRdv ? redev::LOs({0,1,2,3}) : redev::LOs(4);
  auto cuts = isRdv ? redev::Reals({0,0.5,0.75,0.25}) : redev::Reals(4);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(MPI_COMM_WORLD,redev::Partition{std::move(ptn)},static_cast<redev::ProcessType>(isRdv));
  std::string name = "foo";
  adios2::Params params{ {"Streaming", "On"}, {"OpenTimeoutSecs", "2"}};
  auto channel = rdv.CreateAdiosChannel(name, params,
                                                    redev::TransportType::BP4);
  auto commPair = channel.CreateComm<redev::LO>(name);
  // the non-rendezvous app sends to the rendezvous app
  if(!isRdv) {
    redev::LOs dest;
    redev::LOs offsets;
    dest = redev::LOs{0,1};
    offsets = redev::LOs{0,2,6};
    redev::LOs msgs = {0,0,1,1,1,1};
    commPair.SetOutMessageLayout(dest, offsets);
    commPair.Send(msgs.data(),redev::Mode::Synchronous);
  }
  }
  MPI_Finalize();
  return 0;
}
