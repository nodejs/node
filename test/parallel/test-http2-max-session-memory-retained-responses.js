'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { once } = require('events');

// Final responses must still share a session-wide memory budget, including
// when another local stream has a large request body waiting to be sent.
async function test(queuedBody) {
  const sessions = new Set();
  const server = http2.createServer({ settings: { initialWindowSize: 0 } });
  server.on('session', (session) => sessions.add(session));
  server.on('stream', (stream, headers) => {
    stream.on('error', () => {});
    if (headers[':path'] !== '/upload') {
      stream.respond({ ':status': 200, 'x-padding': 'x'.repeat(16 * 1024) });
      // Leave responses open so their headers remain charged to the session.
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
  if (queuedBody) {
    const upload = client.request({ ':method': 'POST', ':path': '/upload' });
    upload.on('error', common.mustNotCall());
    upload.end(Buffer.alloc(4 * 1024 * 1024));
  }

  let rejected = false;
  const retainedHeaders = [];
  for (let i = 0; i < 128; i++) {
    const req = client.request();
    const result = once(req, 'response');
    req.end();
    try {
      const [headers] = await result;
      retainedHeaders.push(headers);
    } catch (err) {
      assert.strictEqual(err.code, 'ERR_HTTP2_STREAM_ERROR');
      rejected = true;
      break;
    }
  }
  const accepted = retainedHeaders.length;
  assert(accepted > 0, 'queued bodies must not consume the response budget');
  assert(rejected, 'retained final response headers must exhaust the budget');
  assert(accepted * 16 * 1024 < 1024 * 1024);
  client.destroy();
  for (const session of sessions) session.destroy();
  server.close();
}

(async () => {
  await test(false);
  await test(true);
})().then(common.mustCall());
