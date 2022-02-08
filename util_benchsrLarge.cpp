#include <iostream>
#include <cstdlib>
#include <cassert>
#include <chrono> //steady_clock, duration
#include <thread> //this_thread
#include "redev.h"
#include "redev_comm.h"

#define MILLION 1024*1024

// send/recv benchmark
// - Mapped
//   - The receiver has reductionFactor times fewer ranks than the sender
//     and each sender computes the destination rank as
//     senderRank/reductionFactor (using integer math).  This emulates a
//     simple transfer pattern that would be hard coded if rendezvous
//     was not used.
// - RendezvousMapped
//   - This uses the same pattern as Mapped and is for measuring the overhead
//     of the Rendezvous APIs.
// - RendezvousFanOut
//   - Each non-rendezvous rank sends 'mbpr' (millions of bytes per rank) data that
//     is uniformly divided across the rendezvous ranks.  This is nearly a
//     worse case pattern resulting from minimal or poor application and
//     rendezvous partition alignment.

void constructCsrOffsetsFanOut(int tot, int rdvRanks, std::vector<int>& offsets) {
  //produces an uniform distribution of values
  const auto delta = tot/rdvRanks;
  assert(delta*rdvRanks == tot);
  offsets.resize(rdvRanks+1);
  offsets[0] = 0;
  for( auto i=1; i<=rdvRanks; i++)
    offsets[i] = offsets[i-1]+delta;
}

