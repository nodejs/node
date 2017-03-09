'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// Test on IPv4 Server
const family_ipv4 = 'IPv4';
const server_ipv4 = net.createServer();

server_ipv4.on('error', function(e) {
  console.log('Error on ipv4 socket: ' + e.toString());
});

server_ipv4.listen(common.PORT, common.localhostIPv4, function() {
  const address_ipv4 = server_ipv4.address();
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
const localhost_ipv6 = '::1';
const family_ipv6 = 'IPv6';
const server_ipv6 = net.createServer();

server_ipv6.on('error', function(e) {
  console.log('Error on ipv6 socket: ' + e.toString());
});

server_ipv6.listen(common.PORT, localhost_ipv6, function() {
  const address_ipv6 = server_ipv6.address();
  assert.strictEqual(address_ipv6.address, localhost_ipv6);
  assert.strictEqual(address_ipv6.port, common.PORT);
  assert.strictEqual(address_ipv6.family, family_ipv6);
  server_ipv6.close();
});

// Test without hostname or ip
const anycast_ipv6 = '::';
const server1 = net.createServer();

server1.on('error', function(e) {
  console.log('Error on ip socket: ' + e.toString());
});

// Specify the port number
server1.listen(common.PORT, function() {
  const address = server1.address();
  assert.strictEqual(address.address, anycast_ipv6);
  assert.strictEqual(address.port, common.PORT);
  assert.strictEqual(address.family, family_ipv6);
  server1.close();
});

// Test without hostname or port
const server2 = net.createServer();

server2.on('error', function(e) {
  console.log('Error on ip socket: ' + e.toString());
});

// Don't specify the port number
server2.listen(function() {
  const address = server2.address();
  assert.strictEqual(address.address, anycast_ipv6);
  assert.strictEqual(address.family, family_ipv6);
  server2.close();
});

// Test without hostname, but with a false-y port
const server3 = net.createServer();

server3.on('error', function(e) {
  console.log('Error on ip socket: ' + e.toString());
});

// Specify a false-y port number
server3.listen(0, function() {
  const address = server3.address();
  assert.strictEqual(address.address, anycast_ipv6);
  assert.strictEqual(address.family, family_ipv6);
  server3.close();
});

// Test without hostname, but with port -1
const server4 = net.createServer();

server4.on('error', function(e) {
  console.log('Error on ip socket: ' + e.toString());
});

// Specify -1 as port number
server4.listen(-1, function() {
  const address = server4.address();
  assert.strictEqual(address.address, anycast_ipv6);
  assert.strictEqual(address.family, family_ipv6);
  server4.close();
});
