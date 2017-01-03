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
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

let SLEEP3_COMMAND;
if (!common.isWindows) {
  // Unix.
  SLEEP3_COMMAND = 'sleep 3';
} else {
  // Windows: `choice` is a command built into cmd.exe. Use another cmd process
  // to create a process tree, so we can catch bugs related to it.
  SLEEP3_COMMAND = 'cmd /c choice /t 3 /c X /d X';
}


let success_count = 0;
let error_count = 0;


exec(
  '"' + process.execPath + '" -p -e process.versions',
  function(err, stdout, stderr) {
    if (err) {
      error_count++;
      console.log('error!: ' + err.code);
      console.log('stdout: ' + JSON.stringify(stdout));
      console.log('stderr: ' + JSON.stringify(stderr));
      assert.strictEqual(false, err.killed);
    } else {
      success_count++;
      console.dir(stdout);
    }
  }
);


exec('thisisnotavalidcommand', function(err, stdout, stderr) {
  if (err) {
    error_count++;
    assert.strictEqual('', stdout);
    assert.strictEqual(typeof err.code, 'number');
    assert.notStrictEqual(err.code, 0);
    assert.strictEqual(false, err.killed);
    assert.strictEqual(null, err.signal);
    console.log('error code: ' + err.code);
    console.log('stdout: ' + JSON.stringify(stdout));
    console.log('stderr: ' + JSON.stringify(stderr));
  } else {
    success_count++;
    console.dir(stdout);
    assert.strictEqual(typeof stdout, 'string');
    assert.notStrictEqual(stdout, '');
  }
});


const sleeperStart = new Date();
exec(SLEEP3_COMMAND, { timeout: 50 }, function(err, stdout, stderr) {
  const diff = (new Date()) - sleeperStart;
  console.log('\'sleep 3\' with timeout 50 took %d ms', diff);
  assert.ok(diff < 500);
  assert.ok(err);
  assert.ok(err.killed);
  assert.strictEqual(err.signal, 'SIGTERM');
  assert.strictEqual(stdout, '');
  assert.strictEqual(stderr, '');
});


const startSleep3 = new Date();
const killMeTwice = exec(SLEEP3_COMMAND, {timeout: 1000}, killMeTwiceCallback);

process.nextTick(function() {
  console.log('kill pid %d', killMeTwice.pid);
  // make sure there is no race condition in starting the process
  // the PID SHOULD exist directly following the exec() call.
  assert.strictEqual('number', typeof killMeTwice._handle.pid);
  // Kill the process
  killMeTwice.kill();
});

function killMeTwiceCallback(err, stdout, stderr) {
  const diff = (new Date()) - startSleep3;
  // We should have already killed this process. Assert that the timeout still
  // works and that we are getting the proper callback parameters.
  assert.ok(err);
  assert.ok(err.killed);
  assert.strictEqual(err.signal, 'SIGTERM');
  assert.strictEqual(stdout, '');
  assert.strictEqual(stderr, '');

  // the timeout should still be in effect
  console.log('\'sleep 3\' was already killed. Took %d ms', diff);
  assert.ok(diff < 1500);
}


exec('python -c "print 200000*\'C\'"', {maxBuffer: 1000},
     function(err, stdout, stderr) {
       assert.ok(err);
       assert.ok(/maxBuffer/.test(err.message));
       assert.strictEqual(stdout, '');
       assert.strictEqual(stderr, '');
     });


process.on('exit', function() {
  assert.strictEqual(1, success_count);
  assert.strictEqual(1, error_count);
});
