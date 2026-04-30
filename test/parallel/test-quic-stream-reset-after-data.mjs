// Flags: --experimental-quic --no-warnings

// Test: resetStream() after all data written but before ACK.
// The stream is in the Data Sent state — all data has been sent
// including FIN, but the peer hasn't acknowledged everything yet.
// Calling resetStream() aborts the stream. The server's onreset
// callback fires with the error code.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

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
    rejects(stream.closed, (error) => {
      strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
      return true;
    }).then(mustCall());

    stream.onreset = mustCall((error) => {
      strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
      ok(error.message.includes('44'));
      serverSession.close();
      serverDone.resolve();
    });
    serverReady.resolve();
  });
}), {
  onerror: mustNotCall(),
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Send a small body — it will be sent quickly (including FIN),
// putting the stream in "Data Sent" state.
const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('small payload'),
});

// Wait for the server to receive the stream before resetting.
await serverReady.promise;

// Reset after data was written. The data and FIN have been sent
// but may not be fully acknowledged yet.
stream.resetStream(44n);

await rejects(stream.closed, (error) => {
  strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
  ok(error.message.includes('44'));
  return true;
});

await serverDone.promise;
await clientSession.closed;
await serverEndpoint.close();