void constructCsrOffsetsMapped(int destRank, int tot, std::vector<int>& offsets) {
  //send everything to one rank
  offsets.resize(2);
  offsets[0] = 0;
  offsets[1] = tot;
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

void sendRecvRdvMapped(MPI_Comm mpiComm, const bool isRdv, const int mbpr,
    const int rdvRanks, const int reductionFactor) {
  int rank, nproc;
  MPI_Comm_rank(mpiComm, &rank);
  MPI_Comm_size(mpiComm, &nproc);
  if(!isRdv) {
    assert(nproc == rdvRanks*reductionFactor);
  } else {
    assert(nproc == rdvRanks);
  }
  //dummy partition vector data
  const auto dim = 2;
  //hard coding the number of rdv ranks to 32 for now....
  if(isRdv) assert(rdvRanks == nproc);
  auto ranks = redev::LOs(rdvRanks);
  //the cuts won't be used since getRank(...) won't be called
  auto cuts = redev::Reals(rdvRanks);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(mpiComm,ptn,isRdv);
  rdv.Setup();
  std::string name = "rendezvous";
  std::stringstream ss;
  ss << mbpr << " B rdvMapped ";
  redev::AdiosComm<redev::LO> comm(mpiComm, ranks.size(), rdv.getToEngine(), rdv.getIO(), name);
  // the non-rendezvous app sends to the rendezvous app
  size_t msgStart, msgCount;
  for(int i=0; i<3; i++) {
    if(!isRdv) {
      //dest and offets define a CSR for which ranks the array of messages get sent to
      const int destRank = rank/reductionFactor;
      redev::LOs dest = {destRank};
      redev::LOs offsets;
      constructCsrOffsetsMapped(destRank, mbpr, offsets);
      redev::LOs msgs(mbpr,rank);
      auto start = std::chrono::steady_clock::now();
      comm.Pack(dest, offsets, msgs.data());
      comm.Send();
      auto end = std::chrono::steady_clock::now();
      std::chrono::duration<double> elapsed_seconds = end-start;
      double min, max, avg;
      timeMinMaxAvg(elapsed_seconds.count(), min, max, avg);
      if( i == 0 ) ss << "write";
      std::string str = ss.str();
      if(!rank) printTime(str, min, max, avg);
    } else {
      redev::LO* msgs;
      redev::GOs rdvSrcRanks;
      redev::GOs offsets;
      auto start = std::chrono::steady_clock::now();
      const bool knownSizes = (i == 0) ? false : true;
      comm.Unpack(rdvSrcRanks,offsets,msgs,msgStart,msgCount,knownSizes);
      auto end = std::chrono::steady_clock::now();
      std::chrono::duration<double> elapsed_seconds = end-start;
      double min, max, avg;
      timeMinMaxAvg(elapsed_seconds.count(), min, max, avg);
      if( i == 0 ) ss << "read";
      std::string str = ss.str();
      if(!rank) printTime(str, min, max, avg);
      delete [] msgs;
    }
  }
}

void sendRecvRdvFanOut(MPI_Comm mpiComm, const bool isRdv, const int mbpr,
    const int rdvRanks, const int reductionFactor) {
  int rank, nproc;
  MPI_Comm_rank(mpiComm, &rank);
  MPI_Comm_size(mpiComm, &nproc);
  if(!isRdv) {
    assert(nproc == rdvRanks*reductionFactor);
  } else {
    assert(nproc == rdvRanks);
  }
  //dummy partition vector data
  const auto dim = 2;
  //hard coding the number of rdv ranks to 32 for now....
  if(isRdv) assert(rdvRanks == nproc);
  auto ranks = redev::LOs(rdvRanks);
  //the cuts won't be used since getRank(...) won't be called
  auto cuts = redev::Reals(rdvRanks);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(mpiComm,ptn,isRdv);
  rdv.Setup();
  std::string name = "rendezvous";
  std::stringstream ss;
  ss << mbpr << " B rdvFanOut ";
  redev::AdiosComm<redev::LO> comm(mpiComm, ranks.size(), rdv.getToEngine(), rdv.getIO(), name);
  // the non-rendezvous app sends to the rendezvous app
  size_t msgStart, msgCount;
  for(int i=0; i<3; i++) {
    if(!isRdv) {
      //dest and offets define a CSR for which ranks the array of messages get sent to
      redev::LOs dest(rdvRanks);
      std::iota(dest.begin(),dest.end(),0);
      redev::LOs offsets(mbpr);
      constructCsrOffsetsFanOut(mbpr,rdvRanks,offsets);
      redev::LOs msgs(mbpr,rank);
      auto start = std::chrono::steady_clock::now();
      comm.Pack(dest, offsets, msgs.data());
      comm.Send();
      auto end = std::chrono::steady_clock::now();
      std::chrono::duration<double> elapsed_seconds = end-start;
      double min, max, avg;
      timeMinMaxAvg(elapsed_seconds.count(), min, max, avg);
      if( i == 0 ) ss << "write";
      std::string str = ss.str();
      if(!rank) printTime(str, min, max, avg);
    } else {
      redev::LO* msgs;
      redev::GOs rdvSrcRanks;
      redev::GOs offsets;
      auto start = std::chrono::steady_clock::now();
      const bool knownSizes = (i == 0) ? false : true;
      comm.Unpack(rdvSrcRanks,offsets,msgs,msgStart,msgCount,knownSizes);
      auto end = std::chrono::steady_clock::now();
      std::chrono::duration<double> elapsed_seconds = end-start;
      double min, max, avg;
      timeMinMaxAvg(elapsed_seconds.count(), min, max, avg);
      if( i == 0 ) ss << "read";
      std::string str = ss.str();
      if(!rank) printTime(str, min, max, avg);
      delete [] msgs;
    }
  }
}

void sendRecvMapped(MPI_Comm mpiComm, const bool isRdv, const int mbpr,
    const int rdvRanks, const int reductionFactor) {
  int rank, nproc;
  MPI_Comm_rank(mpiComm, &rank);
  MPI_Comm_size(mpiComm, &nproc);
  if(!isRdv) {
    assert(nproc == rdvRanks*reductionFactor);
  } else {
    assert(nproc == rdvRanks);
  }
  //using Redev to create engine objects...
  const auto dim = 2;
  auto ranks = redev::LOs(rdvRanks);
  auto cuts = redev::Reals(rdvRanks);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(mpiComm,ptn,isRdv);
  //get adios objs
  auto io = rdv.getIO();
  auto eng = rdv.getToEngine();
  std::string name = "mapped";
  std::stringstream ss;
  ss << mbpr << " B " << name;
  if(!isRdv) { //sender
    adios2::Dims shape{static_cast<size_t>(mbpr)*nproc};
    adios2::Dims start{static_cast<size_t>(mbpr)*rank};
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
    const auto startRead = static_cast<size_t>(mbpr)*reductionFactor*rank;
    msgs.SetSelection({{static_cast<size_t>(startRead)},
                       {static_cast<size_t>(mbpr*reductionFactor)}
                      });
    redev::LOs inMsgs(mbpr*reductionFactor);
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
  int rank, nprocs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  if(argc != 5) {
    if(!rank) {
      std::cerr << "Usage: " << argv[0] << " <1=isRendezvousApp,0=isParticipant> <MBPR> <rdvRanks> <reductionFactor>\n";
      std::cerr << "MBPR: millions of bytes per rank\n";
      std::cerr << "rdvRanks: number of ranks ran by the rendezvous app\n";
      std::cerr << "reductionFactor: ratio of rdvRanks to participant ranks, where participant ranks >> rdvRanks\n";
    }
    exit(EXIT_FAILURE);
  }
  auto isRdv = atoi(argv[1]);
  assert(isRdv==0 || isRdv ==1);
  auto mbpr = atoi(argv[2])*MILLION;
  assert(mbpr>0);
  auto rdvRanks = atoi(argv[3]);
  assert(rdvRanks>0);
  auto reductionFactor = atoi(argv[4]);
  assert(reductionFactor>1);
  if(!isRdv) {
    assert(rdvRanks*reductionFactor == nprocs);
  }

  sendRecvRdvMapped(MPI_COMM_WORLD, isRdv, mbpr, rdvRanks, reductionFactor);
  std::this_thread::sleep_for(std::chrono::seconds(2));
  sendRecvRdvFanOut(MPI_COMM_WORLD, isRdv, mbpr, rdvRanks, reductionFactor);
  std::this_thread::sleep_for(std::chrono::seconds(2));
  sendRecvMapped(MPI_COMM_WORLD, isRdv, mbpr, rdvRanks, reductionFactor);
  MPI_Finalize();
  return 0;
}
