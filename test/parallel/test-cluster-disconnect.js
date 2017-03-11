'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isWorker) {
  net.createServer(function(socket) {
    socket.end('echo');
  }).listen(common.PORT, '127.0.0.1');

  net.createServer(function(socket) {
    socket.end('echo');
  }).listen(common.PORT + 1, '127.0.0.1');

} else if (cluster.isMaster) {
  const servers = 2;

  // test a single TCP server
  const testConnection = function(port, cb) {
    const socket = net.connect(port, '127.0.0.1', function() {
      // buffer result
      let result = '';
      socket.on('data', function(chunk) { result += chunk; });

      // check result
      socket.on('end', function() {
        cb(result === 'echo');
      });
    });
  };

  // test both servers created in the cluster
  const testCluster = function(cb) {
    let done = 0;

    for (let i = 0, l = servers; i < l; i++) {
      testConnection(common.PORT + i, function(success) {
        assert.ok(success);
        done += 1;
        if (done === servers) {
          cb();
        }
      });
    }
  };

  // start two workers and execute callback when both is listening
  const startCluster = function(cb) {
    const workers = 8;
    let online = 0;

    for (let i = 0, l = workers; i < l; i++) {

      const worker = cluster.fork();
      worker.on('listening', function() {
        online += 1;
        if (online === workers * servers) {
          cb();
        }
      });
    }
  };


  const results = {
    start: 0,
    test: 0,
    disconnect: 0
  };

  const test = function(again) {
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
