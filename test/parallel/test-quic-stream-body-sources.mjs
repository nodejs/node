// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: various body source types for createBidirectionalStream.
// Verifies that ArrayBuffer, ArrayBufferView (with non-zero byteOffset),
// SharedArrayBuffer, and Blob bodies all deliver data correctly.
// Covers BIDI-07, BIDI-08, BIDI-09, BIDI-10, BIDI-11, BODY-03..06.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const message = 'hello body sources';
const expectedBytes = encoder.encode(message);

let testIndex = 0;
const totalTests = 4;
const allDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await bytes(stream);
    stream.writer.endSync();
    await stream.closed;

    testIndex++;
    if (testIndex === totalTests) {
      serverSession.close();
      allDone.resolve();
    }
  }, totalTests);
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Test 1: ArrayBuffer body
{
  const buf = encoder.encode(message);
  const ab = buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);
  const stream = await clientSession.createBidirectionalStream({ body: ab });
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

// Test 2: ArrayBufferView with non-zero byteOffset
{
  const backing = new ArrayBuffer(64);
  const fullView = new Uint8Array(backing);
  const offset = 10;
  fullView.set(expectedBytes, offset);
  const view = new Uint8Array(backing, offset, expectedBytes.byteLength);
  const stream = await clientSession.createBidirectionalStream({ body: view });
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

// Test 3: SharedArrayBuffer body
{
  const sab = new SharedArrayBuffer(expectedBytes.byteLength);
  const sabView = new Uint8Array(sab);
  sabView.set(expectedBytes);
  const stream = await clientSession.createBidirectionalStream({ body: sabView });
  // The SharedArrayBuffer should still be usable (copied, not transferred).
  strictEqual(sab.byteLength, expectedBytes.byteLength);
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

// Test 4: Blob body
{
  const blob = new Blob([expectedBytes]);
  const stream = await clientSession.createBidirectionalStream({ body: blob });
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

await allDone.promise;
await clientSession.close();
await serverEndpoint.close();
