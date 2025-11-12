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

// RFC 2616, section 10.2.5:
//
//   The 204 response MUST NOT contain a message-body, and thus is always
//   terminated by the first empty line after the header fields.
//
// Likewise for 304 responses. Verify that no empty chunk is sent when
// the user explicitly sets a Transfer-Encoding header.

test(204);
test(304);

function test(statusCode) {
  const server = http.createServer(common.mustCall((req, res) => {
    res.writeHead(statusCode, { 'Transfer-Encoding': 'chunked' });
    res.end();
    server.close();
  }));

  server.listen(0, common.mustCall(() => {
    const conn = net.createConnection(
      server.address().port,
      common.mustCall(() => {
        conn.write('GET / HTTP/1.1\r\nHost: example.com\r\n\r\n');

        let resp = '';
        conn.setEncoding('utf8');
        conn.on('data', common.mustCall((data) => {
          resp += data;
        }));

        conn.on('end', common.mustCall(() => {
          // Connection: close should be in the response
          assert.match(resp, /^Connection: close\r\n$/m);
          // Make sure this doesn't end with 0\r\n\r\n
          assert.doesNotMatch(resp, /^0\r\n$/m);
        }));
      })
    );
  }));
}
