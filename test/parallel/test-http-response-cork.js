// Flags: --expose-internals

'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');
const { kInternalWritev } = require('internal/streams/utils');

const server = http.createServer(common.mustCallAtLeast((req, res) => {
  let corked = false;
  const originalWritev = res.socket[kInternalWritev];
  res.socket[kInternalWritev] = common.mustCall(function(...args) {
    assert.strictEqual(corked, false);
    return originalWritev.apply(this, args);
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
