'use strict';

const common = require('../common');
const assert = require('assert');
// Monkey-patch `net.Server.listen`
const net = require('net');
const cluster = require('cluster');

// Force round-robin scheduling policy
// as Windows defaults to SCHED_NONE
// https://nodejs.org/docs/latest/api/cluster.html#clusterschedulingpolicy
cluster.schedulingPolicy = cluster.SCHED_RR;

// Ensures that the `backlog` is used to create a `net.Server`.
const kExpectedBacklog = 127;
if (cluster.isMaster) {
  const listen = net.Server.prototype.listen;

  net.Server.prototype.listen = common.mustCall(
    function(...args) {
      const options = args[0];
      if (typeof options === 'object') {
        assert(options.backlog, kExpectedBacklog);
      } else {
        assert(args[1], kExpectedBacklog);
      }
      return listen.call(this, ...args);
    }
  );

  const worker = cluster.fork();
  worker.on('message', () => {
    worker.disconnect();
  });
} else {
  const server = net.createServer();

  server.listen({
    host: common.localhostIPv4,
    port: 0,
    backlog: kExpectedBacklog,
  }, common.mustCall(() => {
    process.send(true);
  }));
}
