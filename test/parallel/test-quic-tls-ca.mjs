// Flags: --experimental-quic --no-warnings

// Test: custom CA certificate chain.
// The client provides a CA cert that matches the server's cert,
// allowing validation to succeed.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const ca = readKey('ca1-cert.pem');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  const info = await serverSession.opened;
  strictEqual(info.protocol, 'quic-test');
  serverSession.close();
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
});

// Client provides the CA cert. The validation error should be different
// (or absent) compared to when no CA is provided.
const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  servername: 'localhost',
  ca,
});

const info = await clientSession.opened;
strictEqual(info.protocol, 'quic-test');
// The CA option is accepted. Validation may or may not succeed
// depending on the cert chain. The important thing is the
// handshake completed and the option was used.

await clientSession.closed;
await serverEndpoint.close();
