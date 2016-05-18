'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

// Test on IPv4 Server
var family_ipv4 = 'IPv4';
var server_ipv4 = net.createServer();

server_ipv4.on('error', function(e) {
  console.log('Error on ipv4 socket: ' + e.toString());
});

server_ipv4.listen(common.PORT, common.localhostIPv4, function() {
  var address_ipv4 = server_ipv4.address();
  assert.strictEqual(address_ipv4.address, common.localhostIPv4);
  assert.strictEqual(address_ipv4.port, common.PORT);
  assert.strictEqual(address_ipv4.family, family_ipv4);
  server_ipv4.close();
});

if (!common.hasIPv6) {
  common.skip('ipv6 part of test, no IPv6 support');
  return;
}

// Test on IPv6 Server
var localhost_ipv6 = '::1';
var family_ipv6 = 'IPv6';
var server_ipv6 = net.createServer();

server_ipv6.on('error', function(e) {
  console.log('Error on ipv6 socket: ' + e.toString());
});

server_ipv6.listen(common.PORT, localhost_ipv6, function() {
  var address_ipv6 = server_ipv6.address();
  assert.strictEqual(address_ipv6.address, localhost_ipv6);
  assert.strictEqual(address_ipv6.port, common.PORT);
  assert.strictEqual(address_ipv6.family, family_ipv6);
  server_ipv6.close();
});

// Test without hostname or ip
var anycast_ipv6 = '::';
var server1 = net.createServer();

server1.on('error', function(e) {
  console.log('Error on ip socket: ' + e.toString());
});

// Specify the port number
server1.listen(common.PORT, function() {
  var address = server1.address();
  assert.strictEqual(address.address, anycast_ipv6);
  assert.strictEqual(address.port, common.PORT);
  assert.strictEqual(address.family, family_ipv6);
  server1.close();
});

// Test without hostname or port
var server2 = net.createServer();

server2.on('error', function(e) {
  console.log('Error on ip socket: ' + e.toString());
});

// Don't specify the port number
server2.listen(function() {
  var address = server2.address();
  assert.strictEqual(address.address, anycast_ipv6);
  assert.strictEqual(address.family, family_ipv6);
  server2.close();
});

// Test without hostname, but with a false-y port
var server3 = net.createServer();

server3.on('error', function(e) {
  console.log('Error on ip socket: ' + e.toString());
});

// Specify a false-y port number
server3.listen(0, function() {
  var address = server3.address();
  assert.strictEqual(address.address, anycast_ipv6);
  assert.strictEqual(address.family, family_ipv6);
  server3.close();
});
