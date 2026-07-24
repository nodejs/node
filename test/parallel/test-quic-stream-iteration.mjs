// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream iteration basics.
// All use a single endpoint with one stream each to avoid the
// sequential endpoint bug.
// for-await yields Uint8Array[] batches.
// text() consumes stream as string.
// bytes() consumes stream as Uint8Array.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual, deepStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes, text } = await import('stream/iter');

const encoder = new TextEncoder();
const message = 'iteration test data';
const expected = encoder.encode(message);

let streamCount = 0;
const allDone = Promise.withResolvers();
const totalStreams = 3;

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const idx = streamCount++;

    if (idx === 0) {
      // for-await yields batches.
      const chunks = [];
      for await (const batch of stream) {
        ok(Array.isArray(batch), 'batch should be an array');
        for (const chunk of batch) {
          ok(chunk instanceof Uint8Array, 'chunk should be Uint8Array');
          chunks.push(chunk);
        }
      }
      ok(chunks.length > 0);
    } else if (idx === 1) {
      // text() consumes as string.
      const result = await text(stream);
      strictEqual(typeof result, 'string');
      strictEqual(result, message);
    } else if (idx === 2) {
      // bytes() consumes as Uint8Array.
      const result = await bytes(stream);
      ok(result instanceof Uint8Array);
      deepStrictEqual(result, expected);
    }

    stream.writer.endSync();
    await stream.closed;

    if (streamCount === totalStreams) {
      serverSession.close();
      allDone.resolve();
    }
  }, totalStreams);
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Send three streams sequentially.
for (let i = 0; i < totalStreams; i++) {
  const stream = await clientSession.createBidirectionalStream({
    body: encoder.encode(message),
  });
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

await allDone.promise;
await clientSession.close();
await serverEndpoint.close();
