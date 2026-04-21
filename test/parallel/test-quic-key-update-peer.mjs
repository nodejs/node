// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: peer-initiated key update handled transparently.
// The server initiates a key update. Data continues flowing on
// the client side without interruption.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;

  // Server initiates key update.
  serverSession.updateKey();

  serverSession.onstream = mustCall(async (stream) => {
    const data = await bytes(stream);
    // Data should arrive correctly despite key update.
    strictEqual(Buffer.from(data).toString(), 'after key update');
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Send data after the server's key update — should work transparently.
const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('after key update'),
});
for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);

await clientSession.closed;
await serverEndpoint.close();
