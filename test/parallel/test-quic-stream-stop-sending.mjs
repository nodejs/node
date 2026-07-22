// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stopSending.
// Server calls stopSending(99n) on an incoming stream from the client.
// The server's stream.closed rejects with error code 99 (the stop
// sending code). The client's stream.closed resolves normally because
// the server's write side completed (endSync sent FIN).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { ok, rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Tell the client to stop sending with code 99.
    stream.stopSending(99n);
    stream.writer.endSync();

    // The server's stream.closed rejects with the stop-sending code
    // because the inbound side was reset by the peer in response.
    await rejects(stream.closed, (error) => {
      strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
      ok(error.message.includes('99'));
      return true;
    });
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('stop me'),
});

// The client's stream.closed resolves because the server sent FIN
// on its write side (endSync) and the read side completed normally.

await Promise.all([serverDone.promise, stream.closed]);
await clientSession.close();
await serverEndpoint.close();
