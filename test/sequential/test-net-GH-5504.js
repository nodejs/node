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

// Ref: https://github.com/nodejs/node-v0.x-archive/issues/5504
//
// This test only fails with CentOS 6.3 using kernel version 2.6.32.
// On other Linuxes and macOS, the `read` call gets an ECONNRESET in
// that case.  On sunos, the `write` call fails with EPIPE.
//
// However, old CentOS will occasionally send an EOF instead of an
// ECONNRESET or EPIPE when the client has been destroyed abruptly.
//
// Make sure we don't keep trying to write or read more in that case.

switch (process.argv[2]) {
  case 'server': return server();
  case 'client': return client();
  case undefined: return parent();
  default: throw new Error('wtf');
}

function server() {
  const net = require('net');
  const content = Buffer.alloc(64 * 1024 * 1024, '#');
  net.createServer(function(socket) {
    this.close();
    socket.on('end', function() {
      console.error('end');
    });
    socket.on('_socketEnd', function() {
      console.error('_socketEnd');
    });
    socket.write(content);
  }).listen(common.PORT, common.localhostIPv4, function() {
    console.log('listening');
  });
}

function client() {
  const net = require('net');
  const client = net.connect({
    host: common.localhostIPv4,
    port: common.PORT
  }, function() {
    client.destroy();
  });
}

function parent() {
  const spawn = require('child_process').spawn;
  const node = process.execPath;

  const s = spawn(node, [__filename, 'server'], {
    env: Object.assign(process.env, {
      NODE_DEBUG: 'net'
    })
  });

  wrap(s.stderr, process.stderr, 'SERVER 2>');
  wrap(s.stdout, process.stdout, 'SERVER 1>');
  s.on('exit', common.mustCall());

  s.stdout.once('data', common.mustCall(function() {
    const c = spawn(node, [__filename, 'client']);
    wrap(c.stderr, process.stderr, 'CLIENT 2>');
    wrap(c.stdout, process.stdout, 'CLIENT 1>');
    c.on('exit', common.mustCall());
  }));

  function wrap(inp, out, w) {
    inp.setEncoding('utf8');
    inp.on('data', function(chunk) {
      chunk = chunk.trim();
      if (!chunk) return;
      out.write(`${w}${chunk.split('\n').join(`\n${w}`)}\n`);
    });
  }
}
