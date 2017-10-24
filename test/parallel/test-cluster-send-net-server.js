'use strict';
// Refs: https://github.com/nodejs/node/issues/15556
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isMaster) {
  const subprocess = cluster.fork();
  const server = net.createServer();

  server.listen(0, common.mustCall(() => {
    subprocess.send('server', server);
  }));

  subprocess.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    server.close();
  }));
} else {
  process.on('message', common.mustCall((m, server) => {
    assert.strictEqual(m, 'server');
    assert(server instanceof net.Server);
    assert.strictEqual(server._connectionKey, '-1:null:-1');
    process.disconnect();
  }));
}
