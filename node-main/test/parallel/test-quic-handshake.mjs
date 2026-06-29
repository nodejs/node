// Flags: --experimental-quic --no-warnings

import { hasQuic, skip } from '../common/index.mjs';
import { ok, partialDeepStrictEqual } from 'node:assert';
import { readKey } from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

// Import after the hasQuic check
const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const keys = createPrivateKey(readKey('agent1-key.pem'));
const certs = readKey('agent1-cert.pem');

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

const serverEndpoint = await listen(async (serverSession) => {
  const info = await serverSession.opened;
  partialDeepStrictEqual(info, check);
  serverOpened.resolve();
  serverSession.close();
}, { keys, certs });

// The server must have an address to connect to after listen resolves.
ok(serverEndpoint.address !== undefined);

const clientSession = await connect(serverEndpoint.address);
clientSession.opened.then((info) => {
  partialDeepStrictEqual(info, check);
  clientOpened.resolve();
});

await Promise.all([serverOpened.promise, clientOpened.promise]);
clientSession.close();
