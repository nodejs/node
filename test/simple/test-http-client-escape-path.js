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
var util = require('util');

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
  function helper(arg, next) {
    var server = http.createServer(function(req, res) {
      assert.equal(req.url, expected);
      res.end('OK');
      server.close(next);
    });
    server.on('clientError', function(err) {
      throw err;
    });
    server.listen(common.PORT, '127.0.0.1', function() {
      http.get(arg);
    });
  }

  // Go the extra mile to ensure that the behavior of
  // http.get("http://example.com/...") matches http.get({ path: ... }).
  test1();

  function test1() {
    console.log('as url: ' + util.inspect(path));
    helper('http://127.0.0.1:' + common.PORT + path, test2);
  }
  function test2() {
    var options = {
      host: '127.0.0.1',
      port: common.PORT,
      path: path
    };
    console.log('as options: ' + util.inspect(options));
    helper(options, done);
  }
  function done() {
    if (next) next();
  }
}
