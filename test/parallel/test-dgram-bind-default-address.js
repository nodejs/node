'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

// skip test in FreeBSD jails since 0.0.0.0 will resolve to default interface
if (common.inFreeBSDJail) {
  common.skip('In a FreeBSD jail');
  return;
}

dgram.createSocket('udp4').bind(0, common.mustCall(function() {
  assert.strictEqual(typeof this.address().port, 'number');
  assert.ok(isFinite(this.address().port));
  assert.ok(this.address().port > 0);
  assert.strictEqual(this.address().address, '0.0.0.0');
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
  let address = this.address().address;
  if (address === '::ffff:0.0.0.0')
    address = '::';
  assert.strictEqual(address, '::');
  this.close();
}));
