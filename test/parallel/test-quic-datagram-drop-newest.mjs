// Flags: --experimental-quic --no-warnings

// Test: datagram drop-newest policy.
// With maxPendingDatagrams=2 and drop-newest, sending 5 datagrams
// rapidly should reject the newest when the queue is full. The
// server should receive the oldest datagrams.

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

const allReceived = Promise.withResolvers();
const allStatusReceived = Promise.withResolvers();

let serverCounter = 0;
let clientAbandonCounter = 0;
let clientAckCounter = 0;

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await Promise.all([serverSession.opened, allStatusReceived.promise]);
  serverSession.close();
  await serverSession.closed;
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram: mustCall(function(data, early) {
    // We whould only receive datagrams 1 and 2
    strictEqual(data.length, 1);
    ok(data[0] === 0 || data[0] === 1);
    ok(!early);
    if (++serverCounter === 2) allReceived.resolve();
  }, 2),
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  transportParams: { maxDatagramFrameSize: 1200 },
  datagramDropPolicy: 'drop-newest',
  ondatagramstatus: mustCall((_, status) => {
    if (status === 'abandoned') {
      clientAbandonCounter++;
    } else if (status === 'acknowledged') {
      clientAckCounter++;
    }
    if (clientAbandonCounter + clientAckCounter === 5) {
      allStatusReceived.resolve();
    }
  }, 5),
});

await clientSession.opened;

clientSession.maxPendingDatagrams = 2;

// Send 5 datagrams. With drop-newest, the 3rd/4th/5th are rejected
// (the queue holds the 1st and 2nd).
for (let i = 0; i < 5; i++) {
  await clientSession.sendDatagram(new Uint8Array([i]));
}

await Promise.all([allReceived.promise, allStatusReceived.promise]);

// 3 abandoned (datagrams 1, 2, 3) and 2 acknowledged (datagrams 4, 5).
strictEqual(clientAbandonCounter, 3);
strictEqual(clientAckCounter, 2);

await clientSession.closed;
await serverEndpoint.close();
