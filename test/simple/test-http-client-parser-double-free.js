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
var http_common = require('_http_common');

var receivedError = 0;
var receivedClose = 0;

var buf = new Buffer(64 * 1024);
buf.fill('A');

var server = http.createServer(function(req, res) {
  res.write(buf, function() {
    res.socket.write(buf);
    res.end(function() {
      req.socket.destroy();
      server.close();
    });
  });
}).listen(common.PORT, function() {
  var req = http.request({ port: common.PORT, agent: false }, function(res) {
    res.once('readable', function() {
      /* read only one buffer */
      res.read(1);
    });
  });
  req.end();
  req.on('close', function() {
    receivedClose++;
  });
  req.on('error', function() {
    receivedError++;
  });
});

process.on('exit', function() {
  assert.equal(receivedError, 1);
  assert.equal(receivedClose, 1);
  assert.equal(http_common.parsers.list.length, 2);
});
