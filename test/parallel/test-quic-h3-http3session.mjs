// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// A request/response round-trip through http3.connect/listen, exercising
// onstream delivery (wrapped streams), request(), header events,
// and bodies in both directions.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, ok } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { connect, listen, Http3Session, Http3Stream } = await import('node:http3');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const decoder = new TextDecoder();
const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((session) => {
  ok(session instanceof Http3Session);
  session.onstream = mustCall((stream) => {
    ok(stream instanceof Http3Stream);
    stream.onheaders = mustCall((headers) => {
      strictEqual(headers[':path'], '/hello');
      strictEqual(headers[':method'], 'GET');
      stream.sendHeaders({
        ':status': '200',
        'content-type': 'text/plain',
      });
      const w = stream.writer;
      w.writeSync('hello h3');
      w.endSync();
    });
    stream.closed.then(mustCall(() => {
      session.close();
      serverDone.resolve();
    }));
  });
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});
ok(clientSession instanceof Http3Session);
const info = await clientSession.opened;
strictEqual(info.protocol, 'h3');

const responseHeaders = Promise.withResolvers();
const stream = await clientSession.request({
  ':method': 'GET',
  ':path': '/hello',
  ':scheme': 'https',
  ':authority': 'localhost',
}, {
  body: encoder.encode(''),
  onheaders: mustCall((headers) => {
    strictEqual(headers[':status'], '200');
    responseHeaders.resolve();
  }),
});
ok(stream instanceof Http3Stream);

const body = decoder.decode(await bytes(stream));
strictEqual(body, 'hello h3');
await responseHeaders.promise;

await serverDone.promise;
await clientSession.closed;
await serverEndpoint.close();
