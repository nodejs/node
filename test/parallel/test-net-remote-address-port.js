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

var net = require('net');

var conns = 0, conns_closed = 0;

var remoteAddrCandidates = [ '127.0.0.1'];
if (common.hasIPv6) remoteAddrCandidates.push('::ffff:127.0.0.1');

var remoteFamilyCandidates = ['IPv4'];
if (common.hasIPv6) remoteFamilyCandidates.push('IPv6');

var server = net.createServer(function(socket) {
  conns++;
  assert.notEqual(-1, remoteAddrCandidates.indexOf(socket.remoteAddress));
  assert.notEqual(-1, remoteFamilyCandidates.indexOf(socket.remoteFamily));
  assert.ok(socket.remotePort);
  assert.notEqual(socket.remotePort, common.PORT);
  socket.on('end', function() {
    if (++conns_closed == 2) server.close();
  });
  socket.resume();
});

server.listen(common.PORT, 'localhost', function() {
  var client = net.createConnection(common.PORT, 'localhost');
  var client2 = net.createConnection(common.PORT);
  client.on('connect', function() {
    assert.notEqual(-1, remoteAddrCandidates.indexOf(client.remoteAddress));
    assert.notEqual(-1, remoteFamilyCandidates.indexOf(client.remoteFamily));
    assert.equal(common.PORT, client.remotePort);
    client.end();
  });
  client2.on('connect', function() {
    assert.notEqual(-1, remoteAddrCandidates.indexOf(client2.remoteAddress));
    assert.notEqual(-1, remoteFamilyCandidates.indexOf(client2.remoteFamily));
    assert.equal(common.PORT, client2.remotePort);
    client2.end();
  });
});

process.on('exit', function() {
  assert.equal(2, conns);
});
