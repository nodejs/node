'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isMaster) {
  // Master opens and binds the socket and shares it with the worker.
  cluster.schedulingPolicy = cluster.SCHED_NONE;
  // Hog the TCP port so that when the worker tries to bind, it'll fail.
  const server = net.createServer(common.mustNotCall());

  server.listen(0, common.mustCall(() => {
    const worker = cluster.fork({PORT: server.address().port});
    worker.on('exit', common.mustCall((exitCode) => {
      assert.strictEqual(exitCode, 0);
      server.close();
    }));
  }));
} else {
  assert(process.env.PORT);
  const s = net.createServer(common.mustNotCall());
  s.listen(process.env.PORT, common.mustNotCall('listen should have failed'));
  s.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'EADDRINUSE');
    process.disconnect();
  }));
}
