// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream errors during graceful close are handled.
// When a session is gracefully closing and an open stream encounters
// an error, the session should still close cleanly without crashing.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Read some data then reset the stream while the client
    // is still sending — this creates a stream error.
    const data = await bytes(stream);
    ok(data.byteLength > 0);
    stream.resetStream(99n);
    serverSession.close();
    serverDone.resolve();
  });
}), {
  transportParams: { initialMaxStreamDataBidiRemote: 256 },
  onerror(err) { ok(err); },
});

const clientSession = await connect(serverEndpoint.address, {
  onerror(err) { ok(err); },
});
await clientSession.opened;

// Send data on a stream. The server will reset it.
const stream = await clientSession.createBidirectionalStream();
stream.setBody(new Uint8Array(4096));

// The stream will error due to the server's reset.
// The session should still close gracefully.
await rejects(stream.closed, (error) => {
  strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
  return true;
});
await Promise.all([serverDone.promise, clientSession.closed]);
await serverEndpoint.close();
