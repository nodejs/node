// Flags: --experimental-quic --no-warnings

// An HTTP/3 session must cleanly fail if the peer advertises fewer than
// the 3 unidirectional streams that HTTP/3 needs for control and QPACK.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, Http3Session } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const serverEndpoint = await listen(async (serverSession) => {
  await serverSession.closed;
}, {
  alpn: ['h3'],
  sni: { '*': { keys: [key], certs: [cert] } },
  // No uni streams allowed:
  transportParams: { initialMaxStreamsUni: 0 },
  onheaders: mustNotCall(),
});

const clientSession = new Http3Session(await connect(serverEndpoint.address, {
  alpn: 'h3',
  servername: 'localhost',
  verifyPeer: 'manual',
}));

// Expect the client to cleanly fail & close - not crash the process.
await assert.rejects(clientSession.closed,
                     { code: 'ERR_QUIC_TRANSPORT_ERROR' });

await serverEndpoint.close();
