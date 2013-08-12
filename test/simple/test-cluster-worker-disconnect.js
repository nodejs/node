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
var cluster = require('cluster');

if (cluster.isWorker) {
  var http = require('http');
  http.Server(function() {

  }).listen(common.PORT, '127.0.0.1');

} else if (cluster.isMaster) {

  var checks = {
    cluster: {
      emitDisconnect: false,
      emitExit: false,
      callback: false
    },
    worker: {
      emitDisconnect: false,
      emitExit: false,
      state: false,
      suicideMode: false,
      died: false
    }
  };

  // helper function to check if a process is alive
  var alive = function(pid) {
    try {
      process.kill(pid, 0);
      return true;
    } catch (e) {
      return false;
    }
  };

  // start worker
  var worker = cluster.fork();

  // Disconnect worker when it is ready
  worker.once('listening', function() {
    worker.disconnect();
  });

  // Check cluster events
  cluster.once('disconnect', function() {
    checks.cluster.emitDisconnect = true;
  });
  cluster.once('exit', function() {
    checks.cluster.emitExit = true;
  });

  // Check worker events and properties
  worker.once('disconnect', function() {
    checks.worker.emitDisconnect = true;
    checks.worker.suicideMode = worker.suicide;
    checks.worker.state = worker.state;
  });

  // Check that the worker died
  worker.once('exit', function() {
    checks.worker.emitExit = true;
    checks.worker.died = !alive(worker.process.pid);
    process.nextTick(function() {
      process.exit(0);
    });
  });

  process.once('exit', function() {

    var w = checks.worker;
    var c = checks.cluster;

    // events
    assert.ok(w.emitDisconnect, 'Disconnect event did not emit');
    assert.ok(c.emitDisconnect, 'Disconnect event did not emit');
    assert.ok(w.emitExit, 'Exit event did not emit');
    assert.ok(c.emitExit, 'Exit event did not emit');

    // flags
    assert.equal(w.state, 'disconnected', 'The state property was not set');
    assert.equal(w.suicideMode, true, 'Suicide mode was not set');

    // is process alive
    assert.ok(w.died, 'The worker did not die');
  });
}
