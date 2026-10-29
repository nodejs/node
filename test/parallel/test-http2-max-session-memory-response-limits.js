'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { once } = require('events');

// The outbound DATA exception must not bypass informational/trailer memory
// checks, or the independent header count and size limits on final responses.
async function test(kind) {
  const sessions = new Set();
  const server = http2.createServer({ settings: { initialWindowSize: 0 } });
  server.on('session', (session) => sessions.add(session));
  server.on('stream', (stream) => {
    stream.on('error', () => {});
    switch (kind) {
      case 'informational':
        stream.additionalHeaders({ ':status': 103 });
        break;
      case 'repeated-informational':
        for (let i = 0; i < 80; i++) {
          stream.additionalHeaders({ ':status': 103,
                                     'x-hint': 'x'.repeat(16 * 1024) });
        }
        break;
      case 'trailers':
        stream.respond({ ':status': 200 }, { waitForTrailers: true });
        stream.on('wantTrailers', () => stream.sendTrailers({ 'x-trailer': 'x' }));
        stream.end();
        return;
      case 'pairs':
        stream.respond({ ':status': 413, 'a': '1', 'b': '2', 'c': '3', 'd': '4' });
        stream.end();
        return;
      case 'size':
        stream.respond({ ':status': 413, 'x-large': 'x'.repeat(2048) });
        stream.end();
        return;
    }
    stream.respond({ ':status': 413 });
    stream.end();
  });
  server.listen(0);
  await once(server, 'listening');
  const client = http2.connect(`http://localhost:${server.address().port}`, {
    maxSessionMemory: 1,
    maxHeaderListPairs: kind === 'pairs' ? 4 : 128,
    settings: { maxHeaderListSize: kind === 'size' ? 1024 : 65535 },
  });
  await Promise.all([
    once(client, 'remoteSettings'),
    once(client, 'localSettings'),
  ]);
  const req = client.request({ ':method': 'POST' });
  const failure = once(req, 'error');
  let hints = 0;
  if (kind === 'repeated-informational') {
    req.on('headers', () => hints++);
  } else {
    req.on('headers', common.mustNotCall());
  }
  req.on('response', kind === 'trailers' ? common.mustCall() : common.mustNotCall());
  req.on('trailers', common.mustNotCall());
  req.end(Buffer.alloc(kind === 'repeated-informational' ? 0 : 4 * 1024 * 1024));
  const [err] = await failure;
  assert.strictEqual(err.code, 'ERR_HTTP2_STREAM_ERROR');
  if (kind === 'repeated-informational') {
    assert(hints > 0 && hints < 80);
  }
  client.destroy();
  for (const session of sessions) session.destroy();
  server.close();
}

(async () => {
  for (const kind of [
    'informational', 'repeated-informational', 'trailers', 'pairs', 'size',
  ]) {
    await test(kind);
  }
})().then(common.mustCall());
