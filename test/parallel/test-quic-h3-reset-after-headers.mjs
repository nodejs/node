// Flags: --experimental-quic --no-warnings

// Test: an HTTP/3 server that abandons a stream right after submitting
// response headers delivers RESET_STREAM to the client, and the session
// keeps working. The headers are still queued in nghttp3 when the stream
// is destroyed. If nghttp3 later tries to write them, reading the body of
// the removed stream fails and the whole session closes silently.

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

// RFC 9114 H3_REQUEST_CANCELLED.
const H3_REQUEST_CANCELLED = 0x10cn;

const cases = [
  ['resetStream() then destroy()', (stream) => {
    stream.resetStream(H3_REQUEST_CANCELLED);
    stream.destroy();
  }],
  ['destroy() with a code', (stream) => {
    stream.onerror = () => {};
    stream.destroy(new Error('abandoned'), { code: H3_REQUEST_CANCELLED });
  }],
];

function request(session, path) {
  return session.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': path,
      ':scheme': 'https',
      ':authority': 'localhost',
    },
  });
}

for (const [label, abandon] of cases) {
  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onerror = () => {};
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders: mustCall(function(headers) {
      // Submit response headers, then abandon the first request in the
      // same tick, before nghttp3 has written the headers.
      this.sendHeaders({ ':status': '200' }, {
        terminal: headers[':path'] !== '/abandon',
      });
      if (headers[':path'] === '/abandon') abandon(this);
    }, 2),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  // The first request is abandoned with H3_REQUEST_CANCELLED.
  const abandoned = await request(clientSession, '/abandon');
  abandoned.onerror = () => {};
  const reset = Promise.withResolvers();
  abandoned.onreset = mustCall((error) => reset.resolve(error));
  const error = await reset.promise;
  assert.strictEqual(error?.errorCode, H3_REQUEST_CANCELLED, label);

  // The session still works: a second request gets a response.
  const ok = await request(clientSession, '/ok');
  const status = Promise.withResolvers();
  ok.onheaders = mustCall((headers) => status.resolve(headers[':status']));
  assert.strictEqual(await status.promise, 200, label);
  await ok.closed;

  await clientSession.close();
  await serverEndpoint.close();
}
