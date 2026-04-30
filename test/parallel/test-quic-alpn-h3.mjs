// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, notStrictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

// Test h3 ALPN negotiation with Http3ApplicationImpl.
// Both server and client use the default ALPN (h3).

const serverOpened = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  const info = await serverSession.opened;
  strictEqual(info.protocol, 'h3');
  serverOpened.resolve();
  serverSession.close();
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
});

notStrictEqual(serverEndpoint.address, undefined);

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
});

async function checkClient() {
  const info = await clientSession.opened;
  strictEqual(info.protocol, 'h3');
}

await Promise.all([serverOpened.promise, checkClient()]);
clientSession.close();
