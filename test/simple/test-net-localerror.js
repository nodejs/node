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
var net = require('net');

var server = net.createServer(function(socket) {
  assert.ok(false, 'no clients should connect');
}).listen(common.PORT).on('listening', function() {
  server.unref();

  function test1(next) {
    connect({
      host: '127.0.0.1',
      port: common.PORT,
      localPort: 'foobar',
    },
    'localPort should be a number: foobar',
    next);
  }

  function test2(next) {
    connect({
      host: '127.0.0.1',
      port: common.PORT,
      localAddress: 'foobar',
    },
    'localAddress should be a valid IP: foobar',
    next)
  }

  test1(test2);
})

function connect(opts, msg, cb) {
  var client = net.connect(opts).on('connect', function() {
    assert.ok(false, 'we should never connect');
  }).on('error', function(err) {
    assert.strictEqual(err.message, msg);
    if (cb) cb();
  });
}
