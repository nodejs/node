// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: zero-length stream via setBody(null).
// Creates a stream with no body, then calls setBody(null) which sends
// FIN immediately. The server receives zero bytes.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    strictEqual(received.byteLength, 0);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
stream.setBody(null);

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars

await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
