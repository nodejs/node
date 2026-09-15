// Flags: --experimental-dtls --no-warnings

// Test: DTLSSession properties after handshake.

import { hasCrypto, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

const serverCert = fixtures.readKey('agent1-cert.pem');
const serverKey = fixtures.readKey('agent1-key.pem');
const ca = fixtures.readKey('ca1-cert.pem');

const endpoint = listen(mustCall((session) => {
  session.onmessage = mustNotCall();
}), {
  cert: serverCert.toString(),
  key: serverKey.toString(),
  port: 0,
  host: '127.0.0.1',
});

const session = connect('127.0.0.1', endpoint.address.port, {
  ca: [ca.toString()],
  rejectUnauthorized: false,
});

await session.opened;

// Protocol should be DTLSv1.2.
assert.match(session.protocol, /DTLS/i);

// Cipher should be an object with name, standardName, version.
const cipher = session.cipher;
assert.strictEqual(typeof cipher?.name, 'string');
assert.strictEqual(typeof cipher?.standardName, 'string');
assert.strictEqual(typeof cipher?.version, 'string');

// Remote address should be defined.
const addr = session.remoteAddress;
assert.ok(addr);

// Peer certificate should be available (PEM string).
const peerCert = session.peerCertificate;
assert.ok(peerCert);
assert.ok(peerCert.includes('BEGIN CERTIFICATE'));

// State should reflect open connection.
assert.ok(session.state);

await session.close();
await endpoint.close();
