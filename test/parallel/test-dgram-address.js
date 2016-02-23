'use strict';
var common = require('../common');
var assert = require('assert');
var dgram = require('dgram');

// IPv4 Test
var socket_ipv4 = dgram.createSocket('udp4');
var family_ipv4 = 'IPv4';

socket_ipv4.on('listening', function() {
  var address_ipv4 = socket_ipv4.address();
  assert.strictEqual(address_ipv4.address, common.localhostIPv4);
  assert.strictEqual(address_ipv4.port, common.PORT);
  assert.strictEqual(address_ipv4.family, family_ipv4);
  socket_ipv4.close();
});

socket_ipv4.on('error', function(e) {
  console.log('Error on udp4 socket. ' + e.toString());
  socket_ipv4.close();
});

socket_ipv4.bind(common.PORT, common.localhostIPv4);

// IPv6 Test
var localhost_ipv6 = '::1';
var socket_ipv6 = dgram.createSocket('udp6');
var family_ipv6 = 'IPv6';

socket_ipv6.on('listening', function() {
  var address_ipv6 = socket_ipv6.address();
  assert.strictEqual(address_ipv6.address, localhost_ipv6);
  assert.strictEqual(address_ipv6.port, common.PORT);
  assert.strictEqual(address_ipv6.family, family_ipv6);
  socket_ipv6.close();
});

socket_ipv6.on('error', function(e) {
  console.log('Error on udp6 socket. ' + e.toString());
  socket_ipv6.close();
});

socket_ipv6.bind(common.PORT, localhost_ipv6);
