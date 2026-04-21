// Flags: --experimental-quic --no-warnings

// Test: session.opened rejects when the session is destroyed before the
// TLS handshake completes.
//
// Per doc/api/quic.md: "If the handshake fails or the session is destroyed
// before the handshake completes, the promise will be rejected."
//
// Cases covered:
//   1. session.destroy() (no error) before handshake -> opened rejects
//      with ERR_INVALID_STATE.
//   2. session.destroy(error) before handshake -> opened rejects with
//      that error.
//   3. endpoint.destroy() while a client handshake is in flight ->
//      session.opened rejects (the endpoint cascades destroy to its
//      sessions).
//   4. session.destroy() AFTER opened has already resolved -> opened
//      stays resolved (no late rejection).

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { QuicEndpoint } = await import('node:quic');

// Use a short idle timeout so any leftover server-side state cleans up
// quickly when the client tears down before the handshake completes.
const transportParams = { maxIdleTimeout: 1 };

// -------------------------------------------------------------------
// 1. session.destroy() (no error) before handshake -> opened rejects
//    with ERR_INVALID_STATE.
// -------------------------------------------------------------------
{
  const serverEndpoint = await listen(() => {
    // The server may or may not see this session before the client
    // destroys; we don't await it here.
  }, { transportParams });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });

  // Synchronously destroy the session before the handshake can finish.
  // `connect()` returns the session as soon as the C++ handle is created;
  // the handshake itself happens asynchronously over the network.
  clientSession.destroy();

  await rejects(clientSession.opened, {
    code: 'ERR_INVALID_STATE',
    message: /destroyed before it opened/,
  });

  await serverEndpoint.close();
}

// -------------------------------------------------------------------
// 2. session.destroy(error) before handshake -> opened rejects with
//    that error. Setting `onerror` registers the session-level error
//    handler and marks both `opened` and `closed` as handled, so we
//    only need to assert the rejection on `opened`.
// -------------------------------------------------------------------
{
  const serverEndpoint = await listen(mustNotCall, { transportParams });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });

  const testError = new Error('intentional early destroy');
  clientSession.onerror = mustCall((err) => {
    assert.strictEqual(err, testError);
  });
  clientSession.destroy(testError);

  await assert.rejects(clientSession.opened, testError);

  await serverEndpoint.close();
}

// -------------------------------------------------------------------
// 3. endpoint.destroy() while a client handshake is in flight ->
//    session.opened rejects.
// -------------------------------------------------------------------
{
  const serverEndpoint = await listen(mustNotCall(), { transportParams });

  // Use a dedicated client endpoint so destroying it does not affect
  // the shared default endpoint used by other tests in this file.
  const clientEndpoint = new QuicEndpoint();

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
    endpoint: clientEndpoint,
  });

  // Synchronously destroy the endpoint while the handshake is still
  // in flight. The endpoint should cascade destroy() to its in-flight
  // session, which should reject session.opened.
  clientEndpoint.destroy();

  await rejects(clientSession.opened, {
    code: 'ERR_INVALID_STATE',
    message: /destroyed before it opened/,
  });

  await serverEndpoint.close();
}

// -------------------------------------------------------------------
// 4. session.destroy() AFTER opened resolved -> opened stays resolved.
//    Sanity check that the new "always reject pendingOpen" logic does
//    not stomp on an already-resolved promise.
// -------------------------------------------------------------------
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
    serverDone.resolve();
  }), { transportParams });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });

  const info = await clientSession.opened;
  strictEqual(typeof info.protocol, 'string');

  clientSession.destroy();

  // Awaiting opened a second time should still resolve to the same
  // info object (PromiseWithResolvers caches it; we just assert the
  // promise is not rejected).
  const infoAgain = await clientSession.opened;
  strictEqual(infoAgain, info);

  await serverDone.promise;
  await serverEndpoint.close();
}
