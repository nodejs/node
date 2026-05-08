// Flags: --experimental-dtls --no-warnings

// Test: ALPN negotiation in DTLS.

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
const { readKey } = fixtures;

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

const serverCert = readKey('agent1-cert.pem');
const serverKey = readKey('agent1-key.pem');
const ca = readKey('ca1-cert.pem');

const serverAlpnChecked = Promise.withResolvers();

const endpoint = listen(mustCall(async (session) => {
  session.onmessage = () => {};
  await session.opened;
  // Server should see the negotiated ALPN protocol.
  strictEqual(session.alpnProtocol, 'coap');
  serverAlpnChecked.resolve();
}), {
  cert: serverCert.toString(),
  key: serverKey.toString(),
  port: 0,
  host: '127.0.0.1',
  alpn: ['coap', 'h2'],
});

const session = connect('127.0.0.1', endpoint.address.port, {
  ca: [ca.toString()],
  rejectUnauthorized: false,
  alpn: ['coap'],
});

await session.opened;

// Client should see the negotiated protocol.
strictEqual(session.alpnProtocol, 'coap');

await serverAlpnChecked.promise;

await session.close();
await endpoint.close();
