'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.createServer((req, res) => {
  let corked = false;
  const originalWrite = res.socket.write;
  res.socket.write = common.mustCall((...args) => {
    assert.strictEqual(corked, false);
    return originalWrite.call(res.socket, ...args);
  }, 5);
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
});

server.listen(0, () => {
  http.get({ port: server.address().port }, (res) => {
    res.on('data', common.mustCall());
    res.on('end', common.mustCall(() => {
      server.close();
    }));
  });
});
