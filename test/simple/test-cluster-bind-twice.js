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
//
// The test may seem complex but most of it is plumbing that routes messages
// from the child processes back to the supervisor. As a tree it looks something
// like this:
//
//          <supervisor>
//         /            \
//    <master 1>     <master 2>
//       /                \
//  <worker 1>         <worker 2>
//
// The first worker starts a server on a fixed port and fires a ready message
// that is routed to the second worker. When it tries to bind, it expects to
// see an EADDRINUSE error.
//
// See https://github.com/joyent/node/issues/2721 for more details.

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var fork = require('child_process').fork;
var http = require('http');

var id = process.argv[2];

if (!id) {
  var a = fork(__filename, ['one']);
  var b = fork(__filename, ['two']);

  a.on('exit', function(c) {
    if (c) {
      b.send('QUIT');
      throw new Error('A exited with ' + c);
    }
  });

  b.on('exit', function(c) {
    if (c) {
      a.send('QUIT');
      throw new Error('B exited with ' + c);
    }
  });


  a.on('message', function(m) {
    if (typeof m === 'object') return;
    assert.equal(m, 'READY');
    b.send('START');
  });

  var ok = false;

  b.on('message', function(m) {
    if (typeof m === 'object') return; // ignore system messages
    assert.equal(m, 'EADDRINUSE');
    ok = true;
    a.send('QUIT');
    b.send('QUIT');
  });

  process.on('exit', function() {
    assert(ok);
  });
}
else if (id === 'one') {
  if (cluster.isMaster) return startWorker();

  var server = http.createServer(assert.fail).listen(common.PORT, function() {
    process.send('READY');
  });

  process.on('message', function(m) {
    if (m === 'QUIT') process.exit();
  });
}
else if (id === 'two') {
  if (cluster.isMaster) return startWorker();

  var ok = false;
  process.on('exit', function() {
    assert(ok);
  });

  var server = http.createServer(assert.fail);
  process.on('message', function(m) {
    if (typeof m === 'object') return; // ignore system messages
    if (m === 'QUIT') process.exit();
    assert.equal(m, 'START');
    server.listen(common.PORT, assert.fail);
    server.on('error', function(e) {
      assert.equal(e.code, 'EADDRINUSE');
      process.send(e.code);
      ok = true;
    });
  });
}
else {
  assert(0); // bad command line argument
}

function startWorker() {
  var worker = cluster.fork();
  worker.on('exit', process.exit);
  worker.on('message', process.send.bind(process));
  process.on('message', worker.send.bind(worker));
}
