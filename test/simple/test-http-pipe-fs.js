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
var fs = require('fs');
var path = require('path');

var file = path.join(common.tmpDir, 'http-pipe-fs-test.txt');
var requests = 0;

var server = http.createServer(function(req, res) {
  ++requests;
  var stream = fs.createWriteStream(file);
  req.pipe(stream);
  stream.on('close', function() {
    res.writeHead(200);
    res.end();
  });
}).listen(common.PORT, function() {
  http.globalAgent.maxSockets = 1;

  for (var i = 0; i < 2; ++i) {
    (function(i) {
      var req = http.request({
        port: common.PORT,
        method: 'POST',
        headers: {
          'Content-Length': 5
        }
      }, function(res) {
        res.on('end', function() {
          common.debug('res' + i + ' end');
          if (i === 2) {
            server.close();
          }
        });
      });
      req.on('socket', function(s) {
        common.debug('req' + i + ' start');
      });
      req.end('12345');
    }(i + 1));
  }
});

process.on('exit', function() {
  assert.equal(requests, 2);
});
