// Flags: --experimental-quic --no-warnings

// Test: diagnostics_channel endpoint busy change.
// quic.endpoint.busy.change fires on endpoint.busy toggle.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';
import * as fixtures from '../common/fixtures.mjs';

const { ok, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, QuicEndpoint } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

let busyChangeCount = 0;
dc.subscribe('quic.endpoint.busy.change', mustCall((msg) => {
  busyChangeCount++;
  ok(msg.endpoint);
  strictEqual(typeof msg.busy, 'boolean');
}, 2));

const endpoint = new QuicEndpoint();
const serverEndpoint = await listen(mustNotCall(), {
  endpoint,
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
});

// Toggle busy on and off.
endpoint.busy = true;
endpoint.busy = false;

strictEqual(busyChangeCount, 2);

await serverEndpoint.close();
