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
const net = require('net');
const util = require('util');

// First, we test an HTTP/1.0 request.
function testHttp10(port, callback) {
  const c = net.createConnection(port);

  c.setEncoding('utf8');

  c.on('connect', () => {
    c.write('GET / HTTP/1.0\r\n\r\n');
  });

  let res_buffer = '';
  c.on('data', (chunk) => {
    res_buffer += chunk;
  });

  c.on('end', function() {
    c.end();
    assert.ok(
      !/x-foo/.test(res_buffer),
      `No trailer in HTTP/1.0 response. Response buffer: ${res_buffer}`
    );
    callback();
  });
}

// Now, we test an HTTP/1.1 request.
function testHttp11(port, callback) {
  const c = net.createConnection(port);

  c.setEncoding('utf8');

  let tid;
  c.on('connect', function() {
    c.write('GET / HTTP/1.1\r\n\r\n');
    tid = setTimeout(common.mustNotCall(), 2000, 'Couldn\'t find last chunk.');
  });

  let res_buffer = '';
  c.on('data', function(chunk) {
    res_buffer += chunk;
    if (/0\r\n/.test(res_buffer)) { // got the end.
      clearTimeout(tid);
      assert.ok(
        /0\r\nx-foo: bar\r\n\r\n$/.test(res_buffer),
        `No trailer in HTTP/1.1 response. Response buffer: ${res_buffer}`
      );
      callback();
    }
  });
}

// Now, see if the client sees the trailers.
function testClientTrailers(port, callback) {
  http.get({ port, path: '/hello', headers: {} }, (res) => {
    res.on('end', function() {
      assert.ok('x-foo' in res.trailers,
                `${util.inspect(res.trailers)} misses the 'x-foo' property`);
      callback();
    });
    res.resume();
  });
}

const server = http.createServer((req, res) => {
  res.writeHead(200, [['content-type', 'text/plain']]);
  res.addTrailers({ 'x-foo': 'bar' });
  res.end('stuff\n');
});
server.listen(0, () => {
  Promise.all([testHttp10, testHttp11, testClientTrailers]
    .map(util.promisify)
    .map((f) => f(server.address().port)))
    .then(() => server.close());
});
