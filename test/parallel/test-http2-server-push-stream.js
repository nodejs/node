'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();
server.on('stream', common.mustCall((stream, headers) => {
  const port = server.address().port;
  if (headers[':path'] === '/') {
    stream.pushStream({
      ':scheme': 'http',
      ':path': '/foobar',
      ':authority': `localhost:${port}`,
    }, common.mustSucceed((push, headers) => {
      push.respond({
        'content-type': 'text/html',
        ':status': 200,
        'x-push-data': 'pushed by server',
      });
      push.end('pushed by server data');

      assert.throws(() => {
        push.pushStream({}, common.mustNotCall());
      }, {
        code: 'ERR_HTTP2_NESTED_PUSH',
        name: 'Error'
      });

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

  client.on('stream', common.mustCall((stream, headers) => {
    assert.strictEqual(headers[':scheme'], 'http');
    assert.strictEqual(headers[':path'], '/foobar');
    assert.strictEqual(headers[':authority'], `localhost:${port}`);
    stream.on('push', common.mustCall((headers, flags) => {
      assert.strictEqual(headers[':status'], 200);
      assert.strictEqual(headers['content-type'], 'text/html');
      assert.strictEqual(headers['x-push-data'], 'pushed by server');
      assert.strictEqual(typeof flags === 'number', true);
    }));
    stream.on('aborted', common.mustNotCall());
    // We have to read the data of the push stream to end gracefully.
    stream.resume();
  }));

  let data = '';

  req.on('data', common.mustCall((d) => data += d));
  req.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'test');
    server.close();
    client.close();
  }));
  req.end();
}));
