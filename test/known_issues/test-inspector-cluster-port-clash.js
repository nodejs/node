// Flags: --inspect=0
'use strict';
const common = require('../common');

// With the current behavior of Node.js (at least as late as 8.1.0), this
// test fails with the following error:
// `AssertionError [ERR_ASSERTION]: worker 2 failed to bind port`
// Ideally, there would be a way for the user to opt out of sequential port
// assignment.
//
// Refs: https://github.com/nodejs/node/issues/13343

common.skipIfInspectorDisabled();

const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

const ports = [process.debugPort];
const clashPort = process.debugPort + 2;
function serialFork() {
  return new Promise((res) => {
    const worker = cluster.fork();
    worker.on('error', (err) => assert.fail(err));
    // no common.mustCall since 1 out of 3 should fail
    worker.on('online', () => {
      worker.on('message', common.mustCall((message) => {
        ports.push(message.debugPort);
      }));
    });
    worker.on('exit', common.mustCall((code, signal) => {
      assert.strictEqual(signal, null);
      // worker 2 should fail because of port clash with `server`
      if (code === 12) {
        return assert.fail(`worker ${worker.id} failed to bind port`);
      }
      assert.strictEqual(0, code);
    }));
    worker.on('disconnect', common.mustCall(res));
  });
}

if (cluster.isMaster) {
  cluster.on('online', common.mustCall((worker) => worker.send('dbgport'), 2));

  // block one of the ports with a listening socket
  const server = net.createServer();
  server.listen(clashPort, common.localhostIPv4, common.mustCall(() => {
    // try to fork 3 workers No.2 should fail
    Promise.all([serialFork(), serialFork(), serialFork()])
      .then(common.mustNotCall())
      .catch((err) => console.error(err));
  }));
  server.unref();
} else {
  const sentinel = common.mustCall();
  process.on('message', (message) => {
    if (message !== 'dbgport') return;
    process.send({ debugPort: process.debugPort });
    sentinel();
    process.disconnect();
  });
}
