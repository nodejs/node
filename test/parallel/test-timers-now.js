'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { getLibuvNow } = internalBinding('timers');

// Return value of getLibuvNow() should easily fit in a SMI after start-up.
assert(getLibuvNow() < 0x3ffffff);
