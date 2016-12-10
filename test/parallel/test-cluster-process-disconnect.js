'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isMaster) {
  const worker = cluster.fork();
  worker.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0, 'worker did not exit normally');
    assert.strictEqual(signal, null, 'worker did not exit normally');
  }));
} else {
  const net = require('net');
  const server = net.createServer();
  server.listen(common.PORT, common.mustCall(() => {
    process.disconnect();
  }));
}
