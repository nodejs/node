'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

if (common.isWindows) {
  common.skip('This test is disabled on windows.');
  return;
}

switch (process.argv[2]) {
  case 'child': return child();
}

var ok;

process.on('exit', function() {
  assert.ok(ok);
});

// WARNING: This is an example of listening on some arbitrary FD number
// that has already been bound elsewhere in advance.  However, binding
// server handles to stdio fd's is NOT a good or reliable way to do
// concurrency in HTTP servers!  Use the cluster module, or if you want
// a more low-level approach, use child process IPC manually.
test(function(child, port) {
  // now make sure that we can request to the child, then kill it.
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
      child.kill();
      child.on('exit', function() {
        assert.equal(s, 'hello from child\n');
        assert.equal(res.statusCode, 200);
        console.log('ok');
        ok = true;
      });
    });
  });
});

function child() {
  // Prevent outliving the parent process in case it is terminated before
  // killing this child process.
  process.on('disconnect', function() {
    console.error('exit on disconnect');
    process.exit(0);
  });

  // start a server on fd=3
  http.createServer(function(req, res) {
    console.error('request on child');
    console.error('%s %s', req.method, req.url, req.headers);
    res.end('hello from child\n');
  }).listen({ fd: 3 }, function() {
    console.error('child listening on fd=3');
    process.send('listening');
  });
}

function test(cb) {
  var server = net.createServer(function(conn) {
    console.error('connection on parent');
    conn.end('hello from parent\n');
  }).listen(0, function() {
    const port = this.address().port;
    console.error('server listening on %d', port);

    var spawn = require('child_process').spawn;
    var child = spawn(process.execPath, [__filename, 'child'], {
      stdio: [ 0, 1, 2, server._handle, 'ipc' ]
    });

    console.log('%j\n', { pid: child.pid });

    // Now close the parent, so that the child is the only thing
    // referencing that handle.  Note that connections will still
    // be accepted, because the child has the fd open.
    server.close();

    child.on('message', function(msg) {
      if (msg === 'listening') {
        cb(child, port);
      }
    });
  });
}
