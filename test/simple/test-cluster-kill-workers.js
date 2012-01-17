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

// This test checks that if we kill a cluster master immediately after fork,
// before the worker has time to register itself, that the master will still
// clean up the worker.
// https://github.com/joyent/node/issues/2047

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var fork = require('child_process').fork;

var isTestRunner = process.argv[2] != 'child';

if (isTestRunner) {
  console.log('starting master...');
  var master = fork(__filename, ['child']);

  console.log('master pid =', master.pid);

  var workerPID;

  master.on('message', function(m) {
    console.log('got message from master:', m);
    if (m.workerPID) {
      console.log('worker pid =', m.workerPID);
      workerPID = m.workerPID;
    }
  });

  var gotExit = false;
  var gotKillException = false;

  master.on('exit', function(code) {
    gotExit = true;
    assert(code != 0);
    assert(workerPID > 0);
    try {
      process.kill(workerPID, 0);
    } catch (e) {
      // workerPID is no longer running
      console.log(e);
      assert(e.code == 'ESRCH');
      gotKillException = true;
    }
  });

  process.on('exit', function() {
    assert(gotExit);
    assert(gotKillException);
  });
} else {
  // Cluster stuff.
  if (cluster.isMaster) {
    var worker = cluster.fork();
    process.send({ workerPID: worker.pid });
    // should kill the worker too
    throw new Error('kill master');
  } else {
    setTimeout(function() {
      assert(false, 'worker should have been killed');
    }, 2500);
  }
}

