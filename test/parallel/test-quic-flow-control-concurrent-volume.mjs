// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: many concurrent streams contending for one constrained
// connection-level flow control window, at volume.
//
// The connection-level window (initialMaxData) is shared by every stream on
// the session, while each stream also has its own window. When several
// streams are all trying to move data through a connection window smaller
// than any single stream's payload, the connection-level credit has to be
// recycled continuously and shared across them.
//
// This is the configuration most likely to expose credit accounting errors:
// per-stream bugs that cancel out on a single stream become visible when
// several streams draw on the same pool, and any net under-crediting
// deadlocks every stream at once rather than just slowing one down.
//
// This exercises the raw QUIC application: the test helpers negotiate the
// 'quic-test' ALPN, not HTTP/3. See test-quic-h3-flow-control-volume.mjs for
// the HTTP/3 equivalent.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, makePayload, hashBytes } =
  await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const kStreams = 8;
const kPerStream = 512 * 1024;          // 4 MB total
const kStreamWindow = 16 * 1024;
const kConnWindow = 32 * 1024;          // Shared by all 8 streams

const kTotal = kStreams * kPerStream;

assert.ok(kConnWindow < kPerStream,
          'connection window must be smaller than a single stream payload');
assert.ok(kTotal / kConnWindow >= 100,
          'aggregate payload must require >=100 connection window refills');

// Distinct payload per stream so a cross-stream mixup is detectable, not
// just a wrong byte total.
const payloads = [];
const hashes = [];
for (let i = 0; i < kStreams; i++) {
  const p = makePayload(kPerStream, i + 1);
  payloads.push(p);
  hashes.push(hashBytes(p));
}

// Map each received payload back to the stream that sent it. The server has
// no ordering guarantee across streams, so match by hash rather than by
// arrival order.
const remaining = new Set(hashes);
const serverDone = Promise.withResolvers();
let completed = 0;

// Track how many streams are being read at the same time. The whole premise
// of this test is contention for the shared connection window, so if the
// streams end up serialized the test still passes but proves much less.
// Assert the overlap explicitly rather than trusting it.
let openStreams = 0;
let maxOpenStreams = 0;

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    openStreams++;
    maxOpenStreams = Math.max(maxOpenStreams, openStreams);

    const received = await bytes(stream);
    openStreams--;
    assert.strictEqual(received.byteLength, kPerStream);

    const h = hashBytes(received);
    // Each payload must arrive exactly once, intact. This catches data from
    // one stream being credited or delivered onto another.
    assert.ok(remaining.has(h),
              'received payload did not match any expected stream payload');
    remaining.delete(h);

    stream.writer.endSync();
    await stream.closed;
    if (++completed === kStreams) {
      serverSession.close();
      serverDone.resolve();
    }
  }, kStreams);
}), {
  transportParams: {
    initialMaxStreamDataBidiRemote: kStreamWindow,
    initialMaxData: kConnWindow,
    // Allow all streams to be open at once; the point is contention.
    initialMaxStreamsBidi: kStreams,
  },
  maxStreamWindow: kStreamWindow,
  maxWindow: kConnWindow,
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Start every stream before draining any of them, so they genuinely contend
// for the shared connection window rather than running one after another.
const streams = [];
for (let i = 0; i < kStreams; i++) {
  const stream = await clientSession.createBidirectionalStream();
  stream.setBody(payloads[i]);
  streams.push(stream);
}

await Promise.all(streams.map(async (stream) => {
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}));

await serverDone.promise;
// Every stream's payload must have arrived exactly once.
assert.strictEqual(remaining.size, 0);
// All streams are created before any is drained, so all of them should be
// open simultaneously. Anything less means they serialized and the window
// contention this test exists to exercise did not actually happen.
assert.strictEqual(maxOpenStreams, kStreams,
                   'all streams must be open simultaneously to contend for ' +
                   'the shared connection window');

await clientSession.close();
await serverEndpoint.close();
