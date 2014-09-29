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

var common = require('../common');
var assert = require('assert');
var http = require('http');

// Verify that ServerResponse.writeHead() works as setHeader.
// Issue 5036 on github.

var s = http.createServer(function(req, res) {
  res.setHeader('test', '1');

  // toLowerCase() is used on the name argument, so it must be a string.
  var threw = false;
  try {
    res.setHeader(0xf00, 'bar');
  } catch (e) {
    assert.ok(e instanceof TypeError);
    threw = true;
  }
  assert.ok(threw, 'Non-string names should throw');

  res.writeHead(200, { Test: '2' });
  res.end();
});

s.listen(common.PORT, runTest);

function runTest() {
  http.get({ port: common.PORT }, function(response) {
    response.on('end', function() {
      assert.equal(response.headers['test'], '2');
      assert(response.rawHeaders.indexOf('Test') !== -1);
      s.close();
    });
    response.resume();
  });
}
