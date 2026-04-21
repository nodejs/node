// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: Multiple concurrent HTTP/3 requests on a single session.
// Client opens several bidi streams in parallel, each with different
// request paths. Server responds to each with a path-specific body.
// Verifies:
// - Multiple streams can be opened concurrently on one session
// - Each stream receives the correct response (no cross-talk)
// - All streams complete independently

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
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

const REQUEST_COUNT = 5;
let serverStreamsCompleted = 0;
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall((stream) => {
    stream.closed.then(mustCall(() => {
      if (++serverStreamsCompleted === REQUEST_COUNT) {
        serverSession.close();
        serverDone.resolve();
      }
    }));
  }, REQUEST_COUNT);
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  onheaders: mustCall(function(headers) {
    const path = headers[':path'];
    this.sendHeaders({
      ':status': '200',
      'content-type': 'text/plain',
    });
    const w = this.writer;
    w.writeSync(`response for ${path}`);
    w.endSync();
  }, REQUEST_COUNT),
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
});
await clientSession.opened;

// Open all requests concurrently.
const paths = Array.from({ length: REQUEST_COUNT }, (_, i) => `/path/${i}`);

const requests = paths.map(mustCall(async (path) => {
  const headersReceived = Promise.withResolvers();

  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': path,
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
      headersReceived.resolve();
    }),
  });

  await headersReceived.promise;
  const body = await bytes(stream);
  const text = decoder.decode(body);
  strictEqual(text, `response for ${path}`);
  await stream.closed;
}, REQUEST_COUNT));

await Promise.all([...requests, serverDone.promise]);
clientSession.close();
