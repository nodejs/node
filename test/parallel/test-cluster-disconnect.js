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
      socket.on('data', common.mustCall((chunk) => { result += chunk; }));

      // check result
      socket.on('end', common.mustCall(() => {
        cb(result === 'echo');
      }));
    });
  };

  // test both servers created in the cluster
  const testCluster = function(cb) {
    let done = 0;

    for (let i = 0, l = servers; i < l; i++) {
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
