'use strict';
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

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const fork = require('child_process').fork;
const http = require('http');

const id = process.argv[2];

if (!id) {
  const a = fork(__filename, ['one']);
  const b = fork(__filename, ['two']);

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

  let ok = false;

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
} else if (id === 'one') {
  if (cluster.isMaster) return startWorker();

  http.createServer(common.fail).listen(common.PORT, function() {
    process.send('READY');
  });

  process.on('message', function(m) {
    if (m === 'QUIT') process.exit();
  });
} else if (id === 'two') {
  if (cluster.isMaster) return startWorker();

  let ok = false;
  process.on('exit', function() {
    assert(ok);
  });

  const server = http.createServer(common.fail);
  process.on('message', function(m) {
    if (typeof m === 'object') return; // ignore system messages
    if (m === 'QUIT') process.exit();
    assert.equal(m, 'START');
    server.listen(common.PORT, common.fail);
    server.on('error', function(e) {
      assert.equal(e.code, 'EADDRINUSE');
      process.send(e.code);
      ok = true;
    });
  });
} else {
  assert(0); // bad command line argument
}

function startWorker() {
  const worker = cluster.fork();
  worker.on('exit', process.exit);
  worker.on('message', process.send.bind(process));
  process.on('message', worker.send.bind(worker));
}
