// Flags: --experimental-quic --no-warnings --expose-internals

// Test: client-created unidirectional streams on an HTTP/3 connection, which
// are not valid HTTP/3 request streams, are handled internally by nghttp3.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { createPrivateKey } = await import('node:crypto');
const { listen, connect, QuicSession } = await import('node:quic');
const { createRequire } = await import('node:module');
const require = createRequire(import.meta.url);
const { createApplicationStream } = require('internal/quic/quic');
const { internalBinding } = require('internal/test/binding');
const { STREAM_DIRECTION_UNIDIRECTIONAL } = internalBinding('quic');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// A stream of an unknown type, here from a client speaking raw QUIC over the h3
// ALPN, is ignored: never exposed as a request, and the connection stays up.
{
  const endpoint = await listen(mustCall((session) => {
    session.onstream = mustNotCall();
  }), {
    alpn: ['h3'],
    sni: { '*': { keys: [key], certs: [cert] } },
  });
  const connection = await connect(endpoint.address, {
    alpn: 'h3',
    autoStart: false,
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  const client = QuicSession.start(connection);

  const stream = await client.createUnidirectionalStream();
  // 0x21 is a reserved stream type (RFC 9114 section 6.2.3).
  stream.writer.writeSync(new Uint8Array([0x21, 0x61, 0x62]));
  stream.writer.endSync();
  // Resolves once the server has acknowledged all of it:
  await stream.closed;

  await client.close();
  await endpoint.close();
}

// Regression test for https://github.com/nodejs/node/issues/65408.
// A raw unidirectional stream created on an HTTP/3 connection is never sent,
// so it stays open, and must not crash during process teardown.
const endpoint = await listen(mustNotCall(), {
  alpn: ['h3'],
  sni: { '*': { keys: [key], certs: [cert] } },
});

const session = await connect(endpoint.address, {
  alpn: 'h3',
  servername: 'localhost',
  verifyPeer: 'manual',
});

const stream = await createApplicationStream(
  session.connection, STREAM_DIRECTION_UNIDIRECTIONAL);
stream.writer.writeSync('x');

endpoint.destroy();
await endpoint.closed;
