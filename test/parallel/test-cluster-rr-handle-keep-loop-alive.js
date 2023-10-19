'use strict';

const common = require('../common');
const cluster = require('cluster');
const net = require('net');
const assert = require('assert');

cluster.schedulingPolicy = cluster.SCHED_RR;

if (cluster.isPrimary) {
  let exited = false;
  const worker = cluster.fork();
  worker.on('exit', () => {
    exited = true;
  });
  setTimeout(() => {
    assert.ok(!exited);
    worker.kill();
  }, 3000);
} else {
  const server = net.createServer(common.mustNotCall());
  server.listen(0, common.mustCall(() => process.channel.unref()));
}
