// Flags: --experimental-quic --no-warnings

// Regression test for https://github.com/nodejs/node/issues/65408.
// A client-created unidirectional stream is not a valid HTTP/3 request stream,
// but nghttp3 handles it internally. Destroying the endpoint after receiving
// data on that stream must not crash during process teardown.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { createPrivateKey } = await import('node:crypto');
const { listen, connect } = await import('node:quic');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const endpoint = await listen(mustNotCall(), {
  sni: { '*': { keys: [key], certs: [cert] } },
});

const session = await connect(endpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});

const stream = await session.createUnidirectionalStream();
stream.writer.writeSync('x');

endpoint.destroy();
await endpoint.closed;
