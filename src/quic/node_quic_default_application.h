#ifndef SRC_QUIC_NODE_QUIC_DEFAULT_APPLICATION_H_
#define SRC_QUIC_NODE_QUIC_DEFAULT_APPLICATION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_quic_session.h"
#include "node_quic_util.h"
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

  bool ReceiveStreamData(
      int64_t stream_id,
      int fin,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset) override;
  void AcknowledgeStreamData(
      int64_t stream_id,
      uint64_t offset,
      size_t datalen) override;

  bool SendPendingData() override;
  bool SendStreamData(QuicStream* stream) override;

  SET_SELF_SIZE(DefaultApplication)
  SET_MEMORY_INFO_NAME(DefaultApplication)
  SET_NO_MEMORY_INFO()
};

}  // namespace quic

}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_QUIC_NODE_QUIC_DEFAULT_APPLICATION_H_
