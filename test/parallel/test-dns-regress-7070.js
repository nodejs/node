'use strict';
require('../common');
const common = require('../common');
const assert = require('assert');
const dns = require('dns');

// Should not raise assertion error. Issue #7070
assert.throws(() => dns.resolveNs([]), // bad name
              common.expectsError({code: 'ERR_INVALID_ARG_TYPE',
                                   type: TypeError}));
assert.throws(() => dns.resolveNs(''), // bad callback
              common.expectsError({code: 'ERR_INVALID_CALLBACK',
                                   type: TypeError}));
