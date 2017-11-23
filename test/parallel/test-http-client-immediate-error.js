'use strict';
// Flags: --expose-internals

// Make sure http.request() can catch immediate errors in
// net.createConnection().

const common = require('../common');
const assert = require('assert');
const net = require('net');
const http = require('http');
const uv = process.binding('uv');
const { async_id_symbol } = process.binding('async_wrap');
const { newUid } = require('internal/async_hooks');

const agent = new http.Agent();
agent.createConnection = common.mustCall((cfg) => {
  const sock = new net.Socket();

  // Fake the handle so we can enforce returning an immediate error
  sock._handle = {
    connect: common.mustCall((req, addr, port) => {
      return uv.UV_ENETUNREACH;
    }),
    readStart() {},
    close() {}
  };

  // Simulate just enough socket handle initialization
  sock[async_id_symbol] = newUid();

  sock.connect(cfg);
  return sock;
});

http.get({
  host: '127.0.0.1',
  port: 1,
  agent
}).on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENETUNREACH');
}));
