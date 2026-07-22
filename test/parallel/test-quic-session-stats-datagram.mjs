// Flags: --experimental-quic --no-warnings

// Test: session datagram stats counters.
// After sending datagrams, the session stats should reflect
// datagramsSent, datagramsReceived, and datagramsAcknowledged.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const allStatusDone = Promise.withResolvers();
const serverGot = Promise.withResolvers();
let statusCount = 0;

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  // Wait for the client to receive all status updates before closing.
  // The server must stay alive long enough to ACK the datagrams.
  await allStatusDone.promise;

  // Server received datagrams.
  ok(serverSession.stats.datagramsReceived > 0n);

  serverSession.close();
  await serverSession.closed;
}), {
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram: mustCall((data) => {
    serverGot.resolve();
  }, 2),
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagramstatus(id, status) {
    if (++statusCount >= 2) allStatusDone.resolve();
  },
});
await clientSession.opened;

// Send two datagrams.
await clientSession.sendDatagram(new Uint8Array([1]));
await clientSession.sendDatagram(new Uint8Array([2]));

await Promise.all([serverGot.promise, allStatusDone.promise]);

// Client sent datagrams.
strictEqual(clientSession.stats.datagramsSent, 2n);
ok(clientSession.stats.datagramsAcknowledged >= 1n);

await clientSession.closed;
await serverEndpoint.close();
