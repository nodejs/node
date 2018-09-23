'use strict';
const common = require('../common');
const assert = require('assert');

// Here we are testing the HTTP server module's flood prevention mechanism.
// When writeable.write returns false (ie the underlying send() indicated the
// native buffer is full), the HTTP server cork()s the readable part of the
// stream. This means that new requests will not be read (however request which
// have already been read, but are awaiting processing will still be
// processed).

// Normally when the writable stream emits a 'drain' event, the server then
// uncorks the readable stream, although we arent testing that part here.

switch (process.argv[2]) {
  case undefined:
    return parent();
  case 'child':
    return child();
  default:
    throw new Error(`Unexpected value: ${process.argv[2]}`);
}

function parent() {
  const http = require('http');
  const bigResponse = new Buffer(10240).fill('x');
  var gotTimeout = false;
  var childClosed = false;
  var requests = 0;
  var connections = 0;
  var backloggedReqs = 0;

  const server = http.createServer(function(req, res) {
    requests++;
    res.setHeader('content-length', bigResponse.length);
    if (!res.write(bigResponse)) {
      if (backloggedReqs === 0) {
        // Once the native buffer fills (ie write() returns false), the flood
        // prevention should kick in.
        // This means the stream should emit no more 'data' events. However we
        // may still be asked to process more requests if they were read before
        // the flood-prevention mechanism activated.
        setImmediate(() => {
          req.socket.on('data', () => common.fail('Unexpected data received'));
        });
      }
      backloggedReqs++;
    }
    res.end();
  });

  server.on('connection', function(conn) {
    connections++;
  });

  server.listen(common.PORT, function() {
    const spawn = require('child_process').spawn;
    const args = [__filename, 'child'];
    const child = spawn(process.execPath, args, { stdio: 'inherit' });
    child.on('close', function() {
      childClosed = true;
      server.close();
    });

    server.setTimeout(common.platformTimeout(200), function(conn) {
      gotTimeout = true;
      child.kill();
    });
  });

  process.on('exit', function() {
    assert(gotTimeout);
    assert(childClosed);
    assert.equal(connections, 1);
  });
}

function child() {
  const net = require('net');

  const conn = net.connect({ port: common.PORT });

  var req = 'GET / HTTP/1.1\r\nHost: localhost:' +
            common.PORT + '\r\nAccept: */*\r\n\r\n';

  req = new Array(10241).join(req);

  conn.on('connect', function() {
    // Terminate child after flooding.
    setTimeout(function() { conn.destroy(); }, common.platformTimeout(1000));
    write();
  });

  conn.on('drain', write);

  function write() {
    while (false !== conn.write(req, 'ascii'));
  }
}
