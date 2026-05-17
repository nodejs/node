// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: iterator cleanup on break.
// When the consumer breaks out of a for-await loop, the iterator's
// finally block should clean up (clear wakeup, release reader).
// The stream should still be usable for closing.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Send multiple chunks so the client can break mid-stream.
    const w = stream.writer;
    for (let i = 0; i < 10; i++) {
      w.writeSync(encoder.encode(`chunk ${i} `));
    }
    w.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('request'),
});

// Break out of the iterator after the first batch.
let batchCount = 0;
for await (const batch of stream) {
  batchCount++;
  ok(Array.isArray(batch));
  break;  // Exit early — should trigger iterator cleanup.
}
strictEqual(batchCount, 1);

// After break, the stream should still be closable.
// End the writable side (it was already ended by the body).
// The stream closed promise should resolve.
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
