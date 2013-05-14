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

// Testing to send an handle twice to the parent process.

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

var workers = {
  toStart: 1
};

if (cluster.isMaster) {
  for (var i = 0; i < workers.toStart; ++i) {
    var worker = cluster.fork();
    worker.on('exit', function(code, signal) {
      assert.equal(code, 0, 'Worker exited with an error code');
      assert(!signal, 'Worker exited by a signal');
    });
  }
} else {
  var server = net.createServer(function(socket) {
    process.send('send-handle-1', socket);
    process.send('send-handle-2', socket);
  });

  server.listen(common.PORT, function() {
    var client = net.connect({ host: 'localhost', port: common.PORT });
    client.on('close', function() { cluster.worker.disconnect(); });
    setTimeout(function() { client.end(); }, 50);
  }).on('error', function(e) {
    console.error(e);
    assert(false, 'server.listen failed');
    cluster.worker.disconnect();
  });
}
