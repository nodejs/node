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
var cluster = require('cluster');
var net = require('net');

if (process.platform === 'win32') {
  console.log('Skipping test, not reliable on Windows.');
  process.exit(0);
}

if (process.getuid() === 0) {
  console.log('Do not run this test as root.');
  process.exit(0);
}

if (cluster.isMaster) {
  cluster.fork().on('exit', common.mustCall(function(exitCode) {
    assert.equal(exitCode, 0);
  }));
}
else {
  var s = net.createServer(assert.fail);
  s.listen(42, assert.fail.bind(null, 'listen should have failed'));
  s.on('error', common.mustCall(function(err) {
    assert.equal(err.code, 'EACCES');
    process.disconnect();
  }));
}
