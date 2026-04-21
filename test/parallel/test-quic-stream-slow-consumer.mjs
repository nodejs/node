// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: slow consumer applies backpressure.
// With a small flow control window and a large body, the sender
// blocks waiting for the receiver to extend the window. The transfer
// completes when the receiver reads the data.

import { hasQuic, skip, mustCall, mustCallAtLeast } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const dataLength = 4096;
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    strictEqual(received.byteLength, dataLength);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  // Small stream window forces the sender to block repeatedly.
  transportParams: { initialMaxStreamDataBidiRemote: 256 },
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

let blockedCount = 0;
const stream = await clientSession.createBidirectionalStream();

// The actual number of blocks can vary on a range of factors. We're
// only validating that blocking occurs at least once.
stream.onblocked = mustCallAtLeast(() => {
  blockedCount++;
}, 1);

stream.setBody(new Uint8Array(dataLength));

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);

// The sender should have been blocked multiple times.
ok(blockedCount > 0, `Expected blocking, got ${blockedCount}`);

await clientSession.closed;
await serverEndpoint.close();
