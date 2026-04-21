// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream.onblocked fires when flow control blocks a stream.
// When the peer's stream-level receive window is exhausted, ngtcp2 returns
// NGTCP2_ERR_STREAM_DATA_BLOCKED. The stream is unscheduled and the
// onblocked callback fires. The stream resumes automatically when the peer
// sends MAX_STREAM_DATA to extend the window.
// Strategy: set the body to a buffer larger than the flow control window.
// ngtcp2 sends the initial window then blocks. onblocked fires. Flow
// control extension eventually unblocks and the full transfer completes.

import { hasQuic, skip, mustCall, mustCallAtLeast } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

// quic.stream.blocked fires when a stream is flow-control blocked.
dc.subscribe('quic.stream.blocked', mustCallAtLeast((msg) => {
  ok(msg.stream, 'stream.blocked should include stream');
  ok(msg.session, 'stream.blocked should include session');
}, 1));

const totalSize = 4096;
const body = new Uint8Array(totalSize);
for (let i = 0; i < totalSize; i++) body[i] = i & 0xff;

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    strictEqual(received.byteLength, totalSize);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  // Small stream window — forces stream-level flow control blocking.
  transportParams: { initialMaxStreamDataBidiRemote: 256 },
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();

let blockedCount = 0;
stream.onblocked = mustCallAtLeast(() => {
  blockedCount++;
}, 1);

// Set the body via setBody() — larger than the flow control window.
// ngtcp2 sends the first 256 bytes then returns
// NGTCP2_ERR_STREAM_DATA_BLOCKED, triggering onblocked.
stream.setBody(body);

for await (const _ of stream) { /* drain readable side */ } // eslint-disable-line no-unused-vars
await stream.closed;
await serverDone.promise;

ok(blockedCount > 0, `Expected onblocked to fire, got ${blockedCount} calls`);

await clientSession.close();
await serverEndpoint.close();
