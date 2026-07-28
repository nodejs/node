'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isPrimary) {
  cluster.fork();
  cluster.on('listening', common.mustCall(function(worker, address) {
    const port = address.port;
    // Ensure that the port is not 0 or null
    assert(port);
    // Ensure that the port is numerical
    assert.strictEqual(typeof port, 'number');
    worker.kill();
  }));
} else {
  net.createServer(common.mustNotCall()).listen(0);
}
