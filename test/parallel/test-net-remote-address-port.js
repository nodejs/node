'use strict';
const common = require('../common');
const assert = require('assert');

const net = require('net');

let conns_closed = 0;

const remoteAddrCandidates = [ common.localhostIPv4 ];
if (common.hasIPv6) remoteAddrCandidates.push('::ffff:127.0.0.1');

const remoteFamilyCandidates = ['IPv4'];
if (common.hasIPv6) remoteFamilyCandidates.push('IPv6');

const server = net.createServer(common.mustCall(function(socket) {
  assert.notStrictEqual(-1, remoteAddrCandidates.indexOf(socket.remoteAddress));
  assert.notStrictEqual(-1,
                        remoteFamilyCandidates.indexOf(socket.remoteFamily));
  assert.ok(socket.remotePort);
  assert.notStrictEqual(socket.remotePort, this.address().port);
  socket.on('end', function() {
    if (++conns_closed === 2) server.close();
  });
  socket.on('close', function() {
    assert.notStrictEqual(-1,
                          remoteAddrCandidates.indexOf(socket.remoteAddress));
    assert.notStrictEqual(-1,
                          remoteFamilyCandidates.indexOf(socket.remoteFamily));
  });
  socket.resume();
}, 2));

server.listen(0, 'localhost', function() {
  const client = net.createConnection(this.address().port, 'localhost');
  const client2 = net.createConnection(this.address().port);
  client.on('connect', function() {
    assert.notStrictEqual(-1,
                          remoteAddrCandidates.indexOf(client.remoteAddress));
    assert.notStrictEqual(-1,
                          remoteFamilyCandidates.indexOf(client.remoteFamily));
    assert.strictEqual(client.remotePort, server.address().port);
    client.end();
  });
  client.on('close', function() {
    assert.notStrictEqual(-1,
                          remoteAddrCandidates.indexOf(client.remoteAddress));
    assert.notStrictEqual(-1,
                          remoteFamilyCandidates.indexOf(client.remoteFamily));
  });
  client2.on('connect', function() {
    assert.notStrictEqual(-1,
                          remoteAddrCandidates.indexOf(client2.remoteAddress));
    assert.notStrictEqual(-1,
                          remoteFamilyCandidates.indexOf(client2.remoteFamily));
    assert.strictEqual(client2.remotePort, server.address().port);
    client2.end();
  });
  client2.on('close', function() {
    assert.notStrictEqual(-1,
                          remoteAddrCandidates.indexOf(client2.remoteAddress));
    assert.notStrictEqual(-1,
                          remoteFamilyCandidates.indexOf(client2.remoteFamily));
  });
});
