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

// Server offers multiple ALPNs. Client requests one that the server supports.
// Verify the negotiated protocol matches on both sides.

const serverOpened = Promise.withResolvers();

async function checkSession(session) {
  const info = await session.opened;
  // The client should negotiate proto-b (the only protocol it requested)
  assert.strictEqual(info.protocol, 'proto-b');
}

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await checkSession(serverSession);
  serverOpened.resolve();
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['proto-a', 'proto-b', 'proto-c'],
});

assert.notStrictEqual(serverEndpoint.address, undefined);

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'proto-b',
  servername: 'localhost',
  verifyPeer: 'manual',
});

await Promise.all([serverOpened.promise, checkSession(clientSession)]);
await clientSession.close();
await serverEndpoint.close();
