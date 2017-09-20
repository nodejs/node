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

const N = 200;
let recv = '';
let chars_recved = 0;

const server = net.createServer(function(connection) {
  function write(j) {
    if (j >= N) {
      connection.end();
      return;
    }
    setTimeout(function() {
      connection.write('C');
      write(j + 1);
    }, 10);
  }
  write(0);
});

server.on('listening', function() {
  const client = net.createConnection(common.PORT);
  client.setEncoding('ascii');
  client.on('data', function(d) {
    console.log(d);
    recv += d;
  });

  setTimeout(function() {
    chars_recved = recv.length;
    console.log(`pause at: ${chars_recved}`);
    assert.strictEqual(true, chars_recved > 1);
    client.pause();
    setTimeout(function() {
      console.log(`resume at: ${chars_recved}`);
      assert.strictEqual(chars_recved, recv.length);
      client.resume();

      setTimeout(function() {
        chars_recved = recv.length;
        console.log(`pause at: ${chars_recved}`);
        client.pause();

        setTimeout(function() {
          console.log(`resume at: ${chars_recved}`);
          assert.strictEqual(chars_recved, recv.length);
          client.resume();

        }, 500);

      }, 500);

    }, 500);

  }, 500);

  client.on('end', function() {
    server.close();
    client.end();
  });
});
server.listen(common.PORT);

process.on('exit', function() {
  assert.strictEqual(N, recv.length);
  console.error('Exit');
});
