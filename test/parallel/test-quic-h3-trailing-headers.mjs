// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 trailing headers.
// Server sends response headers, body data, then trailing headers.
// Client receives all three in order.
// Verifies:
// - onwanttrailers callback fires after body is sent
// - sendTrailers delivers trailing headers to the peer
// - ontrailers callback fires on the receiving side with kind 'trailing'
// - stream.headers still returns the initial headers (not trailers)

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const encoder = new TextEncoder();
const decoder = new TextDecoder();
const responseBody = 'body with trailers';

// quic.stream.headers fires when initial headers are received.
// Fires for both the server (request headers) and client (response headers).
dc.subscribe('quic.stream.headers', mustCall((msg) => {
  ok(msg.stream, 'stream.headers should include stream');
  ok(msg.session, 'stream.headers should include session');
  ok(msg.headers, 'stream.headers should include headers');
}, 2));

// quic.stream.trailers fires when trailing headers are received.
dc.subscribe('quic.stream.trailers', mustCall((msg) => {
  ok(msg.stream, 'stream.trailers should include stream');
  ok(msg.session, 'stream.trailers should include session');
  ok(msg.trailers, 'stream.trailers should include trailers');
  strictEqual(msg.trailers['x-checksum'], 'abc123');
}));

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  onheaders: mustCall(function(headers) {
    // Send response headers.
    this.sendHeaders({
      ':status': '200',
      'content-type': 'text/plain',
    });

    // Write body and close.
    const w = this.writer;
    w.writeSync(encoder.encode(responseBody));
    w.endSync();
  }),
  // Fires after the body is fully sent (EOF + NO_END_STREAM).
  // The server provides trailing headers here.
  onwanttrailers: mustCall(function() {
    this.sendTrailers({
      'x-checksum': 'abc123',
      'x-request-id': '42',
    });
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
});
await clientSession.opened;

const clientHeadersReceived = Promise.withResolvers();
const clientTrailersReceived = Promise.withResolvers();

const stream = await clientSession.createBidirectionalStream({
  headers: {
    ':method': 'GET',
    ':path': '/with-trailers',
    ':scheme': 'https',
    ':authority': 'localhost',
  },
  onheaders: mustCall(function(headers) {
    strictEqual(headers[':status'], '200');
    clientHeadersReceived.resolve();
  }),
  ontrailers: mustCall(function(trailers) {
    strictEqual(trailers['x-checksum'], 'abc123');
    strictEqual(trailers['x-request-id'], '42');
    clientTrailersReceived.resolve();
  }),
});

await clientHeadersReceived.promise;

// Read the response body.
const body = await bytes(stream);
strictEqual(decoder.decode(body), responseBody);

// Trailers arrive after the body.
await clientTrailersReceived.promise;

// stream.headers should still be the initial headers, not trailers.
strictEqual(stream.headers[':status'], '200');

await Promise.all([stream.closed, serverDone.promise]);
clientSession.close();
