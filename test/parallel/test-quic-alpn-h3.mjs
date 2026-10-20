// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// Test h3 ALPN negotiation with Http3ApplicationImpl.
// Both server and client use the default ALPN (h3).

const serverOpened = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  const info = await serverSession.opened;
  assert.strictEqual(info.protocol, 'h3');
  serverOpened.resolve();
  serverSession.close();
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
});

assert.notStrictEqual(serverEndpoint.address, undefined);

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});

async function checkClient() {
  const info = await clientSession.opened;
  assert.strictEqual(info.protocol, 'h3');
}

await Promise.all([serverOpened.promise, checkClient()]);
await clientSession.close();
await serverEndpoint.close();
