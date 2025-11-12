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
const url = require('url');

const body1_s = '1111111111111111';
const body2_s = '22222';

const server = http.createServer(function(req, res) {
  const body = url.parse(req.url).pathname === '/1' ? body1_s : body2_s;
  res.writeHead(200, {
    'Content-Type': 'text/plain',
    'Content-Length': body.length
  });
  res.end(body);
});
server.listen(0);

let body1 = '';
let body2 = '';

server.on('listening', function() {
  const req1 = http.request({ port: this.address().port, path: '/1' });
  req1.end();
  req1.on('response', function(res1) {
    res1.setEncoding('utf8');

    res1.on('data', function(chunk) {
      body1 += chunk;
    });

    res1.on('end', function() {
      const req2 = http.request({ port: server.address().port, path: '/2' });
      req2.end();
      req2.on('response', function(res2) {
        res2.setEncoding('utf8');
        res2.on('data', function(chunk) { body2 += chunk; });
        res2.on('end', function() { server.close(); });
      });
    });
  });
});

process.on('exit', function() {
  assert.strictEqual(body1_s, body1);
  assert.strictEqual(body2_s, body2);
});
