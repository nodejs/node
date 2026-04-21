// Flags: --experimental-quic

// Test: datagram drop-oldest policy.
// With maxPendingDatagrams=2 and drop-oldest, sending 5 datagrams
// rapidly should drop the oldest when the queue overflows. The
// server should receive the most recent datagrams (4th and 5th).

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
  await serverSession.close();
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram: mustCall(function(data, early) {
    // With drop-oldest, the queue keeps the newest. After 5 sends with
    // queue size 2, only datagrams 4 and 5 (values 3 and 4) remain.
    strictEqual(data.length, 1);
    ok(data[0] === 3 || data[0] === 4);
    ok(!early);
    if (++serverCounter === 2) allReceived.resolve();
  }, 2),
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  transportParams: { maxDatagramFrameSize: 1200 },
  datagramDropPolicy: 'drop-oldest',
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

// Send 5 datagrams. With drop-oldest and queue size 2:
// 1 queued, 2 queued, 3 arrives → 1 dropped, 4 arrives → 2 dropped,
// 5 arrives → 3 dropped. Queue ends with [4, 5].
for (let i = 0; i < 5; i++) {
  await clientSession.sendDatagram(new Uint8Array([i]));
}

await Promise.all([allReceived.promise, allStatusReceived.promise]);

// 3 abandoned (datagrams 1, 2, 3) and 2 acknowledged (datagrams 4, 5).
strictEqual(clientAbandonCounter, 3);
strictEqual(clientAckCounter, 2);

await clientSession.closed;
await serverEndpoint.close();
