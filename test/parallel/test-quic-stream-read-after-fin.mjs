// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: reading a stream after its read side has received FIN, although
// the stream is still alive.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import { setTimeout as delay } from 'node:timers/promises';
import assert from 'node:assert';

const { deepStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const expected = new TextEncoder().encode('data sent before any reader');
const serverSent = Promise.withResolvers();
const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Reply with the full body plus FIN:
    stream.setBody(expected);
    serverSent.resolve();
    await stream.closed;
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();

// Write a byte to open the stream:
const writer = stream.writer;
await writer.write(new Uint8Array([1]));

// Wait until the server has sent its response, with a buffer for the local
// delivery time itself. We can't actually read to check without undermining
// the test itself unfortunately.
await serverSent.promise;
await delay(10);

const received = await bytes(stream);
deepStrictEqual(received, expected);

writer.endSync();
await stream.closed;
clientSession.close();
done.resolve();

await done.promise;
await clientSession.closed;
await serverEndpoint.close();
