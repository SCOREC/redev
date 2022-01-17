#include <iostream>
#include <cstdlib>
#include <cassert>
#include <chrono> //steady_clock, duration
#include "redev.h"
#include "redev_comm.h"

#define MILLION 1024*1024
#define THOUSAND 1024
#define RDV_RANKS 32
#define MBPR (1*MILLION) //megabytes per rank

//the following combination appears to work with 8 NR ranks
//#define RDV_RANKS 4
//#define MBPR (1*THOUSAND)

// send/recv benchmark
// - currently hardcoded for RDV_RANKS rendezvous (RD) ranks
// - there are no (intentional) restrictions on the number of non-rendzvous (NR) ranks
// - each NR rank sends 'mbpr' (millions of bytes per rank) data that is uniformly divided
//   across the RD ranks (each one gets mbpr/RDV_RANKS)

void constructCsrOffsets(int tot, int n, std::vector<int>& offsets) {
  //produces an uniform distribution of values
  const auto delta = tot/n;
  assert(delta*n == tot);
  offsets.resize(n+1);
  offsets[0] = 0;
  for( auto i=1; i<=n; i++)
    offsets[i] = offsets[i-1]+delta;
}

void timeMinMaxAvg(double time, double& min, double& max, double& avg) {
  const auto comm = MPI_COMM_WORLD;
  int nproc;
  MPI_Comm_size(comm, &nproc);
  double tot = 0;
  MPI_Allreduce(&time, &min, 1, MPI_DOUBLE, MPI_MIN, comm);
  MPI_Allreduce(&time, &max, 1, MPI_DOUBLE, MPI_MAX, comm);
  MPI_Allreduce(&time, &tot, 1, MPI_DOUBLE, MPI_SUM, comm);
  avg = tot / nproc;
}

void printTime(std::string mode, double min, double max, double avg) {
  std::cout << "elapsed " << mode << " time min, max, avg (s): "
            << min << " " << max << " " << avg << "\n";
}

void sendRecvRdv(MPI_Comm mpiComm, bool isRdv) {
  int rank, nproc;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  //dummy partition vector data
  const auto dim = 2;
  //hard coding the number of rdv ranks to 32 for now....
  if(isRdv) assert(RDV_RANKS == nproc);
  auto ranks = isRdv ? redev::LOs(RDV_RANKS) : redev::LOs(RDV_RANKS);
  //the cuts won't be used since getRank(...) won't be called
  auto cuts = isRdv ? redev::Reals(RDV_RANKS) : redev::Reals(RDV_RANKS);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv);
  rdv.Setup();
  std::string name = "foo";
  redev::AdiosComm<redev::LO> comm(mpiComm, ranks.size(), rdv.getToEngine(), rdv.getIO(), name);
  // the non-rendezvous app sends to the rendezvous app
  if(!isRdv) {
    //dest and offets define a CSR for which ranks the array of messages get sent to
    redev::LOs dest(RDV_RANKS);
    std::iota(dest.begin(),dest.end(),0);
    redev::LOs offsets(MBPR);
    constructCsrOffsets(MBPR,RDV_RANKS,offsets);
    redev::LOs msgs(MBPR,rank);
    auto start = std::chrono::steady_clock::now();
    comm.Pack(dest, offsets, msgs.data());
    comm.Send();
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    double min, max, avg;
    timeMinMaxAvg(elapsed_seconds.count(), min, max, avg);
    if(!rank) printTime("write", min, max, avg);
  } else {
    redev::LO* msgs;
    redev::GOs rdvSrcRanks;
    redev::GOs offsets;
    auto start = std::chrono::steady_clock::now();
    comm.Unpack(rdvSrcRanks,offsets,msgs);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    double min, max, avg;
    timeMinMaxAvg(elapsed_seconds.count(), min, max, avg);
    if(!rank) printTime("read", min, max, avg);
    delete [] msgs;
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
  sendRecvRdv(MPI_COMM_WORLD, isRdv);
  MPI_Finalize();
  return 0;
}
