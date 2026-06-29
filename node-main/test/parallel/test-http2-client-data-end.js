'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const Countdown = require('../common/countdown');

const server = http2.createServer();
server.on('stream', common.mustCall((stream, headers, flags) => {
  if (headers[':path'] === '/') {
    stream.pushStream({ ':path': '/foobar' }, (err, push, headers) => {
      assert.ifError(err);
      push.respond({
        'content-type': 'text/html',
        'x-push-data': 'pushed by server',
      });
      push.write('pushed by server ');
      setImmediate(() => push.end('data'));
      stream.end('st');
    });
  }
  stream.respond({ 'content-type': 'text/html' });
  stream.write('te');
}));


server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  const req = client.request();

  const countdown = new Countdown(2, () => {
    server.close();
    client.close();
  });

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
    stream.on('data', (d) => pushData += d);
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(pushData, 'pushed by server data');
    }));
    stream.on('close', () => countdown.dec());
  }));

  let data = '';

  req.setEncoding('utf8');
  req.on('data', common.mustCallAtLeast((d) => data += d));
  req.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'test');
  }));
  req.on('close', () => countdown.dec());
}));
