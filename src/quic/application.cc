#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC

#include "application.h"
#include <async_wrap-inl.h>
#include <debug_utils-inl.h>
#include <node_bob.h>
#include <node_sockaddr-inl.h>
#include <uv.h>
#include <v8.h>
#include "defs.h"
#include "endpoint.h"
#include "http3.h"
#include "packet.h"
#include "session.h"

namespace node {

using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Object;
using v8::Value;

namespace quic {

// ============================================================================
// Session::Application_Options
const Session::Application_Options Session::Application_Options::kDefault = {};

Session::Application_Options::operator const nghttp3_settings() const {
  // In theory, Application_Options might contain options for more than just
  // HTTP/3. Here we extract only the properties that are relevant to HTTP/3.
  return nghttp3_settings{
      max_field_section_size,
      static_cast<size_t>(qpack_max_dtable_capacity),
      static_cast<size_t>(qpack_encoder_max_dtable_capacity),
      static_cast<size_t>(qpack_blocked_streams),
      enable_connect_protocol,
      enable_datagrams,
  };
}

std::string Session::Application_Options::ToString() const {
  DebugIndentScope indent;
  auto prefix = indent.Prefix();
  std::string res("{");
  res += prefix + "max header pairs: " + std::to_string(max_header_pairs);
  res += prefix + "max header length: " + std::to_string(max_header_length);
  res += prefix +
         "max field section size: " + std::to_string(max_field_section_size);
  res += prefix + "qpack max dtable capacity: " +
         std::to_string(qpack_max_dtable_capacity);
  res += prefix + "qpack encoder max dtable capacity: " +
         std::to_string(qpack_encoder_max_dtable_capacity);
  res += prefix +
         "qpack blocked streams: " + std::to_string(qpack_blocked_streams);
  res += prefix + "enable connect protocol: " +
         (enable_connect_protocol ? std::string("yes") : std::string("no"));
  res += prefix + "enable datagrams: " +
         (enable_datagrams ? std::string("yes") : std::string("no"));
  res += indent.Close();
  return res;
}

Maybe<Session::Application_Options> Session::Application_Options::From(
    Environment* env, Local<Value> value) {
  if (value.IsEmpty() || (!value->IsUndefined() && !value->IsObject())) {
    THROW_ERR_INVALID_ARG_TYPE(env, "options must be an object");
    return Nothing<Application_Options>();
  }

  Application_Options options;
  auto& state = BindingData::Get(env);
  if (value->IsUndefined()) {
    return Just<Application_Options>(options);
  }

  auto params = value.As<Object>();

#define SET(name)                                                              \
  SetOption<Session::Application_Options,                                      \
            &Session::Application_Options::name>(                              \
      env, &options, params, state.name##_string())

  if (!SET(max_header_pairs) || !SET(max_header_length) ||
      !SET(max_field_section_size) || !SET(qpack_max_dtable_capacity) ||
      !SET(qpack_encoder_max_dtable_capacity) || !SET(qpack_blocked_streams) ||
      !SET(enable_connect_protocol) || !SET(enable_datagrams)) {
    return Nothing<Application_Options>();
  }

#undef SET

  return Just<Application_Options>(options);
}

Session::Application::Application(Session* session, const Options& options)
    : session_(session) {}

bool Session::Application::Start() {
  // By default there is nothing to do. Specific implementations may
  // override to perform more actions.
  Debug(session_, "Session application started");
  return true;
}

void Session::Application::AcknowledgeStreamData(Stream* stream,
                                                 size_t datalen) {
  Debug(session_,
        "Application acknowledging stream %" PRIi64 " data: %zu",
        stream->id(),
        datalen);
  DCHECK_NOT_NULL(stream);
  stream->Acknowledge(datalen);
}

void Session::Application::BlockStream(int64_t id) {
  Debug(session_, "Application blocking stream %" PRIi64, id);
  auto stream = session().FindStream(id);
  if (stream) stream->EmitBlocked();
}

bool Session::Application::CanAddHeader(size_t current_count,
                                        size_t current_headers_length,
                                        size_t this_header_length) {
  // By default headers are not supported.
  Debug(session_, "Application cannot add header");
  return false;
}

bool Session::Application::SendHeaders(const Stream& stream,
                                       HeadersKind kind,
                                       const v8::Local<v8::Array>& headers,
                                       HeadersFlags flags) {
  // By default do nothing.
  Debug(session_, "Application cannot send headers");
  return false;
}

void Session::Application::ResumeStream(int64_t id) {
  Debug(session_, "Application resuming stream %" PRIi64, id);
  // By default do nothing.
}

void Session::Application::ExtendMaxStreams(EndpointLabel label,
                                            Direction direction,
                                            uint64_t max_streams) {
  Debug(session_, "Application extending max streams");
  // By default do nothing.
}

void Session::Application::ExtendMaxStreamData(Stream* stream,
                                               uint64_t max_data) {
  Debug(session_, "Application extending max stream data");
  // By default do nothing.
}

void Session::Application::CollectSessionTicketAppData(
    SessionTicket::AppData* app_data) const {
  Debug(session_, "Application collecting session ticket app data");
  // By default do nothing.
}

SessionTicket::AppData::Status
Session::Application::ExtractSessionTicketAppData(
    const SessionTicket::AppData& app_data,
    SessionTicket::AppData::Source::Flag flag) {
  Debug(session_, "Application extracting session ticket app data");
  // By default we do not have any application data to retrieve.
  return flag == SessionTicket::AppData::Source::Flag::STATUS_RENEW
             ? SessionTicket::AppData::Status::TICKET_USE_RENEW
             : SessionTicket::AppData::Status::TICKET_USE;
}

void Session::Application::SetStreamPriority(const Stream& stream,
                                             StreamPriority priority,
                                             StreamPriorityFlags flags) {
  Debug(
      session_, "Application setting stream %" PRIi64 " priority", stream.id());
  // By default do nothing.
}

StreamPriority Session::Application::GetStreamPriority(const Stream& stream) {
  return StreamPriority::DEFAULT;
}

Packet* Session::Application::CreateStreamDataPacket() {
  return Packet::Create(env(),
                        session_->endpoint_.get(),
                        session_->remote_address_,
                        ngtcp2_conn_get_max_tx_udp_payload_size(*session_),
                        "stream data");
}

void Session::Application::StreamClose(Stream* stream, QuicError error) {
  Debug(session_,
        "Application closing stream %" PRIi64 " with error %s",
        stream->id(),
        error);
  stream->Destroy(error);
}

void Session::Application::StreamStopSending(Stream* stream, QuicError error) {
  Debug(session_,
        "Application stopping sending on stream %" PRIi64 " with error %s",
        stream->id(),
        error);
  DCHECK_NOT_NULL(stream);
  stream->ReceiveStopSending(error);
}

void Session::Application::StreamReset(Stream* stream,
                                       uint64_t final_size,
                                       QuicError error) {
  Debug(session_,
        "Application resetting stream %" PRIi64 " with error %s",
        stream->id(),
        error);
  stream->ReceiveStreamReset(final_size, error);
}

void Session::Application::SendPendingData() {
  Debug(session_, "Application sending pending data");
  PathStorage path;

  Packet* packet = nullptr;
  uint8_t* pos = nullptr;
  int err = 0;

  size_t maxPacketCount = std::min(static_cast<size_t>(64000),
                                   ngtcp2_conn_get_send_quantum(*session_));
  size_t packetSendCount = 0;

  const auto updateTimer = [&] {
    Debug(session_, "Application updating the session timer");
    ngtcp2_conn_update_pkt_tx_time(*session_, uv_hrtime());
    session_->UpdateTimer();
  };

  const auto congestionLimited = [&](auto packet) {
    auto len = pos - ngtcp2_vec(*packet).base;
    // We are either congestion limited or done.
    if (len) {
      // Some data was serialized into the packet. We need to send it.
      packet->Truncate(len);
      session_->Send(std::move(packet), path);
    }

    updateTimer();
  };

  for (;;) {
    ssize_t ndatalen;
    StreamData stream_data;

    err = GetStreamData(&stream_data);

    if (err < 0) {
      session_->last_error_ = QuicError::ForNgtcp2Error(NGTCP2_ERR_INTERNAL);
      return session_->Close(Session::CloseMethod::SILENT);
    }

    if (packet == nullptr) {
      packet = CreateStreamDataPacket();
      if (packet == nullptr) {
        session_->last_error_ = QuicError::ForNgtcp2Error(NGTCP2_ERR_INTERNAL);
        return session_->Close(Session::CloseMethod::SILENT);
      }
      pos = ngtcp2_vec(*packet).base;
    }

    ssize_t nwrite = WriteVStream(&path, pos, &ndatalen, stream_data);

    if (nwrite <= 0) {
      switch (nwrite) {
        case 0:
          if (stream_data.id >= 0) ResumeStream(stream_data.id);
          return congestionLimited(std::move(packet));
        case NGTCP2_ERR_STREAM_DATA_BLOCKED: {
          session().StreamDataBlocked(stream_data.id);
          if (session().max_data_left() == 0) {
            if (stream_data.id >= 0) ResumeStream(stream_data.id);
            return congestionLimited(std::move(packet));
          }
          CHECK_LE(ndatalen, 0);
          continue;
        }
        case NGTCP2_ERR_STREAM_SHUT_WR: {
          // Indicates that the writable side of the stream has been closed
          // locally or the stream is being reset. In either case, we can't send
          // any stream data!
          CHECK_GE(stream_data.id, 0);
          // We need to notify the stream that the writable side has been closed
          // and no more outbound data can be sent.
          CHECK_LE(ndatalen, 0);
          auto stream = session_->FindStream(stream_data.id);
          if (stream) stream->EndWritable();
          continue;
        }
        case NGTCP2_ERR_WRITE_MORE: {
          CHECK_GT(ndatalen, 0);
          if (!StreamCommit(&stream_data, ndatalen)) return session_->Close();
          pos += ndatalen;
          continue;
        }
      }

      packet->Done(UV_ECANCELED);
      session_->last_error_ = QuicError::ForNgtcp2Error(nwrite);
      return session_->Close(Session::CloseMethod::SILENT);
    }

    pos += nwrite;
    if (ndatalen > 0 && !StreamCommit(&stream_data, ndatalen)) {
      // Since we are closing the session here, we don't worry about updating
      // the pkt tx time. The failed StreamCommit should have updated the
      // last_error_ appropriately.
      packet->Done(UV_ECANCELED);
      return session_->Close(Session::CloseMethod::SILENT);
    }

    if (stream_data.id >= 0 && ndatalen < 0) ResumeStream(stream_data.id);

    packet->Truncate(nwrite);
    session_->Send(std::move(packet), path);

    pos = nullptr;

    if (++packetSendCount == maxPacketCount) {
      break;
    }
  }

  updateTimer();
}

ssize_t Session::Application::WriteVStream(PathStorage* path,
                                           uint8_t* buf,
                                           ssize_t* ndatalen,
                                           const StreamData& stream_data) {
  CHECK_LE(stream_data.count, kMaxVectorCount);
  uint32_t flags = NGTCP2_WRITE_STREAM_FLAG_NONE;
  if (stream_data.remaining > 0) flags |= NGTCP2_WRITE_STREAM_FLAG_MORE;
  if (stream_data.fin) flags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
  ssize_t ret = ngtcp2_conn_writev_stream(
      *session_,
      &path->path,
      nullptr,
      buf,
      ngtcp2_conn_get_max_tx_udp_payload_size(*session_),
      ndatalen,
      flags,
      stream_data.id,
      stream_data.buf,
      stream_data.count,
      uv_hrtime());
  return ret;
}

// The DefaultApplication is the default implementation of Session::Application
// that is used for all unrecognized ALPN identifiers.
class DefaultApplication final : public Session::Application {
 public:
  // Marked NOLINT because the cpp linter gets confused about this using
  // statement not being sorted with the using v8 statements at the top
  // of the namespace.
  using Application::Application;  // NOLINT

  bool ReceiveStreamData(Stream* stream,
                         const uint8_t* data,
                         size_t datalen,
                         Stream::ReceiveDataFlags flags) override {
    Debug(&session(), "Default application receiving stream data");
    DCHECK_NOT_NULL(stream);
    if (!stream->is_destroyed()) stream->ReceiveData(data, datalen, flags);
    return true;
  }

  int GetStreamData(StreamData* stream_data) override {
    Debug(&session(), "Default application getting stream data");
    DCHECK_NOT_NULL(stream_data);
    // If the queue is empty, there aren't any streams with data yet
    if (stream_queue_.IsEmpty()) return 0;

    const auto get_length = [](auto vec, size_t count) {
      CHECK_NOT_NULL(vec);
      size_t len = 0;
      for (size_t n = 0; n < count; n++) len += vec[n].len;
      return len;
    };

    Stream* stream = stream_queue_.PopFront();
    CHECK_NOT_NULL(stream);
    stream_data->stream.reset(stream);
    stream_data->id = stream->id();
    auto next =
        [&](int status, const ngtcp2_vec* data, size_t count, bob::Done done) {
          switch (status) {
            case bob::Status::STATUS_BLOCK:
              // Fall through
            case bob::Status::STATUS_WAIT:
              return;
            case bob::Status::STATUS_EOS:
              stream_data->fin = 1;
          }

          stream_data->count = count;

          if (count > 0) {
            stream->Schedule(&stream_queue_);
            stream_data->remaining = get_length(data, count);
          } else {
            stream_data->remaining = 0;
          }

          // Not calling done here because we defer committing
          // the data until after we're sure it's written.
        };

    if (LIKELY(!stream->is_eos())) {
      int ret = stream->Pull(std::move(next),
                             bob::Options::OPTIONS_SYNC,
                             stream_data->data,
                             arraysize(stream_data->data),
                             kMaxVectorCount);
      if (ret == bob::Status::STATUS_EOS) {
        stream_data->fin = 1;
      }
    } else {
      stream_data->fin = 1;
    }

    return 0;
  }

  void ResumeStream(int64_t id) override {
    Debug(&session(), "Default application resuming stream %" PRIi64, id);
    ScheduleStream(id);
  }

  bool ShouldSetFin(const StreamData& stream_data) override {
    auto const is_empty = [](auto vec, size_t cnt) {
      size_t i;
      for (i = 0; i < cnt && vec[i].len == 0; ++i) {
      }
      return i == cnt;
    };

    return stream_data.stream && is_empty(stream_data.buf, stream_data.count);
  }

  bool StreamCommit(StreamData* stream_data, size_t datalen) override {
    Debug(&session(), "Default application committing stream data");
    DCHECK_NOT_NULL(stream_data);
    const auto consume = [](ngtcp2_vec** pvec, size_t* pcnt, size_t len) {
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
    };

    CHECK(stream_data->stream);
    stream_data->remaining -= datalen;
    consume(&stream_data->buf, &stream_data->count, datalen);
    stream_data->stream->Commit(datalen);
    return true;
  }

  SET_SELF_SIZE(DefaultApplication)
  SET_MEMORY_INFO_NAME(DefaultApplication)
  SET_NO_MEMORY_INFO()

 private:
  void ScheduleStream(int64_t id) {
    Debug(&session(), "Default application scheduling stream %" PRIi64, id);
    auto stream = session().FindStream(id);
    if (stream && !stream->is_destroyed()) {
      stream->Schedule(&stream_queue_);
    }
  }

  void UnscheduleStream(int64_t id) {
    Debug(&session(), "Default application unscheduling stream %" PRIi64, id);
    auto stream = session().FindStream(id);
    if (stream && !stream->is_destroyed()) stream->Unschedule();
  }

  Stream::Queue stream_queue_;
};

std::unique_ptr<Session::Application> Session::select_application() {
  // In the future, we may end up supporting additional QUIC protocols. As they
  // are added, extend the cases here to create and return them.

  if (config_.options.tls_options.alpn == NGHTTP3_ALPN_H3) {
    Debug(this, "Selecting HTTP/3 application");
    return createHttp3Application(this, config_.options.application_options);
  }

  Debug(this, "Selecting default application");
  return std::make_unique<DefaultApplication>(
      this, config_.options.application_options);
}

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
