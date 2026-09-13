// Flags: --experimental-quic --no-warnings

// An HTTP/3 session must not cleanly fail if the peer advertises fewer than
// the 3 unidirectional streams that HTTP/3 needs for control and QPACK.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const serverEndpoint = await listen(async (serverSession) => {
  await serverSession.closed;
}, {
  sni: { '*': { keys: [key], certs: [cert] } },
  // No uni streams allowed:
  transportParams: { initialMaxStreamsUni: 0 },
  onheaders: mustNotCall(),
});

// Expect the client to cleanly fail - not crash the process
await assert.rejects(async () => {
  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;
}, { code: 'ERR_QUIC_TRANSPORT_ERROR' });

await serverEndpoint.close();
