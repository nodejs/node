'use strict';

const common = require('../common');

// This test verifies that `tls.connect()` honors the `hints` option.

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const dns = require('dns');
const tls = require('tls');

const hints = 512;

assert.notStrictEqual(hints, dns.ADDRCONFIG);
assert.notStrictEqual(hints, dns.V4MAPPED);
assert.notStrictEqual(hints, dns.ALL);
assert.notStrictEqual(hints, dns.ADDRCONFIG | dns.V4MAPPED);
assert.notStrictEqual(hints, dns.ADDRCONFIG | dns.ALL);
assert.notStrictEqual(hints, dns.V4MAPPED | dns.ALL);
assert.notStrictEqual(hints, dns.ADDRCONFIG | dns.V4MAPPED | dns.ALL);

tls.connect({
  lookup: common.mustCall((host, options) => {
    assert.strictEqual(host, 'localhost');
    assert.deepStrictEqual(options, { family: undefined, hints });
  }),
  hints
});
