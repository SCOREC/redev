#include <iostream>
#include <cstdlib>
#include "redev.h"
#include "redev_comm.h"

//void pingpong() {
//  for(int iter=0; iter<3; iter++) {
//    // the non-rendezvous app sends to the rendezvous app
//    if(!isRdv) {
//      if(iter==0) {
//        redev::LOs dest = redev::LOs{0};
//        redev::LOs offsets = redev::LOs{0,1};
//        commPair.c2s.SetOutMessageLayout(dest, offsets);
//      }
//      redev::LOs msgs = redev::LOs(1,42);
//      commPair.c2s.Send(msgs.data());
//    } else {
//      auto msgs = commPair.c2s.Recv();
//      if(iter == 0) {
//        auto inMsg = commPair.c2s.GetInMessageLayout();
//        REDEV_ALWAYS_ASSERT(inMsg.offset == redev::GOs({0,1}));
//        REDEV_ALWAYS_ASSERT(inMsg.srcRanks == redev::GOs({0}));
//        REDEV_ALWAYS_ASSERT(inMsg.start == 0);
//        REDEV_ALWAYS_ASSERT(inMsg.count == 1);
//      }
//      REDEV_ALWAYS_ASSERT(msgs[0] == 42);
//    }
//    // the rendezvous app sends to the non-rendezvous app
//    if(isRdv) {
//      if(iter==0) {
//        redev::LOs dest = redev::LOs{0};
//        redev::LOs offsets = redev::LOs{0,1};
//        commPair.s2c.SetOutMessageLayout(dest, offsets);
//      }
//      redev::LOs msgs = redev::LOs(1,1337);
//      commPair.s2c.Send(msgs.data());
//    } else {
//      auto msgs = commPair.s2c.Recv();
//      if(iter==0) {
//        auto inMsg = commPair.s2c.GetInMessageLayout();
//        REDEV_ALWAYS_ASSERT(inMsg.offset == redev::GOs({0,1}));
//        REDEV_ALWAYS_ASSERT(inMsg.srcRanks == redev::GOs({0}));
//        REDEV_ALWAYS_ASSERT(inMsg.start == 0);
//        REDEV_ALWAYS_ASSERT(inMsg.count == 1);
//      }
//      REDEV_ALWAYS_ASSERT(msgs[0] == 1337);
//    }
//  }
//}

void client(redev::Redev& rdv, const int clientId, adios2::Params params, const bool isSST) {
  std::stringstream clientName;
  clientName << "client" << clientId;
  auto commPair = rdv.CreateAdiosClient<redev::LO>(clientName.str(),params,isSST);

  //setup outbound message
  redev::LOs dest = redev::LOs{0};
  redev::LOs offsets = redev::LOs{0,1};
  commPair.c2s.SetOutMessageLayout(dest, offsets);

  //first outbound send
  redev::LOs msgs = redev::LOs(1,42+clientId);
  commPair.c2s.Send(msgs.data());
}

void server(redev::Redev& rdv, adios2::Params params, const bool isSST) {
  auto client0 = rdv.CreateAdiosClient<redev::LO>("client0",params,isSST);
  auto client1 = rdv.CreateAdiosClient<redev::LO>("client1",params,isSST);

  //first incoming message from client0
  auto msgs0 = client0.c2s.Recv();
  //sanity check layout
  {
    auto inMsg = client0.c2s.GetInMessageLayout();
    REDEV_ALWAYS_ASSERT(inMsg.offset == redev::GOs({0,1}));
    REDEV_ALWAYS_ASSERT(inMsg.srcRanks == redev::GOs({0}));
    REDEV_ALWAYS_ASSERT(inMsg.start == 0);
    REDEV_ALWAYS_ASSERT(inMsg.count == 1);
    REDEV_ALWAYS_ASSERT(msgs0[0] == 42);
  }
  //first incoming message from client1
  auto msgs1 = client1.c2s.Recv();
  {
    auto inMsg = client1.c2s.GetInMessageLayout();
    REDEV_ALWAYS_ASSERT(inMsg.offset == redev::GOs({0,1}));
    REDEV_ALWAYS_ASSERT(inMsg.srcRanks == redev::GOs({0}));
    REDEV_ALWAYS_ASSERT(inMsg.start == 0);
    REDEV_ALWAYS_ASSERT(inMsg.count == 1);
    REDEV_ALWAYS_ASSERT(msgs1[0] == 43);
  }
}

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  if(argc > 2) {
    std::cerr << "Usage: " << argv[0] << "<clientId=0|1>\n";
    exit(EXIT_FAILURE);
  }
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  const auto clientId = argc == 2 ? atoi(argv[1]) : -1;
  REDEV_ALWAYS_ASSERT(clientId >= -1 && clientId <= 1);
  const auto isRdv = (clientId == -1);
  fprintf(stderr, "rank %d isRdv %d clientId %d\n", rank, isRdv, clientId);
  if(nproc != 1) {
      std::cerr << "Each client and the server must have exactly 1 processes.\n";
      exit(EXIT_FAILURE);
  }
  {
  //dummy partition vector data
  const auto dim = 1;
  auto ranks = isRdv ? redev::LOs({0}) : redev::LOs(1);
  auto cuts = isRdv ? redev::Reals({0}) : redev::Reals(1);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv);
  const bool isSST = false;
  adios2::Params params{ {"Streaming", "On"}, {"OpenTimeoutSecs", "4"}};
  if(!isRdv) {
    client(rdv,clientId,params,isSST);
  } else {
    server(rdv,params,isSST);
  }
  }
  MPI_Finalize();
  return 0;
}
