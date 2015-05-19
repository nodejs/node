'use strict';
var common = require('../common');
var assert = require('assert');

switch (process.argv[2]) {
  case undefined:
    return parent();
  case 'child':
    return child();
  default:
    throw new Error('wtf');
}

function parent() {
  var http = require('http');
  var bigResponse = new Buffer(10240).fill('x');
  var gotTimeout = false;
  var childClosed = false;
  var requests = 0;
  var connections = 0;

  var server = http.createServer(function(req, res) {
    requests++;
    res.setHeader('content-length', bigResponse.length);
    res.end(bigResponse);
  });

  server.on('connection', function(conn) {
    connections++;
  });

  // kill the connection after a bit, verifying that the
  // flood of requests was eventually halted.
  server.setTimeout(200, function(conn) {
    gotTimeout = true;
    conn.destroy();
  });

  server.listen(common.PORT, function() {
    var spawn = require('child_process').spawn;
    var args = [__filename, 'child'];
    var child = spawn(process.execPath, args, { stdio: 'inherit' });
    child.on('close', function(code) {
      assert(!code);
      childClosed = true;
      server.close();
    });
  });

  process.on('exit', function() {
    assert(gotTimeout);
    assert(childClosed);
    assert.equal(connections, 1);
    // The number of requests we end up processing before the outgoing
    // connection backs up and requires a drain is implementation-dependent.
    // We can safely assume is more than 250.
    console.log('server got %d requests', requests);
    assert(requests >= 250);
    console.log('ok');
  });
}

function child() {
  var net = require('net');

  var gotEpipe = false;
  var conn = net.connect({ port: common.PORT });

  var req = 'GET / HTTP/1.1\r\nHost: localhost:' +
            common.PORT + '\r\nAccept: */*\r\n\r\n';

  req = new Array(10241).join(req);

  conn.on('connect', function() {
    write();
  });

  conn.on('drain', write);

  conn.on('error', function(er) {
    gotEpipe = true;
  });

  process.on('exit', function() {
    assert(gotEpipe);
    console.log('ok - child');
  });

  function write() {
    while (false !== conn.write(req, 'ascii'));
  }
}
