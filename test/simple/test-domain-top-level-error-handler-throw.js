// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

/*
 * The goal of this test is to make sure that when a top-level error
 * handler throws an error following the handling of a previous error,
 * the process reports the error message from the error thrown in the
 * top-level error handler, not the one from the previous error.
 */

var domainErrHandlerExMessage = 'exception from domain error handler';
var internalExMessage = 'You should NOT see me';

if (process.argv[2] === 'child') {
  var domain = require('domain');
  var d = domain.create();

  d.on('error', function() {
    throw new Error(domainErrHandlerExMessage);
  });

  d.run(function doStuff() {
    process.nextTick(function () {
      throw new Error(internalExMessage);
    });
  });
} else {
  var fork = require('child_process').fork;
  var assert = require('assert');

  function test() {
    var child = fork(process.argv[1], ['child'], {silent:true});
    var gotDataFromStderr = false;
    var stderrOutput = '';
    if (child) {
      child.stderr.on('data', function onStderrData(data) {
        gotDataFromStderr = true;
        stderrOutput += data.toString();
      });

      child.on('exit', function onChildExited(exitCode, signal) {
        assert(gotDataFromStderr);
        assert(stderrOutput.indexOf(domainErrHandlerExMessage) !== -1);
        assert(stderrOutput.indexOf(internalExMessage) === -1);

        var expectedExitCode = 7;
        var expectedSignal = null;

        assert.equal(exitCode, expectedExitCode);
        assert.equal(signal, expectedSignal);
      });
    }
  }

  test();
}
