// Flags: --experimental-quic --no-warnings

// Test: expired NEW_TOKEN is rejected.
// The server issues a NEW_TOKEN with a short tokenExpiration. After
// the token expires, the client provides it on reconnect. The server
// should reject it (the token is invalid) and fall back to Retry
// flow for address validation.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
import { setTimeout } from 'node:timers/promises';

const { ok, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const sni = { '*': { keys: [key], certs: [cert] } };
const alpn = ['quic-test'];

let savedToken;
const gotToken = Promise.withResolvers();

// Server with a very short token expiration (1 second).
const serverEndpoint = await listen((serverSession) => {
  serverSession.onstream = mustNotCall();
}, {
  sni,
  alpn,
  endpoint: { tokenExpiration: 1 },
});

// First connection: receive the token.
const cs1 = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  onnewtoken: mustCall((token) => {
    savedToken = token;
    gotToken.resolve();
  }),
});
await Promise.all([cs1.opened, gotToken.promise]);
ok(savedToken.length > 0);
await cs1.close();

// Wait for the token to expire.
await setTimeout(1500);

// Second connection with the expired token. The server should reject
// the token and fall back to Retry for address validation. The
// connection should still succeed (Retry is transparent).
const cs2 = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  token: savedToken,
});
await cs2.opened;
// The connection succeeded despite the expired token.
strictEqual(cs2.destroyed, false);
await cs2.close();

await serverEndpoint.close();
