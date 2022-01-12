#include <iostream>
#include "redev.h"

int main(int argc, char** argv) {
  int rank = 0, nproc = 1;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  redev::RCBPtn ptn;
  auto isRendezvous=true;
  auto noParticipant=true;
  redev::Redev(MPI_COMM_WORLD,ptn,isRendezvous,noParticipant);
  MPI_Finalize();
  return 0;
}
