'use strict';
const common = require('../common');
const { addresses } = require('../common/internet');
const assert = require('assert');
const dns = require('dns');
const dnsPromises = dns.promises;

assert.rejects(
  dnsPromises.resolveTxt(addresses.CNAME_TO_NO_TXT_HOST),
  { code: 'ENODATA' },
).then(common.mustCall());

dns.resolveTxt(addresses.CNAME_TO_NO_TXT_HOST, common.mustCall((err, records) => {
  assert.ok(err instanceof Error);
  assert.strictEqual(err.code, 'ENODATA');
  assert.strictEqual(records, undefined);
}));
