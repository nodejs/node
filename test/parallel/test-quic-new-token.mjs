// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { ok, rejects } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

// The token option must be an ArrayBufferView if provided
await rejects(connect({ port: 1234 }, {
  alpn: 'quic-test',
  token: 'not-a-buffer',
}), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// After a successful handshake, the server automatically sends a
// NEW_TOKEN frame. The client should receive it via the onnewtoken
// callback set at connection time.

const clientToken = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.opened.then(mustCall());
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  servername: 'localhost',
  // Set onnewtoken at connection time to avoid missing the event.
  onnewtoken: mustCall(function(token, address) {
    ok(Buffer.isBuffer(token), 'token should be a Buffer');
    ok(token.length > 0, 'token should not be empty');
    ok(address !== undefined, 'address should be defined');
    clientToken.resolve();
  }),
});

await Promise.all([clientSession.opened, clientToken.promise]);

clientSession.close();
