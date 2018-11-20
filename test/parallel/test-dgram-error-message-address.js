var common = require('../common');
var assert = require('assert');
var dgram = require('dgram');

// IPv4 Test
var socket_ipv4 = dgram.createSocket('udp4');

socket_ipv4.on('listening', assert.fail);

socket_ipv4.on('error', common.mustCall(function(e) {
  assert.equal(e.message, 'bind EADDRNOTAVAIL 1.1.1.1:' + common.PORT);
  assert.equal(e.address, '1.1.1.1');
  assert.equal(e.port, common.PORT);
  assert.equal(e.code, 'EADDRNOTAVAIL');
  socket_ipv4.close();
}));

socket_ipv4.bind(common.PORT, '1.1.1.1');

// IPv6 Test
var socket_ipv6 = dgram.createSocket('udp6');
var family_ipv6 = 'IPv6';

socket_ipv6.on('listening', assert.fail);

socket_ipv6.on('error', common.mustCall(function(e) {
  // EAFNOSUPPORT or EPROTONOSUPPORT means IPv6 is disabled on this system.
  var allowed = ['EADDRNOTAVAIL', 'EAFNOSUPPORT', 'EPROTONOSUPPORT'];
  assert.notEqual(allowed.indexOf(e.code), -1);
  assert.equal(e.message, 'bind ' + e.code + ' 111::1:' + common.PORT);
  assert.equal(e.address, '111::1');
  assert.equal(e.port, common.PORT);
  socket_ipv6.close();
}));

socket_ipv6.bind(common.PORT, '111::1');
