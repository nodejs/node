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

var test = 1;

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  if (test === 1) {
    // write should accept string
    res.write('string');
    // write should accept buffer
    res.write(new Buffer('asdf'));

    // write should not accept an Array
    assert.throws(function() {
      res.write(['array']);
    }, TypeError, 'first argument must be a string or Buffer');

    // end should not accept an Array
    assert.throws(function() {
      res.end(['moo']);
    }, TypeError, 'first argument must be a string or Buffer');

    // end should accept string
    res.end('string');
  } else if (test === 2) {
    // end should accept Buffer
    res.end(new Buffer('asdf'));
  }
});

server.listen(common.PORT, function() {
  // just make a request, other tests handle responses
  http.get({port: common.PORT}, function(res) {
    res.resume();
    // lazy serial test, because we can only call end once per request
    test += 1;
    // do it again to test .end(Buffer);
    http.get({port: common.PORT}, function(res) {
      res.resume();
      server.close();
    });
  });
});
