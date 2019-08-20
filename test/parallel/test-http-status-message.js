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
const http = require('http');
const net = require('net');

const s = http.createServer(function(req, res) {
  res.statusCode = 200;
  res.statusMessage = 'Custom Message';
  res.end('');
});

s.listen(0, test);

function test() {
  const bufs = [];
  const client = net.connect(
    this.address().port,
    function() {
      client.write('GET / HTTP/1.1\r\nConnection: close\r\n\r\n');
    }
  );
  client.on('data', function(chunk) {
    bufs.push(chunk);
  });
  client.on('end', function() {
    const head = Buffer.concat(bufs)
      .toString('latin1')
      .split('\r\n')[0];
    assert.strictEqual(head, 'HTTP/1.1 200 Custom Message');
    console.log('ok');
    s.close();
  });
}
