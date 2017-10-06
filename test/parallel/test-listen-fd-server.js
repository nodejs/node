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

'use strict';
const common = require('../common');
if (common.isWindows)
  common.skip('This test is disabled on windows.');

const assert = require('assert');
const http = require('http');
const net = require('net');

switch (process.argv[2]) {
  case 'child': return child();
}

let ok;

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
    let s = '';
    res.on('data', function(c) {
      s += c.toString();
    });
    res.on('end', function() {
      child.kill();
      child.on('exit', function() {
        assert.strictEqual(s, 'hello from child\n');
        assert.strictEqual(res.statusCode, 200);
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
  const server = net.createServer(function(conn) {
    console.error('connection on parent');
    conn.end('hello from parent\n');
  }).listen(0, function() {
    const port = this.address().port;
    console.error('server listening on %d', port);

    const spawn = require('child_process').spawn;
    const child = spawn(process.execPath, [__filename, 'child'], {
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
