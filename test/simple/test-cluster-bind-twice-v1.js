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

// This test starts two clustered HTTP servers on the same port. It expects the
// first cluster to succeed and the second cluster to fail with EADDRINUSE.

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var fork = require('child_process').fork;
var http = require('http');

var id = process.argv[2];

if (!id) {
  var a = fork(__filename, ['one']);
  var b = fork(__filename, ['two']);

  a.on('message', function(m) {
    assert.equal(m, 'READY');
    b.send('START');
  });

  var ok = false;

  b.on('message', function(m) {
    assert.equal(m, 'EADDRINUSE');
    a.kill();
    b.kill();
    ok = true;
  });

  process.on('exit', function() {
    a.kill();
    b.kill();
    assert(ok);
  });
}
else if (id === 'one') {
  if (cluster.isMaster) cluster.fork();
  http.createServer(assert.fail).listen(common.PORT, function() {
    process.send('READY');
  });
}
else if (id === 'two') {
  if (cluster.isMaster) cluster.fork();
  process.on('message', function(m) {
    assert.equal(m, 'START');
    var server = http.createServer(assert.fail);
    server.listen(common.PORT, assert.fail);
    server.on('error', function(e) {
      assert.equal(e.code, 'EADDRINUSE');
      process.send(e.code);
    });
  });
}
else {
  assert(0); // bad command line argument
}
