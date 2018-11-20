'use strict';
require('../common');
const assert = require('assert');
const dns = require('dns');
const dnsPromises = dns.promises;

(async function() {
  const result = await dnsPromises.resolveTxt('www.microsoft.com');
  assert.strictEqual(result.length, 0);
})();

dns.resolveTxt('www.microsoft.com', function(err, records) {
  assert.strictEqual(err, null);
  assert.strictEqual(records.length, 0);
});
