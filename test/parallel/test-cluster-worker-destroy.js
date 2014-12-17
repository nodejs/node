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
 * The goal of this test is to cover the Workers' implementation of
 * Worker.prototype.destroy. Worker.prototype.destroy is called within
 * the worker's context: once when the worker is still connected to the
 * master, and another time when it's not connected to it, so that we cover
 * both code paths.
 */

require('../common');
var cluster = require('cluster');
var assert = require('assert');

var worker1, worker2, workerExited, workerDisconnected;

if (cluster.isMaster) {
  worker1 = cluster.fork();
  worker2 = cluster.fork();

  workerExited = 0;
  workerDisconnected = 0;

  [worker1, worker2].forEach(function(worker) {
    worker.on('disconnect', ondisconnect);
    worker.on('exit', onexit);
  });

  process.on('exit', onProcessExit);

} else {
  if (cluster.worker.id === 1) {
    // Call destroy when worker is disconnected
    cluster.worker.process.on('disconnect', function() {
      cluster.worker.destroy();
    });

    cluster.worker.disconnect();
  } else {
    // Call destroy when worker is not disconnected yet
    cluster.worker.destroy();
  }
}

function onProcessExit() {
  assert.equal(workerExited,
               2,
               'When master exits, all workers should have exited too');
  assert.equal(workerDisconnected,
               2,
               'When master exits, all workers should have disconnected');
}

function ondisconnect() {
  ++workerDisconnected;
}

function onexit() {
  ++workerExited;
}
