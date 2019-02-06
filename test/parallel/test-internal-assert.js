// Flags: --expose-internals
'use strict';

require('../common');

const assert = require('assert');
const internalAssert = require('internal/assert');

// Should not throw.
internalAssert(true);
internalAssert(true, 'fhqwhgads');

assert.throws(() => { internalAssert(false); }, assert.AssertionError);
assert.throws(() => { internalAssert(false, 'fhqwhgads'); },
              { code: 'ERR_ASSERTION', message: 'fhqwhgads' });
