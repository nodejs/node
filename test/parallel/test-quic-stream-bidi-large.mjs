// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: large bidirectional data transfer with backpressure.
// The client sends >1MB of data using the writer API, exercising the
// QUIC flow control path. The server reads all data and verifies the
// total byte count and a checksum. This tests that backpressure is
// correctly applied and released across the full transfer.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes, drainableProtocol: dp } = await import('stream/iter');

// 1.5 MB payload — large enough to trigger flow control.
const totalSize = 1.5 * 1024 * 1024;
const chunkSize = 16 * 1024;
const numChunks = Math.ceil(totalSize / chunkSize);

// Build a deterministic payload so we can verify integrity.
function buildChunk(index) {
  const chunk = new Uint8Array(chunkSize);
  // Fill with a pattern derived from the chunk index.
  const val = index & 0xff;
  for (let i = 0; i < chunkSize; i++) {
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
    strictEqual(received.byteLength, numChunks * chunkSize);
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
strictEqual(totalWritten, numChunks * chunkSize);

await Promise.all([stream.closed, done.promise]);
await clientSession.close();
await serverEndpoint.close();
