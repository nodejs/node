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

/* eslint-disable strict */
require('../common');
const assert = require('assert');
const net = require('net');

let binaryString = '';
for (let i = 255; i >= 0; i--) {
  const s = `'\\${i.toString(8)}'`;
  const S = eval(s);
  assert.strictEqual(S.charCodeAt(0), i);
  assert.strictEqual(S, String.fromCharCode(i));
  binaryString += S;
}

// safe constructor
const echoServer = net.Server(function(connection) {
  connection.setEncoding('latin1');
  connection.on('data', function(chunk) {
    connection.write(chunk, 'latin1');
  });
  connection.on('end', function() {
    connection.end();
  });
});
echoServer.listen(0);

let recv = '';

echoServer.on('listening', function() {
  let j = 0;
  const c = net.createConnection({
    port: this.address().port
  });

  c.setEncoding('latin1');
  c.on('data', function(chunk) {
    const n = j + chunk.length;
    while (j < n && j < 256) {
      c.write(String.fromCharCode(j), 'latin1');
      j++;
    }
    if (j === 256) {
      c.end();
    }
    recv += chunk;
  });

  c.on('connect', function() {
    c.write(binaryString, 'binary');
  });

  c.on('close', function() {
    echoServer.close();
  });
});

process.on('exit', function() {
  assert.strictEqual(2 * 256, recv.length);

  const a = recv.split('');

  const first = a.slice(0, 256).reverse().join('');

  const second = a.slice(256, 2 * 256).join('');

  assert.strictEqual(first, second);
});
