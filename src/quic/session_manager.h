#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <env.h>
#include <node_sockaddr.h>
#include <unordered_map>
#include <unordered_set>
#include "cid.h"
#include "tokens.h"

namespace node::quic {

class Endpoint;
class Session;

// SessionManager is a per-Realm singleton that centralizes QUIC session
// routing. It holds the authoritative CID -> Session mapping, enabling
// any Endpoint to route packets to any session. This decouples session
// lifetime from individual endpoints, which is required for preferred
// address, connection migration, and multi-path QUIC.
//
// SessionManager is held by BindingData and lazily created on first access.
// It is not exposed to JavaScript.
class SessionManager final {
 public:
  explicit SessionManager(Environment* env);
  ~SessionManager();

  // Session routing. The sessions_ map holds BaseObjectPtr<Session> (owning
  // references). SessionManager is the single authority for session ownership.
  BaseObjectPtr<Session> FindSession(const CID& dcid);
  void AddSession(const CID& scid, BaseObjectPtr<Session> session);
  void RemoveSession(const CID& scid);

  // Cross-endpoint CID association. This map holds locally-generated CIDs
  // that need to be routable from any endpoint (e.g., preferred address CID,
  // multipath NEW_CONNECTION_ID CIDs). Peer-chosen CIDs from connection
  // establishment (config.dcid, config.ocid) go in Endpoint::dcid_to_scid_
  // instead, because those values can collide across endpoints.
  void AssociateCID(const CID& cid, const CID& scid);
  void DisassociateCID(const CID& cid);

  // Stateless reset token association. The token_map_ holds raw (non-owning)
  // pointers. Entries are valid only while the corresponding session exists
  // in sessions_. Sessions clean up their tokens during teardown.
  void AssociateStatelessResetToken(const StatelessResetToken& token,
                                    Session* session);
  void DisassociateStatelessResetToken(const StatelessResetToken& token);
  Session* FindSessionByStatelessResetToken(
      const StatelessResetToken& token) const;

  // Endpoint registry. Endpoints register themselves when they start
  // receiving and unregister when they close.
  void RegisterEndpoint(Endpoint* endpoint, const SocketAddress& local_address);
  void UnregisterEndpoint(Endpoint* endpoint);
  bool HasEndpoints() const;
  size_t endpoint_count() const;

  // Find the endpoint bound to a given local address. Used by the session
  // send path to route packets through the correct endpoint based on the
  // ngtcp2 packet path. Tries exact match first, then wildcard fallback
  // (0.0.0.0 or [::] on the same port).
  Endpoint* FindEndpointForAddress(const SocketAddress& local_addr) const;

  // Primary endpoint tracking. Each session has one primary endpoint
  // responsible for its lifecycle.
  void SetPrimaryEndpoint(Session* session, Endpoint* endpoint);
  Endpoint* GetPrimaryEndpoint(Session* session) const;

  // Close all sessions whose primary endpoint is the given endpoint.
  // Used by Endpoint::Destroy().
  void CloseAllSessionsFor(Endpoint* endpoint);

  // Destroy all sessions. Used when the last endpoint is removed.
  void DestroyAllSessions();

  bool is_empty() const;

 private:
  Environment* env_;

  // The sessions_ map holds strong owning references keyed by locally-
  // generated SCIDs. This is the single source of truth for session
  // ownership.
  CID::Map<BaseObjectPtr<Session>> sessions_;

  // Cross-endpoint CID -> primary SCID mapping. Contains locally-generated
  // CIDs that need to be routable from any endpoint. Peer-chosen CIDs
  // from connection establishment are in Endpoint::dcid_to_scid_ instead.
  CID::Map<CID> dcid_to_scid_;

  // Stateless reset token -> Session (non-owning).
  StatelessResetToken::Map<Session*> token_map_;

  // All registered endpoints.
  std::unordered_set<Endpoint*> endpoints_;

  // Endpoint -> bound local address, for FindEndpointForAddress lookups.
  std::unordered_map<Endpoint*, SocketAddress> endpoint_addrs_;

  // Session -> primary Endpoint mapping (non-owning both directions;
  // sessions are owned by sessions_, endpoints are externally owned).
  std::unordered_map<Session*, Endpoint*> primary_map_;
};

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
