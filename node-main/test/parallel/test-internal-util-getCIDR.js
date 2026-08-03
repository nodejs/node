// Flags: --expose-internals
'use strict';
require('../common');

// These are tests that verify that the subnetmask is used
// to create the correct CIDR address.
// Tests that it returns null if the subnetmask is not in the correct format.
// (ref: https://www.rfc-editor.org/rfc/rfc1878)

const assert = require('node:assert');
const { getCIDR } = require('internal/util');

assert.strictEqual(getCIDR('127.0.0.1', '255.0.0.0', 'IPv4'), '127.0.0.1/8');
assert.strictEqual(getCIDR('127.0.0.1', '255.255.0.0', 'IPv4'), '127.0.0.1/16');

// 242 = 11110010(2)
assert.strictEqual(getCIDR('127.0.0.1', '242.0.0.0', 'IPv4'), null);

assert.strictEqual(getCIDR('::1', 'ffff:ffff:ffff:ffff::', 'IPv6'), '::1/64');
assert.strictEqual(getCIDR('::1', 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff', 'IPv6'), '::1/128');

// ff00:ffff = 11111111 00000000 : 11111111 11111111(2)
assert.strictEqual(getCIDR('::1', 'ffff:ff00:ffff::', 'IPv6'), null);
