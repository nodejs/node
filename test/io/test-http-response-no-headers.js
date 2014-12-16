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
var net = require('net');

var expected = {
  '0.9': 'I AM THE WALRUS',
  '1.0': 'I AM THE WALRUS',
  '1.1': ''
};

var gotExpected = false;

function test(httpVersion, callback) {
  process.on('exit', function() {
    assert(gotExpected);
  });

  var server = net.createServer(function(conn) {
    var reply = 'HTTP/' + httpVersion + ' 200 OK\r\n\r\n' +
                expected[httpVersion];

    conn.end(reply);
  });

  server.listen(common.PORT, '127.0.0.1', function() {
    var options = {
      host: '127.0.0.1',
      port: common.PORT
    };

    var req = http.get(options, function(res) {
      var body = '';

      res.on('data', function(data) {
        body += data;
      });

      res.on('end', function() {
        assert.equal(body, expected[httpVersion]);
        gotExpected = true;
        server.close();
        if (callback) process.nextTick(callback);
      });
    });

    req.on('error', function(err) {
      throw err;
    });
  });
}

test('0.9', function() {
  test('1.0', function() {
    test('1.1');
  });
});
