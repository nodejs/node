// Flags: --experimental-quic --no-warnings

// Test: when `endpoint.destroy(err)` is called on a side that has open,
// fully-handshaked sessions, the cascade through `session.destroy(err)`
// passes close options derived from the error so each session emits a
// CONNECTION_CLOSE on the wire. The peer learns about the teardown via
// that frame, not via its own idle timer.
//
// How the test distinguishes the two cases:
//
//   * If CONNECTION_CLOSE is sent, the client's `session.closed`
//     rejects after the network round-trip with an
//     `ERR_QUIC_TRANSPORT_ERROR` carrying the cascaded code.
//   * If CONNECTION_CLOSE is NOT sent, the client only learns of the
//     teardown via its own idle timer, which hits `[kFinishClose]`
//     case 3 (`/* Idle close */`) and resolves `session.closed`
//     *cleanly*. The `mustCall` rejection-handler would then never
//     fire and the test fails.
//
// A short `maxIdleTimeout` keeps the failure mode fast.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// `maxIdleTimeout` is measured in seconds. One second is far longer
// than CONNECTION_CLOSE on loopback needs to win, while still short
// enough that a regression in which `CONNECTION_CLOSE` is *not* sent
// fails the test promptly: the idle-close path takes the
// `[kFinishClose]` case-3 branch and resolves `session.closed` cleanly
// instead of rejecting, so the rejection-handler `mustCall` below
// would fail with "expected exactly 1, actual 0".
const transportParams = { maxIdleTimeout: 1 };

const serverError = new Error('cascade close frame test');

// Capture the server-side session and wait for *its* `onhandshake` to
// fire before triggering the cascade. The client's `session.opened`
// resolves as soon as the client receives the server's TLS Finished,
// which can land slightly *before* the server has processed the
// client's reciprocal Finished. Without this synchronization the
// server-side `kHandshakeCompleted` flag may still be `false` at
// destroy time and the cascade would skip emitting `CONNECTION_CLOSE`
// (which is the regression this test is designed to catch).
const serverHandshake = Promise.withResolvers();
const onsession = mustCall((serverSession) => {
  serverSession.onhandshake = mustCall(() => {
    serverHandshake.resolve();
  });
});
const serverEndpoint = await listen(onsession, { transportParams });

const clientSession = await connect(serverEndpoint.address, {
  transportParams,
});
await clientSession.opened;
await serverHandshake.promise;

// Attach the rejection handlers BEFORE triggering destroy so neither
// `serverEndpoint.closed` (rejects with `serverError` via the
// `#pendingError` semantics from B7) nor `clientSession.closed`
// (rejects with the transport error decoded from the CONNECTION_CLOSE
// frame) ends up as an unhandled rejection in the brief window before
// this test gets back to awaiting them.
const serverClosedAssertion = rejects(serverEndpoint.closed, serverError);
const clientClosedAssertion = rejects(clientSession.closed, mustCall((err) => {
  strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
  return true;
}));

serverEndpoint.destroy(serverError);

await clientClosedAssertion;
await serverClosedAssertion;

// Explicit cleanup: the client-side session has been rejected via
// CONNECTION_CLOSE but the underlying client endpoint is still alive.
// Tear it down so the event loop drains promptly.
clientSession.destroy();
