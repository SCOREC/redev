#ifndef REDEV_REDEV_BIDIRECTIONAL_COMM_H
#define REDEV_REDEV_BIDIRECTIONAL_COMM_H
#include "redev_assert.h"
#include "redev_comm.h"
#include <memory>
namespace redev {
/**
 * A BidirectionalComm is a communicator that can send and receive data
 * If you are on a client rank sending sends data to the server and receiving
 * retrieves data from server.
 * If you are on a server rank sending sends data to the client and receiving
 * retrieves data from client.
 */
template <class T> class BidirectionalComm {
public:
  BidirectionalComm() = default;
  BidirectionalComm(std::unique_ptr<Communicator<T>> sender_,
                    std::unique_ptr<Communicator<T>> receiver_)
      : sender(std::move(sender_)), receiver(std::move(receiver_)) {
    REDEV_ALWAYS_ASSERT(sender != nullptr);
    REDEV_ALWAYS_ASSERT(receiver != nullptr);
  }
  void SetOutMessageLayout(LOs &dest, LOs &offsets) {
    REDEV_ALWAYS_ASSERT(sender != nullptr);
    sender->SetOutMessageLayout(dest, offsets);
  }
  InMessageLayout GetInMessageLayout() {
    REDEV_ALWAYS_ASSERT(receiver != nullptr);
    return receiver->GetInMessageLayout();
  }

  void Send(T *msgs, Mode mode = Mode::Deferred) {
    REDEV_ALWAYS_ASSERT(sender != nullptr);
    sender->Send(msgs, mode);
  }
  std::vector<T> Recv(Mode mode = Mode::Deferred) {
    REDEV_ALWAYS_ASSERT(receiver != nullptr);
    return receiver->Recv(mode);
  }

private:
  std::unique_ptr<Communicator<T>> sender;
  std::unique_ptr<Communicator<T>> receiver;
};

enum class CommunicatorDataType {
  INT8,
  INT16,
  INT32,
  INT64,
  UINT8,
  UINT16,
  UINT32,
  UINT64,
  LONG_INT,
  FLOAT,
  DOUBLE,
  LONG_DOUBLE,
  COMPLEX_DOUBLE
};
// support types that adios supports.
// See: ADIOS2/bindings/C/adios2/c/adios2_c_internal.inl
using CommV =
    std::variant<BidirectionalComm<int8_t>, BidirectionalComm<int16_t>,
                 BidirectionalComm<int32_t>, BidirectionalComm<int64_t>,
                 BidirectionalComm<uint8_t>, BidirectionalComm<uint16_t>,
                 BidirectionalComm<uint32_t>, BidirectionalComm<uint64_t>,
                 BidirectionalComm<long int>, BidirectionalComm<float>,
                 BidirectionalComm<double>, BidirectionalComm<long double>,
                 BidirectionalComm<std::complex<double>>>;

template <CommunicatorDataType> struct CommunicatorTypeMap;
template <> struct CommunicatorTypeMap<CommunicatorDataType::INT8> {
  using type = int8_t;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::INT16> {
  using type = int16_t;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::INT32> {
  using type = int32_t;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::INT64> {
  using type = int64_t;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::UINT8> {
  using type = uint8_t;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::UINT16> {
  using type = uint16_t;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::UINT32> {
  using type = uint32_t;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::UINT64> {
  using type = uint64_t;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::LONG_INT> {
  using type = long int;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::FLOAT> {
  using type = float;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::DOUBLE> {
  using type = double;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::LONG_DOUBLE> {
  using type = long double;
};
template <> struct CommunicatorTypeMap<CommunicatorDataType::COMPLEX_DOUBLE> {
  using type = std::complex<double>;
};
template <typename T> struct InvCommunicatorTypeMap;
template <>
struct InvCommunicatorTypeMap<int8_t>
    : std::integral_constant<CommunicatorDataType, CommunicatorDataType::INT8> {
};
template <>
struct InvCommunicatorTypeMap<int16_t>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::INT16> {};
template <>
struct InvCommunicatorTypeMap<int32_t>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::INT32> {};
template <>
struct InvCommunicatorTypeMap<int64_t>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::INT64> {};
template <>
struct InvCommunicatorTypeMap<uint8_t>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::UINT8> {};
template <>
struct InvCommunicatorTypeMap<uint16_t>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::UINT16> {};
template <>
struct InvCommunicatorTypeMap<uint32_t>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::UINT32> {};
template <>
struct InvCommunicatorTypeMap<uint64_t>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::UINT64> {};
template <>
struct InvCommunicatorTypeMap<long int>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::LONG_INT> {};
template <>
struct InvCommunicatorTypeMap<float>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::FLOAT> {};
template <>
struct InvCommunicatorTypeMap<double>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::DOUBLE> {};
template <>
struct InvCommunicatorTypeMap<long double>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::LONG_DOUBLE> {};
template <>
struct InvCommunicatorTypeMap<std::complex<double>>
    : std::integral_constant<CommunicatorDataType,
                             CommunicatorDataType::COMPLEX_DOUBLE> {};

} // namespace redev
#endif // REDEV_REDEV_BIDIRECTIONAL_COMM_H
