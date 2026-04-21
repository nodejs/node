// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: writer backpressure.
// writeSync returns false when highWaterMark is exceeded.
// drainableProtocol returns promise when desiredSize <= 0.
// drainableProtocol promise resolves when drain fires.
// Try-fallback pattern: writeSync false, await drain, retry.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes, drainableProtocol: dp } = await import('stream/iter');

// Total data: 8 x 1KB = 8KB. highWaterMark: 2KB.
const numChunks = 8;
const chunkSize = 1024;
const totalSize = numChunks * chunkSize;

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    strictEqual(received.byteLength, totalSize);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  highWaterMark: 2048,
});
const w = stream.writer;

// Initial desiredSize should be the highWaterMark.
strictEqual(w.desiredSize, 2048);
strictEqual(stream.highWaterMark, 2048);

let backpressureCount = 0;

for (let i = 0; i < numChunks; i++) {
  const chunk = new Uint8Array(chunkSize);
  chunk.fill(i & 0xFF);
  while (!w.writeSync(chunk)) {
    // writeSync returned false.
    backpressureCount++;

    // drainableProtocol returns a promise when backpressured.
    const drain = w[dp]();
    ok(drain instanceof Promise, 'drainableProtocol should return a Promise');

    // The promise resolves when drain fires.
    await drain;

    // After drain, desiredSize should be > 0.
    ok(w.desiredSize > 0, `desiredSize after drain should be > 0, got ${w.desiredSize}`);
  }
}

w.endSync();

// Backpressure should have been hit with a 2KB highWaterMark
// and 1KB chunks (every 2 chunks fills the buffer).
ok(backpressureCount > 0, 'backpressure should have been hit');

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
