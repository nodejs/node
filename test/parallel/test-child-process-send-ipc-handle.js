'use strict';
const common = require('../common');

// Make sure use shared handle
process.env.NODE_CLUSTER_SCHED_POLICY = 'none';

const net = require('net');
const cluster = require('cluster');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');

if (cluster.isPrimary) {
  const worker = cluster.fork();
  worker.on('exit', common.mustCall((code) => {
    assert.ok(code === 0);
  }));
} else {
  tmpdir.refresh();
  const server = net.createServer();
  server.listen(common.PIPE, common.mustCall(() => {
    server.close(() => {
      process.disconnect();
    });
  }));
}
