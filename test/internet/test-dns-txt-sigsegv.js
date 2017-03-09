'use strict';
require('../common');
const assert = require('assert');
const dns = require('dns');

dns.resolveTxt('www.microsoft.com', function(err, records) {
  assert.equal(err, null);
  assert.equal(records.length, 0);
});
