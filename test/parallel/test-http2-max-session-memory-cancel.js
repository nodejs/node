'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { once } = require('events');

(async () => {
  // Keep the entire upload queued until it is canceled.
  const server = http2.createServer({ settings: { initialWindowSize: 0 } });
  server.on('stream', common.mustCall((stream, headers) => {
    stream.on('error', () => {});
    if (headers[':path'] === '/cancel') return;
    stream.respond({ ':status': 200 });
    stream.end();
  }, 2));
  server.listen(0);
  await once(server, 'listening');

  const client = http2.connect(`http://localhost:${server.address().port}`, {
    maxSessionMemory: 1,
  });
  await once(client, 'remoteSettings');
  const uploadArrived = once(server, 'stream');
  const upload = client.request({ ':method': 'POST', ':path': '/cancel' });
  upload.on('response', common.mustNotCall());
  upload.end(Buffer.alloc(4 * 1024 * 1024));
  await uploadArrived;
  const closed = once(upload, 'close');
  upload.destroy();
  await closed;
  // Native write queue cleanup runs on an immediate after stream destruction.
  await new Promise(setImmediate);

  // The canceled upload must no longer count towards maxSessionMemory.
  const req = client.request({ ':path': '/check' });
  const response = once(req, 'response');
  req.end();
  const [headers] = await response;
  assert.strictEqual(headers[':status'], 200);
  const ended = once(req, 'end');
  req.resume();
  await ended;
  client.close();
  server.close();
})().then(common.mustCall());
