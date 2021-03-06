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
const spawn = require('child_process').spawn;

switch (process.argv[2]) {
  case 'child': return child();
  case 'parent': return parent();
  default: return test();
}

// Spawn the parent, and listen for it to tell us the pid of the child.
// WARNING: This is an example of listening on some arbitrary FD number
// that has already been bound elsewhere in advance.  However, binding
// server handles to stdio fd's is NOT a good or reliable way to do
// concurrency in HTTP servers!  Use the cluster module, or if you want
// a more low-level approach, use child process IPC manually.
function test() {
  const parent = spawn(process.execPath, [__filename, 'parent'], {
    stdio: [ 0, 'pipe', 2 ]
  });
  let json = '';
  parent.stdout.on('data', function(c) {
    json += c.toString();
    if (json.includes('\n')) next();
  });
  function next() {
    console.error('output from parent = %s', json);
    const child = JSON.parse(json);
    // Now make sure that we can request to the subprocess, then kill it.
    http.get({
      server: 'localhost',
      port: child.port,
      path: '/',
    }).on('response', function(res) {
      let s = '';
      res.on('data', function(c) {
        s += c.toString();
      });
      res.on('end', function() {
        // Kill the subprocess before we start doing asserts.
        // It's really annoying when tests leave orphans!
        process.kill(child.pid, 'SIGKILL');
        try {
          parent.kill();
        } catch {}

        assert.strictEqual(s, 'hello from child\n');
        assert.strictEqual(res.statusCode, 200);
      });
    });
  }
}

// Listen on port, and then pass the handle to the detached child.
// Then output the child's pid, and immediately exit.
function parent() {
  const server = net.createServer(function(conn) {
    conn.end('HTTP/1.1 403 Forbidden\r\n\r\nI got problems.\r\n');
    throw new Error('Should not see connections on parent');
  }).listen(0, function() {
    console.error('server listening on %d', this.address().port);

    const child = spawn(process.execPath, [__filename, 'child'], {
      stdio: [ 0, 1, 2, server._handle ],
      detached: true
    });

    console.log('%j\n', { pid: child.pid, port: this.address().port });

    // Now close the parent, so that the child is the only thing
    // referencing that handle.  Note that connections will still
    // be accepted, because the child has the fd open, but the parent
    // will exit gracefully.
    server.close();
    child.unref();
  });
}

// Run as a child of the parent() mode.
function child() {
  // Start a server on fd=3
  http.createServer(function(req, res) {
    console.error('request on child');
    console.error('%s %s', req.method, req.url, req.headers);
    res.end('hello from child\n');
  }).listen({ fd: 3 }, function() {
    console.error('child listening on fd=3');
  });
}
