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
