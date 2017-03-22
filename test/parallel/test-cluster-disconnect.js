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
  const servers = 2;

  // test a single TCP server
  const testConnection = function(port, cb) {
    const socket = net.connect(port, '127.0.0.1', () => {
      // buffer result
      let result = '';
      socket.on('data', (chunk) => { result += chunk; });

      // check result
      socket.on('end', common.mustCall(() => {
        cb(result === 'echo');
      }));
    });
  };

  // test both servers created in the cluster
  const testCluster = function(cb) {
    let done = 0;

    for (let i = 0; i < servers; i++) {
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
    const workers = 8;
    let online = 0;

    for (let i = 0, l = workers; i < l; i++) {
      cluster.fork().on('listening', common.mustCall(() => {
        online += 1;
        if (online === workers * servers) {
          cb();
        }
      }, servers));
    }
  };

  const test = function(again) {
    //1. start cluster
    startCluster(common.mustCall(() => {
      //2. test cluster
      testCluster(common.mustCall(() => {
        //3. disconnect cluster
        cluster.disconnect(common.mustCall(() => {
          // run test again to confirm cleanup
          if (again) {
            test();
          }
        }));
      }));
    }));
  };

  test(true);
}
