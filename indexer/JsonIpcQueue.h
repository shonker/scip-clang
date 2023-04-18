#ifndef SCIP_CLANG_JSON_IPC_QUEUE_H
#define SCIP_CLANG_JSON_IPC_QUEUE_H

#include <chrono>
#include <memory>
#include <optional>
#include <system_error>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#include "boost/interprocess/ipc/message_queue.hpp"
#pragma clang diagnostic pop

#include "llvm/ADT/Optional.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

#include "indexer/IpcMessages.h"

namespace scip_clang {

struct TimeoutError : public llvm::ErrorInfo<TimeoutError> {
  static char ID;
  virtual void log(llvm::raw_ostream &os) const override {
    os << "timeout when receiving from queue";
  }
  virtual std::error_code convertToErrorCode() const override {
    return std::make_error_code(std::errc::timed_out);
  }
};

enum class QueueInit {
  CreateOnly,
  OpenOnly,
};

class JsonIpcQueue final {
  using BoostQueue = boost::interprocess::message_queue;
  std::unique_ptr<BoostQueue> queue;
  std::string name;
  QueueInit queueInit;

  [[nodiscard]] std::optional<boost::interprocess::interprocess_exception>
  sendValue(const llvm::json::Value &t);

  // Tries to wait for waitMillis; if that succeeds, then attempts to parse the
  // result.
  llvm::Expected<llvm::json::Value> timedReceive(uint64_t waitMillis);

public:
  // Available for MessageQueues's constructor. DO NOT CALL DIRECTLY.
  JsonIpcQueue() : queue(), name(), queueInit(QueueInit::OpenOnly) {}

  JsonIpcQueue(JsonIpcQueue &&) = default;
  JsonIpcQueue &operator=(JsonIpcQueue &&) = default;
  JsonIpcQueue(const JsonIpcQueue &) = delete;
  JsonIpcQueue &operator=(const JsonIpcQueue &) = delete;

  template <typename... Args>
  static JsonIpcQueue create(std::string &&name, Args &&...args) {
    JsonIpcQueue j{};
    j.name = std::move(name);
    j.queue = std::make_unique<BoostQueue>(boost::interprocess::create_only,
                                           j.name.c_str(),
                                           std::forward<Args>(args)...);
    j.queueInit = QueueInit::CreateOnly;
    return j;
  }

  static JsonIpcQueue open(std::string &&name) {
    JsonIpcQueue j{};
    j.name = std::move(name);
    j.queue = std::make_unique<BoostQueue>(boost::interprocess::open_only,
                                           j.name.c_str());
    j.queueInit = QueueInit::OpenOnly;
    return j;
  }

  /// The destructor removes the queue iff the constructor
  /// created the queue.
  ~JsonIpcQueue();

  template <typename T>
  [[nodiscard]] std::optional<boost::interprocess::interprocess_exception>
  send(const T &t) {
    return this->sendValue(llvm::json::Value(t));
  }

  enum class ReceiveStatus {
    Timeout,
    ParseFailure,
    ParseSuccess,
  };

  template <typename T>
  llvm::Error timedReceive(T &t, std::chrono::seconds waitDuration) {
    auto durationMillis =
        std::chrono::duration_cast<std::chrono::milliseconds>(waitDuration)
            .count();
    auto valueOrErr = this->timedReceive(durationMillis);
    if (auto err = valueOrErr.takeError()) {
      return err;
    }
    llvm::json::Path::Root root("ipc-message");
    if (scip_clang::fromJSON(*valueOrErr, t, root)) {
      return llvm::Error::success();
    }
    return root.getError();
  }
};

struct IpcOptions;

// Type representing the driver<->worker queues.
//
// This type doesn't have a forDriver static method because
// the driver has N send queues for N workers.
struct MessageQueuePair {
  JsonIpcQueue driverToWorker;
  JsonIpcQueue workerToDriver;

  static MessageQueuePair forWorker(const IpcOptions &);
};

} // namespace scip_clang

#endif // SCIP_CLANG_JSON_IPC_QUEUE_H
