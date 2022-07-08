#include <iostream>
#include <cstdlib>
#include "redev.h"

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
 * Client setup begins with creation of the redev::BidirectionalComm communication object via the call to redev::CreateAdiosClient.  Note, the string for each created redev::BidirectionalComm must match in the Server and Client calls.
 *
 * Given that the data being transferred is a single integer and each Client and
 * Server is only running on a single process the data layout (`dest` and
 * `offsets`) for the outbound and inbound messages are trivial.
 *
 * The first outbound message has a single entry in the `dest` vector, `0`, to specify
 * that all messages are being sent to Server process zero.
 * Likewise, the `offsets` array is set to `{0,1}` to denote that messages for
 * the destination process start at position zero in the messages array and there
 * is only a single entry (`offsets[1]-offsets[0]=1-0=1`).
 * These vectors are passed into redev::AdiosComm::SetOutMessageLayout
 * for the `c2s` (Client-to-Server) member of the `commPair` struct.
 * As long as the layout remains the same, no additional calls to
 * redev::AdiosComm::SetOutMessageLayout are needed.
 *
 * As with the outbound message, the inbound message layout defines a single
 * entry coming from Server process zero..
 * redev::AdiosComm::SetOutMessageLayout is called to retrieve the layout of the
 * message that was just read with the call to redev::AdiosComm::Recv.
 * Note, that these methods are called on the `s2c` (Server-to-Client) member
 * of the `commPair` struct.
 *
 * \snippet{lineno} test_twoClients.cpp Client Setup
 *
 * \par Server Create Clients
 *
 * In the Client code only a single redev::BidirectionalComm is needed.
 * As there are two Clients, the Server must call redev::CreateAdiosClient
 * twice; once for "client0" and again for "client1".
 *
 * \snippet{lineno} test_twoClients.cpp Server Create Clients
 *
 * \par First Inbound Messages
 *
 * As was done in the Client code when receiving a message from the Server, the message layout
 * is retrieved and checked that it defines a single entry coming from process zero of each Client.
 * This sequence is repeated for both the message coming from each Client using
 * the respective redev::BidirectionalComm `c2s` (Client-to-Server) struct member.
 *
 * \snippet{lineno} test_twoClients.cpp Server First Inbound Messages from Clients
 *
 * \par First Outbound Messages
 *
 * Now that the first 'Forward' messages (Client-to-Server) have been received
 * the layout of the 'Reverse' messages (Server-to-Client) can be constructed
 * using the data returned from the call to
 * redev::AdiosComm::GetInMessageLayout.
 * But, given that this example is only sending and receiving a single redev::LO
 * between Client and Server, the layout of the Reverse message is hardcoded.
 * See the wdmapp_coupling documentation for an example that constructs the
 * Reverse message layout for data associated with an unstructured mesh.
 *
 * As with the outbound messages from the Client to the Server, the call to
 * redev::AdiosComm::SetOutMessageLayout only needs to be made once for each
 * Server-to-Client (`s2c` within the `client0` and `client1`
 * `commPair` objects).
 *
 * \snippet{lineno} test_twoClients.cpp Server Outbound Messages to Clients
 *
 * \par Client Loop
 *
 * With the outbound message layout setup for each Client-to-Server Send a loop
 * over communication rounds is entered that reuses that layout.
 * The round first packs and sends a message to the Server (using the `c2s`
 * member of the `commPair` struct) then receives a message from the Server
 * (using the `s2c` member).
 *
 * \snippet{lineno} test_twoClients.cpp Client Loop
 *
 * \par Server Loop
 * 
 * The Server loop over communication rounds begins by receiving messages from
 * the Clients (via the `client[0|1].c2s` objects) then sends messages back to
 * the Clients (via `client[0|1].s2c`).
 *
 * \snippet{lineno} test_twoClients.cpp Server Loop
 */

void client(redev::Redev& rdv, const int clientId, adios2::Params params, const bool isSST) {
  /// [Client Setup]
  std::stringstream clientName;
  clientName << "client" << clientId;
  auto commPair = rdv.CreateAdiosClient<redev::LO>(clientName.str(),params,static_cast<redev::TransportType>(isSST));

  //setup outbound message
  std::cout << "sending to server\n";
  redev::LOs dest = redev::LOs{0};
  redev::LOs offsets = redev::LOs{0,1};
  commPair.SetOutMessageLayout(dest, offsets);

  //first outbound send
  redev::LOs msgs = redev::LOs(1,42+clientId);
  commPair.Send(msgs.data());

  //first inbound message from server
  std::cout << "recieving from server\n";
  auto msgFromServer = commPair.Recv();
  auto inMsg = commPair.GetInMessageLayout();
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
    commPair.Send(outMsg.data());
    //inbound message from server
    auto msg = commPair.Recv();
    REDEV_ALWAYS_ASSERT(msg[0] == 1337+clientId);
  }
  /// [Client Loop]
}

void server(redev::Redev& rdv, adios2::Params params, const bool isSST) {
  /// [Server Create Clients]
  auto client0 = rdv.CreateAdiosClient<redev::LO>("client0",params,static_cast<redev::TransportType>(isSST));
  auto client1 = rdv.CreateAdiosClient<redev::LO>("client1",params,static_cast<redev::TransportType>(isSST));
  /// [Server Create Clients]

  /// [Server First Inbound Messages from Clients]
  std::cout << "recieving from client0\n";
  auto msgs0 = client0.Recv();
  {
    auto inMsg = client0.GetInMessageLayout();
    REDEV_ALWAYS_ASSERT(inMsg.offset == redev::GOs({0,1}));
    REDEV_ALWAYS_ASSERT(inMsg.srcRanks == redev::GOs({0}));
    REDEV_ALWAYS_ASSERT(inMsg.start == 0);
    REDEV_ALWAYS_ASSERT(inMsg.count == 1);
    REDEV_ALWAYS_ASSERT(msgs0[0] == 42);
  }

  std::cout << "recieving from client1\n";
  auto msgs1 = client1.Recv();
  {
    auto inMsg = client1.GetInMessageLayout();
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
  client0.SetOutMessageLayout(dest, offsets);
  redev::LOs msgs = redev::LOs(1,1337);
  client0.Send(msgs.data());

  std::cout << "sending to client1\n";
  client1.SetOutMessageLayout(dest, offsets);
  msgs = redev::LOs(1,1338);
  client1.Send(msgs.data());
  /// [Server Outbound Messages to Clients]

  /// [Server Loop]
  for(int iter=0; iter<3; iter++) {
    std::cout << "iter " << iter << "\n";
    //inbound messages from clients
    auto inMsg0 = client0.Recv();
    REDEV_ALWAYS_ASSERT(inMsg0[0] == 42);
    auto inMsg1 = client1.Recv();
    REDEV_ALWAYS_ASSERT(inMsg1[0] == 43);
    //outbound messages to clients
    redev::LOs outMsg0 = redev::LOs(1,1337);
    client0.Send(outMsg0.data());
    redev::LOs outMsg1 = redev::LOs(1,1338);
    client1.Send(outMsg1.data());
  }
  /// [Server Loop]
}
/// [Server]

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  if(argc > 3) {
    std::cerr << "Usage: " << argv[0] << "<enableSST=0|1> <clientId=0|1>\n";
    exit(EXIT_FAILURE);
  }
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  const auto isSST = (atoi(argv[1]) == 1);
  const auto clientId = atoi(argv[2]);
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
  redev::Redev rdv(MPI_COMM_WORLD,ptn,static_cast<redev::ProcessType>(isRdv));
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
