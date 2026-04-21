// Flags: --experimental-quic --no-warnings

// Test: ERR_QUIC_ENDPOINT_CLOSED on bind failure.
// Attempting to listen on a port that's already in use by another
// QUIC endpoint produces ERR_QUIC_ENDPOINT_CLOSED with a
// 'Bind failure' context. The listen() call may return an endpoint
// that is immediately destroyed — the error surfaces via the
// endpoint's closed promise.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, ok, rejects } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const sni = { '*': { keys: [key], certs: [cert] } };
const alpn = ['quic-test'];

// Bind first endpoint to get an assigned port.
const ep1 = await listen(mustNotCall(), { sni, alpn });
const { port } = ep1.address;
ok(port > 0);

// Attempt to listen on the same port — should fail with bind error.
// listen() returns an endpoint that is immediately destroyed.
const ep2 = await listen(mustNotCall(), {
  sni,
  alpn,
  endpoint: { address: `127.0.0.1:${port}` },
});
strictEqual(ep2.destroyed, true);

// The bind failure surfaces as a rejected closed promise.
await rejects(ep2.closed, {
  code: 'ERR_QUIC_ENDPOINT_CLOSED',
  message: /Bind failure/,
});

await ep1.close();
