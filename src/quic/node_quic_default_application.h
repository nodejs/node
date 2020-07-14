#ifndef SRC_QUIC_NODE_QUIC_DEFAULT_APPLICATION_H_
#define SRC_QUIC_NODE_QUIC_DEFAULT_APPLICATION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_quic_stream.h"
#include "node_quic_session.h"
#include "node_quic_util.h"
#include "util.h"
#include "v8.h"

namespace node {

namespace quic {

// The DefaultApplication is used whenever an unknown/unrecognized
// alpn identifier is used. It switches the QUIC implementation into
// a minimal/generic mode that defers all application level processing
// to the user-code level. Headers are not supported by QuicStream
// instances created under the default application.
class DefaultApplication final : public QuicApplication {
 public:
  explicit DefaultApplication(QuicSession* session);

  bool Initialize() override;

  void StopTrackingMemory(void* ptr) override {
    // Do nothing. Not used.
  }

  bool ReceiveStreamData(
      uint32_t flags,
      int64_t stream_id,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset) override;

  int GetStreamData(StreamData* stream_data) override;

  void ResumeStream(int64_t stream_id) override;
  bool ShouldSetFin(const StreamData& stream_data) override;
  bool StreamCommit(StreamData* stream_data, size_t datalen) override;

  SET_SELF_SIZE(DefaultApplication)
  SET_MEMORY_INFO_NAME(DefaultApplication)
  SET_NO_MEMORY_INFO()

 private:
  void ScheduleStream(int64_t stream_id);
  void UnscheduleStream(int64_t stream_id);

  QuicStream::Queue stream_queue_;
};

}  // namespace quic

}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_QUIC_NODE_QUIC_DEFAULT_APPLICATION_H_
