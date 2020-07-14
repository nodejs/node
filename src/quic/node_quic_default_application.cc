#include "debug_utils-inl.h"
#include "node_quic_buffer-inl.h"
#include "node_quic_default_application.h"
#include "node_quic_session-inl.h"
#include "node_quic_socket-inl.h"
#include "node_quic_stream-inl.h"
#include "node_quic_util-inl.h"
#include "node_sockaddr-inl.h"
#include <ngtcp2/ngtcp2.h>

#include <vector>

namespace node {
namespace quic {

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

DefaultApplication::DefaultApplication(
    QuicSession* session)
    : QuicApplication(session) {}

bool DefaultApplication::Initialize() {
  if (needs_init()) {
    Debug(session(), "Default QUIC Application Initialized");
    set_init_done();
  }
  return true;
}

void DefaultApplication::ScheduleStream(int64_t stream_id) {
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  if (LIKELY(stream && !stream->is_destroyed())) {
    Debug(session(), "Scheduling stream %" PRIu64, stream_id);
    stream->Schedule(&stream_queue_);
  }
}

void DefaultApplication::UnscheduleStream(int64_t stream_id) {
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  if (LIKELY(stream)) {
    Debug(session(), "Unscheduling stream %" PRIu64, stream_id);
    stream->Unschedule();
  }
}

void DefaultApplication::ResumeStream(int64_t stream_id) {
  ScheduleStream(stream_id);
}

bool DefaultApplication::ReceiveStreamData(
    uint32_t flags,
    int64_t stream_id,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) {
  // Ensure that the QuicStream exists before deferring to
  // QuicApplication specific processing logic.
  Debug(session(), "Default QUIC Application receiving stream data");
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  if (!stream) {
    // Shutdown the stream explicitly if the session is being closed.
    if (session()->is_graceful_closing()) {
      session()->ShutdownStream(stream_id, NGTCP2_ERR_CLOSING);
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

    stream = session()->CreateStream(stream_id);
  }
  CHECK(stream);

  // If the stream ended up being destroyed immediately after
  // creation, just skip the data processing and return.
  if (UNLIKELY(stream->is_destroyed()))
    return true;

  stream->ReceiveData(flags, data, datalen, offset);
  return true;
}

int DefaultApplication::GetStreamData(StreamData* stream_data) {
  QuicStream* stream = stream_queue_.PopFront();
  // If stream is nullptr, there are no streams with data pending.
  if (stream == nullptr)
    return 0;

  stream_data->stream.reset(stream);
  stream_data->id = stream->id();

  auto next = [&](
      int status,
      const ngtcp2_vec* data,
      size_t count,
      bob::Done done) {
    switch (status) {
      case bob::Status::STATUS_BLOCK:
        // Fall through
      case bob::Status::STATUS_WAIT:
        // Fall through
      case bob::Status::STATUS_EOS:
        return;
      case bob::Status::STATUS_END:
        stream_data->fin = 1;
    }

    stream_data->count = count;

    if (count > 0) {
      stream->Schedule(&stream_queue_);
      stream_data->remaining = get_length(data, count);
    } else {
      stream_data->remaining = 0;
    }
  };

  if (LIKELY(!stream->is_eos())) {
    CHECK_GE(stream->Pull(
        std::move(next),
        bob::Options::OPTIONS_SYNC,
        stream_data->data,
        arraysize(stream_data->data),
        kMaxVectorCount), 0);
  }

  return 0;
}

bool DefaultApplication::StreamCommit(
    StreamData* stream_data,
    size_t datalen) {
  CHECK(stream_data->stream);
  stream_data->remaining -= datalen;
  Consume(&stream_data->buf, &stream_data->count, datalen);
  stream_data->stream->Commit(datalen);
  return true;
}

bool DefaultApplication::ShouldSetFin(const StreamData& stream_data) {
  if (!stream_data.stream ||
      !IsEmpty(stream_data.buf, stream_data.count))
    return false;
  return !stream_data.stream->is_writable();
}

}  // namespace quic
}  // namespace node
