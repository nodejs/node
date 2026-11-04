'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { once } = require('events');

// Queued request bodies must not prevent responses on local streams.
async function test(bodySize, {
  informational = 0,
  blocked = false,
  trailers = false,
} = {}) {
  const server = http2.createServer({
    settings: { initialWindowSize: blocked ? 0 : 65535 },
  });
  const sessions = new Set();
  server.on('session', (session) => sessions.add(session));
  server.on('stream', (stream, headers) => {
    stream.on('error', () => {});
    if (headers[':path'] === '/reuse') {
      stream.additionalHeaders({ ':status': 103 });
      stream.respond({ ':status': 200 });
      stream.end();
    } else {
      if (informational) stream.additionalHeaders({ ':status': informational });
      stream.respond({ ':status': 413 }, { waitForTrailers: trailers });
      if (trailers) {
        stream.on('wantTrailers', () => stream.sendTrailers({ 'x-trailer': 'x' }));
      }
      stream.end();
    }
  });
  server.listen(0);
  await once(server, 'listening');

  const client = http2.connect(`http://localhost:${server.address().port}`, {
    maxSessionMemory: 1,
  });
  await Promise.all([
    once(client, 'remoteSettings'),
    once(client, 'localSettings'),
  ]);
  const req = client.request({ ':method': 'POST' });
  const response = once(req, 'response');
  const ended = once(req, 'end');
  const closed = once(req, 'close');
  req.on('headers', informational ? common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], informational);
  }) : common.mustNotCall());
  req.on('trailers', trailers ? common.mustCall((headers) => {
    assert.strictEqual(headers['x-trailer'], 'x');
  }) : common.mustNotCall());
  req.resume();
  // Fill the write queue before any response headers can arrive.
  req.end(Buffer.alloc(bodySize));
  const [[headers]] = await Promise.all([response, ended]);
  assert.strictEqual(headers[':status'], 413);
  req.destroy();
  await closed;
  // The session must remain usable after canceling the upload.
  const reuse = client.request({ ':path': '/reuse' });
  reuse.on('headers', common.mustCall());
  const reusedResponse = once(reuse, 'response');
  reuse.end();
  const [reusedHeaders] = await reusedResponse;
  assert.strictEqual(reusedHeaders[':status'], 200);
  const reusedClose = once(reuse, 'close');
  reuse.close();
  await reusedClose;
  client.destroy();
  for (const session of sessions) session.destroy();
  server.close();
}

(async () => {
  const bodySize = 4 * 1024 * 1024;
  await test(bodySize);
  await test(bodySize, { blocked: true });
  await test(bodySize, { informational: 100, blocked: true });
  await test(bodySize, { informational: 103, blocked: true });
  await test(bodySize, { trailers: true, blocked: true });
  await test(1);
})().then(common.mustCall());
