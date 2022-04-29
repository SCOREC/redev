#include <iostream>
#include "redev.h"

const std::string timeout="8";

void client() {
  const auto dim = 1;
  auto ranks = redev::LOs(1);
  auto cuts = redev::Reals(1);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  auto isRendezvous=false;
  auto rdv = redev::Redev(MPI_COMM_WORLD,ptn,isRendezvous);
  const bool isSST = false;
  adios2::Params params{ {"Streaming", "On"}, {"OpenTimeoutSecs", timeout}};
  auto commPair = rdv.CreateAdiosClient<redev::LO>("foo",params,isSST);
}

void server() {
  const auto dim = 1;
  auto ranks = redev::LOs({0});
  auto cuts = redev::Reals({0});
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  auto isRendezvous=true;
  auto rdv = redev::Redev(MPI_COMM_WORLD,ptn,isRendezvous);
  const bool isSST = false;
  adios2::Params params{ {"Streaming", "On"}, {"OpenTimeoutSecs", timeout}};
  auto commPair = rdv.CreateAdiosClient<redev::LO>("foo",params,isSST);
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  int nproc;
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  REDEV_ALWAYS_ASSERT(nproc == 1);
  REDEV_ALWAYS_ASSERT(argc == 2);
  const int isRdv = atoi(argv[1]);
  std::cout << "isRdv " << isRdv << "\n";
  REDEV_ALWAYS_ASSERT(isRdv == 1 || isRdv == 0);
  if(isRdv)
    server();
  else
    client();
  std::cout << "done\n";
  MPI_Finalize();
  return 0;
}
