'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');
var cluster = require('cluster');

console.error('Cluster listen fd test', process.argv[2] || 'runner');

if (common.isWindows) {
  common.skip('This test is disabled on windows.');
  return;
}

// Process relationship is:
//
// parent: the test main script
//   -> master: the cluster master
//        -> worker: the cluster worker
switch (process.argv[2]) {
  case 'master': return master();
  case 'worker': return worker();
}

var ok;

process.on('exit', function() {
  assert.ok(ok);
});

// spawn the parent, and listen for it to tell us the pid of the cluster.
// WARNING: This is an example of listening on some arbitrary FD number
// that has already been bound elsewhere in advance.  However, binding
// server handles to stdio fd's is NOT a good or reliable way to do
// concurrency in HTTP servers!  Use the cluster module, or if you want
// a more low-level approach, use child process IPC manually.
test(function(parent, port) {
  // now make sure that we can request to the worker, then kill it.
  http.get({
    server: 'localhost',
    port: port,
    path: '/',
  }).on('response', function(res) {
    var s = '';
    res.on('data', function(c) {
      s += c.toString();
    });
    res.on('end', function() {
      // kill the worker before we start doing asserts.
      // it's really annoying when tests leave orphans!
      parent.kill();
      parent.on('exit', function() {
        assert.equal(s, 'hello from worker\n');
        assert.equal(res.statusCode, 200);
        console.log('ok');
        ok = true;
      });
    });
  });
});

function test(cb) {
  console.error('about to listen in parent');
  var server = net.createServer(function(conn) {
    console.error('connection on parent');
    conn.end('hello from parent\n');
  }).listen(0, function() {
    const port = this.address().port;
    console.error('server listening on %d', port);

    var spawn = require('child_process').spawn;
    var master = spawn(process.execPath, [__filename, 'master'], {
      stdio: [ 0, 'pipe', 2, server._handle, 'ipc' ],
      detached: true
    });

    // Now close the parent, so that the master is the only thing
    // referencing that handle.  Note that connections will still
    // be accepted, because the master has the fd open.
    server.close();

    master.on('exit', function(code) {
      console.error('master exited', code);
    });

    master.on('close', function() {
      console.error('master closed');
    });
    console.error('master spawned');
    master.on('message', function(msg) {
      if (msg === 'started worker') {
        cb(master, port);
      }
    });
  });
}

function master() {
  console.error('in master, spawning worker');
  cluster.setupMaster({
    args: [ 'worker' ]
  });
  var worker = cluster.fork();
  worker.on('message', function(msg) {
    if (msg === 'worker ready') {
      process.send('started worker');
    }
  });
  // Prevent outliving our parent process in case it is abnormally killed -
  // under normal conditions our parent kills this process before exiting.
  process.on('disconnect', function() {
    console.error('master exit on disconnect');
    process.exit(0);
  });
}


function worker() {
  console.error('worker, about to create server and listen on fd=3');
  // start a server on fd=3
  http.createServer(function(req, res) {
    console.error('request on worker');
    console.error('%s %s', req.method, req.url, req.headers);
    res.end('hello from worker\n');
  }).listen({ fd: 3 }, function() {
    console.error('worker listening on fd=3');
    process.send('worker ready');
  });
}
