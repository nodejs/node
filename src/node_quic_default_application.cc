#include "debug_utils.h"
#include "node_quic_default_application.h"
#include "node_quic_session-inl.h"
#include "node_quic_socket.h"
#include "node_quic_stream.h"
#include "node_quic_util-inl.h"
#include "node_sockaddr-inl.h"
#include <ngtcp2/ngtcp2.h>

namespace node {
namespace quic {

DefaultApplication::DefaultApplication(
    QuicSession* session) :
    QuicApplication(session) {}

bool DefaultApplication::Initialize() {
  if (!NeedsInit())
    return false;
  Debug(Session(), "Default QUIC Application Initialized");
  SetInitDone();
  return true;
}

bool DefaultApplication::ReceiveStreamData(
    int64_t stream_id,
    int fin,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) {
  // Ensure that the QuicStream exists before deferring to
  // QuicApplication specific processing logic.
  Debug(Session(), "Default QUIC Application receiving stream data");
  QuicStream* stream = Session()->FindStream(stream_id);
  if (stream == nullptr) {
    // Shutdown the stream explicitly if the session is being closed.
    if (Session()->IsGracefullyClosing()) {
      Session()->ResetStream(stream_id, NGTCP2_ERR_CLOSING);
      return true;
    }

    // One potential DOS attack vector is to send a bunch of
    // empty stream frames to commit resources. Check that
    // here. Essentially, we only want to create a new stream
    // if the datalen is greater than 0, otherwise, we ignore
    // the packet. ngtcp2 should be handling this for us,
    // but we handle it just to be safe.
    if (UNLIKELY(datalen == 0))
      return true;

    stream = Session()->CreateStream(stream_id);
  }
  CHECK_NOT_NULL(stream);

  stream->ReceiveData(fin, data, datalen, offset);
  return true;
}

void DefaultApplication::AcknowledgeStreamData(
    int64_t stream_id,
    uint64_t offset,
    size_t datalen) {
  QuicStream* stream = Session()->FindStream(stream_id);
  Debug(Session(), "Default QUIC Application acknowledging stream data");
  // It's possible that the stream has already been destroyed and
  // removed. If so, just silently ignore the ack
  if (stream != nullptr)
    stream->AckedDataOffset(offset, datalen);
}

bool DefaultApplication::SendPendingData() {
  // TODO(@jasnell): Right now this iterates through the streams
  // in the order they were created. Later, we'll want to implement
  // a prioritization scheme to allow higher priority streams to
  // be serialized first.
  Debug(Session(), "Default QUIC Application sending pending data");
  for (const auto& stream : *(Session()->GetStreams())) {
    if (!SendStreamData(stream.second.get()))
      return false;

    // Check to make sure QuicSession state did not change in this
    // iteration
    if (Session()->IsInDrainingPeriod() ||
        Session()->IsInClosingPeriod() ||
        Session()->IsDestroyed()) {
      break;
    }
  }

  return true;
}

namespace {
void Consume(ngtcp2_vec** pvec, size_t* pcnt, size_t len) {
  ngtcp2_vec* v = *pvec;
  size_t cnt = *pcnt;

  for (; cnt > 0; --cnt, ++v) {
    if (v->len > len) {
      v->len -= len;
      v->base += len;
      break;
    }
    len -= v->len;
  }

  *pvec = v;
  *pcnt = cnt;
}

int IsEmpty(const ngtcp2_vec* vec, size_t cnt) {
  size_t i;
  for (i = 0; i < cnt && vec[i].len == 0; ++i) {}
  return i == cnt;
}
}  // anonymous namespace

bool DefaultApplication::SendStreamData(QuicStream* stream) {
  ssize_t ndatalen = 0;
  QuicPathStorage path;
  Debug(Session(), "Default QUIC Application sending stream %" PRId64 " data",
        stream->GetID());

  std::vector<ngtcp2_vec> vec;

  // remaining is the total number of bytes stored in the vector
  // that are remaining to be serialized.
  size_t remaining = stream->DrainInto(&vec);
  Debug(stream, "Sending %d bytes of stream data. Still writable? %s",
        remaining,
        stream->IsWritable() ? "yes" : "no");

  // c and v are used to track the current serialization position
  // for each iteration of the for(;;) loop below.
  size_t c = vec.size();
  ngtcp2_vec* v = vec.data();

  // If there is no stream data and we're not sending fin,
  // Just return without doing anything.
  if (c == 0 && stream->IsWritable()) {
    Debug(stream, "There is no stream data to send");
    return true;
  }

  for (;;) {
    Debug(stream, "Starting packet serialization. Remaining? %d", remaining);
    auto packet = CreateStreamDataPacket();
    ssize_t nwrite =
        ngtcp2_conn_writev_stream(
            Session()->Connection(),
            &path.path,
            packet->data(),
            Session()->GetMaxPacketLength(),
            &ndatalen,
            NGTCP2_WRITE_STREAM_FLAG_NONE,
            stream->GetID(),
            stream->IsWritable() ? 0 : 1,
            reinterpret_cast<const ngtcp2_vec*>(v),
            c,
            uv_hrtime());

    if (nwrite <= 0) {
      switch (nwrite) {
        case 0:
          // If zero is returned, we've hit congestion limits. We need to stop
          // serializing data and try again later to empty the queue once the
          // congestion window has expanded.
          Debug(stream, "Congestion limit reached");
          return true;
        case NGTCP2_ERR_PKT_NUM_EXHAUSTED:
          // There is a finite number of packets that can be sent
          // per connection. Once those are exhausted, there's
          // absolutely nothing we can do except immediately
          // and silently tear down the QuicSession. This has
          // to be silent because we can't even send a
          // CONNECTION_CLOSE since even those require a
          // packet number.
          Session()->SilentClose();
          return false;
        case NGTCP2_ERR_STREAM_DATA_BLOCKED:
          Debug(stream, "Stream data blocked");
          Session()->StreamDataBlocked(stream->GetID());
          return true;
        case NGTCP2_ERR_STREAM_SHUT_WR:
          Debug(stream, "Stream writable side is closed");
          return true;
        case NGTCP2_ERR_STREAM_NOT_FOUND:
          Debug(stream, "Stream does not exist");
          return true;
        default:
          Debug(stream, "Error writing packet. Code %" PRIu64, nwrite);
          Session()->SetLastError(
              QUIC_ERROR_SESSION,
              static_cast<int>(nwrite));
          return false;
      }
    }

    if (ndatalen > 0) {
      remaining -= ndatalen;
      Debug(stream,
            "%" PRIu64 " stream bytes serialized into packet. %d remaining",
            ndatalen,
            remaining);
      Consume(&v, &c, ndatalen);
      stream->Commit(ndatalen);
    }

    Debug(stream, "Sending %" PRIu64 " bytes in serialized packet", nwrite);
    packet->SetLength(nwrite);
    if (!Session()->SendPacket(std::move(packet), path))
      return false;

    if (IsEmpty(v, c)) {
      // fin will have been set if all of the data has been
      // encoded in the packet and IsWritable() returns false.
      if (!stream->IsWritable()) {
        Debug(stream, "Final stream has been sent");
        stream->SetFinSent();
      }
      break;
    }
  }

  return true;
}

}  // namespace quic
}  // namespace node
