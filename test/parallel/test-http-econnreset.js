'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const server = net.createServer((c) => {
  c.end();
});

server.listen(common.mustCall(() => {
  const port = server.address().port;

  http.get({ port: port, path: '/', host: common.localhostIPv4 })
    .once('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ECONNRESET');
      assert.strictEqual(err.port, port);
      assert.strictEqual(err.host, common.localhostIPv4);
      assert.strictEqual(err.path, undefined);
      server.close();
    }));
}));
