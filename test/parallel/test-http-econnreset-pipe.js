'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

common.refreshTmpDir();

const server = net.createServer((c) => {
  c.end();
});

server.listen(common.PIPE, common.mustCall(() => {
  http.request({ socketPath: common.PIPE, path: '/', method: 'GET' })
    .once('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ECONNRESET');
      assert.strictEqual(err.port, undefined);
      assert.strictEqual(err.host, undefined);
      assert.strictEqual(err.path, common.PIPE);
      server.close();
    }));
}));
