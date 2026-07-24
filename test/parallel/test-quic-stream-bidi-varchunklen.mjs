// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: bidirectional data transfer with varying chunk sizes.
// This is a regression test for a stall caused by a mismatch between
// writeSync (which rejects when chunk > writeDesiredSize) and
// drainableProtocol (which returned null when writeDesiredSize > 0).
// When chunks don't evenly fill the high water mark, writeDesiredSize
// can be positive but smaller than the next chunk, causing the
// while(!writeSync) { dp(); await } loop to spin without yielding.
// See: https://github.com/nodejs/node/issues/63216

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes, drainableProtocol: dp } = await import('stream/iter');

// Varying chunk sizes — the pattern of alternating large and small
// chunks is effective at triggering the writeDesiredSize gap.
const chunkSizes = [60000, 12, 50000, 1600, 20000, 30000, 0, 100];
const numChunks = chunkSizes.length;
const byteLength = chunkSizes.reduce((a, b) => a + b, 0);

// Build a deterministic payload so we can verify integrity.
function buildChunk(index) {
  const chunk = new Uint8Array(chunkSizes[index]);
  const val = index & 0xff;
  for (let i = 0; i < chunkSizes[index]; i++) {
    chunk[i] = (val + i) & 0xff;
  }
  return chunk;
}

function checksum(data) {
  let sum = 0;
  for (let i = 0; i < data.byteLength; i++) {
    sum = (sum + data[i]) | 0;
  }
  return sum;
}

// Compute expected checksum.
let expectedChecksum = 0;
for (let i = 0; i < numChunks; i++) {
  const chunk = buildChunk(i);
  expectedChecksum = (expectedChecksum + checksum(chunk)) | 0;
}

const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    strictEqual(received.byteLength, byteLength);
    strictEqual(checksum(received), expectedChecksum);

    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    done.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
const w = stream.writer;

// Write chunks, respecting backpressure via drainableProtocol.
for (let i = 0; i < numChunks; i++) {
  const chunk = buildChunk(i);
  while (!w.writeSync(chunk)) {
    // Flow controlled — wait for drain before retrying.
    const drainable = w[dp]();
    if (drainable) await drainable;
  }
}

const totalWritten = w.endSync();
strictEqual(totalWritten, byteLength);

await Promise.all([stream.closed, done.promise]);
await clientSession.close();
await serverEndpoint.close();
