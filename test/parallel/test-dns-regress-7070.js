'use strict';
require('../common');
const assert = require('assert');
const dns = require('dns');

// Should not raise assertion error. Issue #7070
assert.throws(() => dns.resolveNs([]), // bad name
              /^Error: "name" argument must be a string$/);
assert.throws(() => dns.resolveNs(''), // bad callback
              /^Error: "callback" argument must be a function$/);
