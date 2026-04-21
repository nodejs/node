// Flags: --experimental-quic --no-warnings

// Test: concurrent close() from both client and server.
// Both sides initiate close simultaneously. Neither should crash
// and both closed promises should settle.

import { hasQuic, skip, mustCall } from '../common/index.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Once the stream arrives, both sides close simultaneously.
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
  });
  await serverSession.closed;
  serverDone.resolve();
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Open a stream so the server session has work, then close from both sides.
const stream = await clientSession.createBidirectionalStream();
stream.writer.endSync();
for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await stream.closed;

// Client close happens around the same time as server close above.
await clientSession.close();

await serverDone.promise;
await serverEndpoint.close();
