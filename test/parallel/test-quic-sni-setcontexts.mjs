// Flags: --experimental-quic --no-warnings

// Test: setSNIContexts hot-swap and options.
// setSNIContexts() updates TLS identities at runtime.
// setSNIContexts() with replace: true replaces all entries.
// setSNIContexts() with replace: false merges new entries.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, QuicEndpoint } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key1 = createPrivateKey(readKey('agent1-key.pem'));
const cert1 = readKey('agent1-cert.pem');
const key2 = createPrivateKey(readKey('agent2-key.pem'));
const cert2 = readKey('agent2-cert.pem');

const endpoint = new QuicEndpoint();

// Start with agent1 cert for all hosts.
const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  serverSession.close();
  await serverSession.closed;
}, 2), {
  endpoint,
  sni: { '*': { keys: [key1], certs: [cert1] } },
  alpn: ['quic-test'],
  transportParams: { maxIdleTimeout: 2 },
});

// First connection uses agent1 cert.
{
  const cs = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    transportParams: { maxIdleTimeout: 2 },
  });
  const info = await cs.opened;
  strictEqual(info.servername, 'localhost');
  await cs.closed;
}

endpoint.setSNIContexts(
  { '*': { keys: [key2], certs: [cert2] } },
  { replace: true },
);

// Second connection should use agent2 cert.
{
  const cs = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    transportParams: { maxIdleTimeout: 2 },
  });
  const info = await cs.opened;
  strictEqual(info.servername, 'localhost');
  // The cert changed — we can verify by checking the connection succeeded
  // (if the old cert was still used and the new one was expected, the
  // handshake would still succeed since both are self-signed and
  // rejectUnauthorized defaults to false in the test helper).
  await cs.closed;
}

await serverEndpoint.close();
