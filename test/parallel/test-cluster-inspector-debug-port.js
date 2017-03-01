'use strict';
// Flags: --inspect={PORT}
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const cluster = require('cluster');
const debuggerPort = common.PORT;

if (cluster.isMaster) {
  function checkExitCode(code, signal) {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }

  function fork(offset, execArgv) {
    if (execArgv)
      cluster.setupMaster({execArgv});

    const check = common.mustCall(checkExitCode);
    cluster.fork({portSet: debuggerPort + offset}).on('exit', check);
  }

  assert.strictEqual(process.debugPort, debuggerPort);

  fork(1);
  fork(2, ['--inspect']);
  fork(3, [`--inspect=${debuggerPort}`]);
  fork(4, ['--inspect', '--debug']);
  fork(5, [`--debug=${debuggerPort}`, '--inspect']);
  fork(6, ['--inspect', `--debug-port=${debuggerPort}`]);
  fork(7, [`--inspect-port=${debuggerPort}`]);
  fork(8, ['--inspect', `--inspect-port=${debuggerPort}`]);
} else {
  const hasDebugArg = process.execArgv.some(function(arg) {
    return /inspect/.test(arg);
  });

  assert.strictEqual(hasDebugArg, true);
  assert.strictEqual(process.debugPort, +process.env.portSet);
  process.exit();
}
