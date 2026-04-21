// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: congestion control algorithm selection.
// Verify that each CC algorithm (reno, cubic, bbr) can be selected
// and that a session completes a data transfer successfully with each.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const payload = encoder.encode('congestion control test');
const payloadLength = payload.byteLength;

for (const cc of ['reno', 'cubic', 'bbr']) {
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const data = await bytes(stream);
      strictEqual(data.byteLength, payloadLength);
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }), { cc });

  const clientSession = await connect(serverEndpoint.address, { cc });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    body: encoder.encode('congestion control test'),
  });

  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await Promise.all([stream.closed, serverDone.promise]);

  // Verify the session stats show congestion control was active.
  ok(clientSession.stats.cwnd > 0n, `${cc}: cwnd should be > 0`);

  await clientSession.closed;
  await serverEndpoint.close();
}
