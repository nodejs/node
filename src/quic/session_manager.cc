#if HAVE_OPENSSL && HAVE_QUIC
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <base_object-inl.h>
#include <node_sockaddr-inl.h>
#include <util-inl.h>
#include "endpoint.h"
#include "session.h"
#include "session_manager.h"

namespace node::quic {

SessionManager::SessionManager(Environment* env) : env_(env) {}

SessionManager::~SessionManager() = default;

BaseObjectPtr<Session> SessionManager::FindSession(const CID& cid) {
  // Direct SCID match.
  auto it = sessions_.find(cid);
  if (it != sessions_.end()) return it->second;

  // Cross-endpoint CID mapping (locally-generated CIDs for preferred
  // address, multipath, etc.).
  auto scid_it = dcid_to_scid_.find(cid);
  if (scid_it != dcid_to_scid_.end()) {
    it = sessions_.find(scid_it->second);
    if (it != sessions_.end()) return it->second;
    // Stale mapping — clean up.
    dcid_to_scid_.erase(scid_it);
  }

  return {};
}

void SessionManager::AddSession(const CID& scid,
                                BaseObjectPtr<Session> session) {
  sessions_[scid] = std::move(session);
}

void SessionManager::AssociateCID(const CID& cid, const CID& scid) {
  if (cid && scid && cid != scid) {
    dcid_to_scid_[cid] = scid;
  }
}

void SessionManager::DisassociateCID(const CID& cid) {
  if (cid) {
    dcid_to_scid_.erase(cid);
  }
}

void SessionManager::RemoveSession(const CID& scid) {
  auto it = sessions_.find(scid);
  if (it != sessions_.end()) {
    primary_map_.erase(it->second.get());
    sessions_.erase(it);
  }
}

void SessionManager::AssociateStatelessResetToken(
    const StatelessResetToken& token, Session* session) {
  token_map_[token] = session;
}

void SessionManager::DisassociateStatelessResetToken(
    const StatelessResetToken& token) {
  token_map_.erase(token);
}

Session* SessionManager::FindSessionByStatelessResetToken(
    const StatelessResetToken& token) const {
  auto it = token_map_.find(token);
  if (it != token_map_.end()) return it->second;
  return nullptr;
}

void SessionManager::RegisterEndpoint(Endpoint* endpoint,
                                      const SocketAddress& local_address) {
  endpoints_.insert(endpoint);
  endpoint_addrs_[endpoint] = local_address;
}

void SessionManager::UnregisterEndpoint(Endpoint* endpoint) {
  endpoints_.erase(endpoint);
  endpoint_addrs_.erase(endpoint);
  // If no endpoints remain, destroy all sessions.
  if (endpoints_.empty()) {
    DestroyAllSessions();
  }
}

bool SessionManager::HasEndpoints() const {
  return !endpoints_.empty();
}

size_t SessionManager::endpoint_count() const {
  return endpoints_.size();
}

Endpoint* SessionManager::FindEndpointForAddress(
    const SocketAddress& local_addr) const {
  // First pass: exact match.
  for (const auto& [endpoint, addr] : endpoint_addrs_) {
    if (addr == local_addr) return endpoint;
  }
  // Second pass: wildcard fallback. An endpoint bound to 0.0.0.0:port
  // or [::]:port can serve any address on that port.
  int port = SocketAddress::GetPort(local_addr.data());
  for (const auto& [endpoint, addr] : endpoint_addrs_) {
    if (SocketAddress::GetPort(addr.data()) == port) {
      auto host = addr.address();
      if (host == "0.0.0.0" || host == "::") {
        return endpoint;
      }
    }
  }
  return nullptr;
}

void SessionManager::SetPrimaryEndpoint(Session* session, Endpoint* endpoint) {
  primary_map_[session] = endpoint;
}

Endpoint* SessionManager::GetPrimaryEndpoint(Session* session) const {
  auto it = primary_map_.find(session);
  if (it != primary_map_.end()) return it->second;
  return nullptr;
}

void SessionManager::CloseAllSessionsFor(Endpoint* endpoint) {
  // Collect sessions whose primary is this endpoint, then close them.
  // We collect first because closing a session modifies primary_map_.
  std::vector<BaseObjectPtr<Session>> to_close;
  for (const auto& [session, ep] : primary_map_) {
    if (ep == endpoint) {
      // Look up the owning reference from sessions_ so the session
      // stays alive during close.
      for (const auto& [cid, sess_ptr] : sessions_) {
        if (sess_ptr.get() == session) {
          to_close.push_back(sess_ptr);
          break;
        }
      }
    }
  }
  for (auto& session : to_close) {
    session->Close(Session::CloseMethod::SILENT);
  }
}

void SessionManager::DestroyAllSessions() {
  // Copy the map since closing sessions will modify it.
  auto sessions = sessions_;
  for (auto& [cid, session] : sessions) {
    session->Close(Session::CloseMethod::SILENT);
  }
  sessions.clear();
  token_map_.clear();
  dcid_to_scid_.clear();
  primary_map_.clear();
}

bool SessionManager::is_empty() const {
  return sessions_.empty();
}

}  // namespace node::quic

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
