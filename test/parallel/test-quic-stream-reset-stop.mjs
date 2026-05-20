// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: RESET_STREAM and STOP_SENDING.
// server's onreset fires with that code.
// NOTE: CTRL-01/CTRL-08 (stopSending with specific code) is tested
// separately because it requires a second endpoint.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { ok, rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();
const serverReady = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall((stream) => {
    // The server's stream.closed will reject when the session is
    // gracefully closed after the peer's reset.
    rejects(stream.closed, (error) => {
      strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
      return true;
    }).then(mustCall());

    stream.onreset = mustCall((error) => {
      // The error is the raw close tuple: [type, code, reason].
      strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
      ok(error.message.includes('42'));
      serverSession.close();
      serverDone.resolve();
    });
    serverReady.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('will be reset'),
});

// Wait for the server to receive the stream before resetting.
await serverReady.promise;
stream.resetStream(42n);

await serverDone.promise;
// After the server closes (sending CONNECTION_CLOSE), the client
// session enters draining and all streams are destroyed. The client's
// stream.closed rejects with the reset code.
await rejects(stream.closed, (error) => {
  strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
  ok(error.message.includes('42'));
  return true;
});
await clientSession.closed;
await serverEndpoint.close();
