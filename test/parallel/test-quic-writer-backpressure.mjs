// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: writer backpressure.
// writeSync returns false when budget is exceeded.
// drainableProtocol returns promise when canWrite is false.
// drainableProtocol promise resolves when drain fires.
// Try-fallback pattern: writeSync false, await drain, retry.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes, drainableProtocol: dp } = await import('stream/iter');

// Total data: 8 x 1KB = 8KB. budget: 2KB.
const numChunks = 8;
const chunkSize = 1024;
const totalSize = numChunks * chunkSize;

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    assert.strictEqual(received.byteLength, totalSize);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  budget: 2048,
});
const w = stream.writer;

// Initial canWrite should be true.
assert.strictEqual(w.canWrite, true);
assert.strictEqual(stream.budget, 2048);

let backpressureCount = 0;

for (let i = 0; i < numChunks; i++) {
  const chunk = new Uint8Array(chunkSize);
  chunk.fill(i & 0xFF);
  while (!w.writeSync(chunk)) {
    // writeSync returned false.
    backpressureCount++;

    // drainableProtocol returns a promise when backpressured.
    const drain = w[dp]();
    assert.ok(drain instanceof Promise, 'drainableProtocol should return a Promise');

    // The promise resolves when drain fires.
    await drain;

    // After drain, canWrite should be true.
    assert.ok(w.canWrite === true, `canWrite after drain should be true, got ${w.canWrite}`);
  }
}

w.endSync();

// Backpressure should have been hit with a 2KB budget
// and 1KB chunks (every 2 chunks fills the buffer).
assert.ok(backpressureCount > 0, 'backpressure should have been hit');

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
