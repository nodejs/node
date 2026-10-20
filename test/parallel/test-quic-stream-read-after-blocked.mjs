// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: a sender blocked on flow control is resumed once the consumer
// starts reading. This confirms that we do flush MAX_STREAM_DATA
// frames as expected when the consumer reads.

import { hasQuic, skip, mustCall, mustCallAtLeast } from '../common/index.mjs';
import { setTimeout as delay } from 'node:timers/promises';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

// Larger than the default 256 KB stream flow-control window:
const size = 1024 * 1024;
const expected = new Uint8Array(size);
for (let i = 0; i < size; i++) expected[i] = i & 0xff;

const senderBlocked = Promise.withResolvers();
const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    stream.onblocked = mustCallAtLeast(() => senderBlocked.resolve(stream), 1);
    stream.setBody(expected);
    await stream.closed;
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Write a byte to open the stream:
const stream = await clientSession.createBidirectionalStream();
await stream.writer.write(new Uint8Array([1]));

// Wait until the sender has filled the window and blocked.
const serverStream = await senderBlocked.promise;

// Poll until everything has been acked, so the stream is fully idle:
while (serverStream.stats.maxOffsetAcknowledged !== serverStream.stats.bytesSent) {
  await delay(1);
}

// Try to read:
const received = await bytes(stream);
assert.strictEqual(received.byteLength, expected.byteLength);
assert.deepStrictEqual(received, expected);

stream.writer.endSync();
await stream.closed;
clientSession.close();
done.resolve();

await Promise.all([done.promise, clientSession.closed]);
await serverEndpoint.close();
