// Flags: --experimental-quic --no-warnings

// Test: When the user's `onsession` callback throws synchronously or
// returns a rejected promise, the endpoint is destroyed with that
// error rather than surfacing as an unhandled exception/rejection out
// of the C++ -> JS callback boundary. The error is observable through
// `endpoint.closed` rejecting with the thrown value.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const transportParams = { maxIdleTimeout: 1 };

// -------------------------------------------------------------------
// 1. Synchronous throw in onsession -> endpoint.closed rejects with
//    that error.
// -------------------------------------------------------------------
{
  const sessionError = new Error('sync onsession failure');

  const serverEndpoint = await listen(mustCall(() => {
    throw sessionError;
  }), { transportParams });

  // Attach the rejection handler BEFORE initiating the connection so
  // there is no window where serverEndpoint.closed is rejected without
  // a handler attached. The throw inside `onsession` is delivered
  // synchronously from the C++ -> JS callback, so the rejection can
  // arrive on the very next microtask after `connect()` returns.
  const closedAssertion = rejects(serverEndpoint.closed, sessionError);

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });

  await closedAssertion;

  // Explicitly tear down the client side. Even though the endpoint
  // cascade now sends CONNECTION_CLOSE so the client learns about the
  // teardown promptly, dropping our reference here keeps the test
  // robust to network-dropped close packets and stops the event loop
  // from waiting on the client's idle timer to expire.
  clientSession.destroy();
}

// -------------------------------------------------------------------
// 2. onsession returns a rejected promise -> endpoint.closed rejects
//    with that error.
// -------------------------------------------------------------------
{
  const sessionError = new Error('async onsession failure');

  const serverEndpoint = await listen(mustCall(async () => {
    throw sessionError;
  }), { transportParams, onerror() {} });

  const closedAssertion = rejects(serverEndpoint.closed, sessionError);

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });

  await closedAssertion;

  clientSession.destroy();
}
