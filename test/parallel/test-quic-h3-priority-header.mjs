// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: a client's initial request priority (set via request({ priority }))
// reaches the server as an RFC 9218 `priority` request header, and the
// server's stream.priority getter reflects it.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { deepStrictEqual, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:http3');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const encoder = new TextEncoder();

const expectedByPath = {
  '/high': { level: 'high', incremental: false },
  '/low-inc': { level: 'low', incremental: true },
  '/default': { level: 'default', incremental: false },
  '/default-inc': { level: 'default', incremental: true },
};

const serverDone = Promise.withResolvers();
let seen = 0;

const serverEndpoint = await listen(mustCall((ss) => {
  ss.onstream = mustCall((stream) => {
    stream.onheaders = mustCall((headers) => {
      deepStrictEqual(stream.priority, expectedByPath[headers[':path']]);
      stream.sendHeaders({ ':status': '200' });
      stream.writer.writeSync(encoder.encode('ok'));
      stream.writer.endSync();
      if (++seen === 4) serverDone.resolve();
    });
  }, 4);
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});
await clientSession.opened;

const get = (path) => ({
  ':method': 'GET',
  ':path': path,
  ':scheme': 'https',
  ':authority': 'localhost',
});

const stream1 = await clientSession.request(get('/high'), {
  priority: 'high',
  onheaders: mustCall((headers) => strictEqual(headers[':status'], '200')),
});
const stream2 = await clientSession.request(get('/low-inc'), {
  priority: 'low',
  incremental: true,
  onheaders: mustCall((headers) => strictEqual(headers[':status'], '200')),
});
const stream3 = await clientSession.request(get('/default'), {
  onheaders: mustCall((headers) => strictEqual(headers[':status'], '200')),
});
const stream4 = await clientSession.request(get('/default-inc'), {
  incremental: true,
  onheaders: mustCall((headers) => strictEqual(headers[':status'], '200')),
});

await Promise.all([
  bytes(stream1),
  bytes(stream2),
  bytes(stream3),
  bytes(stream4),
  serverDone.promise,
]);
await clientSession.close();
await serverEndpoint.close();
