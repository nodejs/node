'use strict';
var common = require('../common');
var assert = require('assert');

var net = require('net');

var conns = 0, conns_closed = 0;

var remoteAddrCandidates = [ common.localhostIPv4 ];
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
  socket.on('close', function() {
    assert.notEqual(-1, remoteAddrCandidates.indexOf(socket.remoteAddress));
    assert.notEqual(-1, remoteFamilyCandidates.indexOf(socket.remoteFamily));
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
  client.on('close', function() {
    assert.notEqual(-1, remoteAddrCandidates.indexOf(client.remoteAddress));
    assert.notEqual(-1, remoteFamilyCandidates.indexOf(client.remoteFamily));
  });
  client2.on('connect', function() {
    assert.notEqual(-1, remoteAddrCandidates.indexOf(client2.remoteAddress));
    assert.notEqual(-1, remoteFamilyCandidates.indexOf(client2.remoteFamily));
    assert.equal(common.PORT, client2.remotePort);
    client2.end();
  });
  client2.on('close', function() {
    assert.notEqual(-1, remoteAddrCandidates.indexOf(client2.remoteAddress));
    assert.notEqual(-1, remoteFamilyCandidates.indexOf(client2.remoteFamily));
  });
});

process.on('exit', function() {
  assert.equal(2, conns);
});
