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
const net = require('net');
const http = require('http');

// wget sends an HTTP/1.0 request with Connection: Keep-Alive
//
// Sending back a chunked response to an HTTP/1.0 client would be wrong,
// so what has to happen in this case is that the connection is closed
// by the server after the entity body if the Content-Length was not
// sent.
//
// If the Content-Length was sent, we can probably safely honor the
// keep-alive request, even though HTTP 1.0 doesn't say that the
// connection can be kept open.  Presumably any client sending this
// header knows that it is extending HTTP/1.0 and can handle the
// response.  We don't test that here however, just that if the
// content-length is not provided, that the connection is in fact
// closed.

const server = http.createServer(function(req, res) {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.write('hello ');
  res.write('world\n');
  res.end();
});
server.listen(0);

server.on('listening', common.mustCall(function() {
  const c = net.createConnection(this.address().port);
  let server_response = '';

  c.setEncoding('utf8');

  c.on('connect', function() {
    c.write('GET / HTTP/1.0\r\n' +
            'Connection: Keep-Alive\r\n\r\n');
  });

  c.on('data', function(chunk) {
    console.log(chunk);
    server_response += chunk;
  });

  c.on('end', common.mustCall(function() {
    const m = server_response.split('\r\n\r\n');
    assert.strictEqual(m[1], 'hello world\n');
    console.log('got end');
    c.end();
  }));

  c.on('close', common.mustCall(function() {
    console.log('got close');
    server.close();
  }));
}));
