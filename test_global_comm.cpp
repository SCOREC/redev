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
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  auto isRdv = atoi(argv[1]);
  fprintf(stderr, "rank %d isRdv %d\n", rank, isRdv);
  {
    // dummy partition vector data
    const auto dim = 2;
    auto ranks = isRdv ? redev::LOs({0, 1, 2, 3}) : redev::LOs(4);
    auto cuts = isRdv ? redev::Reals({0, 0.5, 0.75, 0.25}) : redev::Reals(4);
    auto ptn = redev::RCBPtn(dim, ranks, cuts);
    redev::Redev rdv(MPI_COMM_WORLD, redev::Partition{std::move(ptn)},
                     static_cast<redev::ProcessType>(isRdv));
    adios2::Params params{{"Streaming", "On"}, {"OpenTimeoutSecs", "2"}};
    std::string name = "bar";
    auto channel =
      rdv.CreateAdiosChannel(name, params, redev::TransportType::BP4);
    auto commPair = channel.CreateComm<redev::LO>(name, MPI_COMM_WORLD,
                                                  redev::CommType::Global);

    // test the ptn comm
    // the non-rendezvous app sends to the rendezvous app
    if (!isRdv) {
      // send data to test global comm
      auto msgs = redev::LOs{0, 2};
      if (rank == 0) {
        channel.BeginSendCommunicationPhase();
        commPair.Send(msgs.data(), redev::Mode::Deferred);
        channel.EndSendCommunicationPhase();
      }
    } else {
      channel.BeginReceiveCommunicationPhase();
      auto msgVec = commPair.Recv(redev::Mode::Deferred);
      channel.EndReceiveCommunicationPhase();

      // receive global date
      channel.BeginReceiveCommunicationPhase();
      msgVec = commPair.Recv(redev::Mode::Deferred);
      channel.EndReceiveCommunicationPhase();
      REDEV_ALWAYS_ASSERT(msgVec == redev::LOs({0, 2}));
    }
  }

  MPI_Finalize();
  return 0;
}