// Flags: --experimental-quic --no-warnings

// Test: tokenSecret cross-endpoint token validation.
// Two server endpoints with the same tokenSecret should accept each
// other's NEW_TOKEN tokens. A token from a server with a different
// tokenSecret should be rejected (falls back to Retry).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { ok, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey, randomBytes } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const sni = { '*': { keys: [key], certs: [cert] } };
const alpn = ['quic-test'];

const sharedSecret = randomBytes(16);

let savedToken;
const gotToken = Promise.withResolvers();

// First server with shared tokenSecret.
const ep1 = await listen(async (serverSession) => {
  await serverSession.closed;
}, {
  sni,
  alpn,
  endpoint: { tokenSecret: sharedSecret },
});

// Get a token from the first server.
const cs1 = await connect(ep1.address, {
  alpn: 'quic-test',
  onnewtoken: mustCall((token) => {
    savedToken = token;
    gotToken.resolve();
  }),
});
await Promise.all([cs1.opened, gotToken.promise]);
ok(savedToken.length > 0);
await cs1.close();
await ep1.close();

// Second server with the SAME tokenSecret. The token from ep1
// should be accepted, allowing the connection to skip Retry.
const ep2 = await listen(async (serverSession) => {
  await serverSession.closed;
}, {
  sni,
  alpn,
  endpoint: { tokenSecret: sharedSecret },
});

const cs2 = await connect(ep2.address, {
  alpn: 'quic-test',
  token: savedToken,
});
await cs2.opened;
strictEqual(cs2.destroyed, false);
await cs2.close();
await ep2.close();

// Third server with a DIFFERENT tokenSecret. The token from ep1
// should be rejected. The connection still succeeds (Retry fallback).
const ep3 = await listen(async (serverSession) => {
  await serverSession.closed;
}, {
  sni,
  alpn,
  endpoint: { tokenSecret: randomBytes(16) },
});

const cs3 = await connect(ep3.address, {
  alpn: 'quic-test',
  token: savedToken,
});
await cs3.opened;
strictEqual(cs3.destroyed, false);
await cs3.close();
await ep3.close();
