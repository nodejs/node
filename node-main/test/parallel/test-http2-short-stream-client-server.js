'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const { Readable } = require('stream');

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.respond({
    ':status': 200,
    'content-type': 'text/html'
  });
  const input = new Readable({
    read() {
      this.push('test');
      this.push(null);
    }
  });
  input.pipe(stream);
}));


server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  const req = client.request();

  req.on('response', common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 200);
    assert.strictEqual(headers['content-type'], 'text/html');
  }));

  let data = '';

  const notCallClose = common.mustNotCall();

  setTimeout(() => {
    req.setEncoding('utf8');
    req.removeListener('close', notCallClose);
    req.on('close', common.mustCall(() => {
      server.close();
      client.close();
    }));
    req.on('data', common.mustCallAtLeast((d) => data += d));
    req.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'test');
    }));
  }, common.platformTimeout(100));

  req.on('close', notCallClose);
}));
