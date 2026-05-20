// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: detailed session stats.
// RTT fields populated after data transfer.
// cwnd, bytesInFlight populated under load.
// V2 fields (pktSent, pktReceived, etc.) populated.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await bytes(stream);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Send enough data to generate meaningful stats.
const data = new Uint8Array(8192);
const stream = await clientSession.createBidirectionalStream();
stream.setBody(data);

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);

const stats = clientSession.stats;

// RTT fields populated.
ok(stats.smoothedRtt >= 0n, 'smoothedRtt should be >= 0');
ok(stats.latestRtt >= 0n, 'latestRtt should be >= 0');
ok(stats.minRtt >= 0n, 'minRtt should be >= 0');
strictEqual(typeof stats.rttVar, 'bigint');

// Congestion fields.
ok(stats.cwnd > 0n, 'cwnd should be > 0');
strictEqual(typeof stats.bytesInFlight, 'bigint');
strictEqual(typeof stats.ssthresh, 'bigint');

// V2 packet/byte fields.
ok(stats.pktSent > 0n, 'pktSent should be > 0');
ok(stats.pktRecv > 0n, 'pktRecv should be > 0');
strictEqual(typeof stats.pktLost, 'bigint');
ok(stats.bytesSent > 0n, 'bytesSent should be > 0');
ok(stats.bytesRecv > 0n, 'bytesRecv should be > 0');
strictEqual(typeof stats.bytesLost, 'bigint');

await clientSession.closed;
await serverEndpoint.close();
