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

var common = require('../common');
var assert = require('assert');
var childProcess = require('child_process');

// Child pipe test
if (process.argv[2] === 'pipetest') {
  process.stdout.write('stdout message');
  process.stderr.write('stderr message');

} else if (process.argv[2] === 'ipctest') {
  // Child IPC test
  process.send('message from child');
  process.on('message', function() {
    process.send('got message from master');
  });

} else if (process.argv[2] === 'parent') {
  // Parent | start child pipe test

  var child = childProcess.fork(process.argv[1], ['pipetest'], {silent: true});

  // Allow child process to self terminate
  child._channel.close();
  child._channel = null;

  child.on('exit', function() {
    process.exit(0);
  });

} else {
  // testcase | start parent && child IPC test

  // testing: is stderr and stdout piped to parent
  var args = [process.argv[1], 'parent'];
  var parent = childProcess.spawn(process.execPath, args);

  //got any stderr or std data
  var stdoutData = false;
  parent.stdout.on('data', function() {
    stdoutData = true;
  });
  var stderrData = false;
  parent.stdout.on('data', function() {
    stderrData = true;
  });

  // testing: do message system work when using silent
  var child = childProcess.fork(process.argv[1], ['ipctest'], {silent: true});

  // Manual pipe so we will get errors
  child.stderr.pipe(process.stderr, {end: false});
  child.stdout.pipe(process.stdout, {end: false});

  var childSending = false;
  var childReciveing = false;
  child.on('message', function(message) {
    if (childSending === false) {
      childSending = (message === 'message from child');
    }

    if (childReciveing === false) {
      childReciveing = (message === 'got message from master');
    }

    if (childReciveing === true) {
      child.kill();
    }
  });
  child.send('message to child');

  // Check all values
  process.on('exit', function() {
    // clean up
    child.kill();
    parent.kill();

    // Check std(out|err) pipes
    assert.ok(!stdoutData, 'The stdout socket was piped to parent');
    assert.ok(!stderrData, 'The stderr socket was piped to parent');

    // Check message system
    assert.ok(childSending, 'The child was able to send a message');
    assert.ok(childReciveing, 'The child was able to receive a message');
  });
}
