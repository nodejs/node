// Flags: --expose-internals
'use strict';
require('../common');

// Test conversion of IP addresses to the format returned
// for addresses in Subject Alternative Name section
// of a TLS certificate

const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { canonicalizeIP } = internalBinding('cares_wrap');

assert.strictEqual(canonicalizeIP('127.0.0.1'), '127.0.0.1');
assert.strictEqual(canonicalizeIP('10.1.0.1'), '10.1.0.1');
assert.strictEqual(canonicalizeIP('::1'), '::1');
assert.strictEqual(canonicalizeIP('fe80:0:0:0:0:0:0:1'), 'fe80::1');
assert.strictEqual(canonicalizeIP('fe80:0:0:0:0:0:0:0'), 'fe80::');
assert.strictEqual(canonicalizeIP('fe80::0000:0010:0001'), 'fe80::10:1');
assert.strictEqual(canonicalizeIP('0001:2222:3333:4444:5555:6666:7777:0088'),
                   '1:2222:3333:4444:5555:6666:7777:88');

assert.strictEqual(canonicalizeIP('0001:2222:3333:4444:5555:6666::'),
                   '1:2222:3333:4444:5555:6666::');

assert.strictEqual(canonicalizeIP('a002:B12:00Ba:4444:5555:6666:0:0'),
                   'a002:b12:ba:4444:5555:6666::');

// IPv4 address represented in IPv6
assert.strictEqual(canonicalizeIP('0:0:0:0:0:ffff:c0a8:101'),
                   '::ffff:192.168.1.1');

assert.strictEqual(canonicalizeIP('::ffff:192.168.1.1'),
                   '::ffff:192.168.1.1');
