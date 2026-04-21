// Flags: --experimental-quic --no-warnings

// Test: ondatagramstatus callback.
// After sending a datagram, the ondatagramstatus callback fires
// with the datagram ID and either 'acknowledged' or 'lost'.

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
const statusReceived = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await Promise.all([serverSession.opened, serverGot.promise]);
  // Give a moment for the ACK to propagate to the client so the
  // ondatagramstatus callback fires before the session closes.
  await setTimeout(100);
  await serverSession.close();
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram: mustCall(function() {
    serverGot.resolve();
  }),
});

let statusId;
let statusValue;

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagramstatus: mustCall((id, status) => {
    strictEqual(typeof id, 'bigint');
    strictEqual(typeof status, 'string');
    ok(
      status === 'acknowledged' || status === 'lost' || status === 'abandoned',
      `status should be 'acknowledged', 'lost', or 'abandoned', got '${status}'`,
    );

    statusId = id;
    statusValue = status;
    statusReceived.resolve();
  }),
});

await clientSession.opened;
const id = await clientSession.sendDatagram(new Uint8Array([1, 2, 3]));

// Wait for the server to receive and the status callback to fire.
await Promise.all([serverGot.promise, statusReceived.promise]);

// The status callback should have been called with the same ID.
strictEqual(statusId, id);
// On localhost the datagram should be acknowledged, not lost.
strictEqual(statusValue, 'acknowledged');

await clientSession.closed;

await serverEndpoint.close();
