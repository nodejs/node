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
const http = require('http');

const bufferSize = 5 * 1024 * 1024;
let measuredSize = 0;

const buffer = Buffer.allocUnsafe(bufferSize);
for (let i = 0; i < buffer.length; i++) {
  buffer[i] = i % 256;
}

const server = http.Server(function(req, res) {
  server.close();

  let i = 0;

  req.on('data', (d) => {
    measuredSize += d.length;
    for (let j = 0; j < d.length; j++) {
      assert.strictEqual(d[j], buffer[i]);
      i++;
    }
  });

  req.on('end', common.mustCall(() => {
    assert.strictEqual(measuredSize, bufferSize);
    res.writeHead(200);
    res.write('thanks');
    res.end();
  }));
});

server.listen(0, common.mustCall(() => {
  const req = http.request({
    port: server.address().port,
    method: 'POST',
    path: '/',
    headers: { 'content-length': buffer.length }
  }, common.mustCall((res) => {
    res.setEncoding('utf8');
    let data = '';
    res.on('data', (chunk) => data += chunk);
    res.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'thanks');
    }));
  }));
  req.end(buffer);
}));
