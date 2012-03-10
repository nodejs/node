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
var net = require('net');

if (cluster.isWorker) {
  net.createServer(function(socket) {
    socket.end('echo');
  }).listen(common.PORT, '127.0.0.1');

  net.createServer(function(socket) {
    socket.end('echo');
  }).listen(common.PORT + 1, '127.0.0.1');

} else if (cluster.isMaster) {

  // test a single TCP server
  var testConnection = function(port, cb) {
    var socket = net.connect(port, '127.0.0.1', function() {
      // buffer result
      var result = '';
      socket.on('data', function(chunk) { result += chunk; });

      // check result
      socket.on('end', function() {
        cb(result === 'echo');
      });
    });
  };

  // test both servers created in the cluster
  var testCluster = function(cb) {
    var servers = 2;
    var done = 0;

    for (var i = 0, l = servers; i < l; i++) {
      testConnection(common.PORT + i, function(sucess) {
        assert.ok(sucess);
        done += 1;
        if (done === servers) {
          cb();
        }
      });
    }
  };

  // start two workers and execute callback when both is listening
  var startCluster = function(cb) {
    var workers = 2;
    var online = 0;

    for (var i = 0, l = workers; i < l; i++) {

      var worker = cluster.fork();
      worker.on('listening', function() {
        online += 1;
        if (online === workers) {
          cb();
        }
      });
    }
  };


  var results = {
    start: 0,
    test: 0,
    disconnect: 0
  };

  var test = function(again) {
    //1. start cluster
    startCluster(function() {
      results.start += 1;

      //2. test cluster
      testCluster(function() {
        results.test += 1;

        //3. disconnect cluster
        cluster.disconnect(function() {
          results.disconnect += 1;

          // run test again to confirm cleanup
          if (again) {
            test();
          }
        });
      });
    });
  };

  test(true);

  process.once('exit', function() {
    assert.equal(results.start, 2);
    assert.equal(results.test, 2);
    assert.equal(results.disconnect, 2);
  });
}
