'use strict';

const common = require('../common');
const cluster = require('cluster');
const net = require('net');

cluster.schedulingPolicy = cluster.SCHED_RR;

if (cluster.isPrimary) {
  const worker = cluster.fork();
  worker.on('exit', common.mustCall());
} else {
  const server = net.createServer(common.mustNotCall());
  server.listen(0, common.mustCall(() => {
    process.channel.unref();
    server.close();
  }));
}
