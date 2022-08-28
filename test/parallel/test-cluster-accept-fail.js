// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const cluster = require('cluster');
const rr = require('internal/cluster/round_robin_handle');

if (cluster.isPrimary) {
  const distribute = rr.prototype.distribute;
  rr.prototype.distribute = function(err, handle) {
    assert.strictEqual(err, 0);
    handle.close();
    distribute.call(this, -1, undefined);
  };
  cluster.schedulingPolicy = cluster.SCHED_RR;
  cluster.fork();
} else {
  const server = net.createServer(common.mustNotCall());
  server.listen(0, common.mustCall(() => {

    const socket = net.connect(server.address().port);

    socket.on('close', common.mustCall(() => {
      server.close(common.mustCall(() => {
        process.disconnect();
      }));
    }));
  }));
}
