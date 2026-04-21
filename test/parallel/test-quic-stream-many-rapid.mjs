// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: many streams opened and closed rapidly.
// Open 50 bidirectional streams in rapid succession, each with a
// small body. All streams should close successfully and the server
// should receive all data.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const streamCount = 50;
const encoder = new TextEncoder();
let serverReceived = 0;
const allReceived = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const data = await bytes(stream);
    strictEqual(data.byteLength, 5);
    stream.writer.endSync();
    await stream.closed;
    if (++serverReceived === streamCount) {
      serverSession.close();
      allReceived.resolve();
    }
  }, streamCount);
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Open 50 streams rapidly, each with a small body.
const streams = [];
for (let i = 0; i < streamCount; i++) {
  const stream = await clientSession.createBidirectionalStream({
    body: encoder.encode('hello'),
  });
  streams.push(stream);
}

// Wait for all client streams to close.
await Promise.all(streams.map(async (stream) => {
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}));

await allReceived.promise;
await clientSession.closed;
await serverEndpoint.close();
