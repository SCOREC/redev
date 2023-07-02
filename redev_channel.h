#ifndef REDEV_REDEV_CHANNEL_H
#define REDEV_REDEV_CHANNEL_H
#include "redev_bidirectional_comm.h"
#include <variant>

namespace redev {

class Channel {
public:
  template <typename T>
  Channel(T &&channel)
      : pimpl_{new ChannelModel<std::remove_cv_t<std::remove_reference_t<T>>>(
            std::forward<T>(channel))},
        send_communication_phase_active_(false),
        receive_communication_phase_active_(false) {
        REDEV_FUNCTION_TIMER;
        }

  // For cases where we may be interested in storing the comm variant rather
  // than the exact type this function can be used to reduce the runtime
  // overhead of converting from the variant to the explicit type back to the
  // variant
  template <typename T> [[nodiscard]] CommV CreateCommV(std::string name, MPI_Comm comm) {
    REDEV_FUNCTION_TIMER;
    return pimpl_->CreateComm(std::move(name), comm,
                              InvCommunicatorTypeMap<T>::value);
  }
  // convenience typesafe wrapper to get back the specific communicator type
  // rather than the variant. This is here to simplify updating legacy code
  // that expects a typed communicator to be created.
  template <typename T>
  [[nodiscard]] BidirectionalComm<T> CreateComm(std::string name, MPI_Comm comm) {
    REDEV_FUNCTION_TIMER;
    return std::get<BidirectionalComm<T>>(CreateCommV<T>(std::move(name), comm));
  }
  void BeginSendCommunicationPhase() {
    REDEV_FUNCTION_TIMER;
    REDEV_ALWAYS_ASSERT(InSendCommunicationPhase() == false);
    pimpl_->BeginSendCommunicationPhase();
    send_communication_phase_active_ = true;
  }
  void EndSendCommunicationPhase() {
    REDEV_FUNCTION_TIMER;
    REDEV_ALWAYS_ASSERT(InSendCommunicationPhase() == true);
    pimpl_->EndSendCommunicationPhase();
    send_communication_phase_active_ = false;
  }
  void BeginReceiveCommunicationPhase() {
    REDEV_FUNCTION_TIMER;
    REDEV_ALWAYS_ASSERT(InReceiveCommunicationPhase() == false);
    pimpl_->BeginReceiveCommunicationPhase();
    receive_communication_phase_active_ = true;
  }
  void EndReceiveCommunicationPhase() {
    REDEV_FUNCTION_TIMER;
    REDEV_ALWAYS_ASSERT(InReceiveCommunicationPhase() == true);
    pimpl_->EndReceiveCommunicationPhase();
    receive_communication_phase_active_ = false;
  }
  [[nodiscard]] bool InSendCommunicationPhase() const noexcept {
    REDEV_FUNCTION_TIMER;
    return send_communication_phase_active_;
  }
  [[nodiscard]] bool InReceiveCommunicationPhase() const noexcept {
    REDEV_FUNCTION_TIMER;
    return receive_communication_phase_active_;
  }

  template <typename Func, typename... Args>
  auto SendPhase(const Func &f, Args &&...args) {
    auto sg = SendPhaseScope(*this);
    return f(std::forward<Args>(args)...);
  }
  template <typename Func, typename... Args>
  auto ReceivePhase(const Func &f, Args &&...args) {
    auto sg = ReceivePhaseScope(*this);
    return f(std::forward<Args>(args)...);
  }

private:
  class ChannelConcept {
  public:
    virtual CommV CreateComm(std::string &&, MPI_Comm, CommunicatorDataType) = 0;
    virtual void BeginSendCommunicationPhase() = 0;
    virtual void EndSendCommunicationPhase() = 0;
    virtual void BeginReceiveCommunicationPhase() = 0;
    virtual void EndReceiveCommunicationPhase() = 0;
    virtual ~ChannelConcept() noexcept {}
  };
  template <typename T> class ChannelModel : public ChannelConcept {
  public:
    ChannelModel(T &&impl) : ChannelConcept(), impl_(std::forward<T>(impl)) {}
    // since we don't have templated virtual functions, we convert the type to a
    // runtime parameter then extract out the appropriate type based on the
    // typemap. Assuming the typemap/inverse type map are correct, this is
    // entirely safe unlike doing it in user code. Although, it's not the most
    // beautiful construction in the world.
    ~ChannelModel() noexcept final {}
    [[nodiscard]] CommV CreateComm(std::string &&name, MPI_Comm comm,
                                   CommunicatorDataType type) final {
      REDEV_FUNCTION_TIMER;
      switch (type) {
      case CommunicatorDataType::INT8:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::INT8>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::INT16:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::INT16>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::INT32:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::INT32>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::INT64:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::INT64>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::UINT8:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::UINT8>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::UINT16:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::UINT16>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::UINT32:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::UINT32>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::UINT64:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::UINT64>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::LONG_INT:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::LONG_INT>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::FLOAT:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::FLOAT>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::DOUBLE:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::DOUBLE>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::LONG_DOUBLE:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::LONG_DOUBLE>::type>(
            std::move(name), comm)};
      case CommunicatorDataType::COMPLEX_DOUBLE:
        return CommV{impl_.template CreateComm<
            CommunicatorTypeMap<CommunicatorDataType::COMPLEX_DOUBLE>::type>(
            std::move(name), comm)};
      }
      return {};
    }
    void BeginSendCommunicationPhase() final {
      REDEV_FUNCTION_TIMER;
      impl_.BeginSendCommunicationPhase();
    }
    void EndSendCommunicationPhase() final {
      REDEV_FUNCTION_TIMER;
      impl_.EndSendCommunicationPhase();
    }
    void BeginReceiveCommunicationPhase() final {
      REDEV_FUNCTION_TIMER;
      impl_.BeginReceiveCommunicationPhase();
    }
    void EndReceiveCommunicationPhase() final {
      REDEV_FUNCTION_TIMER;
      impl_.EndReceiveCommunicationPhase();
    }

  private:
    T impl_;
  };
  class SendPhaseScope {
  public:
    explicit SendPhaseScope(Channel &channel) : channel_(channel) {
      channel_.BeginSendCommunicationPhase();
    }
    ~SendPhaseScope() { channel_.EndSendCommunicationPhase(); }

  private:
    Channel &channel_;
  };
  class ReceivePhaseScope {
  public:
    explicit ReceivePhaseScope(Channel &channel) : channel_(channel) {
      channel_.BeginReceiveCommunicationPhase();
    }
    ~ReceivePhaseScope() { channel_.EndReceiveCommunicationPhase(); }

  private:
    Channel &channel_;
  };

  std::unique_ptr<ChannelConcept> pimpl_;
  bool send_communication_phase_active_;
  bool receive_communication_phase_active_;
};

class NoOpChannel {
public:
  template <typename T>
  [[nodiscard]]
  BidirectionalComm<T> CreateComm(std::string, MPI_Comm) {
    return {std::make_unique<NoOpComm<T>>(), std::make_unique<NoOpComm<T>>()};
  }
  void BeginSendCommunicationPhase(){}
  void EndSendCommunicationPhase(){}
  void BeginReceiveCommunicationPhase(){}
  void EndReceiveCommunicationPhase(){}
};

} // namespace redev
#endif // REDEV_REDEV_CHANNEL_H
