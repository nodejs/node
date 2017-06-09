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
const http = require('http');
const url = require('url');

// Make sure no exceptions are thrown when receiving malformed HTTP
// requests.

let nrequests_completed = 0;
const nrequests_expected = 1;

const server = http.createServer(function(req, res) {
  console.log('req: ' + JSON.stringify(url.parse(req.url)));

  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('Hello World');
  res.end();

  if (++nrequests_completed === nrequests_expected) server.close();
});
server.listen(0);

server.on('listening', function() {
  const c = net.createConnection(this.address().port);
  c.on('connect', function() {
    c.write('GET /hello?foo=%99bar HTTP/1.1\r\n\r\n');
    c.end();
  });
});

process.on('exit', function() {
  assert.strictEqual(nrequests_expected, nrequests_completed);
});
