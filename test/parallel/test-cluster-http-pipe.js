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

// It is not possible to send pipe handles over the IPC pipe on Windows.
if (process.platform === 'win32') {
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var http = require('http');

if (cluster.isMaster) {
  var ok = false;
  var worker = cluster.fork();
  worker.on('message', function(msg) {
    assert.equal(msg, 'DONE');
    ok = true;
  });
  worker.on('exit', function() {
    process.exit();
  });
  process.on('exit', function() {
    assert(ok);
  });
  return;
}

http.createServer(function(req, res) {
  assert.equal(req.connection.remoteAddress, undefined);
  assert.equal(req.connection.localAddress, undefined); // TODO common.PIPE?
  res.writeHead(200);
  res.end('OK');
}).listen(common.PIPE, function() {
  var self = this;
  http.get({ socketPath: common.PIPE, path: '/' }, function(res) {
    res.resume();
    res.on('end', function(err) {
      if (err) throw err;
      process.send('DONE');
      process.exit();
    });
  });
});
