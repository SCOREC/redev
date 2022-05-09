#include <iostream>
#include <cstdlib>
#include "redev.h"
#include "redev_comm.h"


// see [test_sendrecv](test_sendrecv).

/**
 * \page test_twoclients
 *
 * \par Summary
 * This example demonstrates two clients connecting to a single server.
 * Each client sends messages to, and receives messages from, the server.
 * The code is broken up into a function for the client and a function for the
 * server.
 * The client and server code first sets up the inbound and outbound message
 * layouts then enters into a loop that repeatedly sends and receives messages
 * using the defined layouts.
 *
 * \remark
 * The message layouts in this example are trivial; only a single integer is
 * being sent/receieved and the client and server only have a single process.
 * For a more complete example discussing message layout see [test_sendrecv](\ref test_sendrecv).
 *
 * \par Launching the client and server
 * A dummy [RCBPtn](\ref redev::RCBPtn) object is created for construction of the 
 * [Redev](\ref redev::Redev) instance; the partition is **not** used for creating message layout
 * arrays in the client and server code below.
 * The ADIOS2 parameters for the [BP4](https://adios2.readthedocs.io/en/latest/engines/engines.html#bp4)
 * engine (the default when `isSST` is `false`) are then set and passed into the functions for the client and server.
 * \snippet{lineno} test_twoClients.cpp Main
 *
 * \par Client Setup
 *
 * \snippet{lineno} test_twoClients.cpp Client Setup
 *
 * \par Server Create Clients
 * haksjdhakjdhkda
 * \snippet{lineno} test_twoClients.cpp Server Create Clients
 *
 * \par First Inbound Messages
 * alskdjalk
 * \snippet{lineno} test_twoClients.cpp Server First Inbound Messages from Clients
 *
 * \par First Outbound Messages
 * asldkjal
 * \snippet{lineno} test_twoClients.cpp Server Outbound Messages to Clients
 *
 * \par Client Loop
 * alskdjalk
 * \snippet{lineno} test_twoClients.cpp Client Loop
 *
 * \par Server Loop
 * alskdjalk
 * \snippet{lineno} test_twoClients.cpp Server Loop
 */

void client(redev::Redev& rdv, const int clientId, adios2::Params params, const bool isSST) {
  /// [Client Setup]
  std::stringstream clientName;
  clientName << "client" << clientId;
  auto commPair = rdv.CreateAdiosClient<redev::LO>(clientName.str(),params,isSST);

  //setup outbound message
  std::cout << "sending to server\n";
  redev::LOs dest = redev::LOs{0};
  redev::LOs offsets = redev::LOs{0,1};
  commPair.c2s.SetOutMessageLayout(dest, offsets);

  //first outbound send
  redev::LOs msgs = redev::LOs(1,42+clientId);
  commPair.c2s.Send(msgs.data());

  //first inbound message from server
  std::cout << "recieving from server\n";
  auto msgFromServer = commPair.s2c.Recv();
  auto inMsg = commPair.s2c.GetInMessageLayout();
  REDEV_ALWAYS_ASSERT(inMsg.offset == redev::GOs({0,1}));
  REDEV_ALWAYS_ASSERT(inMsg.srcRanks == redev::GOs({0}));
  REDEV_ALWAYS_ASSERT(inMsg.start == 0);
  REDEV_ALWAYS_ASSERT(inMsg.count == 1);
  REDEV_ALWAYS_ASSERT(msgFromServer[0] == 1337+clientId);
  /// [Client Setup]

  /// [Client Loop]
  for(int iter=0; iter<3; iter++) {
    std::cout << "iter " << iter << "\n";
    //outbound message to server
    redev::LOs outMsg = redev::LOs(1,42+clientId);
    commPair.c2s.Send(outMsg.data());
    //inbound message from server
    auto msg = commPair.s2c.Recv();
    REDEV_ALWAYS_ASSERT(msg[0] == 1337+clientId);
  }
  /// [Client Loop]
}

void server(redev::Redev& rdv, adios2::Params params, const bool isSST) {
  /// [Server Create Clients]
  auto client0 = rdv.CreateAdiosClient<redev::LO>("client0",params,isSST);
  auto client1 = rdv.CreateAdiosClient<redev::LO>("client1",params,isSST);
  /// [Server Create Clients]

  /// [Server First Inbound Messages from Clients]
  std::cout << "recieving from client0\n";
  auto msgs0 = client0.c2s.Recv();
  {
    auto inMsg = client0.c2s.GetInMessageLayout();
    REDEV_ALWAYS_ASSERT(inMsg.offset == redev::GOs({0,1}));
    REDEV_ALWAYS_ASSERT(inMsg.srcRanks == redev::GOs({0}));
    REDEV_ALWAYS_ASSERT(inMsg.start == 0);
    REDEV_ALWAYS_ASSERT(inMsg.count == 1);
    REDEV_ALWAYS_ASSERT(msgs0[0] == 42);
  }

  std::cout << "recieving from client1\n";
  auto msgs1 = client1.c2s.Recv();
  {
    auto inMsg = client1.c2s.GetInMessageLayout();
    REDEV_ALWAYS_ASSERT(inMsg.offset == redev::GOs({0,1}));
    REDEV_ALWAYS_ASSERT(inMsg.srcRanks == redev::GOs({0}));
    REDEV_ALWAYS_ASSERT(inMsg.start == 0);
    REDEV_ALWAYS_ASSERT(inMsg.count == 1);
    REDEV_ALWAYS_ASSERT(msgs1[0] == 43);
  }
  /// [Server First Inbound Messages from Clients]

  /// [Server Outbound Messages to Clients]
  std::cout << "sending to client0\n";
  redev::LOs dest = redev::LOs{0};
  redev::LOs offsets = redev::LOs{0,1};
  client0.s2c.SetOutMessageLayout(dest, offsets);
  redev::LOs msgs = redev::LOs(1,1337);
  client0.s2c.Send(msgs.data());

  std::cout << "sending to client1\n";
  client1.s2c.SetOutMessageLayout(dest, offsets);
  msgs = redev::LOs(1,1338);
  client1.s2c.Send(msgs.data());
  /// [Server Outbound Messages to Clients]

  /// [Server Loop]
  for(int iter=0; iter<3; iter++) {
    std::cout << "iter " << iter << "\n";
    //inbound messages from clients
    auto inMsg0 = client0.c2s.Recv();
    REDEV_ALWAYS_ASSERT(inMsg0[0] == 42);
    auto inMsg1 = client1.c2s.Recv();
    REDEV_ALWAYS_ASSERT(inMsg1[0] == 43);
    //outbound messages to clients
    redev::LOs outMsg0 = redev::LOs(1,1337);
    client0.s2c.Send(outMsg0.data());
    redev::LOs outMsg1 = redev::LOs(1,1338);
    client1.s2c.Send(outMsg1.data());
  }
  /// [Server Loop]
}
/// [Server]

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
  /// [Main]
  //dummy partition vector data
  const auto dim = 1;
  auto ranks = isRdv ? redev::LOs({0}) : redev::LOs(1);
  auto cuts = isRdv ? redev::Reals({0}) : redev::Reals(1);
  auto ptn = redev::RCBPtn(dim,ranks,cuts);
  redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv);
  const bool isSST = false;
  adios2::Params params{ {"Streaming", "On"}, {"OpenTimeoutSecs", "6"}};
  if(!isRdv) {
    client(rdv,clientId,params,isSST);
  } else {
    server(rdv,params,isSST);
  }
  std::cout << "done\n";
  /// [Main]
  }
  MPI_Finalize();
  return 0;
}
