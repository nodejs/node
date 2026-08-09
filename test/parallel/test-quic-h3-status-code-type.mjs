// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Verify incoming :status is exposed as a number, matching HTTP/2 behavior.
// See https://github.com/nodejs/node/issues/63557

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listenHttp3: listen, connectHttp3: connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const codes = [200, 204, 404];
let serverResponses = 0;
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall((stream) => {
    stream.onheaders = mustCall(() => {
      const status = codes[serverResponses++];
      stream.sendHeaders({ ':status': String(status) }, { terminal: true });
      if (serverResponses === codes.length) {
        serverSession.close();
        serverDone.resolve();
      }
    });
  }, codes.length);
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});
await clientSession.opened;

for (const expected of codes) {
  const stream = await clientSession.request({
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, {
    onheaders: mustCall((headers) => {
      assert.strictEqual(typeof headers[':status'], 'number');
      assert.strictEqual(headers[':status'], expected);
    }),
  });
  await stream.closed;
}

await serverDone.promise;
await clientSession.close();
await serverEndpoint.close();
