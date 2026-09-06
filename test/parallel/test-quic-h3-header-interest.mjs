// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Verify HTTP/3 header interest is tracked independently of onheaders and
// that pre-set trailing headers keep the response open until they are sent.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { createPrivateKey } = await import('node:crypto');
const { listen, connect } = await import('node:quic');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const encoder = new TextEncoder();
const decoder = new TextDecoder();
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  onheaders: mustCall(function() {
    this.sendInformationalHeaders({
      ':status': '103',
      'link': '</style.css>; rel=preload',
    });
    this.sendHeaders({ ':status': '200' });
    this.pendingTrailers = { 'x-checksum': 'abc123' };
    const writer = this.writer;
    writer.writeSync(encoder.encode('body'));
    writer.endSync();
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});
await clientSession.opened;

const infoReceived = Promise.withResolvers();
const trailersReceived = Promise.withResolvers();
const stream = await clientSession.createBidirectionalStream({
  headers: {
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  },
  oninfo: mustCall((headers) => {
    assert.strictEqual(headers[':status'], 103);
    infoReceived.resolve();
  }),
  ontrailers: mustCall((headers) => {
    assert.strictEqual(headers['x-checksum'], 'abc123');
    trailersReceived.resolve();
  }),
});

assert.strictEqual(decoder.decode(await bytes(stream)), 'body');
await Promise.all([infoReceived.promise, trailersReceived.promise]);
assert.strictEqual(stream.headers[':status'], 200);

await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
