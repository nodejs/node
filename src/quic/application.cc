#include "util.h"
#if HAVE_OPENSSL && HAVE_QUIC
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <async_wrap-inl.h>
#include <debug_utils-inl.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <node_bob.h>
#include <node_sockaddr-inl.h>
#include <uv.h>
#include <v8.h>
#include "application.h"
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
  // In theory, Application::Options might contain options for more than just
  // HTTP/3. Here we extract only the properties that are relevant to HTTP/3.
  // Later if we add more application types we can add more properties or
  // divide this up into multiple option structs.
  return nghttp3_settings{
      .max_field_section_size = max_field_section_size,
      .qpack_max_dtable_capacity =
          static_cast<size_t>(qpack_max_dtable_capacity),
      .qpack_encoder_max_dtable_capacity =
          static_cast<size_t>(qpack_encoder_max_dtable_capacity),
      .qpack_blocked_streams = static_cast<size_t>(qpack_blocked_streams),
      .enable_connect_protocol = enable_connect_protocol,
      .h3_datagram = enable_datagrams,
      // origin_list is nullptr here because it is set directly on the
      // nghttp3_settings in Http3ApplicationImpl::InitializeConnection()
      // from the SNI configuration.
      .origin_list = nullptr,
      .glitch_ratelim_burst = 1000,
      .glitch_ratelim_rate = 33,
      .qpack_indexing_strat = NGHTTP3_QPACK_INDEXING_STRAT_EAGER,
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
  if (value.IsEmpty()) [[unlikely]] {
    THROW_ERR_INVALID_ARG_TYPE(env, "options must be an object");
    return Nothing<Application_Options>();
  }

  Application_Options options;
  auto& state = BindingData::Get(env);

#define SET(name)                                                              \
  SetOption<Session::Application_Options,                                      \
            &Session::Application_Options::name>(                              \
      env, &options, params, state.name##_string())

  if (!value->IsUndefined()) {
    if (!value->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(env, "options must be an object");
      return Nothing<Application_Options>();
    }
    auto params = value.As<Object>();
    if (!SET(max_header_pairs) || !SET(max_header_length) ||
        !SET(max_field_section_size) || !SET(qpack_max_dtable_capacity) ||
        !SET(qpack_encoder_max_dtable_capacity) ||
        !SET(qpack_blocked_streams) || !SET(enable_connect_protocol) ||
        !SET(enable_datagrams)) {
      // The call to SetOption should have scheduled an exception to be thrown.
      return Nothing<Application_Options>();
    }
  }

#undef SET

  return Just<Application_Options>(options);
}

// ============================================================================

std::string Session::Application::StreamData::ToString() const {
  DebugIndentScope indent;

  size_t total_bytes = 0;
  for (size_t n = 0; n < count; n++) {
    total_bytes += data[n].len;
  }

  auto prefix = indent.Prefix();
  std::string res("{");
  res += prefix + "count: " + std::to_string(count);
  res += prefix + "id: " + std::to_string(id);
  res += prefix + "fin: " + std::to_string(fin);
  res += prefix + "total: " + std::to_string(total_bytes);
  res += indent.Close();
  return res;
}

Session::Application::Application(Session* session, const Options& options)
    : session_(session) {}

bool Session::Application::Start() {
  // By default there is nothing to do. Specific implementations may
  // override to perform more actions.
  Debug(session_, "Session application started");
  return true;
}

bool Session::Application::AcknowledgeStreamData(stream_id id, size_t datalen) {
  if (auto stream = session().FindStream(id)) [[likely]] {
    stream->Acknowledge(datalen);
  }
  // Returning true even when the stream is not found is intentional.
  // After a stream is destroyed, the peer can still ACK data that was
  // previously sent. This is benign and should not be treated as an error.
  return true;
}

void Session::Application::CollectSessionTicketAppData(
    SessionTicket::AppData* app_data) const {
  // By default, write just the application type byte.
  uint8_t buf[1] = {static_cast<uint8_t>(type())};
  app_data->Set(uv_buf_init(reinterpret_cast<char*>(buf), 1));
}

SessionTicket::AppData::Status
Session::Application::ExtractSessionTicketAppData(
    const SessionTicket::AppData& app_data, Flag flag) {
  // By default we do not have any application data to retrieve.
  return flag == Flag::STATUS_RENEW
             ? SessionTicket::AppData::Status::TICKET_USE_RENEW
             : SessionTicket::AppData::Status::TICKET_USE;
}

std::optional<PendingTicketAppData> Session::Application::ParseTicketData(
    const uv_buf_t& data) {
  if (data.len == 0 || data.base == nullptr) return std::nullopt;
  auto app_type =
      static_cast<Type>(reinterpret_cast<const uint8_t*>(data.base)[0]);
  switch (app_type) {
    case Type::DEFAULT:
      return DefaultTicketData{};
    case Type::HTTP3:
      return ParseHttp3TicketData(data);
    default:
      return std::nullopt;
  }
}

bool Session::Application::ValidateTicketData(
    const PendingTicketAppData& data, const Application_Options& options) {
  if (std::holds_alternative<Http3TicketData>(data)) {
    // TODO(@jasnell): This validation probably belongs in http3.cc but keeping
    // it here for now.
    const auto& ticket = std::get<Http3TicketData>(data);
    return options.max_field_section_size >= ticket.max_field_section_size &&
           options.qpack_max_dtable_capacity >=
               ticket.qpack_max_dtable_capacity &&
           options.qpack_encoder_max_dtable_capacity >=
               ticket.qpack_encoder_max_dtable_capacity &&
           options.qpack_blocked_streams >= ticket.qpack_blocked_streams &&
           (!ticket.enable_connect_protocol ||
            options.enable_connect_protocol) &&
           (!ticket.enable_datagrams || options.enable_datagrams);
  }
  // DefaultTicketData always validates.
  return true;
}

Packet::Ptr Session::Application::CreateStreamDataPacket() {
  return session_->endpoint().CreatePacket(
      session_->remote_address(), session_->max_packet_size(), "stream data");
}

void Session::Application::ReceiveStreamClose(Stream* stream,
                                              QuicError&& error) {
  DCHECK_NOT_NULL(stream);
  stream->Destroy(std::move(error));
}

void Session::Application::ReceiveStreamStopSending(Stream* stream,
                                                    QuicError&& error) {
  DCHECK_NOT_NULL(stream);
  stream->ReceiveStopSending(std::move(error));
}

void Session::Application::ReceiveStreamReset(Stream* stream,
                                              uint64_t final_size,
                                              QuicError&& error) {
  stream->ReceiveStreamReset(final_size, std::move(error));
}

// Attempts to pack a pending datagram into the current packet.
// Returns the nwrite value from ngtcp2_conn_writev_datagram.
// On fatal error, closes the session and returns the error code.
// The caller should check:
//   > 0:  packet is complete, send it (pos was NOT advanced — caller
//         must add nwrite to pos and send)
//   NGTCP2_ERR_WRITE_MORE: datagram packed, room for more
//   0:    congestion controlled or doesn't fit, datagram stays in queue
//   < 0 (other): fatal error, session already closed
ssize_t Session::Application::TryWritePendingDatagram(PathStorage* path,
                                                      uint8_t* dest,
                                                      size_t destlen) {
  CHECK(session_->HasPendingDatagrams());
  auto max_attempts = session_->config().options.max_datagram_send_attempts;

  // Skip datagrams that have already exceeded the send attempt limit
  // from a previous SendPendingData cycle.
  while (session_->HasPendingDatagrams()) {
    auto& front = session_->PeekPendingDatagram();
    if (front.send_attempts < max_attempts) break;
    Debug(session_,
          "Datagram %" PRIu64 " abandoned after %u attempts",
          front.id,
          front.send_attempts);
    session_->DatagramStatus(front.id, DatagramStatus::ABANDONED);
    session_->PopPendingDatagram();
  }

  if (!session_->HasPendingDatagrams()) return 0;
  auto& dg = session_->PeekPendingDatagram();
  ngtcp2_vec dgvec = dg.data;
  int accepted = 0;
  int dg_flags = NGTCP2_WRITE_DATAGRAM_FLAG_MORE;

  ssize_t dg_nwrite = ngtcp2_conn_writev_datagram(*session_,
                                                  &path->path,
                                                  nullptr,
                                                  dest,
                                                  destlen,
                                                  &accepted,
                                                  dg_flags,
                                                  dg.id,
                                                  &dgvec,
                                                  1,
                                                  uv_hrtime());

  if (accepted) {
    // Nice, the datagram was accepted!
    Debug(session_, "Datagram %" PRIu64 " accepted into packet", dg.id);
    session_->DatagramSent(dg.id);
    session_->PopPendingDatagram();
  } else {
    Debug(session_, "Datagram %" PRIu64 " not accepted into packet", dg.id);
  }

  switch (dg_nwrite) {
    case 0: {
      // If dg_nwrite is 0, we are either congestion controlled or
      // there wasn't enough room in the packet for the datagram or
      // we aren't in a state where we can send.
      // We'll skip this attempt and return 0.
      CHECK(!accepted);
      dg.send_attempts++;
      return 0;
    }
    case NGTCP2_ERR_WRITE_MORE: {
      // There's still room left in the packet!
      return NGTCP2_ERR_WRITE_MORE;
    }
    case NGTCP2_ERR_INVALID_STATE:
    case NGTCP2_ERR_INVALID_ARGUMENT: {
      // Non-fatal error cases. Peer either does not support datagrams
      // or the datagram is too large for the peer's max.
      // Abandon the datagram and signal skip by returning std::nullopt.
      session_->DatagramStatus(dg.id, DatagramStatus::ABANDONED);
      session_->PopPendingDatagram();
      return 0;
    }
    default: {
      // Fatal errors: PKT_NUM_EXHAUSTED, CALLBACK_FAILURE, NOMEM, etc.
      Debug(session_, "Fatal datagram error: %zd", dg_nwrite);
      session_->SetLastError(QuicError::ForNgtcp2Error(dg_nwrite));
      session_->Close(CloseMethod::SILENT);
      return dg_nwrite;
    }
  }
  UNREACHABLE();
}

// the SendPendingData method is the primary driver for sending data from the
// application layer. It loops through available stream data and pending
// datagrams and generates packets to send until there is either no more
// data to send or we hit the maximum number of packets to send in one go.
// This method is extremely delicate. A bug in this method can break the
// entire QUIC implementation; so be very careful when making changes here
// and make sure to test thoroughly. When in doubt... don't change it.
void Session::Application::SendPendingData() {
  DCHECK(!session().is_destroyed());
  if (!session().can_send_packets()) [[unlikely]] {
    return;
  }
  static constexpr size_t kMaxPackets = 32;
  Debug(session_, "Application sending pending data");
  PathStorage path;
  StreamData stream_data;

  bool closed = false;
  auto update_stats = OnScopeLeave([&] {
    if (closed) return;
    auto& s = session();
    if (!s.is_destroyed()) [[likely]] {
      s.UpdatePacketTxTime();
      s.UpdateTimer();
      s.UpdateDataStats();
    }
  });

  // The maximum size of packet to create.
  const size_t max_packet_size = session_->max_packet_size();

  // The maximum number of packets to send in this call to SendPendingData.
  const size_t max_packet_count = std::min(
      kMaxPackets, ngtcp2_conn_get_send_quantum(*session_) / max_packet_size);
  if (max_packet_count == 0) return;

  // The number of packets that have been sent in this call to SendPendingData.
  size_t packet_send_count = 0;

  Packet::Ptr packet;

  auto ensure_packet = [&] {
    if (!packet) {
      packet = CreateStreamDataPacket();
      if (!packet) [[unlikely]]
        return false;
    }
    DCHECK(packet);
    return true;
  };

  // We're going to enter a loop here to prepare and send no more than
  // max_packet_count packets.
  for (;;) {
    // ndatalen is the amount of stream data that was accepted into the packet.
    ssize_t ndatalen = 0;

    // Make sure we have a packet to write data into.
    if (!ensure_packet()) [[unlikely]] {
      Debug(session_, "Failed to create packet for stream data");
      // Doh! Could not create a packet. Time to bail.
      session_->SetLastError(QuicError::ForNgtcp2Error(NGTCP2_ERR_INTERNAL));
      closed = true;
      return session_->Close(CloseMethod::SILENT);
    }

    // The stream_data is the next block of data from the application stream.
    if (GetStreamData(&stream_data) < 0) {
      Debug(session_, "Application failed to get stream data");
      session_->SetLastError(QuicError::ForNgtcp2Error(NGTCP2_ERR_INTERNAL));
      closed = true;
      return session_->Close(CloseMethod::SILENT);
    }

    // If we got here, we were at least successful in checking for stream data.
    // There might not be any stream data to send. If there is no stream data,
    // that's perfectly fine, we still need to serialize any frames we do have
    // (pings, acks, datagrams, etc) so we'll just keep going.
    if (stream_data.id >= 0) {
      Debug(session_, "Application using stream data: %s", stream_data);
    } else {
      Debug(session_, "No stream data to send");
    }
    if (session_->HasPendingDatagrams()) {
      Debug(session_, "There are pending datagrams to send");
    }

    // Awesome, let's write our packet!
    ssize_t nwrite = WriteVStream(
        &path, packet->data(), &ndatalen, packet->length(), stream_data);

    // When ndatalen is > 0, that's our indication that stream data was accepted
    // in to the packet. Yay!
    if (ndatalen > 0) {
      Debug(session_,
            "Application accepted %zu bytes from stream %" PRIi64
            " into packet",
            ndatalen,
            stream_data.id);
      if (!StreamCommit(&stream_data, ndatalen)) {
        // Data was accepted into the packet, but for some reason adjusting
        // the stream's committed data failed. Treat as fatal.
        Debug(session_, "Failed to commit accepted bytes in stream");
        session_->SetLastError(QuicError::ForNgtcp2Error(NGTCP2_ERR_INTERNAL));
        closed = true;
        return session_->Close(CloseMethod::SILENT);
      }
    } else if (stream_data.id >= 0) {
      Debug(session_,
            "Application did not accept any bytes from stream %" PRIi64
            " into packet",
            stream_data.id);
    }

    // When nwrite is zero, it means we are congestion limited or it is
    // just not our turn to send something. Re-schedule the stream if it
    // had unsent data (payload or FIN) so the next timer-triggered
    // SendPendingData retries it. Without this, a FIN-only send that
    // hits nwrite=0 is lost forever — the stream already returned EOS
    // from Pull and won't be re-scheduled by anyone else.
    // We call Application::ResumeStream directly (not Session::ResumeStream)
    // to avoid creating a SendPendingDataScope — we're already inside
    // SendPendingData and re-entering would just hit nwrite=0 again.
    if (nwrite == 0) {
      Debug(session_, "Congestion or not our turn to send");
      if (stream_data.id >= 0 && (stream_data.count > 0 || stream_data.fin)) {
        ResumeStream(stream_data.id);
      }
      return;
    }

    // A negative nwrite value indicates either an error or that there is more
    // data to write into the packet.
    if (nwrite < 0) {
      switch (nwrite) {
        case NGTCP2_ERR_STREAM_DATA_BLOCKED: {
          // We could not write any data for this stream into the packet because
          // the flow control for the stream itself indicates that the stream
          // is blocked. We'll skip and move on to the next stream.
          // ndatalen = -1 means that no stream data was accepted into the
          // packet, which is what we want here.
          DCHECK_EQ(ndatalen, -1);
          // We should only have received this error if there was an actual
          // stream identified in the stream data, but let's double check.
          DCHECK_GE(stream_data.id, 0);
          session_->StreamDataBlocked(stream_data.id);
          continue;
        }
        case NGTCP2_ERR_STREAM_SHUT_WR: {
          // Indicates that the writable side of the stream should be closed
          // locally or the stream is being reset. In either case, we can't send
          // data for this stream!
          Debug(session_,
                "Closing stream %" PRIi64 " for writing",
                stream_data.id);
          // ndatalen = -1 means that no stream data was accepted into the
          // packet, which is what we want here.
          DCHECK_EQ(ndatalen, -1);
          // We should only have received this error if there was an actual
          // stream identified in the stream data, but let's double check.
          DCHECK_GE(stream_data.id, 0);
          if (stream_data.stream) [[likely]] {
            stream_data.stream->EndWritable();
          }
          // Notify the application that the stream's write side is shut
          // so it stops queuing data. Without this, GetStreamData would
          // keep returning the same stream and we'd loop forever.
          StreamWriteShut(stream_data.id);
          continue;
        }
        case NGTCP2_ERR_WRITE_MORE: {
          Debug(session_, "Packet buffer not full, coalesce more data into it");
          // Room for more in this packet. Try to pack a pending datagram
          // if there is one. Otherwise just loop around and keep going.
          if (session_->HasPendingDatagrams()) {
            auto result = TryWritePendingDatagram(
                &path, packet->data(), packet->length());
            // When result is 0, either the datagram was congestion controlled,
            // didn't fit in the packet, or was abandoned. Skip and continue.

            // When result is > 0, the packet is done and the result is the
            // completed size of the packet we're sending.
            if (result > 0) {
              size_t len = result;
              Debug(session_, "Sending packet with %zu bytes", len);
              packet->Truncate(len);
              session_->Send(std::move(packet), path);
              if (++packet_send_count == max_packet_count) return;
            } else if (result < 0) {
              // Any negative result other than NGTCP2_ERR_WRITE_MORE
              // at this point is fatal. The session will have been
              // closed.
              if (result != NGTCP2_ERR_WRITE_MORE) return;
            }
          }
          continue;
        }
        case NGTCP2_ERR_CALLBACK_FAILURE: {
          // This case really should not happen. It indicates that the
          // ngtcp2 callback failed for some reason. This would be a
          // bug in our code.
          Debug(session_, "Internal failure with ngtcp2 callback");
          session_->SetLastError(
              QuicError::ForNgtcp2Error(NGTCP2_ERR_INTERNAL));
          closed = true;
          return session_->Close(CloseMethod::SILENT);
        }
      }

      // Some other type of error happened.
      DCHECK_EQ(ndatalen, -1);
      Debug(session_,
            "Application encountered error while writing packet: %s",
            ngtcp2_strerror(nwrite));
      session_->SetLastError(QuicError::ForNgtcp2Error(nwrite));
      closed = true;
      return session_->Close(CloseMethod::SILENT);
    }

    // At this point we have a packet prepared to send. The nwrite
    // is the size of the packet we are sending.
    size_t len = nwrite;
    Debug(session_, "Sending packet with %zu bytes", len);
    packet->Truncate(len);
    session_->Send(std::move(packet), path);
    if (++packet_send_count == max_packet_count) return;

    // If there are pending datagrams, try sending them in a fresh packet.
    // This is necessary because ngtcp2_conn_writev_stream only returns
    // NGTCP2_ERR_WRITE_MORE when there is actual stream data — when no
    // streams are active, the coalescing path above is never reached and
    // datagrams would never be sent.
    if (session_->HasPendingDatagrams()) {
      if (!ensure_packet()) [[unlikely]] {
        Debug(session_, "Failed to create packet for datagram");
        session_->SetLastError(QuicError::ForNgtcp2Error(NGTCP2_ERR_INTERNAL));
        closed = true;
        return session_->Close(CloseMethod::SILENT);
      }
      auto result =
          TryWritePendingDatagram(&path, packet->data(), packet->length());
      if (result > 0) {
        Debug(session_, "Sending datagram packet with %zd bytes", result);
        packet->Truncate(static_cast<size_t>(result));
        session_->Send(std::move(packet), path);
        if (++packet_send_count == max_packet_count) return;
      } else if (result < 0 && result != NGTCP2_ERR_WRITE_MORE) {
        // Fatal error — session already closed by TryWritePendingDatagram.
        return;
      }
      // If result == 0 (congestion) or NGTCP2_ERR_WRITE_MORE (datagram
      // packed but room for more), the loop continues normally.
    }
  }
}

ssize_t Session::Application::WriteVStream(PathStorage* path,
                                           uint8_t* dest,
                                           ssize_t* ndatalen,
                                           size_t max_packet_size,
                                           const StreamData& stream_data) {
  DCHECK_LE(stream_data.count, kMaxVectorCount);
  uint32_t flags = NGTCP2_WRITE_STREAM_FLAG_MORE;
  if (stream_data.fin) flags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
  return ngtcp2_conn_writev_stream(*session_,
                                   &path->path,
                                   // TODO(@jasnell): ECN blocked on libuv
                                   nullptr,
                                   dest,
                                   max_packet_size,
                                   ndatalen,
                                   flags,
                                   stream_data.id,
                                   stream_data,
                                   stream_data.count,
                                   uv_hrtime());
}

// ============================================================================
// The DefaultApplication is the default implementation of Session::Application
// that is used for all unrecognized ALPN identifiers.
class DefaultApplication final : public Session::Application {
 public:
  // Marked NOLINT because the cpp linter gets confused about this using
  // statement not being sorted with the using v8 statements at the top
  // of the namespace.
  using Application::Application;  // NOLINT

  Session::Application::Type type() const override {
    return Session::Application::Type::DEFAULT;
  }

  error_code GetNoErrorCode() const override { return 0; }

  // Raw QUIC has no application-defined "general failure" code, so
  // fall back to the QUIC transport-level INTERNAL_ERROR (0x1) used
  // by ngtcp2 for unspecified failures.
  error_code GetInternalErrorCode() const override {
    return NGTCP2_INTERNAL_ERROR;
  }

  void EarlyDataRejected() override {
    // Destroy all open streams — ngtcp2 has already discarded their
    // internal state when it rejected the early data.
    session().DestroyAllStreams(QuicError::ForApplication(0));
    if (!session().is_destroyed()) {
      session().EmitEarlyDataRejected();
    }
  }

  bool ApplySessionTicketData(const PendingTicketAppData& data) override {
    return std::holds_alternative<DefaultTicketData>(data);
  }

  bool ReceiveStreamOpen(stream_id id) override {
    auto stream = session().CreateStream(id);
    if (!stream || session().is_destroyed()) [[unlikely]] {
      return !session().is_destroyed();
    }
    return true;
  }

  bool ReceiveStreamData(stream_id id,
                         const uint8_t* data,
                         size_t datalen,
                         const Stream::ReceiveDataFlags& flags,
                         void* stream_user_data) override {
    BaseObjectPtr<Stream> stream;
    if (stream_user_data == nullptr) {
      // This is the first time we're seeing this stream. Implicitly create it.
      stream = session().CreateStream(id);
      if (!stream || session().is_destroyed()) [[unlikely]] {
        // We couldn't create the stream, or the session was destroyed
        // during the onstream callback (via MakeCallback re-entrancy).
        return false;
      }
    } else {
      stream = BaseObjectPtr<Stream>(Stream::From(stream_user_data));
      if (!stream) {
        Debug(&session(),
              "Default application failed to get existing stream "
              "from user data");
        return false;
      }
    }

    CHECK(stream);

    // Now we can actually receive the data! Woo!
    stream->ReceiveData(data, datalen, flags);
    return true;
  }

  int GetStreamData(StreamData* stream_data) override {
    // Reset the state of stream_data before proceeding...
    stream_data->id = -1;
    stream_data->count = 0;
    stream_data->fin = 0;
    stream_data->stream.reset();
    Debug(&session(), "Default application getting stream data");
    DCHECK_NOT_NULL(stream_data);
    // If the queue is empty, there aren't any streams with data yet

    // If the connection-level flow control window is exhausted,
    // there is no point in pulling stream data.
    if (!session().max_data_left()) return 0;
    if (stream_queue_.IsEmpty()) return 0;

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

          // It is possible that the data pointers returned are not actually
          // the data pointers in the stream_data. If that's the case, we need
          // to copy over the pointers.
          count = std::min(count, kMaxVectorCount);
          ngtcp2_vec* dest = *stream_data;
          if (dest != data) {
            for (size_t n = 0; n < count; n++) {
              dest[n] = data[n];
            }
          }

          stream_data->count = count;

          if (count > 0) {
            stream->Schedule(&stream_queue_);
          }

          // Not calling done here because we defer committing
          // the data until after we're sure it's written.
        };

    if (!stream->is_eos()) [[likely]] {
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

  void ResumeStream(stream_id id) override { ScheduleStream(id); }

  void BlockStream(stream_id id) override {
    if (auto stream = session().FindStream(id)) [[likely]] {
      // Remove the stream from the send queue. It will be re-scheduled
      // via ExtendMaxStreamData when the peer grants more flow control.
      // Without this, SendPendingData would repeatedly pop and retry
      // the same blocked stream in an infinite loop.
      stream->Unschedule();
      stream->EmitBlocked();
    }
  }

  void ExtendMaxStreamData(Stream* stream, uint64_t max_data) override {
    // The peer granted more flow control for this stream. Re-schedule
    // it so SendPendingData will resume writing.
    DCHECK_NOT_NULL(stream);
    stream->Schedule(&stream_queue_);
  }

  bool StreamCommit(StreamData* stream_data, size_t datalen) override {
    DCHECK_NOT_NULL(stream_data);
    CHECK(stream_data->stream);
    stream_data->stream->Commit(datalen, stream_data->fin);
    return true;
  }

  SET_SELF_SIZE(DefaultApplication)
  SET_MEMORY_INFO_NAME(DefaultApplication)
  SET_NO_MEMORY_INFO()

 private:
  void ScheduleStream(stream_id id) {
    if (auto stream = session().FindStream(id)) [[likely]] {
      stream->Schedule(&stream_queue_);
    }
  }

  Stream::Queue stream_queue_;
};

std::unique_ptr<Session::Application> CreateDefaultApplication(
    Session* session, const Session::Application_Options& options) {
  return std::make_unique<DefaultApplication>(session, options);
}

}  // namespace quic
}  // namespace node

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
