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

first();

function first() {
  test('/~username/', '/~username/', second);
}
function second() {
  test('/\'foo bar\'', '/%27foo%20bar%27', third);
}
function third() {
  var expected = '/%3C%3E%22%60%20%0D%0A%09%7B%7D%7C%5C%5E~%60%27';
  test('/<>"` \r\n\t{}|\\^~`\'', expected);
}

function test(path, expected, next) {
  var server = http.createServer(function(req, res) {
    assert.equal(req.url, expected);
    res.end('OK');
    server.close(function() {
      if (next) next();
    });
  });
  server.on('clientError', function(err) {
    throw err;
  });
  var options = {
    host: '127.0.0.1',
    port: common.PORT,
    path: path
  };
  server.listen(options.port, options.host, function() {
    http.get(options);
  });
}
