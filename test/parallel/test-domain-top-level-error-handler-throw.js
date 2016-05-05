'use strict';

/*
 * The goal of this test is to make sure that when a top-level error
 * handler throws an error following the handling of a previous error,
 * the process reports the error message from the error thrown in the
 * top-level error handler, not the one from the previous error.
 */

require('../common');

const domainErrHandlerExMessage = 'exception from domain error handler';
const internalExMessage = 'You should NOT see me';

if (process.argv[2] === 'child') {
  var domain = require('domain');
  var d = domain.create();

  d.on('error', function() {
    throw new Error(domainErrHandlerExMessage);
  });

  d.run(function doStuff() {
    process.nextTick(function() {
      throw new Error(internalExMessage);
    });
  });
} else {
  var fork = require('child_process').fork;
  var assert = require('assert');

  var child = fork(process.argv[1], ['child'], {silent: true});
  var stderrOutput = '';
  if (child) {
    child.stderr.on('data', function onStderrData(data) {
      stderrOutput += data.toString();
    });

    child.on('close', function onChildClosed() {
      assert(stderrOutput.indexOf(domainErrHandlerExMessage) !== -1);
      assert(stderrOutput.indexOf(internalExMessage) === -1);
    });

    child.on('exit', function onChildExited(exitCode, signal) {
      var expectedExitCode = 7;
      var expectedSignal = null;

      assert.equal(exitCode, expectedExitCode);
      assert.equal(signal, expectedSignal);
    });
  }
}
