'use strict';
require('../common');
var assert = require('assert');
var dns = require('dns');

// Should not raise assertion error. Issue #7070
assert.throws(function() { dns.resolveNs([]); }); // bad name
// The following is no longer valid since dns.resolve returns an
// EventEmitter. The results can be handled by setting event listeners.
//assert.throws(function() { dns.resolveNs(''); }); // bad callback
