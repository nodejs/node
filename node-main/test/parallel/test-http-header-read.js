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

// Verify that ServerResponse.getHeader() works correctly even after
// the response header has been sent. Issue 752 on github.

const s = http.createServer(function(req, res) {
  const contentType = 'Content-Type';
  const plain = 'text/plain';
  res.setHeader(contentType, plain);
  assert.ok(!res.headersSent);
  res.writeHead(200);
  assert.ok(res.headersSent);
  res.end('hello world\n');
  // This checks that after the headers have been sent, getHeader works
  // and does not throw an exception (Issue 752)
  assert.strictEqual(plain, res.getHeader(contentType));
});

s.listen(0, runTest);

function runTest() {
  http.get({ port: this.address().port }, function(response) {
    response.on('end', function() {
      s.close();
    });
    response.resume();
  });
}
