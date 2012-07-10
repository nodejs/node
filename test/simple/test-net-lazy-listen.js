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
var net = require('net');
var fork = require('child_process').fork;

if (process.argv[2] === 'worker') {
  process.once('disconnect', function () {
    process.exit(0);
  });

  process.on('message', function (msg, server) {
    if (msg !== 'server') return;

    server.on('connection', function (socket) {
      socket.end(process.argv[3]);
    });

    process.send('listen');
  });

  process.send('online');
  return;
}

var workers = [];

function spawn(server, id, cb) {
 // create a worker
  var worker = fork(__filename, ['worker', id]);
  workers[id] = worker;

  // wait for ready
  worker.on('message', function (msg) {
    switch (msg) {
      case 'online':
        worker.send('server', server);
        break;
      case 'listen':
        cb(worker);
        break;
      default:
        throw new Error('got wrong message' + msg);
    }
  });

  return worker;
}

// create a server instance there don't take connections
var server = net.createServer().listen(common.PORT, function () {

  setTimeout(function() {
      console.log('spawn worker#0');
      spawn(server, 0, function () {
        console.log('worker#0 spawned');
      });
  }, 250);

  console.log('testing for standby, expect id 0');
  testResponse([0], function () {

    // test with two worker, expect id response to be 0 or 1
    testNewWorker(1, [0, 1], function () {

      // kill worker#0, expect id response to be 1
      testWorkerDeath(0, [1], function () {

        // close server, expect all connections to continue
        testServerClose(server, [1], function () {

          // killing worker#1, expect all connections to fail
          testWorkerDeath(1, [], function () {
            console.log('test complete');
          });
        });
      });
    });
  });
});

function testServerClose(server, expect, cb) {
  console.log('closeing server');
  server.close(function () {
    testResponse(expect, cb);
  });
}

function testNewWorker(workerId, exptect, cb) {
  console.log('spawning worker#' + workerId);
  spawn(server, 1, function () {
    testResponse(exptect, cb);
  });
}

function testWorkerDeath(workerID, exptect, cb) {
  // killing worker#1, expect all connections to fail
  console.log('killing worker#' + workerID);
  workers[workerID].kill();
  workers[workerID].once('exit', function () {
    testResponse(exptect, cb);
  });
}

function testResponse(expect, cb) {
  // make 25 connections
  var count = 25;
  var missing = expect.slice(0);

  var i = count;
  while (i--) {
    makeConnection(function (error, id) {
      if (expect.length === 0) {
        if (error === null) {
          throw new Error('connect should not be possible');
        }
      } else {
        if (expect.indexOf(id) === -1) {
          throw new Error('got unexpected response: ' + id +
                          ', expected: ' + expect.join(', '));
        }

        var index = missing.indexOf(id);
        if (index !== -1) missing.splice(index, 1);
      }

      count -= 1;
      if (count === 0) {
        if (missing.length !== 0) {
          throw new Error('no connection responsed with the following id: ' +
                          missing.join(', '));
        }
        cb();
      }
    });
  }
}

function makeConnection(cb) {
  var client = net.connect({ port: common.PORT });

  var id = null;
  client.once('data', function(data) {
    id = parseInt(data, 10);
  });

  var error = null;
  client.once('error', function (e) {
    error = e;
  });

  client.setTimeout(500);

  client.on('close', function () {
    cb(error, id);
  });
}

process.on('exit', function () {
  workers.forEach(function (worker) {
    try { worker.kill(); } catch(e) {}
  });
});
