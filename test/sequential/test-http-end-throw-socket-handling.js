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

// Make sure that throwing in 'end' handler doesn't lock
// up the socket forever.
//
// This is NOT a good way to handle errors in general, but all
// the same, we should not be so brittle and easily broken.

var http = require('http');

var n = 0;
var server = http.createServer(function(req, res) {
  if (++n === 10) server.close();
  res.end('ok');
});

server.listen(common.PORT, function() {
  for (var i = 0; i < 10; i++) {
    var options = { port: common.PORT };

    var req = http.request(options, function (res) {
      res.resume()
      res.on('end', function() {
        throw new Error('gleep glorp');
      });
    });
    req.end();
  }
});

setTimeout(function() {
  process.removeListener('uncaughtException', catcher);
  throw new Error('Taking too long!');
}, 1000).unref();

process.on('uncaughtException', catcher);
var errors = 0;
function catcher() {
  errors++;
}

process.on('exit', function() {
  assert.equal(errors, 10);
  console.log('ok');
});
