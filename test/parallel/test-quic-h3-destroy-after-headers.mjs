// Flags: --experimental-quic --no-warnings

// Test: an HTTP/3 server that submits non-terminal response headers and then
// destroys the stream without an error code still delivers the headers and
// ends the stream cleanly, and the session keeps working. Nothing resets the
// QUIC stream here, so nghttp3 must flush the queued headers and send FIN
// after the stream is removed on the JS side.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

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

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onerror = () => {};
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  onheaders: mustCall(function(headers) {
    const abandon = headers[':path'] === '/abandon';
    this.sendHeaders({ ':status': '200' }, { terminal: !abandon });
    if (abandon) {
      // Destroy in the same tick, before nghttp3 has written the headers,
      // and without an error code, so no RESET_STREAM is sent.
      this.onerror = () => {};
      this.destroy();
    }
  }, 2),
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});
await clientSession.opened;

// The abandoned request still gets its headers and a clean end of stream.
const abandoned = await request(clientSession, '/abandon');
abandoned.onreset = mustNotCall();
const abandonedStatus = Promise.withResolvers();
abandoned.onheaders = mustCall((headers) => {
  abandonedStatus.resolve(headers[':status']);
});
assert.strictEqual(await abandonedStatus.promise, 200);
let bytes = 0;
for await (const chunks of abandoned) {
  for (const chunk of chunks) bytes += chunk.byteLength;
}
assert.strictEqual(bytes, 0);
await abandoned.closed;

// The session still works: a second request gets a response.
const ok = await request(clientSession, '/ok');
const status = Promise.withResolvers();
ok.onheaders = mustCall((headers) => status.resolve(headers[':status']));
assert.strictEqual(await status.promise, 200);
await ok.closed;

await clientSession.close();
await serverEndpoint.close();
