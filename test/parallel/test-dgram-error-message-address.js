'use strict';
var common = require('../common');
var assert = require('assert');
var dgram = require('dgram');

// IPv4 Test
var socket_ipv4 = dgram.createSocket('udp4');

socket_ipv4.on('listening', common.fail);

socket_ipv4.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.port, undefined);
  assert.equal(e.message, 'bind EADDRNOTAVAIL 1.1.1.1');
  assert.equal(e.address, '1.1.1.1');
  assert.equal(e.code, 'EADDRNOTAVAIL');
  socket_ipv4.close();
}));

socket_ipv4.bind(0, '1.1.1.1');

// IPv6 Test
var socket_ipv6 = dgram.createSocket('udp6');

socket_ipv6.on('listening', common.fail);

socket_ipv6.on('error', common.mustCall(function(e) {
  // EAFNOSUPPORT or EPROTONOSUPPORT means IPv6 is disabled on this system.
  var allowed = ['EADDRNOTAVAIL', 'EAFNOSUPPORT', 'EPROTONOSUPPORT'];
  assert.notEqual(allowed.indexOf(e.code), -1);
  assert.strictEqual(e.port, undefined);
  assert.equal(e.message, 'bind ' + e.code + ' 111::1');
  assert.equal(e.address, '111::1');
  socket_ipv6.close();
}));

socket_ipv6.bind(0, '111::1');
