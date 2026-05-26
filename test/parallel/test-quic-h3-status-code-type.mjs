// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Verify incoming :status is exposed as a number, matching HTTP/2 behavior.
// See https://github.com/nodejs/node/issues/63557

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

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const codes = [200, 204, 404];
let serverResponses = 0;
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (ss) => {
  ss.onstream = mustCall(() => {
    if (++serverResponses === codes.length) {
      ss.close();
      serverDone.resolve();
    }
  }, codes.length);
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  onheaders: mustCall(function() {
    const status = codes[serverResponses - 1];
    this.sendHeaders({ ':status': String(status) }, { terminal: true });
    this.writer.endSync();
  }, codes.length),
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});
await clientSession.opened;

for (const expected of codes) {
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(typeof headers[':status'], 'number');
      strictEqual(headers[':status'], expected);
    }),
  });
  await stream.closed;
}

await serverDone.promise;
await clientSession.close();
await serverEndpoint.close();
