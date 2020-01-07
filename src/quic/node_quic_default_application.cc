#include "debug_utils.h"
#include "node_quic_buffer-inl.h"
#include "node_quic_default_application.h"
#include "node_quic_session-inl.h"
#include "node_quic_socket.h"
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
    QuicSession* session) :
    QuicApplication(session) {}

bool DefaultApplication::Initialize() {
  if (needs_init()) {
    Debug(session(), "Default QUIC Application Initialized");
    set_init_done();
  }
  return needs_init();
}

void DefaultApplication::ScheduleStream(int64_t stream_id) {
  QuicStream* stream = session()->FindStream(stream_id);
  Debug(session(), "Scheduling stream %" PRIu64, stream_id);
  if (stream != nullptr)
    stream->Schedule(&stream_queue_);
}

void DefaultApplication::UnscheduleStream(int64_t stream_id) {
  QuicStream* stream = session()->FindStream(stream_id);
  Debug(session(), "Unscheduling stream %" PRIu64, stream_id);
  if (stream != nullptr)
    stream->Unschedule();
}

void DefaultApplication::StreamClose(
    int64_t stream_id,
    uint64_t app_error_code) {
  if (app_error_code == 0)
    app_error_code = NGTCP2_APP_NOERROR;
  UnscheduleStream(stream_id);
  QuicApplication::StreamClose(stream_id, app_error_code);
}

void DefaultApplication::ResumeStream(int64_t stream_id) {
  Debug(session(), "Stream %" PRId64 " has data to send");
  ScheduleStream(stream_id);
}

bool DefaultApplication::ReceiveStreamData(
    int64_t stream_id,
    int fin,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) {
  // Ensure that the QuicStream exists before deferring to
  // QuicApplication specific processing logic.
  Debug(session(), "Default QUIC Application receiving stream data");
  QuicStream* stream = session()->FindStream(stream_id);
  if (stream == nullptr) {
    // Shutdown the stream explicitly if the session is being closed.
    if (session()->is_gracefully_closing()) {
      session()->ResetStream(stream_id, NGTCP2_ERR_CLOSING);
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
  CHECK_NOT_NULL(stream);

  stream->ReceiveData(fin, data, datalen, offset);
  return true;
}

int DefaultApplication::GetStreamData(StreamData* stream_data) {
  QuicStream* stream = stream_queue_.PopFront();
  // If stream is nullptr, there are no streams with data pending.
  if (stream == nullptr)
    return 0;

  stream_data->remaining =
      stream->DrainInto(
          &stream_data->data,
          &stream_data->count,
          MAX_VECTOR_COUNT);

  stream_data->stream.reset(stream);
  stream_data->id = stream->id();
  stream_data->fin = stream->is_writable() ? 0 : 1;

  // Schedule the stream again only if there is data to write. There
  // might not actually be any more data to write but we can't know
  // that yet as it depends entirely on how much data actually gets
  // serialized by ngtcp2.
  if (stream_data->count > 0)
    stream->Schedule(&stream_queue_);

  Debug(session(), "Selected %" PRId64 " buffers for stream %" PRId64 "%s",
        stream_data->count,
        stream_data->id,
        stream_data->fin == 1 ? " (fin)" : "");
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
