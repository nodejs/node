'use strict';
require('../common');

// Test conversion of IP addresses to the format returned
// for addresses in Subject Alternative Name section
// of a TLS certificate

const assert = require('assert');
const tls = require('internal/tls');

assert.strictEqual(tls.canonicalIp('127.0.0.1'), '127.0.0.1');
assert.strictEqual(tls.canonicalIp('010.001.0.1'), '10.1.0.1');
assert.strictEqual(tls.canonicalIp('::1'), '0:0:0:0:0:0:0:1');
assert.strictEqual(tls.canonicalIp('fe80::1'), 'fe80:0:0:0:0:0:0:1');
assert.strictEqual(tls.canonicalIp('fe80::'), 'fe80:0:0:0:0:0:0:0');
assert.strictEqual(
  tls.canonicalIp('fe80::0000:0010:0001'),
  'fe80:0:0:0:0:0:10:1');
