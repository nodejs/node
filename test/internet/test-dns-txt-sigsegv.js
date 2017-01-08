'use strict';
require('../common');
const assert = require('assert');
const dns = require('dns');

dns.resolveTxt('www.microsoft.com', function(err, records) {
  assert.strictEqual(err, null);
  assert.strictEqual(records.length, 0);
});
