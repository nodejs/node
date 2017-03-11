'use strict';
require('../common');
const assert = require('assert');
const dns = require('dns');

// Should not raise assertion error. Issue #7070
assert.throws(function() { dns.resolveNs([]); }); // bad name
assert.throws(function() { dns.resolveNs(''); }); // bad callback
