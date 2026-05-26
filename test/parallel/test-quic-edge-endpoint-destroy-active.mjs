// Flags: --experimental-quic --no-warnings

// Test: endpoint closed while sessions are active.
// When endpoint.close() is called while sessions are active, the
// endpoint waits for sessions to finish. When the client closes
// its session, the server session closes (via CONNECTION_CLOSE),
// and the endpoint finishes closing.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
    stream.writer.endSync();
    await stream.closed;
  });
  await serverSession.closed;
  serverDone.resolve();
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Create a stream so there's active work.
const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('hello'),
});
for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await stream.closed;

// Close the endpoint while the server session is still active
// (the session is open but the stream is done).
serverEndpoint.close();
strictEqual(serverEndpoint.closing, true);
strictEqual(serverEndpoint.destroyed, false);

// The endpoint is waiting for the server session. Close the
// client session to trigger the server session to close.
await clientSession.close();

// The server session should close from the CONNECTION_CLOSE.
await Promise.all([serverDone.promise, serverEndpoint.closed]);
strictEqual(serverEndpoint.destroyed, true);
