'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const onWriteAfterEndError = common.mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
}, 2);

const server = http.createServer(common.mustCall(function(req, res) {
  res.end('testing ended state', common.mustCall());
  assert.strictEqual(res.writableCorked, 0);
  res.end(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_ALREADY_FINISHED');
  }));
  assert.strictEqual(res.writableCorked, 0);
  res.end('end', onWriteAfterEndError);
  assert.strictEqual(res.writableCorked, 0);
  res.on('error', onWriteAfterEndError);
  res.on('finish', common.mustCall(() => {
    res.end(common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_STREAM_ALREADY_FINISHED');
      server.close();
    }));
  }));
}));

server.listen(0);

server.on('listening', common.mustCall(function() {
  http
    .request({
      port: server.address().port,
      method: 'GET',
      path: '/'
    })
    .end();
}));
