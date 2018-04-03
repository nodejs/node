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
const Countdown = require('../common/countdown');

const server = http.createServer(common.mustCall((req, res) => {
  req.resume();
  res.writeHead(200);
  res.write('');
  setTimeout(() => res.end(req.url), 50);
}, 2));

const countdown = new Countdown(2, () => server.close());

server.on('connect', common.mustCall((req, socket) => {
  socket.write('HTTP/1.1 200 Connection established\r\n\r\n');
  socket.resume();
  socket.on('end', () => socket.end());
}));

server.listen(0, common.mustCall(() => {
  const req = http.request({
    port: server.address().port,
    method: 'CONNECT',
    path: 'google.com:80'
  });
  req.on('connect', common.mustCall((res, socket) => {
    socket.end();
    socket.on('end', common.mustCall(() => {
      doRequest(0);
      doRequest(1);
    }));
    socket.resume();
  }));
  req.end();
}));

function doRequest(i) {
  http.get({
    port: server.address().port,
    path: `/request${i}`
  }, common.mustCall((res) => {
    let data = '';
    res.setEncoding('utf8');
    res.on('data', (chunk) => data += chunk);
    res.on('end', common.mustCall(() => {
      assert.strictEqual(data, `/request${i}`);
      countdown.dec();
    }));
  }));
}
