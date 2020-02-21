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
require('../common');
const assert = require('assert');
const net = require('net');

const N = 1024 * 1024;
const part_N = N / 3;
let chars_recved = 0;
let npauses = 0;

console.log('build big string');
const body = 'C'.repeat(N);

const server = net.createServer((connection) => {
  connection.write(body.slice(0, part_N));
  connection.write(body.slice(part_N, 2 * part_N));
  assert.strictEqual(connection.write(body.slice(2 * part_N, N)), false);
  console.log(`bufferSize: ${connection.bufferSize}`, 'expecting', N);
  assert.ok(connection.bufferSize >= 0 &&
            connection.writableLength <= N);
  connection.end();
});

server.listen(0, () => {
  const port = server.address().port;
  console.log(`server started on port ${port}`);
  let paused = false;
  const client = net.createConnection(port);
  client.setEncoding('ascii');
  client.on('data', (d) => {
    chars_recved += d.length;
    console.log(`got ${chars_recved}`);
    if (!paused) {
      client.pause();
      npauses += 1;
      paused = true;
      console.log('pause');
      const x = chars_recved;
      setTimeout(() => {
        assert.strictEqual(chars_recved, x);
        client.resume();
        console.log('resume');
        paused = false;
      }, 100);
    }
  });

  client.on('end', () => {
    server.close();
    client.end();
  });
});


process.on('exit', () => {
  assert.strictEqual(chars_recved, N);
  assert.strictEqual(npauses > 2, true);
});
