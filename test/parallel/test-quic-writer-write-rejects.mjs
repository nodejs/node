// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: write() rejects when flow-controlled.
// The async write() method rejects with ERR_INVALID_STATE when the
// chunk exceeds capacity (canWrite is false).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual, ok } = assert;

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
strictEqual(w.writeSync(new Uint8Array(1024)), true);

// canWrite should now be false.
strictEqual(w.canWrite, false);

// Async write() should reject when buffer is full.
await rejects(
  w.write(new Uint8Array(512)),
  { code: 'ERR_INVALID_STATE' },
);

// Wait for drain, then write should succeed.
const drain = w[dp]();
ok(drain instanceof Promise);
await drain;
ok(w.canWrite === true);

// Now write succeeds.
await w.write(new Uint8Array(100));

w.endSync();
for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
