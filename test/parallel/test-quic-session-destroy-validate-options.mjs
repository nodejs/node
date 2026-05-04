// Flags: --experimental-quic --no-warnings

// Test: QuicSession.destroy(error, options) validates `options` up front,
// before any side effects. A malformed `options` argument must throw
// without:
//   * publishing on the `quic.session.error` diagnostics channel,
//   * invoking the user's `onerror` callback,
//   * cascading destroy to open streams,
//   * clearing the session's private state,
//   * destroying the underlying C++ handle.
//
// After such a throw the session is still alive and a subsequent
// destroy() with valid options must succeed: the client emits a
// CONNECTION_CLOSE frame with the supplied transport error code, and
// the local `closed` promise rejects with the original error.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import diagnostics_channel from 'node:diagnostics_channel';

const { strictEqual, rejects, throws } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// `maxIdleTimeout` is measured in seconds. Set short so any regression
// that prevents CONNECTION_CLOSE from being sent fails promptly via
// idle close instead of hanging the test.
const transportParams = { maxIdleTimeout: 1 };

// Capture the server-side session in the listen callback. Do not
// `await serverSession.closed` from inside the callback: the final
// destroy in this test sends a transport-error CONNECTION_CLOSE which
// makes that promise reject, and an unhandled rejection inside the
// listen callback would be routed by `safeCallbackInvoke` to
// `endpoint.destroy(err)` and surface as a process-level
// unhandled-rejection crash before the test body has had a chance
// to attach its own rejection handler.
const serverSessionReady = Promise.withResolvers();
let serverSession;
const serverEndpoint = await listen(mustCall((session) => {
  serverSession = session;
  serverSessionReady.resolve();
}), { transportParams });

const clientSession = await connect(serverEndpoint.address, {
  transportParams,
});
await clientSession.opened;
await serverSessionReady.promise;

// Open a stream so the destroy cascade has work to do; if the
// validation hoist regresses and side effects run before the throw,
// this stream will be destroyed by the failed destroy() and the
// `mustNotCall` handler will fire.
const stream = await clientSession.createBidirectionalStream();
stream.onerror = mustNotCall(
  'stream.onerror must not fire when destroy() throws on bad options');

// Subscribe to diagnostics channels so we can assert nothing is
// published before the validation throw.
const errSub = mustNotCall(
  'quic.session.error must not publish when destroy() throws on bad options');
diagnostics_channel.subscribe('quic.session.error', errSub);

clientSession.onerror = mustNotCall(
  'session.onerror must not fire when destroy() throws on bad options');

const goodError = new Error('intended teardown');

// 1. options is not an object -> throws ERR_INVALID_ARG_TYPE.
throws(() => clientSession.destroy(goodError, 'not an object'), {
  code: 'ERR_INVALID_ARG_TYPE',
});
strictEqual(clientSession.destroyed, false);
strictEqual(stream.destroyed, false);

// 2. options.code is the wrong type -> throws ERR_INVALID_ARG_TYPE.
throws(() => clientSession.destroy(goodError, { code: 'oops' }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
strictEqual(clientSession.destroyed, false);
strictEqual(stream.destroyed, false);

// 3. options.type is not in the allowed set -> throws ERR_INVALID_ARG_VALUE.
throws(() => clientSession.destroy(goodError, { type: 'bogus' }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
strictEqual(clientSession.destroyed, false);
strictEqual(stream.destroyed, false);

// 4. options.reason is the wrong type -> throws ERR_INVALID_ARG_TYPE.
throws(() => clientSession.destroy(goodError, { reason: 42 }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
strictEqual(clientSession.destroyed, false);
strictEqual(stream.destroyed, false);

// Now switch the handlers to expect the real teardown so the final
// destroy with valid options can run cleanly.
diagnostics_channel.unsubscribe('quic.session.error', errSub);
clientSession.onerror = mustCall((err) => { strictEqual(err, goodError); });
stream.onerror = mustCall((err) => { strictEqual(err, goodError); });

// Pre-attach rejection handlers on both sides BEFORE triggering the
// final destroy, so the rejections do not race ahead of any awaits in
// the test body. The client rejects with the original `goodError`;
// the server decodes the CONNECTION_CLOSE frame transport code into
// an `ERR_QUIC_TRANSPORT_ERROR`.
const clientClosedAssertion = rejects(clientSession.closed, goodError);
const serverClosedAssertion = rejects(serverSession.closed, mustCall((err) => {
  strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
  return true;
}));

// 5. Valid options after the failed attempts -> session destroys
//    normally, the underlying handle sends CONNECTION_CLOSE with the
//    supplied transport code, and the local closed promise rejects
//    with the original error.
clientSession.destroy(goodError, {
  code: 1n,
  type: 'transport',
  reason: 'after validation throw',
});
strictEqual(clientSession.destroyed, true);
strictEqual(stream.destroyed, true);

await clientClosedAssertion;
await serverClosedAssertion;
await serverEndpoint.close();
