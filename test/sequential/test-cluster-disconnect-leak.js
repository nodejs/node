'use strict';

// Test fails in Node v5.4.0 and passes in v5.4.1 and newer.

const common = require('../common');
const net = require('net');
const cluster = require('cluster');

cluster.schedulingPolicy = cluster.SCHED_NONE;

if (cluster.isMaster) {
  // This is the important part of the test: Confirm that `disconnect` fires.
  cluster.fork().on('disconnect', common.mustCall(() => {}));
  cluster.disconnect();
  return;
}

const server = net.createServer();

server.listen(common.PORT, function() {
  process.send('listening');
});
