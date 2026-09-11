// Flags: --experimental-quic --no-warnings

// Regression test for https://github.com/nodejs/node/issues/65408.
// A client-created unidirectional stream is not a valid HTTP/3 request
// stream, and data arriving on one could crash during process teardown.
// HTTP/3 frames its own streams, so the QUIC session now refuses to open
// streams directly at all, which puts that state out of reach.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { createPrivateKey } = await import('node:crypto');
const { listen, connect, Http3Session } = await import('node:quic');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const endpoint = await listen(mustNotCall(), {
  alpn: ['h3'],
  sni: { '*': { keys: [key], certs: [cert] } },
});

const session = new Http3Session(await connect(endpoint.address, {
  alpn: 'h3',
  servername: 'localhost',
  verifyPeer: 'manual',
}));

const refused = {
  code: 'ERR_INVALID_STATE',
  message: /Raw QUIC streams cannot be created/,
};
await assert.rejects(session.quicSession.createUnidirectionalStream(), refused);
await assert.rejects(session.quicSession.createBidirectionalStream(), refused);

endpoint.destroy();
await endpoint.closed;
