// Flags: --experimental-quic --no-warnings

import { hasQuic, hasIPv6, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

if (!hasIPv6) {
  skip('IPv6 is not supported');
}

// Import after the hasQuic check
const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const keys = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const certs = fixtures.readKey('agent1-cert.pem');

const check = {
  // The SNI value
  servername: 'localhost',
  // The selected ALPN protocol
  protocol: 'h3',
  // The negotiated cipher suite
  cipher: 'TLS_AES_128_GCM_SHA256',
  cipherVersion: 'TLSv1.3',
};

// The opened promise should resolve when the handshake is complete.

const serverOpened = Promise.withResolvers();
const clientOpened = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.opened.then((info) => {
    assert.partialDeepStrictEqual(info, check);
    serverOpened.resolve();
    serverSession.close();
  }).then(mustCall());
}), { keys, certs, endpoint: {
  address: {
    address: '::1',
    family: 'ipv6',
  },
  ipv6Only: true,
} });
// Buffer is not detached.
assert.strictEqual(certs.buffer.detached, false);

// The server must have an address to connect to after listen resolves.
assert.ok(serverEndpoint.address !== undefined);

const clientSession = await connect(serverEndpoint.address, {
  endpoint: {
    address: {
      address: '::',
      family: 'ipv6',
    },
  }
});
clientSession.opened.then((info) => {
  assert.partialDeepStrictEqual(info, check);
  clientOpened.resolve();
}).then(mustCall());

await Promise.all([serverOpened.promise, clientOpened.promise]);
clientSession.close();
