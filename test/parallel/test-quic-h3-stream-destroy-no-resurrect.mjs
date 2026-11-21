// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 request streams are not resurrected after being destroyed
// with response DATA still in flight.
//
// This is the HTTP/3 counterpart to test-quic-stream-destroy-no-resurrect.
// The HTTP/3 receive path finds or creates a Stream for each nghttp3
// callback, so it has to make the same distinction: a locally-initiated
// request stream that is no longer tracked was destroyed by the application
// and must not be recreated when the remaining DATA frames arrive.
//
// It also has to keep crediting flow control for the payload it discards.
// nghttp3 hands DATA payload to the application uncredited (it is excluded
// from the framing bytes credited when the stream data is read), so silently
// dropping it would permanently shrink the session's shared receive window --
// which is why this test runs enough requests to outlast a small window.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { makePayload } = await import('../common/quic.mjs');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const kResponseSize = 256 * 1024;
const kRequests = 12;
// Deliberately smaller than the aggregate discarded payload, so the run only
// completes if discarded DATA is still credited back.
const kConnWindow = 256 * 1024;

const responseBody = makePayload(kResponseSize, 17);

assert.ok(kResponseSize * kRequests > kConnWindow * 4,
          'aggregate response data must far exceed the connection window');

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall((stream) => {
    // The client destroys these early; the truncated write is expected.
    stream.onerror = () => {};
  }, kRequests);
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  onheaders: mustCall(function() {
    this.sendHeaders({ ':status': '200' });
    this.setBody(responseBody);
  }, kRequests),
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
  transportParams: {
    initialMaxData: kConnWindow,
    initialMaxStreamDataBidiLocal: kResponseSize * 2,
  },
});

// The client opens every stream itself; the server opens none. Any onstream
// here is a destroyed request stream being resurrected and misreported as
// peer-initiated.
clientSession.onstream = mustNotCall(
  'client must not receive onstream for its own destroyed request streams');

const info = await clientSession.opened;
assert.strictEqual(info.protocol, 'h3');

for (let i = 0; i < kRequests; i++) {
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': `/${i}`,
      ':scheme': 'https',
      ':authority': 'localhost',
    },
  });

  // Take one batch of the response, then abandon the rest and destroy, so
  // DATA frames keep arriving for a stream we no longer track.
  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream) break;

  stream.destroy();
}

// Exactly one locally-opened stream per request.
assert.strictEqual(Number(clientSession.stats.bidiOutStreamCount), kRequests);

await clientSession.close();
await serverEndpoint.close();
