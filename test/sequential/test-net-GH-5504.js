'use strict';
var common = require('../common');

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
  var net = require('net');
  var content = Buffer.alloc(64 * 1024 * 1024, '#');
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
  var net = require('net');
  var client = net.connect({
    host: common.localhostIPv4,
    port: common.PORT
  }, function() {
    client.destroy();
  });
}

function parent() {
  var spawn = require('child_process').spawn;
  var node = process.execPath;

  var s = spawn(node, [__filename, 'server'], {
    env: Object.assign(process.env, {
      NODE_DEBUG: 'net'
    })
  });
  var c;

  wrap(s.stderr, process.stderr, 'SERVER 2>');
  wrap(s.stdout, process.stdout, 'SERVER 1>');
  s.on('exit', common.mustCall(function(c) {}));

  s.stdout.once('data', common.mustCall(function() {
    c = spawn(node, [__filename, 'client']);
    wrap(c.stderr, process.stderr, 'CLIENT 2>');
    wrap(c.stdout, process.stdout, 'CLIENT 1>');
    c.on('exit', common.mustCall(function(c) {}));
  }));

  function wrap(inp, out, w) {
    inp.setEncoding('utf8');
    inp.on('data', function(c) {
      c = c.trim();
      if (!c) return;
      out.write(w + c.split('\n').join('\n' + w) + '\n');
    });
  }
}
