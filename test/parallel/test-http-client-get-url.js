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
const url = require('url');
const URL = url.URL;
const testPath = '/foo?bar';

const server = http.createServer(common.mustCall((req, res) => {
  assert.strictEqual('GET', req.method);
  assert.strictEqual(testPath, req.url);
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.write('hello\n');
  res.end();
}, 3));

server.listen(0, common.localhostIPv4, common.mustCall(() => {
  const u = `http://${common.localhostIPv4}:${server.address().port}${testPath}`;
  http.get(u, common.mustCall(() => {
    http.get(url.parse(u), common.mustCall(() => {
      http.get(new URL(u), common.mustCall(() => {
        server.close();
      }));
    }));
  }));
}));
