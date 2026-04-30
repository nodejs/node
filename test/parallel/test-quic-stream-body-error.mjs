// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream destroyed during async source consumption.
// When the stream is destroyed while an async iterable body source is
// active, the source consumption should stop.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';
const { setTimeout } = await import('node:timers/promises');

const { ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), { transportParams: { maxIdleTimeout: 1 } });

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
});
await clientSession.opened;

let yieldCount = 0;
async function* slowSource() {
  while (true) {
    yield encoder.encode(`chunk ${yieldCount++} `);
    await setTimeout(50);
  }
}

const stream = await clientSession.createBidirectionalStream();
stream.setBody(slowSource());

// Destroy the stream after a short delay.
await setTimeout(200);
stream.destroy();
await stream.closed;

// The source should have stopped. It may yield a few chunks
// but not an unbounded number.
ok(yieldCount < 50, `yieldCount too high: ${yieldCount}`);

await clientSession.closed;
await serverEndpoint.close();
