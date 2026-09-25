// Flags: --experimental-quic --no-warnings

// An incoming HTTP/3 request stream that is rejected without any
// application processing (here, the session has no stream consumer) is
// reset with H3_REQUEST_REJECTED (0x10b) so the peer learns the request
// was not processed. See RFC 9114 section 4.1.1.
// Refs: https://github.com/nodejs/node/issues/65441

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

// RFC 9114 H3_REQUEST_REJECTED.
const H3_REQUEST_REJECTED = 0x10bn;

// The server registers no stream consumer, so an incoming request stream
// is rejected on arrival.
const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onerror = () => {};
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});
await clientSession.opened;

const reset = Promise.withResolvers();
const stream = await clientSession.createBidirectionalStream({
  headers: {
    ':method': 'GET',
    ':path': '/test',
    ':scheme': 'https',
    ':authority': 'localhost',
  },
});
stream.onreset = mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_QUIC_APPLICATION_ERROR');
  assert.strictEqual(err.errorCode, H3_REQUEST_REJECTED);
  reset.resolve();
});
await assert.rejects(stream.closed, { code: 'ERR_QUIC_APPLICATION_ERROR' });

await reset.promise;
await clientSession.close();
await serverEndpoint.close();
