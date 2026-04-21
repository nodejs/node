// Flags: --experimental-quic --no-warnings

// Test: Create two listening endpoints, one secondary and one
// preferred. Initiate a connection with the secondary, with
// preferred advertised. Client should ignore the preferred
// address and continue on with the original

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const allStatusDone = Promise.withResolvers();
const serverGot = Promise.withResolvers();
let statusCount = 0;

const handleSession = mustCall(async (serverSession) => {
  await allStatusDone.promise;
  await serverGot.promise;
  await serverSession.close();
});

const sessionOptions = {
  ondatagram: mustCall((data) => {
    serverGot.resolve();
  }, 4),
  onpathvalidation: mustNotCall(),
};

const preferredEndpoint = await listen(handleSession, sessionOptions);
const serverEndpoint = await listen(handleSession, {
  ...sessionOptions,
  transportParams: {
    preferredAddressIpv4: preferredEndpoint.address,
  }
});

const clientSession = await connect(serverEndpoint.address, {
  // We don't want this endpoint to reuse either of the two listening endpoints.
  reuseEndpoint: false,
  preferredAddressPolicy: 'ignore',
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagramstatus: mustCall((id, status) => {
    if (++statusCount >= 4) allStatusDone.resolve();
  }, 4),
  onpathvalidation: mustNotCall(),
});
await clientSession.opened;

// Send datagrams.
await clientSession.sendDatagram(new Uint8Array([1]));
await clientSession.sendDatagram(new Uint8Array([2]));
await clientSession.sendDatagram(new Uint8Array([3]));
await clientSession.sendDatagram(new Uint8Array([4]));

await Promise.all([serverGot.promise, allStatusDone.promise]);

strictEqual(clientSession.stats.datagramsSent, 4n);
ok(clientSession.stats.datagramsAcknowledged >= 1n);

await clientSession.closed;
await serverEndpoint.close();
await preferredEndpoint.close();
