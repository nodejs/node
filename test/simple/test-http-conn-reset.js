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

var caughtError = false;

var options = {
  host: '127.0.0.1',
  port: common.PORT
};

// start a tcp server that closes incoming connections immediately
var server = net.createServer(function(client) {
  client.destroy();
  server.close();
});
server.listen(options.port, options.host, onListen);

// do a GET request, expect it to fail
function onListen() {
  var req = http.request(options, function(res) {
    assert.ok(false, 'this should never run');
  });
  req.on('error', function(err) {
    assert.equal(err.code, 'ECONNRESET');
    assert.equal(err.message, 'socket hang up');
    caughtError = true;
  });
  req.end();
}

process.on('exit', function() {
  assert.equal(caughtError, true);
});

