// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: basic HTTP/3 request/response.
// Client sends a GET request with H3 pseudo-headers, server receives
// the request and sends back a 200 response with a text body.
// Verifies:
// - Request pseudo-headers are delivered to the server via onheaders
// - stream.headers property returns the initial headers
// - Response headers and status are delivered to the client
// - Response body data is readable

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { strictEqual } = assert;

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
const responseBody = 'Hello from H3 server';

const serverDone = Promise.withResolvers();

// The onheaders callback signature is (headers, kind) with `this` bound
// to the stream. A regular function is used so `this` is accessible.
// safeCallbackInvoke(fn, owner, ...args) consumes the owner for error
// handling and forwards only ...args to fn.
const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  // Default ALPN is h3 — omitted intentionally to exercise the default.
  //
  // onheaders is provided via listen options so it is applied to
  // incoming streams (via kStreamCallbacks) BEFORE onstream fires.
  // For H3, onheaders must be set because the H3 application delivers
  // headers and stream[kHeaders] asserts the callback exists.
  onheaders: mustCall(function(headers) {
    // Verify request pseudo-headers.
    strictEqual(headers[':method'], 'GET');
    strictEqual(headers[':path'], '/index.html');
    strictEqual(headers[':scheme'], 'https');
    strictEqual(headers[':authority'], 'localhost');

    // After onheaders, stream.headers returns the initial headers.
    // `this` is the stream (bound by the onheaders setter).
    strictEqual(this.headers[':method'], 'GET');

    // Send response headers (terminal: false is the default — body follows).
    this.sendHeaders({
      ':status': '200',
      'content-type': 'text/plain',
    });

    // Write response body and close the write side.
    const w = this.writer;
    w.writeSync(encoder.encode(responseBody));
    w.endSync();
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  // Default ALPN is h3.
});

const info = await clientSession.opened;
strictEqual(info.protocol, 'h3');

const clientHeadersReceived = Promise.withResolvers();

// Send a GET request. With body omitted, the terminal flag is set
// automatically (END_STREAM on the HEADERS frame).
const stream = await clientSession.createBidirectionalStream({
  headers: {
    ':method': 'GET',
    ':path': '/index.html',
    ':scheme': 'https',
    ':authority': 'localhost',
  },
  onheaders: mustCall(function(headers) {
    strictEqual(headers[':status'], '200');
    strictEqual(headers['content-type'], 'text/plain');
    clientHeadersReceived.resolve();
  }),
});

await clientHeadersReceived.promise;

// Read the full response body.
const body = await bytes(stream);
strictEqual(decoder.decode(body), responseBody);

// stream.headers should return the buffered response headers.
strictEqual(stream.headers[':status'], '200');

await Promise.all([stream.closed, serverDone.promise]);
clientSession.close();
