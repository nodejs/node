// In Node 4.2.1 on operating systems other than Linux, this test triggers an
// assertion in cluster.js. The assertion protects against memory leaks.
// https://github.com/nodejs/node/pull/3510

'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const cluster = require('cluster');
cluster.schedulingPolicy = cluster.SCHED_NONE;

if (cluster.isMaster) {
  var conn, worker1, worker2;

  worker1 = cluster.fork();
  worker1.on('message', common.mustCall(function() {
    worker2 = cluster.fork();
    worker2.on('online', function() {
      conn = net.connect(common.PORT, common.mustCall(function() {
        worker1.disconnect();
        worker2.disconnect();
      }));
      conn.on('error', function(e) {
        // ECONNRESET is OK
        if (e.code !== 'ECONNRESET')
          throw e;
      });
    });
  }));

  cluster.on('exit', function(worker, exitCode, signalCode) {
    assert(worker === worker1 || worker === worker2);
    assert.strictEqual(exitCode, 0);
    assert.strictEqual(signalCode, null);
    if (Object.keys(cluster.workers).length === 0)
      conn.destroy();
  });

  return;
}

const server = net.createServer(function(c) {
  c.on('error', function(e) {
    // ECONNRESET is OK, so we don't exit with code !== 0
    if (e.code !== 'ECONNRESET')
      throw e;
  });
  c.end('bye');
});

server.listen(common.PORT, function() {
  process.send('listening');
});
