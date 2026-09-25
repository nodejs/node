// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: a slow consumer keeps buffered data bounded by the flow control
// window, over a transfer far larger than that window.
//
// The existing slow-consumer test only checks that a small transfer still
// completes and that onblocked fired. The stronger property -- that the
// receiver never buffers more than the window allows, no matter how far
// behind the consumer falls -- needs volume to be meaningful, because it is
// the *repeated* refusal to over-credit that keeps memory bounded.
//
// Credit for inbound data is only returned once the consumer actually reads
// it, so if the receiver ever credited data it had merely buffered, the
// sender would be free to run arbitrarily far ahead and buffered bytes would
// grow without bound. Asserting a ceiling on maxBytesAccumulated over a
// multi-megabyte transfer is a direct check that this does not happen.

import { hasQuic, skip, mustCall, mustCallAtLeast } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, makePayload, hashBytes } =
  await import('../common/quic.mjs');
const { setImmediate: yieldToLoop } = await import('node:timers/promises');

const kTotal = 4 * 1024 * 1024;
const kStreamWindow = 16 * 1024;
const kConnWindow = 32 * 1024;

// Yield to the event loop every few reads. This lets the sender run as far
// ahead as flow control permits without adding wall-clock delay to the test.
const kYieldEvery = 4;

assert.ok(kTotal / kStreamWindow >= 100,
          'payload must be far larger than the window to be meaningful');

const payload = makePayload(kTotal, 5);
const expectedHash = hashBytes(payload);

const serverDone = Promise.withResolvers();
let peakAccumulated = 0;
let received = 0;
const parts = [];

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    let reads = 0;
    for await (const chunks of stream) {
      for (const chunk of chunks) {
        received += chunk.byteLength;
        parts.push(chunk);
      }

      // maxBytesAccumulated is the high water mark of data that has been
      // received but not yet consumed.
      const accumulated = Number(stream.stats.maxBytesAccumulated);
      if (accumulated > peakAccumulated) peakAccumulated = accumulated;

      if (++reads % kYieldEvery === 0) await yieldToLoop();
    }

    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  transportParams: {
    initialMaxStreamDataBidiRemote: kStreamWindow,
    initialMaxData: kConnWindow,
  },
  maxStreamWindow: kStreamWindow,
  maxWindow: kConnWindow,
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
// The sender must actually hit the window, otherwise the consumer was not
// slow relative to the sender and the ceiling below proves nothing.
stream.onblocked = mustCallAtLeast(1);
stream.setBody(payload);

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);

// Integrity first: bounded buffering is only interesting if no data was lost.
assert.strictEqual(received, kTotal);
const assembled = new Uint8Array(received);
let offset = 0;
for (const part of parts) {
  assembled.set(part, offset);
  offset += part.byteLength;
}
assert.strictEqual(hashBytes(assembled), expectedHash);

// Upper bound: buffered-but-unread data never exceeded the window, even
// though the payload was 256x the window. This is the property that keeps
// receiver memory bounded regardless of consumer speed.
assert.ok(peakAccumulated <= kStreamWindow,
          `buffered data (${peakAccumulated}) must stay within the flow ` +
          `control window (${kStreamWindow})`);

// Lower bound: the buffer genuinely filled up. Without this the ceiling
// above could be satisfied trivially by a consumer that kept pace with the
// sender, which would not test the bound at all.
assert.ok(peakAccumulated > kStreamWindow / 2,
          `consumer was not slow enough to exercise the bound, peak ` +
          `buffered was only ${peakAccumulated}`);

await clientSession.close();
await serverEndpoint.close();
