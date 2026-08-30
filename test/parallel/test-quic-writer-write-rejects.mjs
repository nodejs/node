// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: strict backpressure allows one pending write and rejects the next.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes, drainableProtocol: dp } = await import('stream/iter');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await bytes(stream);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Use a small budget to trigger backpressure easily.
const stream = await clientSession.createBidirectionalStream({
  budget: 1024,
});
const w = stream.writer;

// Fill the buffer.
assert.strictEqual(w.writeSync(new Uint8Array(1024)), true);

// canWrite should now be false.
assert.strictEqual(w.canWrite, false);

// The first async write waits for capacity.
const pendingWrite = w.write(new Uint8Array(512));

// A second pending write violates strict backpressure.
await assert.rejects(
  w.writev([new Uint8Array(512)]),
  { code: 'ERR_INVALID_STATE', name: 'RangeError' },
);

// The drain event admits the pending write.
const drain = w[dp]();
assert.ok(drain instanceof Promise);
await Promise.all([drain, pendingWrite]);
assert.ok(w.canWrite === true);

// Now write succeeds.
await w.write(new Uint8Array(100));

w.endSync();
for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
