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

function pingPongTest(port, host, on_complete) {
  const N = 100;
  const DELAY = 1;
  let count = 0;
  let client_ended = false;

  const server = net.createServer({ allowHalfOpen: true }, function(socket) {
    socket.setEncoding('utf8');

    socket.on('data', function(data) {
      console.log(data);
      assert.strictEqual(data, 'PING');
      assert.strictEqual(socket.readyState, 'open');
      assert.strictEqual(count <= N, true);
      setTimeout(function() {
        assert.strictEqual(socket.readyState, 'open');
        socket.write('PONG');
      }, DELAY);
    });

    socket.on('timeout', function() {
      console.error('server-side timeout!!');
      assert.strictEqual(false, true);
    });

    socket.on('end', function() {
      console.log('server-side socket EOF');
      assert.strictEqual(socket.readyState, 'writeOnly');
      socket.end();
    });

    socket.on('close', function(had_error) {
      console.log('server-side socket.end');
      assert.strictEqual(had_error, false);
      assert.strictEqual(socket.readyState, 'closed');
      socket.server.close();
    });
  });

  server.listen(port, host, common.mustCall(function() {
    const client = net.createConnection(port, host);

    client.setEncoding('utf8');

    client.on('connect', function() {
      assert.strictEqual(client.readyState, 'open');
      client.write('PING');
    });

    client.on('data', function(data) {
      console.log(data);
      assert.strictEqual(data, 'PONG');
      assert.strictEqual(client.readyState, 'open');

      setTimeout(function() {
        assert.strictEqual(client.readyState, 'open');
        if (count++ < N) {
          client.write('PING');
        } else {
          console.log('closing client');
          client.end();
          client_ended = true;
        }
      }, DELAY);
    });

    client.on('timeout', function() {
      console.error('client-side timeout!!');
      assert.strictEqual(false, true);
    });

    client.on('close', common.mustCall(function() {
      console.log('client.end');
      assert.strictEqual(count, N + 1);
      assert.ok(client_ended);
      if (on_complete) on_complete();
    }));
  }));
}

pingPongTest(common.PORT);
