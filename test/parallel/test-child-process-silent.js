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

'use strict';
require('../common');
const assert = require('assert');
const childProcess = require('child_process');

// Child pipe test
if (process.argv[2] === 'pipe') {
  process.stdout.write('stdout message');
  process.stderr.write('stderr message');

} else if (process.argv[2] === 'ipc') {
  // Child IPC test
  process.send('message from child');
  process.on('message', function() {
    process.send('got message from primary');
  });

} else if (process.argv[2] === 'primary') {
  // Primary | start child pipe test

  const child = childProcess.fork(process.argv[1], ['pipe'], { silent: true });

  // Allow child process to self terminate
  child.disconnect();

  child.on('exit', function() {
    process.exit(0);
  });

} else {
  // Testcase | start primary && child IPC test

  // testing: is stderr and stdout piped to primary
  const args = [process.argv[1], 'primary'];
  const primary = childProcess.spawn(process.execPath, args);

  // Got any stderr or std data
  let stdoutData = false;
  primary.stdout.on('data', function() {
    stdoutData = true;
  });
  let stderrData = false;
  primary.stderr.on('data', function() {
    stderrData = true;
  });

  // testing: do message system work when using silent
  const child = childProcess.fork(process.argv[1], ['ipc'], { silent: true });

  // Manual pipe so we will get errors
  child.stderr.pipe(process.stderr, { end: false });
  child.stdout.pipe(process.stdout, { end: false });

  let childSending = false;
  let childReceiving = false;
  child.on('message', function(message) {
    if (childSending === false) {
      childSending = (message === 'message from child');
    }

    if (childReceiving === false) {
      childReceiving = (message === 'got message from primary');
    }

    if (childReceiving === true) {
      child.kill();
    }
  });
  child.send('message to child');

  // Check all values
  process.on('exit', function() {
    // clean up
    child.kill();
    primary.kill();

    // Check std(out|err) pipes
    assert.ok(!stdoutData);
    assert.ok(!stderrData);

    // Check message system
    assert.ok(childSending);
    assert.ok(childReceiving);
  });
}
