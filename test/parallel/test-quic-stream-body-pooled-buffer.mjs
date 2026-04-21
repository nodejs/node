// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: pooled Buffer body copies correctly.
// Buffer.from() creates pooled buffers that share a larger ArrayBuffer.
// The QUIC body handling must copy (not transfer) partial views.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const message = 'pooled buffer test data';
const expected = Buffer.from(message);

// Verify this IS a pooled buffer (byteLength < buffer.byteLength).
ok(
  expected.buffer.byteLength > expected.byteLength,
  'Buffer should be pooled for this test to be meaningful',
);

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    strictEqual(Buffer.from(received).toString(), message);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Send the pooled buffer as body via setBody.
const stream = await clientSession.createBidirectionalStream();
stream.setBody(expected);

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
