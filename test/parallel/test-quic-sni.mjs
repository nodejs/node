// Flags: --experimental-quic --no-warnings

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

// Use two different keys/certs for the default and SNI host.
const defaultKey = createPrivateKey(readKey('agent1-key.pem'));
const defaultCert = readKey('agent1-cert.pem');
const sniKey = createPrivateKey(readKey('agent2-key.pem'));
const sniCert = readKey('agent2-cert.pem');

// Server with SNI: default ('*') uses agent1, 'localhost' uses agent2.
const serverEndpoint = await listen(mustCall(async (serverSession) => {
  const info = await serverSession.opened;
  // The server should see the client's requested servername.
  strictEqual(info.servername, 'localhost');
  await serverSession.close();
}), {
  sni: {
    '*': { keys: [defaultKey], certs: [defaultCert] },
    'localhost': { keys: [sniKey], certs: [sniCert] },
  },
  alpn: ['quic-test'],
});

ok(serverEndpoint.address !== undefined);

// Client connects with servername 'localhost' — should match the SNI entry.
const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  alpn: 'quic-test',
});
const clientInfo = await clientSession.opened;
strictEqual(clientInfo.servername, 'localhost');

await clientSession.close();
await serverEndpoint.close();
