// Flags: --experimental-quic --no-warnings

// Test: resetStream() mid-transfer.
// The stream is in the Send state — data is being sent. Calling
// resetStream() aborts the transfer. The server's onreset callback
// fires with the error code. The server may receive partial data
// before the reset.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

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
      ok(error.message.includes('43'));
      serverSession.close();
      serverDone.resolve();
    });
    serverReady.resolve();
  });
}), {
  // Small flow control window to keep data in flight longer.
  transportParams: { initialMaxStreamDataBidiRemote: 256 },
  onerror: mustNotCall(),
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Send a large body — with the 256-byte flow control window, the
// transfer will be in progress when we reset.
const stream = await clientSession.createBidirectionalStream();
stream.setBody(new Uint8Array(8192));

// Wait for the server to receive the stream (first STREAM frames).
await serverReady.promise;

// Reset mid-transfer.
stream.resetStream(43n);

await rejects(stream.closed, (error) => {
  strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
  ok(error.message.includes('43'));
  return true;
});

await serverDone.promise;
await clientSession.closed;
await serverEndpoint.close();
