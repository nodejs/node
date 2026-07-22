// Flags: --experimental-quic --no-warnings

// Test: client reuses NEW_TOKEN on reconnect.
// The server sends a NEW_TOKEN after handshake. The client saves it
// and provides it in the token option on a subsequent connection to
// the same server. The second connection should succeed.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

let savedToken;
const gotToken = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall((stream) => {
    stream.writer.endSync();
    serverSession.close();
  });
}, 2), {
  onerror() {},
});

// First connection: receive the token.
const cs1 = await connect(serverEndpoint.address, {
  onnewtoken: mustCall((token) => {
    ok(Buffer.isBuffer(token));
    ok(token.length > 0);
    savedToken = token;
    gotToken.resolve();
  }),
});
await Promise.all([cs1.opened, gotToken.promise]);

// Signal the server to close this session.
const s1 = await cs1.createBidirectionalStream();
s1.writer.endSync();
for await (const _ of s1) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([s1.closed, cs1.closed]);

// Second connection: reuse the token. The connection should succeed.
const cs2 = await connect(serverEndpoint.address, {
  token: savedToken,
});
await cs2.opened;

// Verify data transfer works on the second connection.
const s2 = await cs2.createBidirectionalStream();
s2.writer.endSync();
for await (const _ of s2) { /* drain */ } // eslint-disable-line no-unused-vars
await s2.closed;

// Close from the client side.
await cs2.close();
await serverEndpoint.close();
