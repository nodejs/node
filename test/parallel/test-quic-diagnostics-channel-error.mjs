// Flags: --experimental-quic --no-warnings

// Test: diagnostics_channel endpoint error event.
// quic.endpoint.error fires on endpoint error.
// Trigger a bind failure (port conflict) and verify the channel fires.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';
import * as fixtures from '../common/fixtures.mjs';

const { readKey } = fixtures;

const { ok, rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const sni = { '*': { keys: [key], certs: [cert] } };
const alpn = ['quic-test'];

dc.subscribe('quic.endpoint.error', mustCall((msg) => {
  ok(msg.endpoint);
  ok(msg.error);
}));

// Create first endpoint to occupy a port.
const ep1 = await listen(mustNotCall(), { sni, alpn });
const { port } = ep1.address;

// Create second endpoint on the same port — triggers bind error.
const ep2 = await listen(mustNotCall(), {
  sni,
  alpn,
  endpoint: { address: `127.0.0.1:${port}` },
});

// ep2 is destroyed due to bind failure.
strictEqual(ep2.destroyed, true);
await rejects(ep2.closed, {
  code: 'ERR_QUIC_ENDPOINT_CLOSED',
  message: /Bind failure/,
});
await ep1.close();
