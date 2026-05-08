// Flags: --experimental-dtls --no-warnings

// Test: Symbol.asyncDispose for DTLSEndpoint and DTLSSession.

import { hasCrypto, skip, mustCall, mustNotCall } from '../common/index.mjs';
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

// Test that Symbol.asyncDispose exists.
strictEqual(typeof session[Symbol.asyncDispose], 'function');
strictEqual(typeof endpoint[Symbol.asyncDispose], 'function');

// Dispose the session.
await session[Symbol.asyncDispose]();

// Dispose the endpoint.
await endpoint[Symbol.asyncDispose]();
