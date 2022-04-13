#ifndef SRC_NODE_BOB_H_
#define SRC_NODE_BOB_H_

#include <functional>

namespace node {
namespace bob {

constexpr size_t kMaxCountHint = 16;

// Negative status codes indicate error conditions.
enum Status : int {
  // Indicates that an attempt was made to pull after end.
  STATUS_EOS = -1,

  // Indicates the end of the stream. No additional
  // data will be available and the consumer should stop
  // pulling.
  STATUS_END = 0,

  // Indicates that there is additional data available
  // and the consumer may continue to pull.
  STATUS_CONTINUE = 1,

  // Indicates that there is no additional data available
  // but the stream has not ended. The consumer should not
  // continue to pull but may resume pulling later when
  // data is available.
  STATUS_BLOCK = 2,

  // Indicates that there is no additional data available
  // but the stream has not ended and the source will call
  // next again later when data is available. STATUS_WAIT
  // must not be used with the OPTIONS_SYNC option.
  STATUS_WAIT = 3,
};

enum Options : int {
  OPTIONS_NONE = 0,

  // Indicates that the consumer is requesting the end
  // of the stream.
  OPTIONS_END = 1,

  // Indicates that the consumer requires the source to
  // invoke Next synchronously. If the source is
  // unable to provide data immediately but the
  // stream has not yet ended, it should call Next
  // using STATUS_BLOCK. When not set, the source
  // may call Next asynchronously.
  OPTIONS_SYNC = 2
};

// There are Sources and there are Consumers.
//
// Consumers get data by calling Source::Pull,
// optionally passing along a status and allocated
// buffer space for the source to fill, and a Next
// function the Source will call when data is
// available.
//
// The source calls Next to deliver the data. It can
// choose to either use the allocated buffer space
// provided by the consumer or it can allocate its own
// buffers and push those instead. If it allocates
// its own, it can send a Done function that the
// Sink will call when it is done consuming the data.
using Done = std::function<void(size_t)>;
template <typename T>
using Next = std::function<void(int, const T*, size_t count, Done done)>;

template <typename T>
class Source {
 public:
  virtual int Pull(
      Next<T> next,
      int options,
      T* data,
      size_t count,
      size_t max_count_hint = kMaxCountHint) = 0;
};


template <typename T>
class SourceImpl : public Source<T> {
 public:
  int Pull(
      Next<T> next,
      int options,
      T* data,
      size_t count,
      size_t max_count_hint = kMaxCountHint) override;

  bool is_eos() const { return eos_; }

 protected:
  virtual int DoPull(
      Next<T> next,
      int options,
      T* data,
      size_t count,
      size_t max_count_hint) = 0;

 private:
  bool eos_ = false;
};

}  // namespace bob
}  // namespace node

#endif  // SRC_NODE_BOB_H_
