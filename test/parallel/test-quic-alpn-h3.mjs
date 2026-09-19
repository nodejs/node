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

// Negotiating the h3 ALPN does not itself activate HTTP/3. The ALPN is
// reported as usual, but the session keeps the default application unless it
// has an Http3Session attached.

const serverOpened = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  assert.strictEqual(serverSession.alpnProtocol, 'h3');
  const info = await serverSession.opened;
  assert.strictEqual(info.protocol, 'h3');
  serverOpened.resolve();
}), {
  alpn: ['h3'],
  sni: { '*': { keys: [key], certs: [cert] } },
});

assert.notStrictEqual(serverEndpoint.address, undefined);

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'h3',
  servername: 'localhost',
  verifyPeer: 'manual',
});

const info = await clientSession.opened;
assert.strictEqual(info.protocol, 'h3');
await serverOpened.promise;

// Not an HTTP/3 request stream, so it cannot carry headers.
const stream = await clientSession.createBidirectionalStream();
assert.throws(() => stream.sendHeaders({ ':status': '200' }), {
  code: 'ERR_INVALID_STATE',
  message: /does not support headers/,
});

clientSession.destroy();
await serverEndpoint.close();
