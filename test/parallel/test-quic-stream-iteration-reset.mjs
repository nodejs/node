// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: a peer RESET_STREAM truncates the readable. The async iterator
// delivers the data received before the reset, then throws
// ERR_QUIC_STREAM_RESET (carrying the peer's code) at the end - rather than
// ending cleanly.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import { setTimeout as delay } from 'node:timers/promises';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    stream.writer.write(new Uint8Array(1000).fill(7));
    while (stream.stats.maxOffsetAcknowledged < 1000n) await delay(5);
    stream.resetStream(42n);
    stream.closed.catch(() => {});
  });
}));

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
});
await clientSession.opened;

// Keep our write side open so the stream stays alive while we read.
const stream = await clientSession.createBidirectionalStream();
await stream.writer.write(new Uint8Array([1]));
stream.closed.catch(() => {});

let received = 0;
let threw;
try {
  for await (const chunk of stream) {
    for (const c of chunk) received += c.byteLength;
  }
} catch (err) {
  threw = err;
}

// The buffered data was delivered before the error.
assert.strictEqual(received, 1000);
// The reset surfaced as a reset error (with its code), not a clean end.
assert.strictEqual(threw?.code, 'ERR_QUIC_STREAM_RESET');

clientSession.close();
await serverEndpoint.close();
