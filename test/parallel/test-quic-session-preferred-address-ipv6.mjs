// Flags: --experimental-quic --no-warnings

// Test: Create two listening ipv6 endpoints, one secondary and one
// preferred. Initiate a connection with the secondary, with
// preferred advertised. Client should automatically migrate
// to the preferred address without interupting data flow.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { ok, strictEqual, notStrictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { createPrivateKey } = await import('node:crypto');

const allStatusDone = Promise.withResolvers();
const serverGot = Promise.withResolvers();
const serverPathValidated = Promise.withResolvers();
let statusCount = 0;

const handleSession = mustCall(async (serverSession) => {
  await allStatusDone.promise;
  await serverGot.promise;
  await serverSession.close();
});

function assertEqualAddress(addr1, addr2) {
  strictEqual(addr1.address, addr2.address);
  strictEqual(addr1.port, addr2.port);
  strictEqual(addr1.family, addr2.family);
}

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const sessionOptions = {
  ondatagram: mustCall((data) => {
    serverGot.resolve();
  }, 4),
  onpathvalidation: mustCall((result, newLocal, newRemote, oldLocal, oldRemote, preferred) => {
    // The status here can be 'success' or 'aborted' depending on timing.
    // The 'aborted' status only means that path validation is no longer
    // necessary for a number of reasons (usually ngtcp2 received a non-probing
    // packet on the new path).
    notStrictEqual(result, 'failure');
    assertEqualAddress(newLocal, preferredEndpoint.address);
    assertEqualAddress(oldLocal, serverEndpoint.address);
    assertEqualAddress(newRemote, oldRemote);
    // The preferred arg is only passed on client side
    strictEqual(preferred, undefined);
    serverPathValidated.resolve();
  }),
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
  endpoint: {
    address: {
      address: '::1',
      family: 'ipv6',
    },
    ipv6Only: true,
  },
};

const preferredEndpoint = await listen(handleSession, sessionOptions);
const serverEndpoint = await listen(handleSession, {
  ...sessionOptions,
  transportParams: {
    preferredAddressIpv6: preferredEndpoint.address,
  }
});

console.log(preferredEndpoint.address);
console.log(serverEndpoint.address);

const clientSession = await connect(serverEndpoint.address, {
  // We don't want this endpoint to reuse either of the two listening endpoints.
  reuseEndpoint: false,
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagramstatus: mustCall((id, status) => {
    if (++statusCount >= 4) allStatusDone.resolve();
  }, 4),
  onpathvalidation: mustCall((result, newLocal, newRemote, oldLocal, oldRemote, preferred) => {
    strictEqual(result, 'success');
    assertEqualAddress(newLocal, clientSession.endpoint.address);
    assertEqualAddress(newRemote, preferredEndpoint.address);
    strictEqual(oldLocal, null);
    strictEqual(oldRemote, null);
    strictEqual(preferred, true);
  }),
  endpoint: {
    address: {
      address: '::',
      family: 'ipv6',
    },
  },
});
await clientSession.opened;

// Send two datagrams.
await clientSession.sendDatagram(new Uint8Array([1]));
await clientSession.sendDatagram(new Uint8Array([2]));

await serverPathValidated.promise;

// Send more datagrams after the preferred address migration completes
// To show that data is still flowing after we close the original
// endpoint.
await clientSession.sendDatagram(new Uint8Array([3]));
await clientSession.sendDatagram(new Uint8Array([4]));

await Promise.all([serverGot.promise, allStatusDone.promise]);

strictEqual(clientSession.stats.datagramsSent, 4n);
ok(clientSession.stats.datagramsAcknowledged >= 1n);

await clientSession.closed;
await serverEndpoint.close();
await preferredEndpoint.close();
