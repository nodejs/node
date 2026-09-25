// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: initialRtt session option is accepted and the session functions
// correctly with a custom initial RTT estimate.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const payload = encoder.encode('hello rtt');
const serverDone = Promise.withResolvers();

// Use a low initialRtt (1ms) to simulate a low-latency environment.
// The session should complete successfully and the smoothed RTT in
// stats should converge to a value well below the default 333ms.
const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const data = await bytes(stream);
    assert.ok(data.byteLength > 0);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  initialRtt: 1,  // 1ms
});

const clientSession = await connect(serverEndpoint.address, {
  initialRtt: 1,  // 1ms
});
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: payload,
});

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await stream.closed;
await serverDone.promise;

// After data exchange, the smoothed RTT should have converged to a
// realistic value. On loopback it should be well under 10ms (10,000,000ns).
// The stat is in nanoseconds.
const smoothedRtt = clientSession.stats.smoothedRtt;
assert.ok(smoothedRtt > 0n, 'smoothedRtt should be non-zero after data exchange');
assert.ok(smoothedRtt < 10_000_000n,
          `smoothedRtt should be under 10ms on loopback, got ${smoothedRtt}ns`);

await clientSession.close();
await serverEndpoint.close();
