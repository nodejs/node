// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { partialDeepStrictEqual, strictEqual, ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

// Import after the hasQuic check
const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const check = {
  // The SNI value
  servername: 'localhost',
  // The selected ALPN protocol
  protocol: 'quic-test',
  // The negotiated cipher suite
  cipher: 'TLS_AES_128_GCM_SHA256',
  cipherVersion: 'TLSv1.3',
  // No session ticket provided, so early data was not attempted
  earlyDataAttempted: false,
  earlyDataAccepted: false,
};

// The opened promise should resolve when the handshake is complete.

const serverOpened = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.opened.then((info) => {
    partialDeepStrictEqual(info, check);
    serverOpened.resolve();
    serverSession.close();
  }).then(mustCall());
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
});

// Buffer is not detached.
strictEqual(cert.buffer.detached, false);

// The server must have an address to connect to after listen resolves.
ok(serverEndpoint.address !== undefined);

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
});

const info = await clientSession.opened;
partialDeepStrictEqual(info, check);

await serverOpened.promise;
clientSession.close();
