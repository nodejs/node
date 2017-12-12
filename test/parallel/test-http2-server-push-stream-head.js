'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const Countdown = require('../common/countdown');

// Check that pushStream handles method HEAD correctly
// - stream should end immediately (no body)

const server = http2.createServer();
server.on('stream', common.mustCall((stream, headers) => {
  const port = server.address().port;
  if (headers[':path'] === '/') {
    stream.pushStream({
      ':scheme': 'http',
      ':method': 'HEAD',
      ':authority': `localhost:${port}`,
    }, common.mustCall((err, push, headers) => {
      assert.strictEqual(push._writableState.ended, true);
      push.respond();
      assert(!push.write('test'));
      stream.end('test');
    }));
  }
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  const countdown = new Countdown(2, () => {
    server.close();
    client.close();
  });

  const req = client.request();
  req.setEncoding('utf8');

  client.on('stream', common.mustCall((stream, headers) => {
    assert.strictEqual(headers[':method'], 'HEAD');
    assert.strictEqual(headers[':scheme'], 'http');
    assert.strictEqual(headers[':path'], '/');
    assert.strictEqual(headers[':authority'], `localhost:${port}`);
    stream.on('push', common.mustCall(() => {
      stream.on('data', common.mustNotCall());
      stream.on('end', common.mustCall());
    }));
    stream.on('close', common.mustCall(() => countdown.dec()));
  }));

  let data = '';

  req.on('data', common.mustCall((d) => data += d));
  req.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'test');
  }));
  req.on('close', common.mustCall(() => countdown.dec()));
  req.end();
}));
