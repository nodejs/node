'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

// A test to ensure that cluster properly interoperates with the
// --inspect-brk option.

const assert = require('assert');
const cluster = require('cluster');
const debuggerPort = common.PORT;

if (cluster.isMaster) {
  function test(execArgv) {

    cluster.setupMaster({
      execArgv: execArgv,
      stdio: ['pipe', 'pipe', 'pipe', 'ipc', 'pipe']
    });

    const worker = cluster.fork();

    // Debugger listening on port [port].
    worker.process.stderr.once('data', common.mustCall(function() {
      worker.process.kill('SIGTERM');
    }));

    worker.process.on('exit', common.mustCall(function(code, signal) {
      assert.strictEqual(signal, 'SIGTERM');
    }));
  }

  test(['--inspect-brk']);
  test([`--inspect-brk=${debuggerPort}`]);
} else {
  // Cluster worker is at a breakpoint, should not reach here.
  assert.fail('Test failed: cluster worker should be at a breakpoint.');
}
