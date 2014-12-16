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

var assert = require('assert');
var http = require('http');

// Verify that ServerResponse.getHeader() works correctly even after
// the response header has been sent. Issue 752 on github.

var s = http.createServer(function(req, res) {
  var contentType = 'Content-Type';
  var plain = 'text/plain';
  var userAgent = 'User-Agent';
  var browser = 'Mozilla';
  var expected = {
    'content-type': plain,
    'user-agent': browser
  }
  res.setHeader(contentType, plain);
  res.setHeader(userAgent, browser);
  assert.ok(!res.headersSent);
  res.writeHead(200);
  assert.ok(res.headersSent);
  res.end('hello world\n');
  var headers;
  // This checks that after the headers have been sent, getAllHeaders works
  // and does not throw an exception
  assert.doesNotThrow(
      function() {
        headers = res.getAllHeaders();
      }
  );
  // Check that all fields in headers have the same value as expected, and
  // vice-versa. This ensures that there no fields which *don't* exist in one
  // or the other.
  for (var field in headers) {
    assert.equal(headers[field], expected[field]);
  }
  for (var field in expected) {
    assert.equal(headers[field], expected[field]);
  }
});

s.listen(process.env.PORT, runTest);

function runTest() {
  http.get({ port: process.env.PORT }, function(response) {
    response.on('end', function() {
      s.close();
    });
    response.resume();
  });
}
