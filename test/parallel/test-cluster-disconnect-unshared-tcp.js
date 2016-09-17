'use strict';
require('../common');
process.env.NODE_CLUSTER_SCHED_POLICY = 'none';

const cluster = require('cluster');
const net = require('net');

if (cluster.isMaster) {
  const unbound = cluster.fork().on('online', bind);

  function bind() {
    cluster.fork({BOUND: 'y'}).on('listening', disconnect);
  }

  function disconnect() {
    unbound.disconnect();
    unbound.on('disconnect', cluster.disconnect);
  }
} else {
  if (process.env.BOUND === 'y') {
    const source = net.createServer();

    source.listen(0);
  }
}
