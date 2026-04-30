// Flags: --experimental-quic --no-warnings

// Test: basic datagram send and receive.
// Client sends datagram, server receives via ondatagram.
// maxDatagramSize reflects peer's transport param.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { ok, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const serverGot = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  // maxDatagramSize reflects peer's max payload (frame size
  // minus DATAGRAM frame overhead of type byte + varint length).
  ok(serverSession.maxDatagramSize > 0);
  ok(serverSession.maxDatagramSize < 1200);
  // Wait for the datagram before closing.
  await serverGot.promise;
  await serverSession.close();
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram: mustCall((data) => {
    ok(data instanceof Uint8Array);
    strictEqual(data.byteLength, 3);
    serverGot.resolve();
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  transportParams: { maxDatagramFrameSize: 1200 },
});

await clientSession.opened;

// Client maxDatagramSize reflects actual payload max.
ok(clientSession.maxDatagramSize > 0);
ok(clientSession.maxDatagramSize < 1200);

// Client sends datagram.
const id = await clientSession.sendDatagram(new Uint8Array([1, 2, 3]));
assert.strictEqual(id, 1n);

await Promise.all([serverGot.promise, clientSession.closed]);
await serverEndpoint.close();
