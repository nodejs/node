// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: receive-side coalescing produces reasonably-sized chunks.
// Sends multiple payloads of varying sizes through a bidi stream and
// verifies that the received chunks are coalesced rather than
// delivered as ~1154-byte QUIC frame fragments.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { drainableProtocol: dp } = await import('stream/iter');

// Payloads of varying sizes with diverse prime factors to exercise
// different flow control boundary alignments. Avoids round multiples
// of 1000 so that frame boundaries, accumulation buffer thresholds,
// and flow control windows are hit at irregular offsets.
const payloadSizes = [
  97,        // Small prime: fits in a single frame
  4_999,     // Medium prime: a few frames
  65_521,    // Largest prime below 64 KB buffer flush threshold
  1_153,     // Small prime after large
  196_613,   // Large prime: well beyond 64 KB buffer
  509,       // Small prime tail
];
const totalBytes = payloadSizes.reduce((a, b) => a + b, 0);

// Build deterministic payloads for integrity checking.
function buildPayload(size, seed) {
  const buf = new Uint8Array(size);
  for (let i = 0; i < size; i++) {
    buf[i] = (seed + i) & 0xff;
  }
  return buf;
}

function checksum(data) {
  let sum = 0;
  for (let i = 0; i < data.byteLength; i++) {
    sum = (sum + data[i]) | 0;
  }
  return sum;
}

// Compute expected checksum across all payloads.
let expectedChecksum = 0;
for (let i = 0; i < payloadSizes.length; i++) {
  expectedChecksum = (expectedChecksum +
    checksum(buildPayload(payloadSizes[i], i))) | 0;
}

// --- Server side ---

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const receivedChunkSizes = [];
    let receivedBytes = 0;
    let receivedChecksum = 0;

    for await (const chunks of stream) {
      for (const chunk of chunks) {
        receivedChunkSizes.push(chunk.byteLength);
        receivedBytes += chunk.byteLength;
        receivedChecksum = (receivedChecksum + checksum(chunk)) | 0;
      }
    }

    // Data integrity.
    strictEqual(receivedBytes, totalBytes);
    strictEqual(receivedChecksum, expectedChecksum);

    // Coalescing effectiveness: verify that at least some chunks are larger
    // than the ~1154-byte QUIC frame payload, showing that the accumulator
    // is combining callbacks. On localhost, round-trip latency is near
    // zero so coalescing opportunities are limited to I/O bursts where
    // multiple packets arrive in the same event loop iteration. Under
    // real network conditions the effect is much more pronounced.
    const maxChunkSize = Math.max(...receivedChunkSizes);
    ok(
      maxChunkSize > 1154,
      `Expected at least one chunk larger than a single QUIC frame ` +
      `(1154 bytes), but max was ${maxChunkSize}`
    );

    // Stats: after full drain, bytes_accumulated should be 0.
    strictEqual(stream.stats.bytesAccumulated, 0n);

    // max_bytes_accumulated should be nonzero — data passed through
    // the accumulator.
    ok(
      stream.stats.maxBytesAccumulated > 0n,
      'maxBytesAccumulated should be nonzero'
    );

    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

// --- Client side ---

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
const w = stream.writer;

for (let i = 0; i < payloadSizes.length; i++) {
  const payload = buildPayload(payloadSizes[i], i);
  while (!w.writeSync(payload)) {
    const drainable = w[dp]();
    if (drainable) await drainable;
  }
}
w.endSync();

// Drain the server's empty response to let the stream close cleanly.
// eslint-disable-next-line no-unused-vars
for await (const _ of stream) { /* drain response */ }
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
