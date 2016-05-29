'use strict';
var common = require('../common');
var assert = require('assert');
var dgram = require('dgram');

// skip test in FreeBSD jails since 0.0.0.0 will resolve to default interface
if (common.inFreeBSDJail) {
  common.skip('In a FreeBSD jail');
  return;
}

dgram.createSocket('udp4').bind(0, common.mustCall(function() {
  assert.strictEqual(typeof this.address().port, 'number');
  assert.ok(isFinite(this.address().port));
  assert.ok(this.address().port > 0);
  assert.equal(this.address().address, '0.0.0.0');
  this.close();
}));

if (!common.hasIPv6) {
  common.skip('udp6 part of test, because no IPv6 support');
  return;
}

dgram.createSocket('udp6').bind(0, common.mustCall(function() {
  assert.strictEqual(typeof this.address().port, 'number');
  assert.ok(isFinite(this.address().port));
  assert.ok(this.address().port > 0);
  var address = this.address().address;
  if (address === '::ffff:0.0.0.0')
    address = '::';
  assert.equal(address, '::');
  this.close();
}));
