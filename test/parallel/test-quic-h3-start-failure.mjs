// Flags: --experimental-quic --no-warnings

// Test: HTTP/3 can't start when the peer allows fewer than the three
// unidirectional streams it needs for its control and QPACK streams. That
// must close the session, rather than leave it running without HTTP/3.

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
const headers = {
  ':method': 'GET',
  ':path': '/',
  ':scheme': 'https',
  ':authority': 'localhost',
};
const internalError = {
  code: 'ERR_QUIC_TRANSPORT_ERROR',
  message: /INTERNAL_ERROR/,
};

async function attachToLowUniServer() {
  const endpoint = await listen(mustCall((quicSession) => {
    quicSession.onerror = () => {};
  }), {
    alpn: ['h3'],
    sni: { '*': { keys: [key], certs: [cert] } },
    transportParams: { initialMaxStreamsUni: 2 },
  });
  const client = await connect(endpoint.address, {
    alpn: 'h3',
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  return { endpoint, client };
}

// Nothing opened: the session closes when the handshake completes.
{
  const { endpoint, client } = await attachToLowUniServer();
  await assert.rejects(client.closed, internalError);
  await endpoint.close();
}

// A request opened as soon as the session opens fails, and the session
// closes rather than accepting further requests.
{
  const { endpoint, client } = await attachToLowUniServer();
  client.onerror = mustCall((err) => {
    assert.strictEqual(err.code, internalError.code);
  });
  await client.opened;
  await assert.rejects(client.createBidirectionalStream({ headers }),
                       { code: 'ERR_QUIC_OPEN_STREAM_FAILED' });
  await assert.rejects(client.closed, internalError);
  await assert.rejects(client.createBidirectionalStream({ headers }),
                       { code: 'ERR_INVALID_STATE' });
  await endpoint.close();
}
