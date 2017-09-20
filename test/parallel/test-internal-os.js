// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const getCIDRSuffix = require('internal/os').getCIDRSuffix;

const specs = [
  // valid
  ['128.0.0.0', 'ipv4', 1],
  ['255.0.0.0', 'ipv4', 8],
  ['255.255.255.128', 'ipv4', 25],
  ['255.255.255.255', 'ipv4', 32],
  ['ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff', 'ipv6', 128],
  ['ffff:ffff:ffff:ffff::', 'ipv6', 64],
  ['ffff:ffff:ffff:ff80::', 'ipv6', 57],
  // invalid
  ['255.0.0.1', 'ipv4', null],
  ['255.255.9.0', 'ipv4', null],
  ['255.255.1.0', 'ipv4', null],
  ['ffff:ffff:43::', 'ipv6', null],
  ['ffff:ffff:ffff:1::', 'ipv6', null]
];

specs.forEach(([mask, protocol, expectedSuffix]) => {
  const actualSuffix = getCIDRSuffix(mask, protocol);

  assert.strictEqual(
    actualSuffix, expectedSuffix,
    `Mask: ${mask}, expected: ${expectedSuffix}, actual: ${actualSuffix}`
  );
});
