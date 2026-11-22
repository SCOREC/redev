#include <iostream>
#include "redev.h"
#include<unistd.h>

int main(int argc, char** argv) {
  int rank = 0, nproc = 1;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  redev::RCBPtn ptn;
  auto isRendezvous=true;
  if(static_cast<redev::ProcessType>(isRendezvous) == redev::ProcessType::Server)
  {

    redev::Redev(MPI_COMM_WORLD,redev::Partition{ptn},redev::ProcessType::Server);
    sleep(1);
  }
  else
    redev::Redev(MPI_COMM_WORLD,redev::ProcessType::Client);
  MPI_Finalize();
  return 0;
}
