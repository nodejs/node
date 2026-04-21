// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 informational (1xx) headers.
// Server sends a 103 Early Hints response before the final 200 response.
// Client receives the informational headers via oninfo, then the final
// response via onheaders.
// Verifies:
// - sendInformationalHeaders delivers 1xx headers to the client
// - oninfo callback fires with the informational headers
// - onheaders callback fires separately with the final response
// - Body data is delivered after the final response headers

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';
import * as fixtures from '../common/fixtures.mjs';

const { ok, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const decoder = new TextDecoder();
const responseBody = 'final response';

// quic.stream.info fires when informational (1xx) headers are received.
dc.subscribe('quic.stream.info', mustCall((msg) => {
  ok(msg.stream, 'stream.info should include stream');
  ok(msg.session, 'stream.info should include session');
  ok(msg.headers, 'stream.info should include headers');
  strictEqual(msg.headers[':status'], '103');
}));

// quic.stream.headers also fires for the final response headers.
dc.subscribe('quic.stream.headers', mustCall((msg) => {
  ok(msg.stream, 'stream.headers should include stream');
  ok(msg.headers, 'stream.headers should include headers');
}, 2));

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
    // Send 103 Early Hints before the final response.
    this.sendInformationalHeaders({
      ':status': '103',
      'link': '</style.css>; rel=preload; as=style',
    });

    // Send final response headers + body.
    this.sendHeaders({
      ':status': '200',
      'content-type': 'text/plain',
    });

    const w = this.writer;
    w.writeSync(responseBody);
    w.endSync();
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
});
await clientSession.opened;

const clientInfoReceived = Promise.withResolvers();
const clientHeadersReceived = Promise.withResolvers();

const stream = await clientSession.createBidirectionalStream({
  headers: {
    ':method': 'GET',
    ':path': '/page',
    ':scheme': 'https',
    ':authority': 'localhost',
  },
  oninfo: mustCall(function(headers) {
    strictEqual(headers[':status'], '103');
    strictEqual(headers.link, '</style.css>; rel=preload; as=style');
    clientInfoReceived.resolve();
  }),
  onheaders: mustCall(function(headers) {
    strictEqual(headers[':status'], '200');
    strictEqual(headers['content-type'], 'text/plain');
    clientHeadersReceived.resolve();
  }),
});

await Promise.all([clientInfoReceived.promise, clientHeadersReceived.promise]);

// Read the response body.
const body = await bytes(stream);
strictEqual(decoder.decode(body), responseBody);

// stream.headers should return the final (initial) headers, not 1xx.
strictEqual(stream.headers[':status'], '200');

await Promise.all([stream.closed, serverDone.promise]);
clientSession.close();
