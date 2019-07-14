'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer(common.mustCall(function(req, res) {
  res.setHeader('test-header', 'header');
  assert.strictEqual(res.bytesWritten, 0);
  res.write('hello');
  assert.strictEqual(res.bytesWritten, 5);
  res.end('world');
  assert.strictEqual(res.bytesWritten, 10);
  server.close();
}));

server.listen(0);

server.on('listening', common.mustCall(function() {
  const url = `http://localhost:${server.address().port}`;
  const client = h2.connect(url, common.mustCall(() => {
    const request = client.request();
    request.end();
  }));
}));
