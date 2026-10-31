'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { once } = require('events');

// Queued request bodies must not prevent a final response on a local stream.
async function test(bodySize, informational, blocked) {
  const server = http2.createServer({
    settings: { initialWindowSize: blocked ? 0 : 65535 },
  });
  const sessions = new Set();
  let serverStream;
  server.on('session', (session) => sessions.add(session));
  server.on('stream', (stream, headers) => {
    serverStream = stream;
    stream.on('error', () => {});
    if (headers[':path'] === '/reuse') {
      stream.additionalHeaders({ ':status': 103 });
      stream.respond({ ':status': 200 });
      stream.end();
    } else if (informational) {
      stream.additionalHeaders({ ':status': 103 });
    } else {
      stream.respond({ ':status': 413 });
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
  if (informational) {
    // A final response following 1xx has category NGHTTP2_HCAT_HEADERS.
    await once(req, 'headers');
  }
  req.end(Buffer.alloc(bodySize));
  if (informational) {
    // Wait for the 103 to reach the client before filling its write queue.
    serverStream.respond({ ':status': 413 });
    serverStream.end();
  }
  const [headers] = await response;
  assert.strictEqual(headers[':status'], 413);
  const closed = once(req, 'close');
  req.destroy();
  await closed;
  // Canceling the blocked upload must release its memory accounting too.
  // A subsequent 1xx uses the full memory check, without the DATA exception.
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
  await test(4 * 1024 * 1024, false, false);
  await test(4 * 1024 * 1024, false, true);
  await test(4 * 1024 * 1024, true, true);
  await test(1, false, false);
})().then(common.mustCall());
