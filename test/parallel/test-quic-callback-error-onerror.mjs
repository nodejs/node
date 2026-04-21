// Flags: --experimental-quic --no-warnings

// Test: onerror callback behavior
// session.onerror fires when session is destroyed with error.
// session.onerror receives the original error as argument.
// session.closed rejects with the original error after onerror.
// session.onerror not called when destroy() has no error.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

const { ok, rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// quic.session.error fires when a session is destroyed with an error.
// It should fire once for the first client session (destroyed with error)
// and not for the second (destroyed without error).
dc.subscribe('quic.session.error', mustCall((msg) => {
  ok(msg.session, 'session.error should include session');
  ok(msg.error, 'session.error should include error');
}));

const transportParams = { maxIdleTimeout: 1 };

// All tested using a single endpoint with two client sessions.
const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}, 2), { transportParams });

// First client: destroy WITH error — onerror fires.
{
  const testError = new Error('destroy with error');
  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });
  await clientSession.opened;

  let onerrorCalled = false;
  clientSession.onerror = mustCall((err) => {
    // Receives the original error.
    strictEqual(err, testError);
    onerrorCalled = true;
  });

  clientSession.destroy(testError);

  // Onerror was called synchronously during destroy.
  strictEqual(onerrorCalled, true);

  // Closed rejects with the original error.
  await rejects(clientSession.closed, testError);
}

// Second client: destroy WITHOUT error — onerror should NOT fire.
{
  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });
  await clientSession.opened;

  clientSession.onerror = mustNotCall('onerror should not be called');

  clientSession.destroy();

  // Closed resolves (no error).
  await clientSession.closed;
}

serverEndpoint.close();
await serverEndpoint.closed;
