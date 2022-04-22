#include <iostream>
#include <cstdlib>
#include <cassert>
#include <chrono> //steady_clock, duration
#include <thread> //this_thread
#include "redev.h"
#include "redev_comm.h"

#define MILLION 1024*1024

// send/recv benchmark
// - Rendezvous
//   - each non-rendezvous rank sends 'mbpr' (millions of bytes per rank) data that
//     is uniformly divided across the rendezvous ranks
// - Mapped
//   - the sender and receiver have the same number of ranks and data layout to
//     emulate matching partitions
//   - sender rank i sends to receiver rank i mbpr data

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
  std::cout << mode << " elapsed time min, max, avg (s): "
            << min << " " << max << " " << avg << "\n";
}

void sendRecvRdv(MPI_Comm mpiComm, const bool isRdv, const int mbpr, const int rdvRanks) {
  int rank, nproc;
  MPI_Comm_rank(mpiComm, &rank);
  MPI_Comm_size(mpiComm, &nproc);
  //dummy partition vector data
  const auto dim = 2;
  //hard coding the number of rdv ranks to 32 for now....
  if(isRdv) assert(rdvRanks == nproc);
  auto ranks = redev::LOs(rdvRanks);
  //the cuts won't be used since getRank(...) won't be called
  auto cuts = redev::Reals(rdvRanks);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(mpiComm,ptn,isRdv);
  std::string name = "foo";
  std::stringstream ss;
  ss << mbpr << " B rdv ";
  redev::AdiosComm<redev::LO> comm(mpiComm, ranks.size(), rdv.getToEngine(), rdv.getToIO(), name);
  // the non-rendezvous app sends to the rendezvous app
  if(!isRdv) {
    //dest and offets define a CSR for which ranks the array of messages get sent to
    redev::LOs dest(rdvRanks);
    std::iota(dest.begin(),dest.end(),0);
    redev::LOs offsets(mbpr);
    constructCsrOffsets(mbpr,rdvRanks,offsets);
    redev::LOs msgs(mbpr,rank);
    auto start = std::chrono::steady_clock::now();
    comm.SetOutMessageLayout(dest, offsets);
    comm.Send(msgs.data());
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    double min, max, avg;
    timeMinMaxAvg(elapsed_seconds.count(), min, max, avg);
    ss << "write";
    std::string str = ss.str();
    if(!rank) printTime(str, min, max, avg);
  } else {
    auto start = std::chrono::steady_clock::now();
    comm.Recv();
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    double min, max, avg;
    timeMinMaxAvg(elapsed_seconds.count(), min, max, avg);
    ss << "read";
    std::string str = ss.str();
    if(!rank) printTime(str, min, max, avg);
  }
}

void sendRecvMapped(MPI_Comm mpiComm, const bool isRdv, const int mbpr, const int rdvRanks) {
  int rank, nproc;
  MPI_Comm_rank(mpiComm, &rank);
  MPI_Comm_size(mpiComm, &nproc);
  assert(nproc == rdvRanks);
  //using Redev to create engine objects...
  const auto dim = 2;
  auto ranks = redev::LOs(rdvRanks);
  auto cuts = redev::Reals(rdvRanks);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(mpiComm,ptn,isRdv);
  //get adios objs
  auto io = rdv.getToIO();
  auto eng = rdv.getToEngine();
  std::string name = "mapped";
  std::stringstream ss;
  ss << mbpr << " B " << name;
  if(!isRdv) { //sender
    adios2::Dims shape{static_cast<size_t>(mbpr*nproc)};
    adios2::Dims start{static_cast<size_t>(mbpr*rank)};
    adios2::Dims count{static_cast<size_t>(mbpr)};
    auto var = io.DefineVariable<redev::LO>(name, shape, start, count);
    assert(var);
    redev::LOs msgs(mbpr,rank);
    auto tStart = std::chrono::steady_clock::now();
    eng.BeginStep();
    eng.Put<redev::LO>(var, msgs.data());
    eng.PerformPuts();
    eng.EndStep();
    auto tEnd = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = tEnd-tStart;
    double min, max, avg;
    timeMinMaxAvg(elapsed_seconds.count(), min, max, avg);
    ss << " write";
    std::string str = ss.str();
    if(!rank) printTime(str, min, max, avg);
  } else {
    auto start = std::chrono::steady_clock::now();
    eng.BeginStep();
    auto msgs = io.InquireVariable<redev::LO>(name);
    assert(msgs);
    redev::LOs inMsgs(mbpr);
    msgs.SetSelection({{static_cast<size_t>(mbpr*rank)},
                       {static_cast<size_t>(mbpr)}
                      });
    eng.Get(msgs, inMsgs.data());
    eng.PerformGets();
    eng.EndStep();
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    double min, max, avg;
    timeMinMaxAvg(elapsed_seconds.count(), min, max, avg);
    ss << " read";
    std::string str = ss.str();
    if(!rank) printTime(str, min, max, avg);
  }
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if(argc != 4) {
    if(!rank) {
      std::cerr << "Usage: " << argv[0] << " <1=isRendezvousApp,0=isParticipant> <MBPR> <rdvRanks>\n";
      std::cerr << "MBPR: millions of bytes per rank\n";
      std::cerr << "rdvRanks: number of ranks ran by the rendezvous app\n";
    }
    exit(EXIT_FAILURE);
  }
  auto isRdv = atoi(argv[1]);
  assert(isRdv==0 || isRdv ==1);
  auto mbpr = atoi(argv[2])*MILLION;
  assert(mbpr>0);
  auto rdvRanks = atoi(argv[3]);
  assert(rdvRanks>0);

  sendRecvRdv(MPI_COMM_WORLD, isRdv, mbpr, rdvRanks);
  std::this_thread::sleep_for(std::chrono::seconds(2));
  sendRecvMapped(MPI_COMM_WORLD, isRdv, mbpr, rdvRanks);
  MPI_Finalize();
  return 0;
}
