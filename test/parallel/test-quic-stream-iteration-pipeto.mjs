// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: pipeTo pipes a QUIC stream to another writable.
// The server reads from the incoming stream and pipes it to the
// outgoing writer side of the same bidi stream (echo).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes, pipeTo } = await import('stream/iter');

const encoder = new TextEncoder();
const message = 'pipeTo test data';
const expected = encoder.encode(message);

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Pipe the readable side of the stream to the writable side (echo).
    // pipeTo(source, destination) where destination is the stream's writer.
    await pipeTo(stream, stream.writer);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode(message),
});

// Read the echoed data.
const echoed = await bytes(stream);
assert.deepStrictEqual(echoed, expected);

await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
