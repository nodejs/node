'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

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
    }, common.mustCall((push, headers) => {
      assert.strictEqual(push._writableState.ended, true);
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
  const headers = { ':path': '/' };
  const client = http2.connect(`http://localhost:${port}`);
  const req = client.request(headers);
  req.setEncoding('utf8');

  client.on('stream', common.mustCall((stream, headers) => {
    assert.strictEqual(headers[':scheme'], 'http');
    assert.strictEqual(headers[':path'], '/');
    assert.strictEqual(headers[':authority'], `localhost:${port}`);
  }));

  let data = '';

  req.on('data', common.mustCall((d) => data += d));
  req.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'test');
    server.close();
    client.destroy();
  }));
  req.end();
}));
