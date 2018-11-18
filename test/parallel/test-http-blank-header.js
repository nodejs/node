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

const server = http.createServer(common.mustCall((req, res) => {
  assert.strictEqual(req.method, 'GET');
  assert.strictEqual(req.url, '/blah');
  assert.deepStrictEqual(req.headers, {
    host: 'example.org:443',
    origin: 'http://example.org',
    cookie: ''
  });
}));


server.listen(0, common.mustCall(() => {
  const c = net.createConnection(server.address().port);
  let received = '';

  c.on('connect', common.mustCall(() => {
    c.write('GET /blah HTTP/1.1\r\n' +
            'Host: example.org:443\r\n' +
            'Cookie:\r\n' +
            'Origin: http://example.org\r\n' +
            '\r\n\r\nhello world'
    );
  }));
  c.on('data', common.mustCall((data) => {
    received += data.toString();
  }));
  c.on('end', common.mustCall(() => {
    assert.strictEqual(received, 'HTTP/1.1 400 Bad Request\r\n\r\n');
    c.end();
  }));
  c.on('close', common.mustCall(() => server.close()));
}));
