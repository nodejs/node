// Flags: --experimental-quic --no-warnings
// Test: client-initiated unidirectional stream with no onstream
// The client creates a uni stream with no onstream.
// Verify that this causes no crash.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';

const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { createPrivateKey } = await import('node:crypto');
const { listen, connect } = await import('../common/quic.mjs');

const serverKey = createPrivateKey(readKey('agent1-key.pem'));
const serverCert = readKey('agent1-cert.pem');

const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (session) => {
  await session.opened;
  done.resolve();
}), {
  sni: { '*': { keys: [serverKey], certs: [serverCert] } },
  alpn: ['repro'],
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  alpn: 'repro',
  ca: serverCert,
});

await clientSession.opened;

await clientSession.createUnidirectionalStream({
  body: 'something something darkside',
});

await done.promise;

await clientSession.close();

await serverEndpoint.close();
