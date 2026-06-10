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

const { listen, connect } = await import('node:http3');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const encoder = new TextEncoder();
const decoder = new TextDecoder();
const responseBody = 'Hello from H3 server';

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Attach onheaders synchronously in the onstream frame, before any
    // await — header delivery follows stream creation.
    stream.onheaders = mustCall((headers) => {
      // Verify request pseudo-headers.
      strictEqual(headers[':method'], 'GET');
      strictEqual(headers[':path'], '/index.html');
      strictEqual(headers[':scheme'], 'https');
      strictEqual(headers[':authority'], 'localhost');

      // After onheaders, stream.headers returns the initial headers.
      strictEqual(stream.headers[':method'], 'GET');

      // Send response headers (terminal: false is the default — body
      // follows).
      stream.sendHeaders({
        ':status': '200',
        'content-type': 'text/plain',
      });

      // Write response body and close the write side.
      const w = stream.writer;
      w.writeSync(encoder.encode(responseBody));
      w.endSync();
    });
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});

const info = await clientSession.opened;
strictEqual(info.protocol, 'h3');

const clientHeadersReceived = Promise.withResolvers();

// Send a GET request. With body omitted, the terminal flag is set
// automatically (END_STREAM on the HEADERS frame).
const stream = await clientSession.request({
  ':method': 'GET',
  ':path': '/index.html',
  ':scheme': 'https',
  ':authority': 'localhost',
}, {
  onheaders: mustCall((headers) => {
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
await clientSession.close();
await serverEndpoint.close();
