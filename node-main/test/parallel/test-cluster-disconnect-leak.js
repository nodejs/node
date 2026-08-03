'use strict';

// Test fails in Node v5.4.0 and passes in v5.4.1 and newer.

const common = require('../common');
const net = require('net');
const cluster = require('cluster');

cluster.schedulingPolicy = cluster.SCHED_NONE;

if (cluster.isPrimary) {
  const worker = cluster.fork();

  // This is the important part of the test: Confirm that `disconnect` fires.
  worker.on('disconnect', common.mustCall());

  // These are just some extra stuff we're checking for good measure...
  worker.on('exit', common.mustCall());
  cluster.on('exit', common.mustCall());

  cluster.disconnect();
  return;
}

const server = net.createServer();

server.listen(0);
