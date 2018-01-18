'use strict';
const common = require('../common');
const net = require('net');
const cluster = require('cluster');
const assert = require('assert');

const nbConnectionsPerWorker = 10;

if (cluster.isMaster) {

  cluster.schedulingPolicy = cluster.SCHED_STICKY;

  let clusterId;
  const nbWorkers = 3;

  for (let i = 0; i < nbWorkers; i++) {
    const worker = cluster.fork({ id: i + 1 });

    worker.on('message', common.mustCallAtLeast(function(msg) {

      // save first worker's id
      // all connections should land to this worker (same IP)
      if (!clusterId) {
        clusterId = this.id;
      } else {
        // check if current worker is the same for the first any connection
        assert.strictEqual(this.id, clusterId);
      }
    }, 0));

    worker.on('disconnect', common.mustCall());
    worker.on('exit', common.mustCall());
  }
} else {
  const server = net.createServer();

  server.on('connection', function(socket) {
    process.send('connection');
  });

  server.on('listening', function() {
    for (let i = 0; i < nbConnectionsPerWorker; i++) {
      net.connect(server.address().port, common.localhostIPv4);
    }
    setTimeout(function() {
      cluster.worker.disconnect();
      cluster.worker.destroy();
    }, 100);
  });

  process.on('exit', function() {
    server.close();
  });

  server.listen(0, common.localhostIPv4);
}
