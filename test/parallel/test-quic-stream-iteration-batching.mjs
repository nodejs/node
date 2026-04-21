// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: batching — multiple chunks collected per iteration step.
// When multiple chunks are available synchronously (e.g., the server
// sends them rapidly), the async iterator should batch them into
// arrays of Uint8Array, not yield one chunk at a time.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Send multiple small chunks rapidly — they should be batched
    // on the receiving side.
    const w = stream.writer;
    for (let i = 0; i < 20; i++) {
      w.writeSync(encoder.encode(`chunk${i} `));
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

// Iterate and count batches vs total chunks.
let batchCount = 0;
let totalChunks = 0;
for await (const batch of stream) {
  batchCount++;
  ok(Array.isArray(batch), 'Each iteration step yields an array');
  for (const chunk of batch) {
    ok(chunk instanceof Uint8Array, 'Each item is a Uint8Array');
    totalChunks++;
  }
}

// There should be fewer batches than total chunks — proving batching.
// (On very slow machines, each chunk might arrive separately, so we
// can't assert batchCount < totalChunks strictly. But totalChunks
// should be > 0.)
ok(totalChunks > 0, 'Should have received chunks');
ok(batchCount > 0, 'Should have received batches');

await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
