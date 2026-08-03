'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.createServer(common.mustCallAtLeast((req, res) => {
  let corked = false;
  const originalWrite = res.socket._write;
  res.socket._write = common.mustCall(function(...args) {
    assert.strictEqual(corked, false);
    return originalWrite.apply(this, args);
  });
  corked = true;
  res.cork();
  assert.strictEqual(res.writableCorked, res.socket.writableCorked);
  res.cork();
  assert.strictEqual(res.writableCorked, res.socket.writableCorked);
  res.writeHead(200, { 'a-header': 'a-header-value' });
  res.uncork();
  assert.strictEqual(res.writableCorked, res.socket.writableCorked);
  corked = false;
  res.end('asd');
  assert.strictEqual(res.writableCorked, res.socket.writableCorked);
}));

server.listen(0, common.mustCall(() => {
  http.get({ port: server.address().port }, common.mustCall((res) => {
    res.on('data', common.mustCall());
    res.on('end', common.mustCall(() => {
      server.close();
    }));
  }));
}));
