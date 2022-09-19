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

const net = require('net');

// This test creates 20 connections to a server and sets the server's
// maxConnections property to 10. The first 10 connections make it through
// and the last 10 connections are rejected.

const N = 20;
let closes = 0;
const waits = [];

const server = net.createServer(common.mustCall(function(connection) {
  connection.write('hello');
  waits.push(function() { connection.end(); });
}, N / 2));

server.listen(0, function() {
  makeConnection(0);
});

server.maxConnections = N / 2;


function makeConnection(index) {
  const c = net.createConnection(server.address().port);
  let gotData = false;

  c.on('connect', function() {
    if (index + 1 < N) {
      makeConnection(index + 1);
    }

    c.on('close', function() {
      console.error(`closed ${index}`);
      closes++;

      if (closes < N / 2) {
        assert.ok(
          server.maxConnections <= index,
          `${index} should not have been one of the first closed connections`
        );
      }

      if (closes === N / 2) {
        let cb;
        console.error('calling wait callback.');
        while ((cb = waits.shift()) !== undefined) {
          cb();
        }
        server.close();
      }

      if (index < server.maxConnections) {
        assert.strictEqual(gotData, true,
                           `${index} didn't get data, but should have`);
      } else {
        assert.strictEqual(gotData, false,
                           `${index} got data, but shouldn't have`);
      }
    });
  });

  c.on('end', function() { c.end(); });

  c.on('data', function(b) {
    gotData = true;
    assert.ok(b.length > 0);
  });

  c.on('error', function(e) {
    // Retry if SmartOS and ECONNREFUSED. See
    // https://github.com/nodejs/node/issues/2663.
    if (common.isSunOS && (e.code === 'ECONNREFUSED')) {
      c.connect(server.address().port);
    }
    console.error(`error ${index}: ${e}`);
  });
}


process.on('exit', function() {
  assert.strictEqual(closes, N);
});
