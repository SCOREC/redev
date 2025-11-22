#include <iostream>
#include <cstdlib>
#include "redev.h"

int main(int argc, char** argv)
{
  int rank, nproc;
  MPI_Init(&argc, &argv);
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0]
              << " <1=isRendezvousApp,0=isParticipant>\n";
    exit(EXIT_FAILURE);
  }

  MPI_Comm world = MPI_COMM_WORLD;
  MPI_Comm_rank(world, &rank);
  MPI_Comm_size(world, &nproc);
  auto isRdv = atoi(argv[1]);
  fprintf(stderr, "rank %d isRdv %d\n", rank, isRdv);

  const auto dim = 2;
  auto ranks = isRdv ? redev::LOs({0}) : redev::LOs(1);
  auto cuts = isRdv ? redev::Reals({0}) : redev::Reals(1);
  auto ptn = redev::RCBPtn(dim, ranks, cuts);

  int color = isRdv ? 0 : 1;

  // Split communicator into two groups
  MPI_Comm localComm;
  MPI_Comm_split(world, color, rank, &localComm);

  // dummy partition vector data
  redev::Redev rdv(localComm, redev::Partition{std::move(ptn)},
                   static_cast<redev::ProcessType>(isRdv));
  adios2::Params params{{"Streaming", "On"}, {"OpenTimeoutSecs", "4"}};

  std::string name = "bar";


  auto channel =
    rdv.CreateAdiosChannel(name, params, redev::TransportType::BP4);
  auto commPair =
    channel.CreateComm<redev::Real>(name, localComm, redev::CommType::Global);

  // send data to test global comm
  redev::Reals vals = {3.14};
  auto* msgs = &vals[0];
  std::string varName = "barVar";
  size_t n = vals.size();
  // test the ptn comm
  // the non-rendezvous app sends to the rendezvous app
  if (!isRdv) {
    commPair.SetCommParams(varName, n);
    if (rank == 0) {
      channel.BeginSendCommunicationPhase();
      commPair.Send(msgs, redev::Mode::Synchronous);
      channel.EndSendCommunicationPhase();
    }
  } else {
    // receive global date
    channel.BeginReceiveCommunicationPhase();
    commPair.SetCommParams(varName, n);
    auto msgVec = commPair.Recv(redev::Mode::Synchronous);
    channel.EndReceiveCommunicationPhase();
    REDEV_ALWAYS_ASSERT(msgVec[0] == redev::Real{3.14});
    printf("\nTest passed.");
  }
  MPI_Finalize();
  return 0;
}