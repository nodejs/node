'use strict';
require('../common');
var assert = require('assert');
var net = require('net');

assert.equal(net.isIP('127.0.0.1'), 4);
assert.equal(net.isIP('x127.0.0.1'), 0);
assert.equal(net.isIP('example.com'), 0);
assert.equal(net.isIP('0000:0000:0000:0000:0000:0000:0000:0000'), 6);
assert.equal(net.isIP('0000:0000:0000:0000:0000:0000:0000:0000::0000'), 0);
assert.equal(net.isIP('1050:0:0:0:5:600:300c:326b'), 6);
assert.equal(net.isIP('2001:252:0:1::2008:6'), 6);
assert.equal(net.isIP('2001:dead:beef:1::2008:6'), 6);
assert.equal(net.isIP('2001::'), 6);
assert.equal(net.isIP('2001:dead::'), 6);
assert.equal(net.isIP('2001:dead:beef::'), 6);
assert.equal(net.isIP('2001:dead:beef:1::'), 6);
assert.equal(net.isIP('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'), 6);
assert.equal(net.isIP(':2001:252:0:1::2008:6:'), 0);
assert.equal(net.isIP(':2001:252:0:1::2008:6'), 0);
assert.equal(net.isIP('2001:252:0:1::2008:6:'), 0);
assert.equal(net.isIP('2001:252::1::2008:6'), 0);
assert.equal(net.isIP('::2001:252:1:2008:6'), 6);
assert.equal(net.isIP('::2001:252:1:1.1.1.1'), 6);
assert.equal(net.isIP('::2001:252:1:255.255.255.255'), 6);
assert.equal(net.isIP('::2001:252:1:255.255.255.255.76'), 0);
assert.equal(net.isIP('::anything'), 0);
assert.equal(net.isIP('::1'), 6);
assert.equal(net.isIP('::'), 6);
assert.equal(net.isIP('0000:0000:0000:0000:0000:0000:12345:0000'), 0);
assert.equal(net.isIP('0'), 0);
assert.equal(net.isIP(), 0);
assert.equal(net.isIP(''), 0);
assert.equal(net.isIP(null), 0);
assert.equal(net.isIP(123), 0);
assert.equal(net.isIP(true), 0);
assert.equal(net.isIP({}), 0);
assert.equal(net.isIP({ toString: () => '::2001:252:1:255.255.255.255' }), 6);
assert.equal(net.isIP({ toString: () => '127.0.0.1' }), 4);
assert.equal(net.isIP({ toString: () => 'bla' }), 0);

assert.equal(net.isIPv4('127.0.0.1'), true);
assert.equal(net.isIPv4('example.com'), false);
assert.equal(net.isIPv4('2001:252:0:1::2008:6'), false);
assert.equal(net.isIPv4(), false);
assert.equal(net.isIPv4(''), false);
assert.equal(net.isIPv4(null), false);
assert.equal(net.isIPv4(123), false);
assert.equal(net.isIPv4(true), false);
assert.equal(net.isIPv4({}), false);
assert.equal(net.isIPv4({
  toString: () => '::2001:252:1:255.255.255.255'
}), false);
assert.equal(net.isIPv4({ toString: () => '127.0.0.1' }), true);
assert.equal(net.isIPv4({ toString: () => 'bla' }), false);

assert.equal(net.isIPv6('127.0.0.1'), false);
assert.equal(net.isIPv6('example.com'), false);
assert.equal(net.isIPv6('2001:252:0:1::2008:6'), true);
assert.equal(net.isIPv6(), false);
assert.equal(net.isIPv6(''), false);
assert.equal(net.isIPv6(null), false);
assert.equal(net.isIPv6(123), false);
assert.equal(net.isIPv6(true), false);
assert.equal(net.isIPv6({}), false);
assert.equal(net.isIPv6({
  toString: () => '::2001:252:1:255.255.255.255'
}), true);
assert.equal(net.isIPv6({ toString: () => '127.0.0.1' }), false);
assert.equal(net.isIPv6({ toString: () => 'bla' }), false);
