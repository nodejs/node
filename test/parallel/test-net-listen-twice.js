'use strict';
const common = require('../common');
const net = require('net');
const cluster = require('cluster');
const assert = require('assert');

if (cluster.isPrimary) {
  const worker = cluster.fork();
  worker.on('exit', common.mustCall((code) => {
    assert.ok(code === 0);
  }));
} else {
  const server = net.createServer();
  server.listen(common.mustCall(() => {
    server.close(() => process.disconnect());
  }));

  assert.throws(() => server.listen(), {
    code: 'ERR_SERVER_ALREADY_LISTEN',
    name: 'Error'
  });
}
