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

// The token option must be an ArrayBufferView if provided
await assert.rejects(connect({ port: 1234 }, {
  alpn: 'quic-test',
  token: 'not-a-buffer',
}), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// After a successful handshake, the server automatically sends a
// NEW_TOKEN frame. The client should receive it and make it
// available via session.token.

const serverOpened = Promise.withResolvers();
const clientToken = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.opened.then(mustCall((info) => {
    serverOpened.resolve();
    // Don't close immediately — give time for NEW_TOKEN to be sent
  }));
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  servername: 'localhost',
});

await clientSession.opened;
await serverOpened.promise;

// Wait briefly for the NEW_TOKEN frame to arrive. The server submits
// it during handshake confirmation, but it may take an additional
// packet exchange to reach the client.
const checkToken = () => {
  if (clientSession.token !== undefined) {
    clientToken.resolve();
  } else {
    setTimeout(checkToken, 10);
  }
};
checkToken();

await clientToken.promise;

const { token, address } = clientSession.token;
assert.ok(Buffer.isBuffer(token), 'token should be a Buffer');
assert.ok(token.length > 0, 'token should not be empty');
assert.ok(address !== undefined, 'address should be defined');

clientSession.close();
