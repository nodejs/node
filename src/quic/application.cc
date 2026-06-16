#include "util.h"
#if HAVE_OPENSSL && HAVE_QUIC
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <async_wrap-inl.h>
#include <debug_utils-inl.h>
#include <uv.h>
#include <mutex>
#include <string>
#include <vector>
#include "application.h"
#include "defs.h"
#include "session.h"

namespace node {
namespace quic {

// ============================================================================
// The application factory registry.

namespace {
struct ApplicationFactoryEntry {
  std::string name;
  ApplicationFactory factory;
};
// Process-wide registry. Registered once per process at binding
// initialization (registration is idempotent for repeated binding inits,
// e.g. workers); intentionally leaked, process lifetime.
std::mutex application_factories_mutex;
std::vector<ApplicationFactoryEntry>* application_factories = nullptr;
}  // namespace

void RegisterApplicationFactory(std::string_view name,
                                const ApplicationFactory& factory) {
  DCHECK_NOT_NULL(factory.create);
  std::lock_guard<std::mutex> lock(application_factories_mutex);
  if (application_factories == nullptr) {
    application_factories = new std::vector<ApplicationFactoryEntry>();
  }
  for (const auto& entry : *application_factories) {
    if (entry.factory.create == factory.create) return;
  }
  application_factories->push_back({std::string(name), factory});
}

const ApplicationFactory* FindApplicationFactory(std::string_view name) {
  std::lock_guard<std::mutex> lock(application_factories_mutex);
  if (application_factories == nullptr) return nullptr;
  for (const auto& entry : *application_factories) {
    if (entry.name == name) return &entry.factory;
  }
  return nullptr;
}

// ============================================================================

Session::Application::Application(Session* session) : session_(session) {}

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

}  // namespace quic
}  // namespace node

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
