'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isWorker) {
  net.createServer((socket) => {
    socket.end('echo');
  }).listen(common.PORT, '127.0.0.1');

  net.createServer((socket) => {
    socket.end('echo');
  }).listen(common.PORT + 1, '127.0.0.1');

} else if (cluster.isMaster) {
  var servers = 2;

  // test a single TCP server
  const testConnection = function(port, cb) {
    var socket = net.connect(port, '127.0.0.1', () => {
      // buffer result
      var result = '';
      socket.on('data', common.mustCall((chunk) => { result += chunk; }));

      // check result
      socket.on('end', common.mustCall(() => {
        cb(result === 'echo');
      }));
    });
  };

  // test both servers created in the cluster
  const testCluster = function(cb) {
    var done = 0;

    for (var i = 0, l = servers; i < l; i++) {
      testConnection(common.PORT + i, (success) => {
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
    var workers = 8;
    var online = 0;

    for (var i = 0, l = workers; i < l; i++) {
      cluster.fork().on('listening', common.mustCall(() => {
        online += 1;
        if (online === workers * servers) {
          cb();
        }
      }, servers));
    }
  };


  const results = {
    start: 0,
    test: 0,
    disconnect: 0
  };

  const test = function(again) {
    //1. start cluster
    startCluster(() => {
      results.start += 1;

      //2. test cluster
      testCluster(() => {
        results.test += 1;

        //3. disconnect cluster
        cluster.disconnect(() => {
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

  process.once('exit', () => {
    assert.strictEqual(results.start, 2);
    assert.strictEqual(results.test, 2);
    assert.strictEqual(results.disconnect, 2);
  });
}
