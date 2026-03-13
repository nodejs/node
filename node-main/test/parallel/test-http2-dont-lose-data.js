'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

server.on('stream', (s) => {
  assert(s.pushAllowed);

  s.pushStream({ ':path': '/file' }, common.mustSucceed((pushStream) => {
    pushStream.respond();
    pushStream.end('a push stream');
  }));

  s.respond();
  s.end('hello world');
});

server.listen(0, () => {
  server.unref();

  const url = `http://localhost:${server.address().port}`;

  const client = http2.connect(url);
  const req = client.request();

  let pushStream;

  client.on('stream', common.mustCall((s, headers) => {
    assert.strictEqual(headers[':path'], '/file');
    pushStream = s;
  }));

  req.on('response', common.mustCall((headers) => {
    let pushData = '';
    pushStream.setEncoding('utf8');
    pushStream.on('data', (d) => pushData += d);
    pushStream.on('end', common.mustCall(() => {
      assert.strictEqual(pushData, 'a push stream');

      // Removing the setImmediate causes the test to pass
      setImmediate(function() {
        let data = '';
        req.setEncoding('utf8');
        req.on('data', (d) => data += d);
        req.on('end', common.mustCall(() => {
          assert.strictEqual(data, 'hello world');
          client.close();
        }));
      });
    }));
  }));
});
