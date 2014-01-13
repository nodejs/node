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
var fork = require('child_process').fork;
var net = require('net');

if (process.argv[2] !== 'child') {
  console.error('[%d] master', process.pid);

  var worker = fork(__filename, ['child']);
  var called = false;

  worker.once('message', function(msg, handle) {
    assert.equal(msg, 'handle');
    assert.ok(handle);
    worker.send('got');

    handle.on('data', function(data) {
      called = true;
      assert.equal(data.toString(), 'hello');
    });

    handle.on('end', function() {
      worker.kill();
    });
  });

  process.once('exit', function() {
    assert.ok(called);
  });
} else {
  console.error('[%d] worker', process.pid);

  var server = net.createServer(function(c) {
    process.once('message', function(msg) {
      assert.equal(msg, 'got');
      c.end('hello');
    });
  });
  server.listen(common.PORT, function() {
    var socket = net.connect(common.PORT, '127.0.0.1', function() {
      process.send('handle', socket);
    });
  });

  process.on('disconnect', function() {
    process.exit();
    server.close();
  });
}
