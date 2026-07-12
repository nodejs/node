'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');
const assert = require('assert');


const server = h2.createServer(common.mustCall(function(req, res) {
  req.on('aborted', common.mustCall(function() {
    assert.strictEqual(this.aborted, true);
    assert.strictEqual(this.complete, true);
  }));
  assert.strictEqual(req.aborted, false);
  assert.strictEqual(req.complete, false);
  res.write('hello');
  server.close();
}));

server.listen(0, common.mustCall(function() {
  const url = `http://localhost:${server.address().port}`;
  const client = h2.connect(url, common.mustCall(() => {
    const request = client.request();
    request.on('data', common.mustCall((chunk) => {
      client.destroy();
    }));
  }));
}));
