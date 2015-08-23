'use strict';
const common = require('../common');
const assert = require('assert');

// this test only fails with CentOS 6.3 using kernel version 2.6.32
// On other linuxes and darwin, the `read` call gets an ECONNRESET in
// that case.  On sunos, the `write` call fails with EPIPE.
//
// However, old CentOS will occasionally send an EOF instead of a
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
  const content = new Buffer(64 * 1024 * 1024);
  content.fill('#');
  net.createServer(function(socket) {
    this.close();
    socket.on('end', function() {
      console.error('end');
    });
    socket.on('_socketEnd', function() {
      console.error('_socketEnd');
    });
    socket.write(content);
  }).listen(common.PORT, function() {
    console.log('listening');
  });
}

function client() {
  const net = require('net');
  const client = net.connect({
    host: 'localhost',
    port: common.PORT
  }, function() {
    client.destroy();
  });
}

function parent() {
  const spawn = require('child_process').spawn;
  const node = process.execPath;
  const assert = require('assert');
  var serverExited = false;
  var clientExited = false;
  var serverListened = false;
  const opt = {
    env: {
      NODE_DEBUG: 'net',
      NODE_COMMON_PORT: process.env.NODE_COMMON_PORT,
    }
  };

  process.on('exit', function() {
    assert(serverExited);
    assert(clientExited);
    assert(serverListened);
    console.log('ok');
  });

  setTimeout(function() {
    if (s) s.kill();
    if (c) c.kill();
    setTimeout(function() {
      throw new Error('hang');
    });
  }, common.platformTimeout(2000)).unref();

  const s = spawn(node, [__filename, 'server'], opt);
  var c;

  wrap(s.stderr, process.stderr, 'SERVER 2>');
  wrap(s.stdout, process.stdout, 'SERVER 1>');
  s.on('exit', function(c) {
    console.error('server exited', c);
    serverExited = true;
  });

  s.stdout.once('data', function() {
    serverListened = true;
    c = spawn(node, [__filename, 'client']);
    wrap(c.stderr, process.stderr, 'CLIENT 2>');
    wrap(c.stdout, process.stdout, 'CLIENT 1>');
    c.on('exit', function(c) {
      console.error('client exited', c);
      clientExited = true;
    });
  });

  function wrap(inp, out, w) {
    inp.setEncoding('utf8');
    inp.on('data', function(c) {
      c = c.trim();
      if (!c) return;
      out.write(w + c.split('\n').join('\n' + w) + '\n');
    });
  }
}
