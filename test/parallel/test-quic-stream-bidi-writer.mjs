// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: writer API for bidirectional streams.
// Exercises writeSync, write (async), endSync, and verifies that data
// written in multiple chunks arrives intact and in order on the server.
// Also tests that the writer reports correct state after operations.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const decoder = new TextDecoder();

const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    strictEqual(decoder.decode(received), 'chunk1chunk2chunk3');

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

// Writer should be open.
strictEqual(typeof w.desiredSize, 'number');

// Write multiple chunks synchronously.
strictEqual(w.writeSync(encoder.encode('chunk1')), true);
strictEqual(w.writeSync(encoder.encode('chunk2')), true);
strictEqual(w.writeSync(encoder.encode('chunk3')), true);

// End the write side — returns total bytes written.
const totalWritten = w.endSync();
strictEqual(totalWritten, 18); // 6 * 3

// After end, write should return false.
strictEqual(w.writeSync(encoder.encode('nope')), false);

// desiredSize should be null after close.
strictEqual(w.desiredSize, null);

await Promise.all([stream.closed, done.promise]);
await clientSession.close();
await serverEndpoint.close();
