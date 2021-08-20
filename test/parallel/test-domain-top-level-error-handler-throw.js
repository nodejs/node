'use strict';

// The goal of this test is to make sure that when a top-level error
// handler throws an error following the handling of a previous error,
// the process reports the error message from the error thrown in the
// top-level error handler, not the one from the previous error.

require('../common');

const domainErrHandlerExMessage = 'exception from domain error handler';
const internalExMessage = 'You should NOT see me';

if (process.argv[2] === 'child') {
  const domain = require('domain');
  const d = domain.create();

  d.on('error', function() {
    throw new Error(domainErrHandlerExMessage);
  });

  d.run(function doStuff() {
    process.nextTick(function() {
      throw new Error(internalExMessage);
    });
  });
} else {
  const fork = require('child_process').fork;
  const assert = require('assert');

  const child = fork(process.argv[1], ['child'], { silent: true });
  let stderrOutput = '';
  if (child) {
    child.stderr.on('data', function onStderrData(data) {
      stderrOutput += data.toString();
    });

    child.on('close', function onChildClosed() {
      assert(stderrOutput.includes(domainErrHandlerExMessage));
      assert.strictEqual(stderrOutput.includes(internalExMessage), false);
    });

    child.on('exit', function onChildExited(exitCode, signal) {
      const expectedExitCode = 7;
      const expectedSignal = null;

      assert.strictEqual(exitCode, expectedExitCode);
      assert.strictEqual(signal, expectedSignal);
    });
  }
}
