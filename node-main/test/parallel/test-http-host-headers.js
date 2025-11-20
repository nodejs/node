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
const http = require('http');
const assert = require('assert');
const httpServer = http.createServer(reqHandler);

function reqHandler(req, res) {
  if (req.url === '/setHostFalse5') {
    assert.strictEqual(req.headers.host, undefined);
  } else {
    assert.strictEqual(
      req.headers.host, `localhost:${this.address().port}`,
      `Wrong host header for req[${req.url}]: ${req.headers.host}`);
  }
  res.writeHead(200, {});
  res.end('ok');
}

testHttp();

function testHttp() {

  let counter = 0;

  function cb(res) {
    counter--;
    if (counter === 0) {
      httpServer.close();
    }
    res.resume();
  }

  httpServer.listen(0, (er) => {
    assert.ifError(er);
    http.get({
      method: 'GET',
      path: `/${counter++}`,
      host: 'localhost',
      port: httpServer.address().port,
      rejectUnauthorized: false
    }, cb).on('error', common.mustNotCall());

    http.request({
      method: 'GET',
      path: `/${counter++}`,
      host: 'localhost',
      port: httpServer.address().port,
      rejectUnauthorized: false
    }, cb).on('error', common.mustNotCall()).end();

    http.request({
      method: 'POST',
      path: `/${counter++}`,
      host: 'localhost',
      port: httpServer.address().port,
      rejectUnauthorized: false
    }, cb).on('error', common.mustNotCall()).end();

    http.request({
      method: 'PUT',
      path: `/${counter++}`,
      host: 'localhost',
      port: httpServer.address().port,
      rejectUnauthorized: false
    }, cb).on('error', common.mustNotCall()).end();

    http.request({
      method: 'DELETE',
      path: `/${counter++}`,
      host: 'localhost',
      port: httpServer.address().port,
      rejectUnauthorized: false
    }, cb).on('error', common.mustNotCall()).end();
  });
}
