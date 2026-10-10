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

// Server who allows no unidirectional streams
const serverEndpoint = await listen(async (serverSession) => {
  await serverSession.closed;
}, {
  alpn: ['h3'],
  sni: { '*': { keys: [key], certs: [cert] } },
  // No uni streams allowed:
  transportParams: { initialMaxStreamsUni: 0 },
  onheaders: mustNotCall(),
});

// Expect an autostart client to cleanly fail
await assert.rejects(async () => {
  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'h3',
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;
}, { code: 'ERR_QUIC_TRANSPORT_ERROR' });

await serverEndpoint.close();

// Expect manual session start to cleanly fail:
{
  const endpoint = await listen(async (serverSession) => {
    await serverSession.closed.catch(() => {});
  }, {
    alpn: ['h3'],
    sni: { '*': { keys: [key], certs: [cert] } },
    transportParams: { initialMaxStreamsUni: 0 },
  });
  const connection = await connect(endpoint.address, {
    alpn: 'h3',
    autoStart: false,
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await connection.opened;
  const failed = {
    code: 'ERR_INVALID_STATE',
    message: /could not be started/,
  };
  assert.throws(() => Http3Session.start(connection), failed);
  await assert.rejects(connection.closed, { code: 'ERR_QUIC_TRANSPORT_ERROR' });
  await endpoint.close();
}
