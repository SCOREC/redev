#include <iostream>
#include "redev.h"

int main(int argc, char** argv) {
  int rank = 0, nproc = 1;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  auto ptn = std::make_unique<redev::RCBPtn>();
  auto isRendezvous=true;
  auto noClients=true;
  redev::Redev(MPI_COMM_WORLD,std::move(ptn),static_cast<redev::ProcessType>(isRendezvous),noClients);
  MPI_Finalize();
  return 0;
}
