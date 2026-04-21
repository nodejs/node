// Flags: --experimental-quic --no-warnings

// Test: datagram server-to-client and echo round-trip
// Server sends datagram, client receives via ondatagram.
// Datagram echo — client sends, server echoes back in
//           its ondatagram callback (queued, flushed via SendPendingData).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { setTimeout } = await import('node:timers/promises');

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
const clientGot = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await Promise.all([serverSession.opened, serverGot.promise]);
  // Give time for the echo to be sent before closing.
  await setTimeout(100);
  await serverSession.close();
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
  transportParams: { maxDatagramFrameSize: 10 },
  // Server echoes received datagram data back to client.
  // The sendDatagram call happens inside ondatagram (ngtcp2 callback
  // scope). The datagram is queued and flushed by SendPendingData.
  ondatagram: mustCall((data, early, session) => {
    ok(data instanceof Uint8Array);
    ok(!early);
    session.sendDatagram(data);
    serverGot.resolve();
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  transportParams: { maxDatagramFrameSize: 10 },
  // Client receives datagram from server.
  ondatagram: mustCall(function(data) {
    ok(data instanceof Uint8Array);
    strictEqual(data.byteLength, 3);
    strictEqual(data[0], 10);
    strictEqual(data[1], 20);
    strictEqual(data[2], 30);
    clientGot.resolve();
  }),
});

await clientSession.opened;

// Client sends datagram to trigger the echo.
await clientSession.sendDatagram(new Uint8Array([10, 20, 30]));

await Promise.all([serverGot.promise, clientGot.promise, clientSession.closed]);

await serverEndpoint.close();
