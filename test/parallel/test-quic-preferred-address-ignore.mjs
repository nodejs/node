// Flags: --experimental-quic --no-warnings

// Test: preferred address ignored by client.
// Server advertises a preferred address, but the client is configured
// with preferredAddressPolicy: 'ignore'. No path validation should
// occur and all data stays on the original path.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const allStatusDone = Promise.withResolvers();
const serverGot = Promise.withResolvers();
let statusCount = 0;

const preferredEndpoint = await listen(mustNotCall());

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await allStatusDone.promise;
  await serverGot.promise;
  await serverSession.close();
}), {
  transportParams: {
    preferredAddressIpv4: preferredEndpoint.address,
  },
  ondatagram: mustCall(() => {
    serverGot.resolve();
  }, 2),
});

const clientSession = await connect(serverEndpoint.address, {
  reuseEndpoint: false,
  preferredAddressPolicy: 'ignore',
  transportParams: { maxDatagramFrameSize: 1200 },
  // Path validation should NOT fire when ignoring preferred address.
  onpathvalidation: mustNotCall(),
  ondatagramstatus: mustCall((id, status) => {
    if (++statusCount >= 2) allStatusDone.resolve();
  }, 2),
});
await clientSession.opened;

await clientSession.sendDatagram(new Uint8Array([1]));
await clientSession.sendDatagram(new Uint8Array([2]));

await Promise.all([serverGot.promise, allStatusDone.promise]);

strictEqual(clientSession.stats.datagramsSent, 2n);
ok(clientSession.stats.datagramsAcknowledged >= 1n);

await clientSession.closed;
await serverEndpoint.close();
await preferredEndpoint.close();
