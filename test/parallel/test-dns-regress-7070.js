'use strict';
require('../common');
var assert = require('assert');
var dns = require('dns');

// Should not raise assertion error. Issue #7070
assert.throws(function() { dns.resolveNs([]); }); // bad name
assert.throws(function() { dns.resolveNs(''); }); // bad callback
