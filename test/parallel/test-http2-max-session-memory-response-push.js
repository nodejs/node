'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { once } = require('events');

(async () => {
  const sessions = new Set();
  let serverRequest;
  let serverPush;
  const server = http2.createServer({ settings: { initialWindowSize: 0 } });
  server.on('session', (session) => sessions.add(session));
  server.on('stream', (stream) => {
    serverRequest = stream;
    stream.on('error', () => {});
    stream.pushStream({ ':path': '/push' }, common.mustSucceed((push) => {
      serverPush = push;
      push.on('error', () => {});
    }));
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
  const promised = once(client, 'stream');
  const req = client.request({ ':method': 'POST' });
  const response = once(req, 'response');
  const [push] = await promised;
  push.on('push', common.mustNotCall());
  const failure = once(push, 'error');

  // Accept the PUSH_PROMISE first, then exhaust the budget. Even an existing
  // pushed stream must not exclude queued DATA when receiving its response.
  req.end(Buffer.alloc(4 * 1024 * 1024));
  serverPush.respond({ ':status': 200 });
  serverPush.end();
  const [err] = await failure;
  assert.strictEqual(err.code, 'ERR_HTTP2_STREAM_ERROR');

  serverRequest.respond({ ':status': 413 });
  serverRequest.end();
  const [headers] = await response;
  assert.strictEqual(headers[':status'], 413);
  req.close();
  client.destroy();
  for (const session of sessions) session.destroy();
  server.close();
})().then(common.mustCall());
