var common = require('../common');
var assert = require('assert');
var dns = require('dns');

dns.resolveTxt('www.microsoft.com', function(err, records) {
  assert.equal(err, null);
  assert.equal(records.length, 0);
});
