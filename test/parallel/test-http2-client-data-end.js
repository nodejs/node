// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();
server.on('stream', common.mustCall((stream, headers, flags) => {
  const port = server.address().port;
  if (headers[':path'] === '/') {
    stream.pushStream({
      ':scheme': 'http',
      ':path': '/foobar',
      ':authority': `localhost:${port}`,
    }, (push, headers) => {
      push.respond({
        'content-type': 'text/html',
        ':status': 200,
        'x-push-data': 'pushed by server',
      });
      push.write('pushed by server ');
      // Sending in next immediate ensures that a second data frame
      // will be sent to the client, which will cause the 'data' event
      // to fire multiple times.
      setImmediate(() => {
        push.end('data');
      });
      stream.end('st');
    });
  }
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.write('te');
}));


server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const headers = { ':path': '/' };
  const client = http2.connect(`http://localhost:${port}`);

  const req = client.request(headers);

  let expected = 2;
  function maybeClose() {
    if (--expected === 0) {
      server.close();
      client.destroy();
    }
  }

  req.on('response', common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 200);
    assert.strictEqual(headers['content-type'], 'text/html');
  }));

  client.on('stream', common.mustCall((stream, headers, flags) => {
    assert.strictEqual(headers[':scheme'], 'http');
    assert.strictEqual(headers[':path'], '/foobar');
    assert.strictEqual(headers[':authority'], `localhost:${port}`);
    stream.on('push', common.mustCall((headers, flags) => {
      assert.strictEqual(headers[':status'], 200);
      assert.strictEqual(headers['content-type'], 'text/html');
      assert.strictEqual(headers['x-push-data'], 'pushed by server');
    }));

    stream.setEncoding('utf8');
    let pushData = '';
    stream.on('data', common.mustCall((d) => {
      pushData += d;
    }, 2));
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(pushData, 'pushed by server data');
      maybeClose();
    }));
  }));

  let data = '';

  req.setEncoding('utf8');
  req.on('data', common.mustCall((d) => data += d));
  req.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'test');
    maybeClose();
  }));
  req.end();
}));
