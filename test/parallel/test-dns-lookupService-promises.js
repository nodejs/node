'use strict';

const common = require('../common');

const assert = require('assert');
const dnsPromises = require('dns').promises;

dnsPromises.lookupService('127.0.0.1', 22).then(common.mustCall((result) => {
  assert(['ssh', '22'].includes(result.service));
  assert.strictEqual(typeof result.hostname, 'string');
  assert.notStrictEqual(result.hostname.length, 0);
}));

// Use an IP from the RFC 5737 test range to cause an error.
// Refs: https://tools.ietf.org/html/rfc5737
assert.rejects(
  () => dnsPromises.lookupService('192.0.2.1', 22),
  { code: /^(?:ENOTFOUND|EAI_AGAIN)$/ }
);
