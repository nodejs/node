// Flags: --inspect=0
'use strict';
const common = require('../common');
const assert = require('assert');

// With the current behavior of Node.js (at least as late as 8.1.0), this
// test fails with the following error:
// `AssertionError [ERR_ASSERTION]: worker 2 failed to bind port`
// Ideally, there would be a way for the user to opt out of sequential port
// assignment.
//
// Refs: https://github.com/nodejs/node/issues/13343

// This following check should be replaced by common.skipIfInspectorDisabled()
// if moved out of the known_issues directory.
if (process.config.variables.v8_enable_inspector === 0) {
  // When the V8 inspector is disabled, using either --without-inspector or
  // --without-ssl, this test will not fail which it is expected to do.
  // The following fail will allow this test to be skipped by failing it.
  assert.fail('skipping as V8 inspector is disabled');
}

const cluster = require('cluster');
const net = require('net');

common.crashOnUnhandledRejection();

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
