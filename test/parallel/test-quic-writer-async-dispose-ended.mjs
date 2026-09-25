// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: Symbol.asyncDispose only fails if writable side not ended.
// If the writer was already ended (via endSync/end), asyncDispose
// should not fail — it's a no-op.

import { hasQuic, skip, mustCall } from '../common/index.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
const w = stream.writer;

// End the writer normally.
w.writeSync(encoder.encode('data'));
w.endSync();

// After end, asyncDispose should be a no-op (writer already ended).
await w[Symbol.asyncDispose]();

// The stream should close cleanly.
for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars

await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
