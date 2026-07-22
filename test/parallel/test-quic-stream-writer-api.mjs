// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: writer API methods (WRIT-02, WRIT-03, WRIT-04, WRIT-06,
// WRIT-07, WRIT-08, WRIT-12, WRIT-15).
// Uses a single endpoint with multiple streams, one per test.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { ok, rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();

const totalStreams = 5;
const serverResults = [];
const allDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    serverResults.push(received);
    stream.writer.endSync();
    await stream.closed;

    if (serverResults.length === totalStreams) {
      serverSession.close();
      allDone.resolve();
    }
  }, totalStreams);
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// write() async
{
  const stream = await clientSession.createBidirectionalStream();
  const w = stream.writer;
  await w.write(encoder.encode('async write'));
  const n = w.endSync();
  strictEqual(n, 11);
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

// writevSync() vectored write
{
  const stream = await clientSession.createBidirectionalStream();
  const w = stream.writer;
  const result = w.writevSync([
    encoder.encode('hello '),
    encoder.encode('writev'),
  ]);
  strictEqual(result, true);
  const n = w.endSync();
  strictEqual(n, 12);
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

// writev() async vectored write
{
  const stream = await clientSession.createBidirectionalStream();
  const w = stream.writer;
  await w.writev([
    encoder.encode('async '),
    encoder.encode('writev'),
  ]);
  const n = w.endSync();
  strictEqual(n, 12);
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

// end() async close
{
  const stream = await clientSession.createBidirectionalStream();
  const w = stream.writer;
  w.writeSync(encoder.encode('end async'));
  const n = await w.end();
  strictEqual(n, 9);
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

{
  const stream = await clientSession.createBidirectionalStream();
  const w = stream.writer;
  // desiredSize should be a number (may be 0 initially before flow
  // control window opens, or > 0 if the window is already open).
  strictEqual(typeof w.desiredSize, 'number');
  ok(w.desiredSize >= 0, `desiredSize should be >= 0, got ${w.desiredSize}`);
  // drainableProtocol should return null when desiredSize > 0 (has capacity),
  // or a promise when desiredSize <= 0 (backpressured). Either way, it
  // should not throw.
  const { drainableProtocol: dp } = await import('stream/iter');
  const drain = w[dp]();
  ok(drain === null || drain instanceof Promise);
  w.writeSync(encoder.encode('capacity'));
  w.endSync();
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

// Return null when errored.
{
  const stream = await clientSession.createBidirectionalStream();
  const w = stream.writer;
  const testError = new Error('writer fail test');
  w.fail(testError);
  // After fail, desiredSize is null.
  strictEqual(w.desiredSize, null);
  // drainableProtocol returns null when errored.
  const { drainableProtocol: dp } = await import('stream/iter');
  strictEqual(w[dp](), null);
  // endSync after fail returns -1 (errored).
  strictEqual(w.endSync(), -1);
  // WriteSync after fail returns false.
  strictEqual(w.writeSync(encoder.encode('x')), false);
  // Write after fail throws with the original error.
  await rejects(w.write(encoder.encode('x')), testError);
  // Don't await stream.closed here — the reset stream may not trigger
  // server onstream (no data was sent before fail), so the server
  // won't count it. The stream is cleaned up when the session closes.
}

await allDone.promise;
await clientSession.close();
await serverEndpoint.close();

// Verify server received the right data.
const decoder = new TextDecoder();
strictEqual(decoder.decode(serverResults[0]), 'async write');
strictEqual(decoder.decode(serverResults[1]), 'hello writev');
strictEqual(decoder.decode(serverResults[2]), 'async writev');
strictEqual(decoder.decode(serverResults[3]), 'end async');
strictEqual(decoder.decode(serverResults[4]), 'capacity');
