// Flags: --expose-internals

'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const { kTimeout } = require('internal/timers');

const server = http.createServer((req, res) => {
  // This space is intentionally left blank.
});

server.listen(0, common.localhostIPv4, common.mustCall(() => {
  const port = server.address().port;
  const req = http.get(`http://${common.localhostIPv4}:${port}`);

  req.setTimeout(1);
  req.on('socket', common.mustCall((socket) => {
    assert.strictEqual(socket[kTimeout], null);
    socket.on('connect', common.mustCall(() => {
      assert.strictEqual(socket[kTimeout]._idleTimeout, 1);
    }));
  }));
  req.on('timeout', common.mustCall(() => req.abort()));
  req.on('error', common.mustCall((err) => {
    assert.strictEqual('socket hang up', err.message);
    server.close();
  }));
}));
