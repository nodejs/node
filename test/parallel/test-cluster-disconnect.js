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
  }).listen(0, '127.0.0.1');

  net.createServer((socket) => {
    socket.end('echo');
  }).listen(0, '127.0.0.1');
} else if (cluster.isMaster) {
  const servers = 2;
  const serverPorts = new Set();

  // test a single TCP server
  const testConnection = (port, cb) => {
    const socket = net.connect(port, '127.0.0.1', () => {
      // buffer result
      let result = '';
      socket.on('data', (chunk) => { result += chunk; });

      // check result
      socket.on('end', common.mustCall(() => {
        cb(result === 'echo');
        serverPorts.delete(port);
      }));
    });
  };

  // test both servers created in the cluster
  const testCluster = (cb) => {
    let done = 0;
    const portsArray = Array.from(serverPorts);

    for (let i = 0; i < servers; i++) {
      testConnection(portsArray[i], (success) => {
        assert.ok(success);
        done += 1;
        if (done === servers) {
          cb();
        }
      });
    }
  };

  // Start two workers and execute callback when both is listening
  const startCluster = (cb) => {
    const workers = 8;
    let online = 0;

    for (let i = 0, l = workers; i < l; i++) {
      cluster.fork().on('listening', common.mustCall((address) => {
        serverPorts.add(address.port);

        online += 1;
        if (online === workers * servers) {
          cb();
        }
      }, servers));
    }
  };

  const test = (again) => {
    // 1. start cluster
    startCluster(common.mustCall(() => {
      // 2. test cluster
      testCluster(common.mustCall(() => {
        // 3. disconnect cluster
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
